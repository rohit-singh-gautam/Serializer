// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <managed_entity_references.hpp>
#include <rohit/managed_collaboration.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace managed = rohit::managed;
namespace records = managed::collaboration;
using root = entity_graph::document;
using node = entity_graph::node;
using traits = managed::model_traits<root>;
using node_traits = managed::model_traits<node>;
using graph_storage = typename traits::storage_type;
using id_type = typename traits::id_type;
using token = managed::managed_ref<node, id_type>;
using store_type = managed::model_store<root>;
using authority_type = managed::collaboration_authority<root>;
using replica_type = managed::collaboration_replica<root>;
constexpr managed::document_id graph_document{701U, 702U};
constexpr std::uint64_t alice_session = 10U;
constexpr std::uint64_t bob_session = 20U;

// Begin with two identified targets and an ordinary integer that must never be remapped.
root initial_graph() {
  root result{};
  result.nodes.resize(2U);
  result.nodes[0].label = "first";
  result.nodes[1].label = "second";
  result.ordinary_id = 1234U;
  return result;
}

// Establish a real source graph whose root/child/cycle tokens survive every transport boundary.
void link_source(store_type& source) {
  const auto& nodes = traits::payload(*source.read()).nodes;
  const auto first = nodes[0].persistent_id;
  const auto second = nodes[1].persistent_id;
  source.execute_transaction([&](auto& edit) {
    const auto registry = edit.registry();
    registry.template edit<node>(first).set_next(registry.template reference<node>(second));
    registry.template edit<node>(second).set_next(registry.template reference<node>(first));
    const auto owner = registry.template reference<root>(source.read()->persistent_id);
    registry.template edit<node>(first).set_owner(owner);
    registry.template edit<node>(second).set_owner(owner);
  }).throw_if_failed();
}

// Adopt both targets before linking forward and reciprocal edges in the same complete candidate.
std::array<id_type, 2> append_cycle(auto& edit, const std::string& label) {
  const auto registry = edit.registry();
  const auto attach = [](auto& storage, auto incoming) {
    traits::payload(storage).nodes.push_back(std::move(incoming));
  };
  const auto first = registry.adopt(node{}, attach);
  const auto second = registry.adopt(node{}, attach);
  registry.template edit<node>(first).set_label(label + "-first");
  registry.template edit<node>(second).set_label(label + "-second");
  registry.template edit<node>(first).set_next(registry.template reference<node>(second));
  registry.template edit<node>(second).set_next(registry.template reference<node>(first));
  const auto owner = edit.root().get_id();
  const auto first_reference = registry.template reference<node>(first);
  const auto second_reference = registry.template reference<node>(second);
  edit.update([&](auto& storage) {
    auto& value = traits::payload(storage);
    node_traits::payload(value.nodes[value.nodes.size() - 2U]).owner = owner;
    node_traits::payload(value.nodes.back()).owner = owner;
    value.info.links.push_back(first_reference);
    value.info.links.push_back(second_reference);
  });
  return {first, second};
}

// Inspect owning values independently of registry lookup, including root-reference namespace metadata.
void expect_cycle(const graph_storage& storage, std::array<id_type, 2> ids) {
  const auto& value = traits::payload(storage);
  for (std::size_t index = 0; index < ids.size(); ++index) {
    const auto found = std::find_if(value.nodes.begin(), value.nodes.end(), [&](const auto& item) {
      return item.persistent_id == ids[index];
    });
    ASSERT_NE(found, value.nodes.end());
    const auto& payload = node_traits::payload(*found);
    ASSERT_TRUE(payload.next);
    EXPECT_EQ(payload.next->id(), ids[1U - index]);
    EXPECT_EQ(payload.next->document(), graph_document);
    ASSERT_TRUE(payload.owner);
    EXPECT_EQ(payload.owner->id(), storage.persistent_id);
    EXPECT_EQ(payload.owner->document(), graph_document);
  }
}

// Decode an otherwise valid typed envelope without requiring that its target exists in this graph.
token node_token(managed::document_id document, id_type id) {
  const auto text = std::string{"{\"document_high\":"} + std::to_string(document.high) +
      ",\"document_low\":" + std::to_string(document.low) + ",\"class_id\":" +
      std::to_string(node_traits::class_id) + ",\"object_id\":" + std::to_string(id) + "}";
  const auto input = rohit::make_constant_full_stream(text.data(), text.size());
  return rohit::serializer::deserialize_exact<token, rohit::serializer::json>(input);
}

// Fail the public scoped root/control-block resource without changing ordinary test allocations.
class failing_resource final : public std::pmr::memory_resource {
public:
  bool reject{};
  std::size_t attempts{};
  std::size_t live_bytes{};
private:
  // Account successful holders and fail deterministically before exposing a replacement root.
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    ++attempts;
    if (reject) { throw std::bad_alloc{}; }
    auto* pointer = std::pmr::new_delete_resource()->allocate(bytes, alignment);
    live_bytes += bytes;
    return pointer;
  }
  // Balance discarded candidates and the final accepted holder through their original resource.
  void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
    live_bytes -= bytes;
    std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
  }
  // Resource equality is intentionally restricted to this fault-injection instance.
  bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;
  }
};

// Retain exact submitted wire bytes while failing either before acceptance or after a lost reply.
struct unreliable_transport {
  managed::collaboration_transport<authority_type> direct;
  bool offline{};
  bool lose_reply{};
  bool foreign_snapshot{};
  std::vector<std::vector<std::uint8_t>> requests;
  // Keep authentication separate from client-authored record fields.
  unreliable_transport(authority_type& authority, std::uint64_t session)
      : direct{authority, session} {}
  // Read-only transport operations use the real authority endpoint.
  auto context() { return direct.context(); }
  // Preserve the authoritative baseline, including burned allocation gaps.
  auto snapshot() {
    auto result = direct.snapshot();
    if (foreign_snapshot) {
      auto value = managed::decode_collaboration_record<graph_storage>(result.snapshot);
      traits::payload(value).info.links.push_back(node_token({801U, 802U},
          traits::payload(value).nodes.front().persistent_id));
      result.snapshot = managed::encode_collaboration_record(value);
    }
    return result;
  }
  // Deliver actual ordered acceptances without translating native object IDs.
  auto accepted_since(std::uint64_t cursor) { return direct.accepted_since(cursor); }
  // Burn native capacity through the independently bound session before offline editing.
  auto reserve_ids(std::uint64_t count) { return direct.reserve_ids(count); }
  // A failed transport never fabricates a response or reruns the authored transaction callback.
  auto submit_change(const authority_type::change_proposal_type& proposal) {
    requests.push_back(managed::encode_collaboration_record(proposal));
    if (offline) { throw std::runtime_error{"typed graph offline"}; }
    auto result = direct.submit_change(proposal);
    if (std::exchange(lose_reply, false)) { throw std::runtime_error{"typed graph lost reply"}; }
    return result;
  }
};
} // namespace

// Two offline creators cannot reuse rejected IDs; exact retries and epoch recovery preserve cycles.
TEST(managed_reference_collaboration, concurrent_cycles_retry_conflict_and_fresh_epoch_recovery) {
  store_type source{initial_graph(), graph_document};
  link_source(source);
  const auto source_before = source.save();
  authority_type authority{root{}, 1U};
  authority.load_document(source_before);
  authority.open_session(alice_session);
  authority.open_session(bob_session);
  replica_type alice, bob;
  alice.synchronize(authority.snapshot(), authority.context());
  bob.synchronize(authority.snapshot(), authority.context());
  const auto alice_lease = authority.reserve_ids(alice_session, 4U);
  const auto bob_lease = authority.reserve_ids(bob_session, 4U);
  ASSERT_LT(alice_lease.last, bob_lease.first);
  alice.initialize_identity_lease(alice_session, alice_lease);
  bob.initialize_identity_lease(bob_session, bob_lease);
  std::array<id_type, 2> alice_ids{}, rejected_bob_ids{}, bob_ids{};
  const auto first = alice.propose(alice_session, 1U, [&](auto& edit) {
    alice_ids = append_cycle(edit, "alice");
  });
  const auto stale = bob.propose(bob_session, 1U, [&](auto& edit) {
    rejected_bob_ids = append_cycle(edit, "rejected-bob");
  });
  EXPECT_EQ(traits::payload(*alice.read()).nodes.size(), 2U);
  EXPECT_EQ(traits::payload(*bob.read()).nodes.size(), 2U);
  EXPECT_EQ(alice_ids[0], alice_lease.first);
  EXPECT_EQ(rejected_bob_ids[0], bob_lease.first);
  const auto accepted = authority.submit_change(first, alice_session, 0U);
  ASSERT_EQ(accepted->status, managed::collaboration_status::accepted);
  const auto authority_pin = authority.read();
  const auto saved = authority.save_document();
  EXPECT_EQ(authority.submit_change(first, alice_session, 0U), accepted);
  EXPECT_EQ(authority.read(), authority_pin);
  EXPECT_EQ(authority.undo_operation(alice_session), 1U);
  EXPECT_EQ(authority.submit_change(stale, bob_session, 0U)->status,
      managed::collaboration_status::conflict);
  EXPECT_EQ(authority.save_document(), saved);
  EXPECT_EQ(authority.undo_operation(bob_session), 0U);
  bob.synchronize(authority.snapshot(), authority.context());
  const auto rebased = bob.propose(bob_session, 2U, [&](auto& edit) {
    bob_ids = append_cycle(edit, "bob");
  });
  EXPECT_EQ(bob_ids[0], rejected_bob_ids[1] + id_type{1U});
  const auto second = authority.submit_change(rebased, bob_session, 0U);
  ASSERT_EQ(second->status, managed::collaboration_status::accepted);
  EXPECT_EQ(alice.apply_accepted_change(*second->accepted, authority.context()),
      managed::replication_status::resync_required);
  EXPECT_EQ(alice.apply_accepted_change(*accepted->accepted, authority.context()),
      managed::replication_status::applied);
  EXPECT_EQ(alice.apply_accepted_change(*second->accepted, authority.context()),
      managed::replication_status::applied);
  EXPECT_EQ(alice.apply_accepted_change(*second->accepted, authority.context()),
      managed::replication_status::duplicate);
  bob.synchronize(authority.snapshot(), authority.context());
  expect_cycle(*alice.read(), alice_ids);
  expect_cycle(*alice.read(), bob_ids);
  expect_cycle(*bob.read(), alice_ids);
  expect_cycle(*authority.read(), bob_ids);
  EXPECT_EQ(authority.snapshot().allocated_id, bob_lease.watermark);
  EXPECT_EQ(source.save(), source_before);
  authority_type recovered{root{}, 2U};
  recovered.load_document(authority.save_document());
  EXPECT_EQ(recovered.snapshot().allocated_id, bob_lease.watermark);
  EXPECT_EQ(recovered.sequence(), 0U);
  recovered.open_session(alice_session);
  EXPECT_THROW((void)recovered.submit_change(first, alice_session, 0U), std::invalid_argument);
  alice.synchronize(recovered.snapshot(), recovered.context());
  const auto fresh = recovered.reserve_ids(alice_session, 2U);
  EXPECT_GT(fresh.first, bob_lease.watermark);
  alice.initialize_identity_lease(alice_session, fresh);
  std::array<id_type, 2> recovered_ids{};
  const auto next = alice.propose(alice_session, 1U, [&](auto& edit) {
    recovered_ids = append_cycle(edit, "recovered");
  });
  const auto final = recovered.submit_change(next, alice_session, 0U);
  ASSERT_EQ(final->status, managed::collaboration_status::accepted);
  EXPECT_EQ(recovered_ids[0], fresh.first);
  expect_cycle(*recovered.read(), alice_ids);
  expect_cycle(*recovered.read(), bob_ids);
  expect_cycle(*recovered.read(), recovered_ids);
}

// Foreign, wrong-type, deleted and malformed typed edges never publish authority state or history.
TEST(managed_reference_collaboration, forged_typed_proposals_fail_atomically_and_then_recover) {
  store_type source{initial_graph(), graph_document};
  link_source(source);
  authority_type authority{root{}, 1U};
  authority.load_document(source.save());
  authority.open_session(alice_session);
  replica_type replica;
  replica.synchronize(authority.snapshot(), authority.context());
  auto proposal = replica.propose(alice_session, 1U, [](auto&) {});
  const auto original = managed::decode_collaboration_record<records::model_change>(proposal.command);
  const auto root_before = authority.read();
  const auto before = authority.save_document();
  const auto& baseline = traits::payload(*root_before);
  std::vector<std::vector<std::uint8_t>> invalid;
  for (const auto reference : {node_token({801U, 802U}, baseline.nodes[0].persistent_id),
           node_token(graph_document, baseline.extra.persistent_id),
           node_token(graph_document, std::numeric_limits<id_type>::max())}) {
    auto value = *root_before;
    traits::payload(value).info.links.push_back(reference);
    invalid.push_back(managed::encode_collaboration_record(value));
  }
  auto deleted = *root_before;
  traits::payload(deleted).nodes.erase(traits::payload(deleted).nodes.begin());
  invalid.push_back(managed::encode_collaboration_record(deleted));
  auto malformed = original.snapshot;
  const auto reference = *node_traits::payload(baseline.nodes[0]).next;
  rohit::full_stream_auto_alloc token_output{};
  rohit::serializer::serialize_to<rohit::serializer::binary_integer>(token_output, reference);
  const std::vector<std::uint8_t> reference_bytes(token_output.begin(),
      token_output.begin() + token_output.current_offset());
  const auto found = std::search(malformed.begin(), malformed.end(),
      reference_bytes.begin(), reference_bytes.end());
  ASSERT_NE(found, malformed.end());
  std::array<std::uint8_t, sizeof(std::uint64_t)> class_bytes{};
  for (std::size_t index = 0; index < class_bytes.size(); ++index) {
    class_bytes[index] = static_cast<std::uint8_t>(node_traits::class_id >>
        (index * std::numeric_limits<std::uint8_t>::digits));
  }
  const auto class_field = std::search(reference_bytes.begin(), reference_bytes.end(),
      class_bytes.begin(), class_bytes.end());
  ASSERT_NE(class_field, reference_bytes.end());
  std::fill_n(found + (class_field - reference_bytes.begin()), class_bytes.size(), std::uint8_t{});
  invalid.push_back(std::move(malformed));
  for (const auto& snapshot : invalid) {
    auto change = original;
    change.snapshot = snapshot;
    proposal.command = managed::encode_collaboration_record(change);
    const auto result = authority.submit_change(proposal, alice_session, 0U);
    EXPECT_EQ(result->status, managed::collaboration_status::failed);
    EXPECT_FALSE(result->accepted);
    EXPECT_EQ(authority.read(), root_before);
    EXPECT_EQ(authority.save_document(), before);
    EXPECT_EQ(authority.sequence(), 0U);
    EXPECT_EQ(authority.undo_operation(alice_session), 0U);
    ++proposal.operation;
  }
  const auto valid = replica.propose(alice_session, proposal.operation, [&](auto& edit) {
    edit.registry().template edit<node>(baseline.nodes[0].persistent_id).set_label("recovered");
  });
  EXPECT_EQ(authority.submit_change(valid, alice_session, 0U)->status,
      managed::collaboration_status::accepted);
  expect_cycle(*authority.read(), {baseline.nodes[0].persistent_id, baseline.nodes[1].persistent_id});
}

// Receiver resource failure cannot publish a valid typed graph or advance its accepted cursor.
TEST(managed_reference_collaboration, receiver_resource_failure_preserves_graph_and_cursor) {
  store_type source{initial_graph(), graph_document};
  link_source(source);
  authority_type authority{root{}, 1U};
  authority.load_document(source.save());
  authority.open_session(alice_session);
  auto resource = std::make_shared<failing_resource>();
  managed::store_options options{};
  options.allocation_resource = resource;
  replica_type replica{options};
  replica.synchronize(authority.snapshot(), authority.context());
  replica.initialize_identity_lease(alice_session, authority.reserve_ids(alice_session, 2U));
  std::array<id_type, 2> ids{};
  const auto proposal = replica.propose(alice_session, 1U, [&](auto& edit) {
    ids = append_cycle(edit, "resource");
  });
  const auto accepted = authority.submit_change(proposal, alice_session, 0U);
  ASSERT_EQ(accepted->status, managed::collaboration_status::accepted);
  const auto pin = replica.read();
  const auto bytes = resource->live_bytes;
  resource->reject = true;
  EXPECT_THROW(replica.apply_accepted_change(*accepted->accepted, authority.context()), std::bad_alloc);
  EXPECT_EQ(replica.read(), pin);
  EXPECT_EQ(replica.sequence(), 0U);
  EXPECT_EQ(resource->live_bytes, bytes);
  resource->reject = false;
  EXPECT_EQ(replica.apply_accepted_change(*accepted->accepted, authority.context()),
      managed::replication_status::applied);
  expect_cycle(*replica.read(), ids);
  EXPECT_EQ(replica.sequence(), 1U);
}

// Joining uses the incoming namespace while retaining caller policy and all-or-nothing publication.
TEST(managed_reference_collaboration, initial_join_rejects_foreign_edges_and_original_policy_then_recovers) {
  store_type source{initial_graph(), graph_document};
  link_source(source);
  const auto& nodes = traits::payload(*source.read()).nodes;
  const std::array<id_type, 2> source_ids{nodes[0].persistent_id, nodes[1].persistent_id};
  authority_type authority{root{}, 1U};
  authority.load_document(source.save());
  authority.open_session(alice_session);
  unreliable_transport transport{authority, alice_session};
  auto resource = std::make_shared<failing_resource>();
  managed::store_options options{};
  options.allocation_resource = resource;
  bool reject_policy{};
  std::size_t policy_calls{}, notifications{};
  store_type client{root{}, {901U, 902U}, options, [&](const auto& value) {
    if (!traits::payload(value).nodes.empty()) {
      ++policy_calls;
      expect_cycle(value, source_ids);
      if (reject_policy) { throw std::runtime_error{"original typed-graph policy"}; }
    }
  }};
  client.collaborate(alice_session).bind(transport);
  client.set_publication_callback([&](const auto&) {
    ++notifications;
    EXPECT_EQ(client.document(), graph_document);
    expect_cycle(*client.read(), source_ids);
  });
  const auto pin = client.read();
  const auto placeholder = client.document();
  const auto state = client.state();
  const auto history = client.history_cursor();
  const auto watermark = client.allocated_id();
  const auto allocations = resource->attempts;
  transport.foreign_snapshot = true;
  EXPECT_THROW(client.synchronize(), managed::registry_error);
  EXPECT_EQ(policy_calls, 0U);
  transport.foreign_snapshot = false;
  reject_policy = true;
  EXPECT_THROW(client.synchronize(), std::runtime_error);
  EXPECT_EQ(policy_calls, 1U);
  EXPECT_EQ(client.read(), pin);
  EXPECT_EQ(client.document(), placeholder);
  EXPECT_EQ(client.state(), state);
  EXPECT_EQ(client.history_cursor(), history);
  EXPECT_EQ(client.allocated_id(), watermark);
  EXPECT_EQ(client.collaboration().state()->context.epoch, 0U);
  EXPECT_EQ(resource->attempts, allocations);
  EXPECT_EQ(notifications, 0U);
  reject_policy = false;
  EXPECT_NO_THROW(client.synchronize());
  EXPECT_EQ(policy_calls, 2U);
  EXPECT_EQ(notifications, 1U);
  EXPECT_EQ(client.document(), graph_document);
  expect_cycle(*client.read(), source_ids);
}

// Lost replies survive client recovery without replaying callbacks or remapping graph/history IDs.
TEST(managed_reference_collaboration, local_graph_lost_reply_checkpoint_notifications_and_undo) {
  store_type source{initial_graph(), graph_document};
  link_source(source);
  authority_type authority{root{}, 1U};
  authority.load_document(source.save());
  authority.open_session(alice_session);
  authority.open_session(bob_session);
  unreliable_transport transport{authority, alice_session};
  managed::collaboration_transport bob_transport{authority, bob_session};
  auto alice = std::make_unique<store_type>(root{});
  store_type bob{root{}};
  alice->collaborate(alice_session).bind(transport);
  bob.collaborate(bob_session).bind(bob_transport);
  alice->synchronize();
  bob.synchronize();
  std::size_t callbacks{}, notifications{};
  std::array<id_type, 2> ids{};
  alice->set_publication_callback([&](const auto&) {
    ++notifications;
    expect_cycle(*alice->read(), ids);
  });
  alice->execute_transaction([&](auto& edit) {
    ++callbacks;
    ids = append_cycle(edit, "offline");
  }).throw_if_failed();
  ASSERT_EQ(notifications, 1U);
  bob.execute_transaction([](auto& edit) { edit.root().set_ordinary_id(9000U); }).throw_if_failed();
  transport.offline = true;
  EXPECT_THROW(alice->synchronize(), std::runtime_error);
  EXPECT_EQ(authority.sequence(), 0U);
  const auto request = alice->collaboration().state()->in_flight;
  ASSERT_FALSE(request.empty());
  transport.offline = false;
  transport.lose_reply = true;
  EXPECT_THROW(alice->synchronize(), std::runtime_error);
  EXPECT_EQ(authority.sequence(), 1U);
  EXPECT_EQ(alice->collaboration().state()->in_flight, request);
  EXPECT_EQ(callbacks, 1U);
  const auto checkpoint = alice->save();
  const auto checkpoint_watermark = alice->allocated_id();
  const auto burned = authority.snapshot().allocated_id;
  alice.reset();
  alice = std::make_unique<store_type>(root{});
  alice->collaborate(alice_session).bind(transport);
  std::size_t recovered_notifications{};
  alice->set_publication_callback([&](const auto&) {
    ++recovered_notifications;
    const auto& value = traits::payload(*alice->read());
    if (value.nodes.size() == 4U) { expect_cycle(*alice->read(), ids); }
  });
  alice->load(checkpoint);
  EXPECT_EQ(recovered_notifications, 1U);
  EXPECT_EQ(alice->allocated_id(), checkpoint_watermark);
  EXPECT_EQ(alice->collaboration().state()->in_flight, request);
  alice->synchronize();
  ASSERT_EQ(transport.requests.size(), 3U);
  EXPECT_EQ(transport.requests[0], transport.requests[1]);
  EXPECT_EQ(transport.requests[1], transport.requests[2]);
  EXPECT_EQ(authority.sequence(), 1U);
  EXPECT_TRUE(alice->collaboration().state()->pending.empty());
  bob.synchronize();
  alice->synchronize();
  EXPECT_EQ(traits::payload(*alice->read()).ordinary_id, 9000U);
  EXPECT_EQ(traits::payload(*authority.read()).ordinary_id, 9000U);
  expect_cycle(*bob.read(), ids);
  expect_cycle(*alice->read(), ids);
  EXPECT_EQ(alice->collaboration().remote_id(ids[0]), ids[0]);
  EXPECT_EQ(callbacks, 1U);
  const auto quiet = recovered_notifications;
  alice->synchronize();
  EXPECT_EQ(recovered_notifications, quiet);
  alice->undo();
  EXPECT_EQ(traits::payload(*alice->read()).nodes.size(), 2U);
  alice->synchronize();
  EXPECT_EQ(traits::payload(*authority.read()).nodes.size(), 2U);
  alice->redo();
  alice->synchronize();
  expect_cycle(*authority.read(), ids);
  EXPECT_EQ(traits::payload(*authority.read()).ordinary_id, 9000U);
  EXPECT_EQ(alice->allocated_id(), burned);
}
