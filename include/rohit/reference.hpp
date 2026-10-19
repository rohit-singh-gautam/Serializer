// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// See LICENSE-RUNTIME for permission to use this runtime in proprietary applications.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rohit::serializer {

// Select a live binding lifetime independently of its portable durable address.
enum class reference_mode { borrowed, shared, weak };

// Borrow address metadata only for one synchronous validation or lookup.
struct reference_address_view {
  std::string_view document_id{};
  std::uint64_t owner_id{};
  std::uint64_t generation{};
  std::span<const std::uint32_t> field_path{};
};

// Own an address snapshot so later metadata edits cannot silently redirect a binding.
struct reference_address {
  std::string document_id{};
  std::uint64_t owner_id{};
  std::uint64_t generation{};
  std::vector<std::uint32_t> field_path{};

  // Borrow this address; the owning object must outlive the returned view.
  reference_address_view view() const noexcept {
    return {document_id, owner_id, generation, field_path};
  }
};

// Compare address components without allocating or dereferencing an owner.
inline bool reference_address_equal(reference_address_view left,
                                    reference_address_view right) noexcept {
  if (left.document_id != right.document_id || left.owner_id != right.owner_id ||
      left.generation != right.generation || left.field_path.size() != right.field_path.size()) {
    return false;
  }
  for (std::size_t index{}; index < left.field_path.size(); ++index) {
    if (left.field_path[index] != right.field_path[index]) { return false; }
  }
  return true;
}

// Reject incomplete addresses before retaining any owner or changing live bindings.
inline void validate_reference_address(reference_address_view address) {
  if (address.document_id.empty() || address.owner_id == 0 || address.generation == 0 ||
      address.field_path.empty()) {
    throw std::invalid_argument{"Reference address needs a document, owner, generation and field path"};
  }
  for (const auto field : address.field_path) {
    if (field == 0) { throw std::invalid_argument{"Reference field IDs must be nonzero"}; }
  }
}

// Keep nonserialized owner state and validate its exact durable address on every access.
// Borrowed owners must remain alive and at a stable address. Their fields may change
// between calls. Referents must stay unchanged during access; bindings add no synchronization.
// Shared and weak bindings retain a strong owner locally during callbacks.
template <typename Owner, reference_mode Mode>
class reference_binding {
  using owner_handle = std::conditional_t<Mode == reference_mode::borrowed, const Owner*,
      std::conditional_t<Mode == reference_mode::shared,
          std::shared_ptr<const Owner>, std::weak_ptr<const Owner>>>;
  owner_handle owner_{};
  std::optional<reference_address> address_{};

  // Prepare the complete address before publishing any retained owner state.
  static reference_address copy_address(reference_address_view address) {
    validate_reference_address(address);
    return {std::string{address.document_id}, address.owner_id, address.generation,
        std::vector<std::uint32_t>{address.field_path.begin(), address.field_path.end()}};
  }

public:
  // Start without an address or retained owner.
  reference_binding() = default;
  // Copy a live binding and its independent address guard.
  reference_binding(const reference_binding&) = default;
  // Transfer both components and leave every moved-from binding unbound.
  reference_binding(reference_binding&& other) noexcept { swap(other); }
  // Destroy the address guard before releasing an owner that can destroy its container.
  ~reference_binding() = default;

  // Swap fully prepared bindings without releasing either owner during publication.
  void swap(reference_binding& other) noexcept {
    using std::swap;
    swap(address_, other.address_);
    swap(owner_, other.owner_);
  }

  // Prepare every allocation before replacing this binding; release the old owner last.
  reference_binding& operator=(const reference_binding& other) {
    if (this == std::addressof(other)) { return *this; }
    reference_binding candidate{other};
    swap(candidate);
    return *this;
  }

  // Publish both components before releasing an old owner that may contain this object.
  reference_binding& operator=(reference_binding&& other) noexcept {
    if (this == std::addressof(other)) { return *this; }
    reference_binding candidate{std::move(other)};
    swap(candidate);
    return *this;
  }

  // Bind an immutable lvalue without extending its lifetime; failures preserve this binding.
  void bind(const Owner& owner, reference_address_view address)
      requires (Mode == reference_mode::borrowed) {
    reference_binding candidate;
    candidate.address_ = copy_address(address);
    candidate.owner_ = std::addressof(owner);
    swap(candidate);
  }

  // Temporary owners cannot satisfy the borrowed lifetime contract.
  void bind(const Owner&&, reference_address_view)
      requires (Mode == reference_mode::borrowed) = delete;

  // Retain an immutable shared owner only after validating and copying its address.
  void bind(std::shared_ptr<const Owner> owner, reference_address_view address)
      requires (Mode == reference_mode::shared) {
    if (!owner || owner.use_count() == 0) {
      throw std::invalid_argument{"Shared reference needs an owning handle"};
    }
    reference_binding candidate;
    candidate.address_ = copy_address(address);
    candidate.owner_ = std::move(owner);
    swap(candidate);
  }

  // Observe an existing shared owner without extending its lifetime beyond each access.
  void bind(std::weak_ptr<const Owner> owner, reference_address_view address)
      requires (Mode == reference_mode::weak) {
    const auto pin = owner.lock();
    if (!pin || pin.use_count() == 0) {
      throw std::invalid_argument{"Weak reference needs a live owning handle"};
    }
    reference_binding candidate;
    candidate.address_ = copy_address(address);
    candidate.owner_ = std::move(owner);
    swap(candidate);
  }

  // Release live ownership without modifying the separately stored durable address.
  void unbind() noexcept {
    address_.reset();
    // Releasing an aliasing owner may destroy the object containing this binding.
    // Detach all remaining state first and never access members after the release.
    owner_ = {};
  }

  // Test address identity and weak expiry; borrowed lifetime remains the caller's contract.
  bool is_bound(reference_address_view current) const noexcept {
    if (!address_ || !reference_address_equal(address_->view(), current)) { return false; }
    if constexpr (Mode == reference_mode::weak) { return !owner_.expired(); }
    else { return static_cast<bool>(owner_); }
  }

  // Invoke against an immutable live owner. Returned values own their storage; callbacks
  // must not retain borrowed addresses. Callback exceptions propagate without rebinding.
  template <typename Callback>
  decltype(auto) with_owner(reference_address_view current, Callback&& callback) const {
    static_assert(!std::is_reference_v<std::invoke_result_t<Callback, const Owner&>>,
        "Reference callbacks cannot return a borrowed reference");
    if (!address_ || !reference_address_equal(address_->view(), current)) {
      throw std::logic_error{"Reference is unbound or its durable address changed"};
    }
    if constexpr (Mode == reference_mode::borrowed) {
      if (!owner_) { throw std::logic_error{"Reference is unbound"}; }
      return std::invoke(std::forward<Callback>(callback), *owner_);
    } else {
      const auto pin = [&] {
        if constexpr (Mode == reference_mode::weak) { return owner_.lock(); }
        else { return owner_; }
      }();
      if (!pin) { throw std::logic_error{"Reference owner expired"}; }
      return std::invoke(std::forward<Callback>(callback), *pin);
    }
  }
};

} // namespace rohit::serializer
