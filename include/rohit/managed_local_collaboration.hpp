// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <rohit/managed_collaboration.hpp>
#include <rohit/managed_record_budget.hpp>

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

namespace rohit::managed {

enum class pending_change_status : std::uint32_t {
  queued = 1,
  accepted = 2,
  conflict = 3,
  denied = 4,
  obsolete_grant = 5,
  failed = 6,
  uncertain = 7,
  discarded = 8
};

// Store-owned local editing and synchronization. Transport and session authentication belong to the host.
template <typename Store, typename Session, typename Policy>
class store_collaboration final : public detail::collaboration_attachment {
public:
  using traits = typename Store::traits_type;
  using storage_type = typename Store::storage_type;
  using id_type = typename Store::id_type;
  using record_types = collaboration_record_types<Session, Policy>;
  using accepted_type = typename record_types::accepted_change;
  using proposal_type = typename record_types::change_proposal;
  using result_type = basic_collaboration_result<Session, Policy>;
  using state_type = std::conditional_t<Store::has_history, collaboration::client_state,
                                        collaboration::pending_client_state>;
  using transaction_type = typename decltype(state_type::transactions)::value_type;
  using checkpoint_type = std::conditional_t<Store::has_history, collaboration::client_checkpoint,
                                             collaboration::pending_client_checkpoint>;

private:
  static constexpr std::string_view checkpoint_binding =
      Store::has_history ? "serializer.collaboration.client.v1"
                         : "serializer.collaboration.pending.v1";
  Store& store_;
  Session session_;
  collaboration_client_options options_;
  std::shared_ptr<state_type> state_;
  std::shared_ptr<state_type> prepared_{};
  std::vector<collaboration::grant_reference> grants_{};
  struct no_history {
    static constexpr std::uint32_t action =
        static_cast<std::uint32_t>(collaboration_history_action::edit);
    static constexpr std::uint64_t target = 0;
  };
  struct history_action_state {
    std::uint32_t action{static_cast<std::uint32_t>(collaboration_history_action::edit)};
    std::uint64_t target{};
    std::vector<std::uint64_t> saved_redo{};
    std::uint64_t saved_tip{};
  };
#if defined(_MSC_VER)
  [[msvc::no_unique_address]]
#else
  [[no_unique_address]]
#endif
  std::conditional_t<Store::has_history, history_action_state, no_history> history_action_{};
  bool prepared_in_place_{};
  bool busy_{};
  bool receive_only_{};
  std::optional<std::uint64_t> last_poll_{};
  std::optional<std::uint64_t> last_sync_{};
  std::function<collaboration::domain()> context_{};
  std::function<accepted_type()> snapshot_{};
  std::function<std::vector<std::shared_ptr<const accepted_type>>(std::uint64_t)> changes_{};
  std::function<std::shared_ptr<const result_type>(const proposal_type&)> submit_{};

  // Keep callbacks, disk hooks and network delivery inside the store's exclusive owner-thread boundary.
  struct operation_scope {
    store_collaboration& owner;
    // Acquire the store boundary before any transport or persistence callback.
    explicit operation_scope(store_collaboration& value) : owner{value} {
      owner.store_.require_idle();
      if (owner.busy_) {
        throw std::logic_error{"Reentrant collaboration operation"};
      }
      owner.busy_ = owner.store_.collaboration_state_.busy = true;
    }
    // Release only transient guards; durable decisions are never rolled back here.
    ~operation_scope() {
      owner.busy_ = owner.store_.collaboration_state_.busy = false;
    }
  };

  // Wrap existing store journal records and synchronization metadata in one durable frame.
  class client_journal final : public detail::journal_sink {
    store_collaboration& owner_;

  public:
    std::unique_ptr<detail::journal_sink> sink;
    // Keep the paired session alive through its owning store.
    explicit client_journal(store_collaboration& owner) : owner_{owner} {}
    // Repair the validated tail using the existing durable adapter.
    void finish_recovery() override {
      sink->finish_recovery();
    }
    // Commit a native operation and matching outbox state in the same frame.
    void append(std::span<const std::uint8_t> prefix, std::span<const std::uint8_t> label,
                std::span<const std::uint8_t> snapshot) override {
      std::size_t record_bytes = 0;
      for (const auto part : {prefix, label, snapshot}) {
        if (part.size() > owner_.options_.max_state_bytes - record_bytes) {
          throw std::length_error{"Client journal record budget exceeded"};
        }
        record_bytes += part.size();
      }
      std::vector<std::uint8_t> record;
      record.reserve(record_bytes);
      for (const auto part : {prefix, label, snapshot}) {
        record.insert(record.end(), part.begin(), part.end());
      }
      const auto bytes = owner_.checkpoint(2, std::move(record),
                                           owner_.prepared_ ? *owner_.prepared_ : *owner_.state_);
      sink->append(bytes);
    }
    // The store already supplied a complete model/session checkpoint.
    void save(const std::vector<std::uint8_t>& bytes) override {
      sink->save(bytes);
    }
    // Expose the underlying durable sequence without creating another cursor.
    std::uint64_t sequence() const noexcept override {
      return sink->sequence();
    }
    // Preserve storage uncertainty through the normal model_store outcome.
    bool needs_recovery() const noexcept override {
      return sink->needs_recovery();
    }
  };

  // Convert a public state enum to its stable checkpoint value.
  static std::uint32_t status(pending_change_status value) {
    return static_cast<std::uint32_t>(value);
  }
  // Index only validated, contiguous transaction numbers.
  static auto& transaction(auto& state, std::uint64_t number) {
    if constexpr (Store::has_history) {
      if (number == 0 || number > state.transactions.size()) {
        throw std::invalid_argument{"Unknown local transaction"};
      }
      return state.transactions[static_cast<std::size_t>(number - 1)];
    } else {
      const auto found =
          std::ranges::lower_bound(state.transactions, number, {}, &transaction_type::number);
      if (found == state.transactions.end() || found->number != number) {
        throw std::invalid_argument{"Unknown pending transaction"};
      }
      return *found;
    }
  }
  // History-free transactions can only submit ordinary edits.
  static std::uint32_t action(const transaction_type& item) {
    if constexpr (Store::has_history) {
      return item.action;
    } else {
      static_cast<void>(item);
      return static_cast<std::uint32_t>(collaboration_history_action::edit);
    }
  }
  // Check retained state against recovery limits without encoding or allocating a checkpoint.
  void check_state_budget(const state_type& state) const {
    if (state.transactions.size() > options_.max_transactions) {
      throw std::length_error{"Client transaction budget exhausted"};
    }
    auto limits = store_.options_.decode;
    limits.max_input_bytes = options_.max_state_bytes;
    detail::record_budget budget{limits};
    budget.check(detail::checkpoint_view<state_type>{2, {}, state, checkpoint_binding});
  }
  // Encode a coherent checkpoint only for explicit Save or attached durable publication.
  std::vector<std::uint8_t> checkpoint(std::uint32_t kind, std::vector<std::uint8_t> model,
                                       const state_type& state) const {
    auto limits = store_.options_.decode;
    limits.max_input_bytes = options_.max_state_bytes;
    detail::record_budget budget{limits};
    budget.check(detail::checkpoint_view<state_type>{kind, model, state, checkpoint_binding});
    return encode_collaboration_record(
        detail::checkpoint_view<state_type>{kind, model, state, checkpoint_binding},
        options_.max_state_bytes);
  }
  // Export the original managed envelope without recursively wrapping the attached session.
  std::vector<std::uint8_t> model_bytes(const Store& store) const {
    auto bytes = store.encode_state(store.snapshot_, store.history_, store.allocated_id_);
    store.check_journal_envelope(bytes);
    return bytes;
  }
  // Decode a generated model through the store's existing limits.
  storage_type decode(const std::vector<std::uint8_t>& bytes) const {
    return detail::decode<storage_type>(bytes, store_.options_.decode);
  }
  // Bound model encoding separately from retained client metadata.
  std::vector<std::uint8_t> encode(const storage_type& value) const {
    return detail::encode(value, store_.options_.max_snapshot_bytes);
  }
  // Validate and prepare a replacement through the existing model-store identity and schema checks.
  std::unique_ptr<Store> replacement(const state_type& state,
                                     const std::vector<std::uint8_t>& bytes,
                                     std::uint64_t allocated) const {
    auto result =
        std::unique_ptr<Store>{new Store{typename Store::recovery_tag{}, store_.options_, {}}};
    result->document_ = {state.context.document_high, state.context.document_low};
    result->allocated_id_ = allocated;
    auto value = decode(bytes);
    if (state_->context.epoch != 0 && value.persistent_id != store_.current_->persistent_id) {
      throw std::invalid_argument{"Synchronization changes the document root identity"};
    }
    result->identities_ = result->inspect(value, allocated);
    for (const auto& [id, type] : result->identities_) {
      const auto old = store_.identities_.find(id);
      if (state_->context.epoch != 0 && old != store_.identities_.end() && old->second != type) {
        throw std::invalid_argument{"Synchronization retypes a local identity"};
      }
    }
    store_.validate(value);
    result->current_ = std::make_shared<const storage_type>(std::move(value));
    result->snapshot_ = bytes;
    if constexpr (Store::has_history) {
      if constexpr (requires { result->history_.revision_high_water; }) {
        result->history_.current_revision =
            detail::next_collaboration_sequence(store_.history_.revision_high_water);
        result->history_.revision_high_water = result->history_.current_revision;
      }
      result->history_ = result->make_history();
    }
    return result;
  }
  // Publish only preallocated model/session state after its single durable decision.
  void swap_model(Store& replacement) noexcept {
    store_.current_.swap(replacement.current_);
    store_.snapshot_.swap(replacement.snapshot_);
    store_.identities_.swap(replacement.identities_);
    if constexpr (Store::has_history) {
      store_.history_.swap(replacement.history_);
    }
    store_.document_ = replacement.document_;
    store_.allocated_id_ = replacement.allocated_id_;
  }
  // Metadata-only decisions also precede any network send or model publication.
  void commit(state_type next, std::unique_ptr<Store> model = {}) {
    if constexpr (!Store::has_history) {
      std::erase_if(next.transactions, [&](const auto& item) {
        return item.status == status(pending_change_status::discarded) ||
               (item.status == status(pending_change_status::accepted) &&
                item.accepted_sequence <= next.sequence);
      });
    }
    auto prepared = std::make_shared<state_type>(std::move(next));
    check_state_budget(*prepared);
    if constexpr (Store::has_journal) {
      if (store_.journal_state_.sink) {
        // Metadata-only decisions reuse the current durable model instead of encoding it again.
        const auto bytes =
            model ? checkpoint(1, model_bytes(*model), *prepared) : checkpoint(2, {}, *prepared);
        auto& journal = static_cast<client_journal&>(*store_.journal_state_.sink);
        store_.journal_operation([&] { journal.sink->append(bytes); });
      }
    }
    if (model) {
      swap_model(*model);
    }
    state_.swap(prepared);
  }
  // Reject cross-document, wrong-schema or incompatible transport baselines before translating IDs.
  void check_context(const collaboration::domain& context, bool joining = false) const {
    if (context.protocol_version != record_types::model_protocol ||
        context.schema_id != traits::schema_id ||
        context.id_bits != std::numeric_limits<id_type>::digits || context.epoch == 0 ||
        (context.document_high == 0 && context.document_low == 0) ||
        (!joining && (context.document_high != state_->context.document_high ||
                      context.document_low != state_->context.document_low))) {
      throw std::invalid_argument{"Incompatible collaboration authority"};
    }
  }
  // Decode authoritative IDs into stable client-local identities, retaining mappings for deleted objects.
  storage_type translate(state_type& state, const accepted_type& batch,
                         std::uint64_t& allocated) const {
    if (batch.allocated_id > std::numeric_limits<id_type>::max()) {
      throw std::invalid_argument{"Invalid authoritative allocation watermark"};
    }
    auto value = decode(batch.snapshot);
    std::map<std::uint64_t, std::uint64_t> reverse;
    for (const auto& pair : state.identities) {
      reverse.emplace(pair.remote, pair.local);
    }
    traits::visit_entities(value, [&](id_type& id, std::string_view) {
      if (id == 0 || id > batch.allocated_id) {
        throw std::invalid_argument{"Invalid authoritative entity identity"};
      }
      auto found = reverse.find(id);
      if (found == reverse.end()) {
        if (allocated == std::numeric_limits<id_type>::max() ||
            state.identities.size() >= collaboration_options{}.max_entities) {
          throw std::length_error{"Client identity budget exhausted"};
        }
        const auto local = ++allocated;
        state.identities.push_back({local, id});
        found = reverse.emplace(id, local).first;
      }
      id = static_cast<id_type>(found->second);
    });
    return value;
  }
  // Describe every changed field and ownership guard using the same generated traversal as undo.
  std::set<detail::collaboration_field_address> touched(const transaction_type& item) const {
    const auto before = decode(item.before);
    const auto after = decode(item.after);
    detail::collaboration_field_collector old{store_.options_.max_snapshot_bytes,
                                              collaboration_options{}.max_tracked_fields};
    detail::collaboration_field_collector next{store_.options_.max_snapshot_bytes,
                                               collaboration_options{}.max_tracked_fields};
    traits::visit_collaboration_fields(before, old);
    traits::visit_collaboration_fields(after, next);
    detail::collaboration_collector old_entities{collaboration_options{}.max_entities,
                                                 store_.options_.max_snapshot_bytes};
    detail::collaboration_collector new_entities{collaboration_options{}.max_entities,
                                                 store_.options_.max_snapshot_bytes};
    traits::visit_collaboration(before, id_type{}, old_entities);
    traits::visit_collaboration(after, id_type{}, new_entities);
    std::set<detail::collaboration_field_address> result;
    for (const auto& [address, bytes] : old.fields) {
      const auto found = next.fields.find(address);
      if (found == next.fields.end() || found->second != bytes ||
          !detail::same_collaboration_ancestry(old_entities.entities, new_entities.entities,
                                               address.first)) {
        result.insert(address);
      }
    }
    for (const auto& [address, bytes] : next.fields) {
      if (!old.fields.contains(address)) {
        result.insert(address);
      }
      static_cast<void>(bytes);
    }
    return result;
  }
  // Reject malformed authoritative identities even when a conflict keeps the working view unchanged.
  void validate_remote(const storage_type& value, std::uint64_t allocated) const {
    if (value.persistent_id != store_.current_->persistent_id) {
      throw std::invalid_argument{"Synchronization changes the document root identity"};
    }
    const auto identities = store_.inspect(value, allocated);
    const auto acknowledged = store_.inspect(decode(state_->acknowledged), store_.allocated_id_);
    for (const auto& [id, type] : identities) {
      const auto old = acknowledged.find(id);
      if (old != acknowledged.end() && old->second != type) {
        throw std::invalid_argument{"Synchronization retypes an acknowledged identity"};
      }
    }
    store_.validate(value);
  }
  // Remote contributions invalidate affected local intentions even when values changed away and back.
  void detect_conflicts(state_type& next, const accepted_type& batch) const {
    if (!Policy::less(batch.session, session_) && !Policy::less(session_, batch.session)) {
      for (const auto& item : next.transactions) {
        if (item.operation == batch.operation && item.epoch == batch.context.epoch &&
            item.status == status(pending_change_status::accepted)) {
          return;
        }
      }
    }
    std::map<std::uint64_t, std::uint64_t> local;
    for (const auto& pair : next.identities) {
      local.emplace(pair.remote, pair.local);
    }
    for (auto& item : next.transactions) {
      if (item.status == status(pending_change_status::discarded)) {
        continue;
      }
      const auto addresses = touched(item);
      bool conflict = false;
      for (const auto& field : batch.history.fields) {
        const auto id = local.find(field.entity);
        if (id != local.end() && addresses.contains({id->second, field.field})) {
          conflict = true;
        }
      }
      for (const auto& dependency : batch.history.dependencies) {
        const auto id = local.find(dependency.entity);
        if (id != local.end() && dependency.before_version != dependency.after_version &&
            std::ranges::any_of(addresses,
                                [&](const auto& address) { return address.first == id->second; })) {
          conflict = true;
        }
      }
      if (conflict) {
        item.invalidated = true;
        if (item.status == status(pending_change_status::queued)) {
          item.status = status(pending_change_status::conflict);
        }
      }
    }
  }
  // Reapply local transactions without rerunning callbacks; on conflict preserve the complete working view.
  std::unique_ptr<Store> rebase(state_type& next, std::uint64_t allocated) const {
    auto value = decode(next.acknowledged);
    for (const auto number : next.pending) {
      auto& item = transaction(next, number);
      if (item.status != status(pending_change_status::queued)) {
        return {};
      }
      try {
        const auto before = encode(value);
        detail::collaboration_history_merger merger{store_.options_.max_snapshot_bytes};
        traits::merge_collaboration(value, decode(item.before), decode(item.after), merger);
        item.before = before;
        item.after = encode(value);
      } catch (const detail::collaboration_history_conflict&) {
        item.status = status(pending_change_status::conflict);
        return {};
      }
    }
    return replacement(next, encode(value), allocated);
  }
  // Incorporate complete ordered authority records, then refresh allocator-only reservations.
  void pull() {
    auto next = *state_;
    auto allocated = store_.allocated_id_;
    bool changed = false;
    for (const auto& batch : changes_(next.sequence)) {
      if (!batch || !detail::same_domain(batch->context, next.context) ||
          batch->sequence != detail::next_collaboration_sequence(next.sequence) ||
          batch->allocated_id < next.allocated_id || batch->operation == 0 ||
          batch->history.action < 1 || batch->history.action > 3 ||
          !Policy::valid(batch->session)) {
        throw std::invalid_argument{"Incomplete or invalid collaboration change stream"};
      }
      auto value = translate(next, *batch, allocated);
      validate_remote(value, allocated);
      detect_conflicts(next, *batch);
      next.acknowledged = encode(value);
      next.allocated_id = batch->allocated_id;
      next.sequence = batch->sequence;
      changed = true;
    }
    const auto snapshot = snapshot_();
    if (!detail::same_domain(snapshot.context, next.context) || snapshot.sequence < next.sequence ||
        snapshot.allocated_id < next.allocated_id) {
      throw std::invalid_argument{"Regressing collaboration snapshot"};
    }
    if (snapshot.sequence == next.sequence) {
      const auto value = translate(next, snapshot, allocated);
      if (encode(value) != next.acknowledged) {
        throw std::invalid_argument{"Conflicting collaboration snapshot"};
      }
      changed = changed || next.allocated_id != snapshot.allocated_id;
      next.allocated_id = snapshot.allocated_id;
    }
    if (changed) {
      auto model = rebase(next, allocated);
      // Keep new identity reservations even when a conflicted working view must remain pinned.
      if (!model && allocated != store_.allocated_id_) {
        model = replacement(next, store_.snapshot_, allocated);
      }
      commit(std::move(next), std::move(model));
    }
  }
  // Share one checked operation counter between document submissions and host coordination requests.
  static std::uint64_t allocate_operation(state_type& state) {
    if (state.next_operation == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error{"Client operation space exhausted"};
    }
    return ++state.next_operation;
  }
  // Persist exact request bytes and provisional identity assignments before handing them to transport.
  void prepare_request() {
    auto next = *state_;
    auto& item = transaction(next, next.pending.front());
    const auto operation = allocate_operation(next);
    collaboration::model_change change;
    if (action(item) == static_cast<std::uint32_t>(collaboration_history_action::edit)) {
      change.format_version = collaboration_model_change_version;
      change.base_allocated_id = change.allocated_id = next.allocated_id;
      std::map<std::uint64_t, std::uint64_t> ids;
      for (const auto& pair : next.identities) {
        ids.emplace(pair.local, pair.remote);
      }
      auto value = decode(item.after);
      traits::visit_entities(value, [&](id_type& id, std::string_view) {
        auto found = ids.find(id);
        if (found == ids.end()) {
          if (change.allocated_id == std::numeric_limits<id_type>::max() ||
              next.in_flight_ids.size() >= collaboration_options{}.max_created_ids) {
            throw std::length_error{"Authority entity allocation budget exceeded"};
          }
          const auto remote = ++change.allocated_id;
          next.in_flight_ids.push_back({id, remote});
          found = ids.emplace(id, remote).first;
        }
        id = static_cast<id_type>(found->second);
      });
      change.snapshot = encode(value);
    } else {
      if constexpr (Store::has_history) {
        const auto& target = transaction(next, item.target);
        if (target.epoch != next.context.epoch || target.operation == 0) {
          item.status = status(pending_change_status::uncertain);
          commit(std::move(next));
          return;
        }
        change.format_version = collaboration_history_change_version;
        change.history_action = action(item);
        change.target_operation = target.operation;
      }
    }
    proposal_type request{
        next.context,
        session_,
        operation,
        next.sequence,
        encode_collaboration_record(change, collaboration_options{}.max_command_bytes),
        item.grants};
    next.in_flight = encode_collaboration_record(request, store_.options_.decode.max_input_bytes);
    item.operation = operation;
    item.epoch = next.context.epoch;
    commit(std::move(next));
  }
  // Resolve a known result; transport exceptions leave the durable request intact for exact retry.
  void deliver() {
    const auto request =
        decode_collaboration_record<proposal_type>(state_->in_flight, store_.options_.decode);
    const auto result = submit_(request);
    if (!result) {
      throw std::runtime_error{"Transport returned no collaboration result"};
    }
    auto next = *state_;
    auto& item = transaction(next, next.pending.front());
    if (result->status == collaboration_status::accepted) {
      if (!result->accepted || !detail::same_domain(result->accepted->context, request.context) ||
          result->accepted->operation != request.operation ||
          Policy::less(result->accepted->session, session_) ||
          Policy::less(session_, result->accepted->session)) {
        throw std::invalid_argument{"Unbound collaboration acknowledgement"};
      }
      next.identities.insert(next.identities.end(), next.in_flight_ids.begin(),
                             next.in_flight_ids.end());
      item.status = status(pending_change_status::accepted);
      item.accepted_sequence = result->accepted->sequence;
      next.pending.erase(next.pending.begin());
    } else if (result->status == collaboration_status::no_change) {
      item.status = status(pending_change_status::accepted);
      item.operation = 0;
      next.pending.erase(next.pending.begin());
    } else {
      switch (result->status) {
      case collaboration_status::denied:
        item.status = status(pending_change_status::denied);
        break;
      case collaboration_status::obsolete_grant:
        item.status = status(pending_change_status::obsolete_grant);
        break;
      case collaboration_status::conflict: {
        const auto latest = snapshot_();
        const auto command =
            decode_collaboration_record<collaboration::model_change>(request.command);
        const bool stale =
            detail::same_domain(latest.context, request.context) &&
            action(item) == static_cast<std::uint32_t>(collaboration_history_action::edit) &&
            (latest.sequence > request.base_sequence ||
             latest.allocated_id > command.base_allocated_id);
        item.status =
            status(stale ? pending_change_status::queued : pending_change_status::conflict);
        break;
      }
      case collaboration_status::indeterminate:
        item.status = status(pending_change_status::uncertain);
        break;
      default:
        item.status = status(pending_change_status::failed);
        break;
      }
    }
    if (item.status != status(pending_change_status::uncertain)) {
      next.in_flight.clear();
      next.in_flight_ids.clear();
    }
    commit(std::move(next));
  }

public:
  // Attach before creating/recovering the client journal; a first baseline is required before editing.
  store_collaboration(Store& store, Session session, collaboration_client_options options)
      : store_{store}, session_{std::move(session)}, options_{options},
        state_{std::make_shared<state_type>()} {
    if (!Policy::valid(session_) || options.max_state_bytes == 0 || options.max_transactions == 0 ||
        options.max_flush_operations == 0 ||
        Policy::encode(session_).size() > Policy::max_encoded_bytes) {
      throw std::invalid_argument{"Invalid collaboration session or client limits"};
    }
    state_->format_version = 1;
    state_->session_format = std::string{Policy::wire_name};
    state_->session = Policy::encode(session_);
    state_->sync_interval_ms = options.sync_interval_ms;
  }
  // Inspect session identity; applications continue to own its creation and lifecycle.
  const Session& session() const {
    store_.check_thread();
    return session_;
  }
  // Reserve an ID for a host lock request after joining, on the store owner thread.
  // Persist the counter through the attached journal before returning; failure returns no ID.
  // The host retains the request for exact retries; unused reservations are never reclaimed.
  std::uint64_t reserve_operation_id() {
    operation_scope scope{*this};
    if (state_->context.epoch == 0) {
      throw std::logic_error{"Join collaboration before reserving an operation ID"};
    }
    auto next = *state_;
    const auto operation = allocate_operation(next);
    commit(std::move(next));
    return operation;
  }
  // Pin pending, rejected and historical transactions; snapshots survive subsequent synchronization.
  std::shared_ptr<const state_type> state() const {
    store_.check_thread();
    return state_;
  }
  // Report pending work independently of the local document's saved-value indicator.
  std::size_t pending_count() const {
    store_.check_thread();
    return state_->pending.size();
  }
  // Report the oldest pending outcome; a missing value means the outbox is empty.
  std::optional<pending_change_status> pending_status() const {
    store_.check_thread();
    if (state_->pending.empty()) {
      return {};
    }
    return static_cast<pending_change_status>(transaction(*state_, state_->pending.front()).status);
  }
  // Inspect remote state separately when a conflict preserves the user's local working copy.
  std::shared_ptr<const storage_type> acknowledged_read() const {
    store_.check_thread();
    return std::make_shared<const storage_type>(decode(state_->acknowledged));
  }
  // Translate stable local entity handles for server lease APIs; unsent objects have no remote ID.
  std::uint64_t remote_id(std::uint64_t local) const {
    store_.check_thread();
    for (const auto& pair : state_->identities) {
      if (pair.local == local) {
        return pair.remote;
      }
    }
    return 0;
  }
  // Translate a server identity for local editors; zero means the entity has not been observed yet.
  std::uint64_t local_id(std::uint64_t remote) const {
    store_.check_thread();
    for (const auto& pair : state_->identities) {
      if (pair.remote == remote) {
        return pair.local;
      }
    }
    return 0;
  }
  // Bind a host-authenticated transport by reference; it must outlive its binding to this store.
  template <typename Transport>
  void bind(Transport& transport) {
    operation_scope scope{*this};
    decltype(context_) context = [&transport] { return transport.context(); };
    decltype(snapshot_) snapshot = [&transport] { return transport.snapshot(); };
    decltype(changes_) changes = [&transport](std::uint64_t cursor) {
      return transport.accepted_since(cursor);
    };
    decltype(submit_) submit = [&transport](const proposal_type& request) {
      return transport.submit_change(request);
    };
    context_.swap(context);
    snapshot_.swap(snapshot);
    changes_.swap(changes);
    submit_.swap(submit);
  }
  // Establish the initial document exactly once; later reconnects preserve all pending local work.
  void join(const accepted_type& batch, const collaboration::domain& trusted) {
    operation_scope scope{*this};
    if (state_->context.epoch != 0 || !detail::same_domain(batch.context, trusted) ||
        batch.allocated_id > std::numeric_limits<id_type>::max()) {
      throw std::logic_error{"Collaboration baseline already established or untrusted"};
    }
    check_context(trusted, true);
    auto next = *state_;
    next.context = trusted;
    next.sequence = batch.sequence;
    next.allocated_id = batch.allocated_id;
    next.acknowledged = batch.snapshot;
    auto value = decode(batch.snapshot);
    traits::visit_entities(
        value, [&](id_type& id, std::string_view) { next.identities.push_back({id, id}); });
    auto model = replacement(next, batch.snapshot, batch.allocated_id);
    commit(std::move(next), std::move(model));
  }
  // Capture one edit. Unpinned, non-journal state is appended in place with bounded rollback;
  // pinned readers and journal callbacks retain the immutable copy-on-write preparation path.
  void prepare(const std::vector<std::uint8_t>& snapshot, std::string_view label) override {
    if (state_->context.epoch == 0) {
      throw std::logic_error{"Join collaboration before editing"};
    }
    if (state_->transactions.size() >= options_.max_transactions) {
      throw std::length_error{"Client transaction budget exhausted"};
    }
    const bool in_place = (state_.use_count() == 1) && !store_.journal_state_.sink;
    auto next = in_place ? state_ : std::make_shared<state_type>(*state_);
    transaction_type item;
    if constexpr (Store::has_history) {
      item.number = detail::next_collaboration_sequence(next->transactions.size());
      item.action = history_action_.action;
      item.target = history_action_.target;
      item.label = history_action_.target == 0 ? std::string{label}
                                               : transaction(*next, history_action_.target).label;
    } else {
      item.number = detail::next_collaboration_sequence(next->next_transaction);
      static_cast<void>(label);
    }
    item.before = store_.snapshot_;
    item.after = snapshot;
    item.status = status(pending_change_status::queued);
    item.grants = grants_;
    const auto number = item.number;
    next->transactions.push_back(std::move(item));
    bool pending_added = false;
    bool history_changed = false;
    try {
      next->pending.push_back(number);
      pending_added = true;
      if constexpr (Store::has_history) {
        if (history_action_.action ==
            static_cast<std::uint32_t>(collaboration_history_action::edit)) {
          next->undo.push_back(number);
          history_action_.saved_redo.swap(next->redo);
        } else if (history_action_.action ==
                   static_cast<std::uint32_t>(collaboration_history_action::undo)) {
          next->redo.push_back(number);
          history_action_.saved_tip = next->undo.back();
          next->undo.pop_back();
        } else {
          next->undo.push_back(number);
          history_action_.saved_tip = next->redo.back();
          next->redo.pop_back();
        }
      }
      history_changed = true;
      check_state_budget(*next);
    } catch (...) {
      if (history_changed) {
        rollback_history(*next);
      }
      if (pending_added) {
        next->pending.pop_back();
      }
      next->transactions.pop_back();
      throw;
    }
    if constexpr (!Store::has_history) {
      next->next_transaction = number;
    }
    prepared_in_place_ = in_place;
    if (!in_place) {
      prepared_ = std::move(next);
    }
  }

private:
  // Restore stack changes without allocation; removed tips retain their original vector capacity.
  void rollback_history(state_type& state) noexcept {
    if constexpr (Store::has_history) {
      if (history_action_.action ==
          static_cast<std::uint32_t>(collaboration_history_action::edit)) {
        state.undo.pop_back();
        state.redo.swap(history_action_.saved_redo);
      } else if (history_action_.action ==
                 static_cast<std::uint32_t>(collaboration_history_action::undo)) {
        state.redo.pop_back();
        state.undo.push_back(history_action_.saved_tip);
      } else {
        state.undo.pop_back();
        state.redo.push_back(history_action_.saved_tip);
      }
    } else {
      static_cast<void>(state);
    }
  }

public:
  // The store calls this after its model publication can no longer fail; no callbacks intervene.
  void publish() noexcept override {
    if (!prepared_in_place_) {
      state_.swap(prepared_);
      prepared_.reset();
    }
    prepared_in_place_ = false;
    if constexpr (Store::has_history) {
      history_action_.saved_redo.clear();
    }
  }
  // Undo provisional appends or discard a private copy; retained pins never observe preparation.
  void abort() noexcept override {
    if (prepared_in_place_) {
      rollback_history(*state_);
      state_->pending.pop_back();
      state_->transactions.pop_back();
      if constexpr (!Store::has_history) {
        --state_->next_transaction;
      }
      prepared_in_place_ = false;
    }
    prepared_.reset();
    if constexpr (Store::has_history) {
      history_action_.saved_redo.clear();
    }
  }
  // Local labels follow their transaction through synchronization, undo/redo and recovery.
  std::string history_label(bool redo) const override {
    if constexpr (!Store::has_history) {
      static_cast<void>(redo);
      throw std::logic_error{"Collaborative history is disabled"};
    } else {
      const auto& stack = redo ? state_->redo : state_->undo;
      if (stack.empty()) {
        throw std::out_of_range{"No collaborative history label"};
      }
      return transaction(*state_, stack.back()).label;
    }
  }
  // Reverse local intent immediately through the store's existing transaction/validation mechanism.
  void undo(bool redo) override {
    if constexpr (!Store::has_history) {
      static_cast<void>(redo);
      throw std::logic_error{"Collaborative history is disabled"};
    } else {
      operation_scope scope{*this};
      const auto& stack = redo ? state_->redo : state_->undo;
      if (stack.empty()) {
        throw std::out_of_range{"No collaborative undo/redo transaction"};
      }
      const auto& item = transaction(*state_, stack.back());
      if (item.invalidated) {
        throw detail::collaboration_history_conflict{};
      }
      auto desired = decode(item.before);
      auto expected = decode(item.after);
      auto identities = store_.inspect(desired, store_.allocated_id_);
      history_action_.action = static_cast<std::uint32_t>(
          redo ? collaboration_history_action::redo : collaboration_history_action::undo);
      history_action_.target = item.number;
      store_.collaboration_state_.restoration = &identities;
      store_.collaboration_state_.busy = false;
      const auto outcome = store_.execute_transaction([&](auto& edit) {
        edit.update([&](auto& value) {
          detail::collaboration_history_merger merger{store_.options_.max_snapshot_bytes};
          traits::merge_collaboration(value, expected, desired, merger);
        });
      });
      store_.collaboration_state_.restoration = nullptr;
      history_action_.action = static_cast<std::uint32_t>(collaboration_history_action::edit);
      history_action_.target = 0;
      outcome.throw_if_failed();
    }
  }
  // Refresh remote changes, retry uncertain delivery exactly, and submit the FIFO until blocked/bounded.
  void synchronize() override {
    store_.require_idle();
    if (!context_) {
      throw std::logic_error{"Bind a collaboration transport before synchronizing"};
    }
    if (state_->context.epoch == 0) {
      collaboration::domain trusted;
      accepted_type batch;
      {
        operation_scope scope{*this};
        trusted = context_();
        batch = snapshot_();
      }
      join(batch, trusted);
    }
    operation_scope scope{*this};
    const auto trusted = context_();
    check_context(trusted);
    if (!detail::same_domain(trusted, state_->context)) {
      const auto batch = snapshot_();
      if (!detail::same_domain(batch.context, trusted) ||
          batch.allocated_id < state_->allocated_id) {
        throw std::invalid_argument{"Untrusted or regressing reconnect"};
      }
      auto next = *state_;
      next.context = trusted;
      next.sequence = batch.sequence;
      next.allocated_id = batch.allocated_id;
      auto allocated = store_.allocated_id_;
      const auto value = translate(next, batch, allocated);
      validate_remote(value, allocated);
      next.acknowledged = encode(value);
      for (const auto number : next.pending) {
        transaction(next, number).status = status(pending_change_status::uncertain);
      }
      for (auto& item : next.transactions) {
        item.invalidated = true;
      }
      // Keep outstanding bytes as evidence; they must never be submitted to a different epoch.
      auto model = next.pending.empty() ? replacement(next, next.acknowledged, allocated)
                                        : replacement(next, store_.snapshot_, allocated);
      commit(std::move(next), std::move(model));
      return;
    }
    if (!state_->pending.empty() && transaction(*state_, state_->pending.front()).status ==
                                        status(pending_change_status::uncertain)) {
      return;
    }
    if (!state_->in_flight.empty()) {
      if (receive_only_) {
        throw std::logic_error{"Resolve outstanding delivery before receiving changes"};
      }
      deliver();
    }
    pull();
    if (receive_only_) {
      return;
    }
    for (std::size_t count = 0; count < options_.max_flush_operations && !state_->pending.empty();
         ++count) {
      if (transaction(*state_, state_->pending.front()).status !=
          status(pending_change_status::queued)) {
        break;
      }
      prepare_request();
      if (state_->in_flight.empty()) {
        break;
      }
      deliver();
      pull();
    }
  }
  // Send one FIFO request (or retry its exact bytes); receive ordered changes before sending its successor.
  void send_pending() override {
    operation_scope scope{*this};
    if (!context_ || state_->context.epoch == 0) {
      throw std::logic_error{"Join and bind before sending changes"};
    }
    if (!detail::same_domain(context_(), state_->context)) {
      throw std::logic_error{"Receive the new authority epoch before sending changes"};
    }
    if (state_->pending.empty()) {
      return;
    }
    for (const auto& item : state_->transactions) {
      if (item.epoch == state_->context.epoch && item.accepted_sequence > state_->sequence) {
        return;
      }
    }
    if (transaction(*state_, state_->pending.front()).status !=
        status(pending_change_status::queued)) {
      return;
    }
    if (state_->in_flight.empty()) {
      prepare_request();
    }
    if (!state_->in_flight.empty()) {
      deliver();
    }
  }
  // Pull-only operation; uncertain sends must be resolved before their echoed identities can be applied.
  void receive_changes() override {
    store_.require_idle();
    receive_only_ = true;
    try {
      synchronize();
    } catch (...) {
      receive_only_ = false;
      throw;
    }
    receive_only_ = false;
  }
  // Monotonic caller ticks give configurable scheduling without touching the store from a worker thread.
  bool synchronize_if_due(std::uint64_t now_ms) override {
    store_.require_idle();
    if (last_poll_ && now_ms < *last_poll_) {
      throw std::invalid_argument{"Synchronization clock regressed"};
    }
    last_poll_ = now_ms;
    if (state_->sync_interval_ms == 0) {
      return false;
    }
    if (!last_sync_) {
      last_sync_ = now_ms;
    }
    if (state_->sync_interval_ms != 0 && now_ms - *last_sync_ < state_->sync_interval_ms) {
      return false;
    }
    synchronize();
    last_sync_ = now_ms;
    return true;
  }
  // Persist a new interval; zero means explicit flush only.
  void set_sync_interval(std::uint64_t interval_ms) {
    operation_scope scope{*this};
    auto next = *state_;
    next.sync_interval_ms = interval_ms;
    commit(std::move(next));
  }
  // Refresh authority lease references for queued edits; never alter an uncertain in-flight request.
  void set_grants(std::vector<collaboration::grant_reference> grants) {
    operation_scope scope{*this};
    if (!state_->in_flight.empty() || grants.size() > collaboration_options{}.max_locks) {
      throw std::logic_error{"Resolve outstanding delivery before changing grants"};
    }
    auto next = *state_;
    for (const auto number : next.pending) {
      transaction(next, number).grants = grants;
    }
    commit(std::move(next));
    grants_.swap(grants);
  }
  // Retry known permission/lease/failure rejections after the host resolves their cause.
  void retry_pending() {
    operation_scope scope{*this};
    if (!state_->in_flight.empty()) {
      throw std::logic_error{"Outstanding delivery requires exact retry"};
    }
    auto next = *state_;
    for (const auto number : next.pending) {
      auto& item = transaction(next, number);
      if (!item.invalidated && (item.status == status(pending_change_status::denied) ||
                                item.status == status(pending_change_status::obsolete_grant) ||
                                item.status == status(pending_change_status::failed))) {
        item.status = status(pending_change_status::queued);
      }
    }
    auto model = rebase(next, store_.allocated_id_);
    commit(std::move(next), std::move(model));
  }
  // Explicitly choose the acknowledged baseline; archived drafts and the returned local pin preserve work.
  std::shared_ptr<const storage_type> discard_pending() {
    operation_scope scope{*this};
    if (!state_->in_flight.empty() &&
        detail::same_domain(decode_collaboration_record<proposal_type>(state_->in_flight).context,
                            state_->context)) {
      throw std::logic_error{"Resolve outstanding delivery before discarding work"};
    }
    const auto previous = store_.current_;
    auto next = *state_;
    for (const auto number : next.pending) {
      transaction(next, number).status = status(pending_change_status::discarded);
    }
    next.pending.clear();
    next.in_flight.clear();
    next.in_flight_ids.clear();
    if constexpr (Store::has_history) {
      next.undo.clear();
      next.redo.clear();
      for (const auto& item : next.transactions) {
        if (item.status != status(pending_change_status::accepted) || item.invalidated) {
          continue;
        }
        if (action(item) == static_cast<std::uint32_t>(collaboration_history_action::edit)) {
          next.undo.push_back(item.number);
          next.redo.clear();
        } else if (action(item) == static_cast<std::uint32_t>(collaboration_history_action::undo)) {
          if (!next.undo.empty()) {
            next.undo.pop_back();
          }
          next.redo.push_back(item.number);
        } else {
          if (!next.redo.empty()) {
            next.redo.pop_back();
          }
          next.undo.push_back(item.number);
        }
      }
    }
    auto model = replacement(next, next.acknowledged, store_.allocated_id_);
    commit(std::move(next), std::move(model));
    return previous;
  }
  // Save ordinary model history and the complete session/outbox in one exact checkpoint.
  std::vector<std::uint8_t> save() const override {
    if (state_->context.epoch == 0) {
      throw std::logic_error{"Join before saving a collaboration client"};
    }
    return checkpoint(1, model_bytes(store_), *state_);
  }

private:
  // Validate the retained queue and bindings before accepting a recovered journal or memory checkpoint.
  void validate_state(const state_type& state, const Store& model) const {
    if (state.format_version != 1 || state.session_format != Policy::wire_name ||
        state.session != Policy::encode(session_) || state.context.schema_id != traits::schema_id ||
        state.context.protocol_version != record_types::model_protocol ||
        state.context.epoch == 0 || state.context.document_high != model.document_.high ||
        state.context.document_low != model.document_.low ||
        state.context.id_bits != std::numeric_limits<id_type>::digits ||
        state.transactions.size() > options_.max_transactions) {
      throw std::invalid_argument{"Invalid collaboration client checkpoint binding"};
    }
    const auto acknowledged = decode(state.acknowledged);
    if (acknowledged.persistent_id != model.current_->persistent_id ||
        state.allocated_id > std::numeric_limits<id_type>::max()) {
      throw std::invalid_argument{"Invalid acknowledged client identity"};
    }
    static_cast<void>(model.inspect(acknowledged, model.allocated_id_));
    std::set<std::uint64_t> local_ids, remote_ids, pending;
    if (state.identities.size() > collaboration_options{}.max_entities ||
        state.in_flight_ids.size() > collaboration_options{}.max_created_ids) {
      throw std::length_error{"Client identity mapping budget exceeded"};
    }
    for (const auto& pair : state.identities) {
      if (pair.local == 0 || pair.local > model.allocated_id_ || pair.remote == 0 ||
          pair.remote > std::numeric_limits<id_type>::max() ||
          !local_ids.insert(pair.local).second || !remote_ids.insert(pair.remote).second) {
        throw std::invalid_argument{"Invalid client identity mapping"};
      }
    }
    for (std::size_t index = 0; index < state.transactions.size(); ++index) {
      const auto& item = state.transactions[index];
      if constexpr (Store::has_history) {
        if (item.number != index + 1 || item.action < 1 || item.action > 3 ||
            item.target >= item.number || (item.action == 1) != (item.target == 0)) {
          throw std::invalid_argument{"Invalid client history transaction"};
        }
      } else if (item.number == 0 || item.number > state.next_transaction ||
                 (index != 0 && state.transactions[index - 1].number >= item.number)) {
        throw std::invalid_argument{"Invalid pending transaction number"};
      }
      if (item.status < 1 || item.status > 8 || item.operation > state.next_operation) {
        throw std::invalid_argument{"Invalid client transaction"};
      }
      for (const auto* bytes : {&item.before, &item.after}) {
        const auto historical = decode(*bytes);
        if (historical.persistent_id != model.current_->persistent_id) {
          throw std::invalid_argument{"Client history changes root identity"};
        }
        static_cast<void>(model.inspect(historical, model.allocated_id_));
      }
    }
    for (const auto number : state.pending) {
      if (!pending.insert(number).second ||
          transaction(state, number).status == status(pending_change_status::accepted) ||
          transaction(state, number).status == status(pending_change_status::discarded)) {
        throw std::invalid_argument{"Invalid client pending queue"};
      }
    }
    if (!std::ranges::is_sorted(state.pending)) {
      throw std::invalid_argument{"Unordered client pending queue"};
    }
    for (const auto& item : state.transactions) {
      const bool outstanding = item.status != status(pending_change_status::accepted) &&
                               item.status != status(pending_change_status::discarded);
      if (outstanding != pending.contains(item.number)) {
        throw std::invalid_argument{"Missing pending transaction"};
      }
    }
    if constexpr (Store::has_history) {
      for (const auto& stack : {state.undo, state.redo}) {
        std::set<std::uint64_t> seen;
        for (const auto number : stack) {
          if (number == 0 || number > state.transactions.size() || !seen.insert(number).second) {
            throw std::invalid_argument{"Invalid client history stack"};
          }
        }
      }
    }
    if (!state.in_flight.empty()) {
      const auto request =
          decode_collaboration_record<proposal_type>(state.in_flight, store_.options_.decode);
      if (state.pending.empty() ||
          request.operation != transaction(state, state.pending.front()).operation ||
          request.operation == 0 || request.operation > state.next_operation ||
          request.context.document_high != state.context.document_high ||
          request.context.document_low != state.context.document_low ||
          (request.context.epoch != state.context.epoch &&
           transaction(state, state.pending.front()).status !=
               status(pending_change_status::uncertain)) ||
          Policy::less(request.session, session_) || Policy::less(session_, request.session)) {
        throw std::invalid_argument{"Invalid client outstanding request"};
      }
      std::set<std::uint64_t> local, remote;
      for (const auto& pair : state.in_flight_ids) {
        if (pair.local == 0 || pair.local > model.allocated_id_ || pair.remote == 0 ||
            pair.remote > std::numeric_limits<id_type>::max() || !local.insert(pair.local).second ||
            !remote.insert(pair.remote).second) {
          throw std::invalid_argument{"Invalid provisional identity assignment"};
        }
      }
    } else if (!state.in_flight_ids.empty()) {
      throw std::invalid_argument{"Identity assignments have no outstanding request"};
    }
  }
  // Read one bounded wrapper, rejecting foreign journals and malformed frame kinds.
  checkpoint_type decode_checkpoint(std::span<const std::uint8_t> bytes) const {
    auto limits = store_.options_.decode;
    limits.max_input_bytes = options_.max_state_bytes;
    auto result = decode_collaboration_record<checkpoint_type>(bytes, limits);
    if (result.binding != checkpoint_binding || (result.kind != 1 && result.kind != 2)) {
      throw std::invalid_argument{"Invalid collaboration checkpoint format"};
    }
    return result;
  }

public:
  // Memory restore is atomic and requires the same application session; it never sends network messages.
  void load(const std::vector<std::uint8_t>& bytes) override {
    operation_scope scope{*this};
    if (store_.journal_state_.sink || state_->context.epoch != 0) {
      throw std::logic_error{"Restore collaboration into a fresh attached store"};
    }
    auto checkpoint = decode_checkpoint(bytes);
    if (checkpoint.kind != 1) {
      throw std::invalid_argument{"Expected a full client checkpoint"};
    }
    auto model = std::unique_ptr<Store>{
        new Store{typename Store::recovery_tag{}, store_.options_, store_.validate_}};
    model->load(checkpoint.model);
    validate_state(checkpoint.state, *model);
    commit(std::move(checkpoint.state), std::move(model));
  }
  // Journal creation uses the existing exclusive file adapter with one atomic model/session baseline.
  void create_journal(const std::filesystem::path& path, journal_storage_mode mode,
                      journal_options options) override {
    if constexpr (!Store::has_journal) {
      throw std::logic_error{"Journaling is compiled out of this store"};
    } else {
      operation_scope scope{*this};
      if (store_.journal_state_.sink || state_->context.epoch == 0) {
        throw std::logic_error{"Join before opening a new client journal"};
      }
      auto journal = std::make_unique<client_journal>(*this);
      auto saved = store_.snapshot_;
      const auto bytes = save();
      store_.journal_operation([&] {
        journal->sink = detail::create_file_journal(path, mode, bytes, std::move(options),
                                                    store_.journal_state_.indeterminate);
      });
      store_.journal_state_.saved_snapshot.swap(saved);
      store_.journal_state_.sink = std::move(journal);
    }
  }
  // Replay the same native model operations and their paired session state before repairing a torn tail.
  void recover_journal(const std::filesystem::path& path, journal_options options) override {
    if constexpr (!Store::has_journal) {
      throw std::logic_error{"Journaling is compiled out of this store"};
    } else {
      operation_scope scope{*this};
      if (store_.journal_state_.sink || state_->context.epoch != 0) {
        throw std::logic_error{"Recover collaboration into a fresh attached store"};
      }
      auto model =
          std::unique_ptr<Store>{new Store{typename Store::recovery_tag{}, store_.options_, {}}};
      auto next = std::make_shared<state_type>();
      auto journal = std::make_unique<client_journal>(*this);
      std::vector<std::uint8_t> saved;
      store_.journal_operation([&] {
        journal->sink = detail::recover_file_journal(
            path, std::move(options), [&](bool base, std::span<const std::uint8_t> bytes) {
              auto checkpoint = decode_checkpoint(bytes);
              if (base && checkpoint.kind != 1) {
                throw std::invalid_argument{"Invalid client journal base"};
              }
              if (!base && (checkpoint.state.context.document_high != next->context.document_high ||
                            checkpoint.state.context.document_low != next->context.document_low ||
                            checkpoint.state.next_operation < next->next_operation ||
                            (Store::has_history &&
                             checkpoint.state.transactions.size() < next->transactions.size()) ||
                            checkpoint.state.allocated_id < next->allocated_id ||
                            (checkpoint.state.context.epoch == next->context.epoch &&
                             checkpoint.state.sequence < next->sequence))) {
                throw std::invalid_argument{"Regressing collaboration journal state"};
              }
              if constexpr (!Store::has_history) {
                if (!base && checkpoint.state.next_transaction < next->next_transaction) {
                  throw std::invalid_argument{"Regressing pending transaction allocation"};
                }
              }
              const auto previous_allocated = model->allocated_id_;
              if (checkpoint.kind == 1) {
                model->load(checkpoint.model);
              } else if (!checkpoint.model.empty()) {
                model->replay_journal_record(checkpoint.model);
              }
              if (!base && model->allocated_id_ < previous_allocated) {
                throw std::invalid_argument{"Regressing client identity reservation"};
              }
              validate_state(checkpoint.state, *model);
              *next = std::move(checkpoint.state);
              if (base) {
                saved = model->snapshot_;
              }
            });
        if (store_.validate_) {
          store_.validate_(*model->current_);
        }
        journal->sink->finish_recovery();
      });
      swap_model(*model);
      state_.swap(next);
      store_.journal_state_.saved_snapshot.swap(saved);
      store_.journal_state_.sink = std::move(journal);
    }
  }
};

// Associate typed collaboration state with the existing store, preserving its editor and transaction APIs.
template <typename Root, history_mode Mode, history_labels Labels, typename Traits,
          store_features Features>
template <typename Session, typename Policy>
store_collaboration<model_store<Root, Mode, Labels, Traits, Features>, Session, Policy>&
model_store<Root, Mode, Labels, Traits, Features>::collaborate(Session session,
                                                               collaboration_client_options options)
  requires(has_collaboration)
{
  require_idle();
  if (collaboration_state_.attachment || journal_state_.sink) {
    throw std::logic_error{"Attach collaboration once, before opening its journal"};
  }
  auto attachment = std::make_unique<store_collaboration<model_store, Session, Policy>>(
      *this, std::move(session), options);
  auto& result = *attachment;
  collaboration_state_.attachment = std::move(attachment);
  return result;
}

// A mismatched requested session type fails explicitly; session identity is never silently converted.
template <typename Root, history_mode Mode, history_labels Labels, typename Traits,
          store_features Features>
template <typename Session, typename Policy>
store_collaboration<model_store<Root, Mode, Labels, Traits, Features>, Session, Policy>&
model_store<Root, Mode, Labels, Traits, Features>::collaboration()
  requires(has_collaboration)
{
  check_thread();
  auto* result = dynamic_cast<store_collaboration<model_store, Session, Policy>*>(
      collaboration_state_.attachment.get());
  if (!result) {
    throw std::logic_error{"No collaboration attachment for this session type"};
  }
  return *result;
}

// In-process transport example/test adapter. Production transports implement the same four methods.
template <typename Authority>
class collaboration_transport {
  Authority& authority_;
  typename Authority::session_type trusted_session_;
  std::uint64_t now_ms_{};

public:
  // The host, not a decoded proposal, supplies the trusted session bound to this connection.
  collaboration_transport(Authority& authority, typename Authority::session_type session)
      : authority_{authority}, trusted_session_{std::move(session)} {}
  // Keep lease evaluation on the authority's monotonic clock.
  void set_time(std::uint64_t now_ms) {
    now_ms_ = now_ms;
  }
  // Return a trusted endpoint binding independently of message contents.
  auto context() const {
    return authority_.context();
  }
  // Fetch the acknowledged baseline and allocator watermark.
  auto snapshot() const {
    return authority_.snapshot();
  }
  // Retrieve the complete retained accepted stream after the caller's cursor.
  auto accepted_since(std::uint64_t cursor) const {
    return authority_.accepted_since(cursor);
  }
  // Deliver one exact request using host-supplied session authentication.
  auto submit_change(const typename Authority::change_proposal_type& request) {
    return authority_.submit_change(request, trusted_session_, now_ms_);
  }
};
} // namespace rohit::managed
