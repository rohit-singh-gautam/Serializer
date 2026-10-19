// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "reference_portable_writer.hpp"

#include <rohit/serializer_creator.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace rohit::serializer::writer::reference_support {

// Identify a compiler-lowered inline value reference without changing named address references.
inline bool native_value_reference(const class_node& value) {
  return value.reference && value.reference->value_projection;
}

// Build the selected native field path from validated public member and base components.
template <typename Field>
inline std::string native_value_path(const class_node& value, std::string_view language,
    std::string root, Field field) {
  const auto& reference = *value.reference;
  if (language == "c" && reference.source_path.empty()) { return "(*" + root + ")"; }
  const auto first = root;
  for (std::size_t index{}; index < reference.source_path.size(); ++index) {
    for (const auto base : reference.source_base_paths.at(index)) {
      root += (language == "c" && root == first ? "->" : ".") +
          std::string{"base"} + std::to_string(base);
    }
    root += (language == "c" && root == first ? "->" : ".") +
        field(reference.source_path[index]);
  }
  return root;
}

// Emit Rust's small export-only binding holder without storing the selected payload.
inline std::string native_value_runtime(std::string_view language) {
  if (language != "rust") { return {}; }
  return R"SRL(
/// Runtime ownership never enters the wire payload or stores a selected payload copy.
#[derive(Clone)]
pub struct SrlValueReferenceBinding<Handle>(pub Option<Handle>);
impl<Handle> std::fmt::Debug for SrlValueReferenceBinding<Handle> {
    /// Format binding state without borrowing or inspecting the source owner.
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { formatter.write_str("SrlValueReferenceBinding") }
}
impl<Handle> PartialEq for SrlValueReferenceBinding<Handle> {
    /// Exclude process-local ownership handles from generated structural equality.
    fn eq(&self, _other: &Self) -> bool { true }
}
)SRL";
}

// Emit live value access for Rust and mobile outputs without owning selected-value storage.
template <typename Type, typename Field>
inline std::string native_value_methods(const class_node& value, std::string_view language,
    Type type, Field field) {
  const auto& reference = *value.reference;
  const bool shared = reference.mode == reference_declaration::mode_type::shared;
  const bool borrowed = reference.mode == reference_declaration::mode_type::borrowed;
  std::string source;
  if (language == "rust" && borrowed) {
    source = R"SRL(
    /// Borrow a selected field for the native lifetime of its owner; never retain the owner.
    pub fn bind(&mut self, owner: &'a @owner@) -> SrlResult<()> { self.srl_owner = Some(&@borrowed_path@); Ok(()) }
    /// Detach the export reference.
    pub fn unbind(&mut self) { self.srl_owner = None; }
    /// Read the selected field while Rust enforces its source lifetime.
    pub fn with_value<Result>(&self, callback: impl FnOnce(&@value@) -> Result) -> SrlResult<Result> {
        self.srl_owner.map(callback).ok_or(Error("Unbound export reference"))
    }
    /// Copy only when the caller explicitly asks for an owning result.
    pub fn read_value(&self) -> SrlResult<@value@> { self.with_value(|value| value.clone()) }
    /// Report whether an ordinary native borrow is attached.
    pub fn is_bound(&self) -> bool { self.srl_owner.is_some() }
)SRL";
  } else if (language == "rust") {
    source = R"SRL(
    /// Bind the live owner without copying its selected field.
    pub fn bind(&mut self, owner: &std::rc::Rc<std::cell::RefCell<@owner@>>) -> SrlResult<()> {
        self.srl_owner.0 = Some(@retain@);
        Ok(())
    }
    /// Detach the export reference; output requires a new live binding.
    pub fn unbind(&mut self) { self.srl_owner.0 = None; }
    /// Borrow live storage during a synchronous callback; unbound or expired owners fail.
    pub fn with_value<Result>(&self, callback: impl FnOnce(&@value@) -> Result) -> SrlResult<Result> {
        let binding = self.srl_owner.0.as_ref().ok_or(Error("Unbound export reference"))?;
        let handle = @handle@;
        let srl_owner = handle.try_borrow().map_err(|_| Error("Reference owner is mutably borrowed"))?;
        Ok(callback(&@path@))
    }
    /// Copy the selected value only when an owning result is explicitly requested.
    pub fn read_value(&self) -> SrlResult<@value@> { self.with_value(|value| value.clone()) }
    /// Report whether the export reference can currently borrow its owner.
    pub fn is_bound(&self) -> bool { self.srl_owner.0.is_some() && self.with_value(|_| ()).is_ok() }
)SRL";
  } else if (language == "swift") {
    source = R"SRL(
  private @weak@var srlOwner: SrlReferenceOwner<@owner@>? = nil
  /// Bind stable owner identity without materializing the selected payload.
  public mutating func bind(_ owner: SrlReferenceOwner<@owner@>) { srlOwner = owner }
  /// Detach the export reference; output requires a new live binding.
  public mutating func unbind() { srlOwner = nil }
  /// Read the current field using Swift value semantics.
  public func readValue() throws -> @value@ {
    guard let holder = srlOwner else { throw SerializerError.invalid("Unbound or expired export reference") }
    return @path@
  }
  /// Consume a selected value synchronously; do not retain scoped projections of it.
  public func withValue<Result>(_ callback: (@value@) throws -> Result) throws -> Result { try callback(readValue()) }
  /// Report whether the export reference still has a live owner.
  public func isBound() -> Bool { srlOwner != nil }
)SRL";
  } else if (language == "kotlin") {
    source = R"SRL(
  private var srlOwner: @owner_storage@? = null
  /** Bind an owner without copying its selected value. */
  fun bind(owner: @owner@) { srlOwner = @retain@ }
  /** Detach the export reference; output requires a new live binding. */
  fun unbind() { srlOwner = null }
  /** Read the selected value; unbound or expired owners fail. */
  fun readValue(): @value@ {
    val holder = @owner_read@ ?: throw IllegalStateException("Unbound or expired export reference")
    return @path@
  }
  /** Invoke the callback synchronously using the live selected field. */
  fun <Result> withValue(callback: (@value@) -> Result): Result = callback(readValue())
  /** Report whether the export reference still has a live owner. */
  fun isBound(): Boolean = @owner_read@ != null
)SRL";
  }
  const auto root = language == "rust" ? "srl_owner" : language == "swift" ? "holder.value" : "holder";
  const auto retain = language == "rust" ? (shared ? "std::rc::Rc::clone(owner)" : "std::rc::Rc::downgrade(owner)") :
      shared ? "owner" : "java.lang.ref.WeakReference(owner)";
  const auto owner_type = type(reference.owner);
  return reference_expand(std::move(source), {{"@owner@", type(reference.owner)},
      {"@value@", type(reference.value_type)},
      {"@path@", native_value_path(value, language, root, field)}, {"@retain@", retain},
      {"@borrowed_path@", native_value_path(value, language, "owner", field)},
      {"@weak@", shared ? "" : "weak "},
      {"@owner_storage@", shared ? owner_type : "java.lang.ref.WeakReference<" + owner_type + ">"},
      {"@owner_read@", shared ? "srlOwner" : "srlOwner?.get()"},
      {"@handle@", shared ? "binding" : "binding.upgrade().ok_or(Error(\"Expired reference owner\"))?"}});
}

// Emit C's checked owner leases and typed export-only access without durable identities.
template <typename Type, typename Field>
inline std::string c_native_value_methods(const class_node& value, const std::string& name,
    Type type, Field field) {
  const auto& reference = *value.reference;
  const bool shared = reference.mode == reference_declaration::mode_type::shared;
  if (reference.mode == reference_declaration::mode_type::borrowed) {
    return reference_expand(R"SRL(
/* Borrow a selected field; the application keeps its owner alive and stable until unbind. */
static inline srl_status @name@_bind(@name@* srl_value, const @owner@* srl_owner) {
  if (!srl_value || !srl_owner) { return srl_invalid; }
  srl_value->srl_value = &@path@; return srl_ok;
}
/* Detach without freeing or accessing the application-owned payload. */
static inline void @name@_unbind(@name@* srl_value) { if (srl_value) { srl_value->srl_value = NULL; } }
/* Return a borrowed pointer; the caller must preserve the source lifetime and storage. */
static inline srl_status @name@_read_value(const @name@* srl_value, const @value@** srl_result) {
  if (!srl_value || !srl_result || !srl_value->srl_value) { return srl_invalid; }
  *srl_result = srl_value->srl_value; return srl_ok;
}
/* Consume the application-owned payload during one synchronous callback. */
static inline srl_status @name@_with_value(const @name@* srl_value, void (*srl_callback)(const @value@*, void*), void* srl_context) {
  const @value@* srl_selected = NULL;
  if (!srl_callback || @name@_read_value(srl_value, &srl_selected) != srl_ok) { return srl_invalid; }
  srl_callback(srl_selected, srl_context); return srl_ok;
}
/* Report attachment; raw C borrows cannot detect a violated source lifetime. */
static inline bool @name@_is_bound(const @name@* srl_value) { return srl_value && srl_value->srl_value; }
)SRL", {{"@name@", name}, {"@owner@", type(reference.owner)}, {"@value@", type(reference.value_type)},
      {"@path@", native_value_path(value, "c", "srl_owner", field)}});
  }
  return reference_expand(R"SRL(
/* Create a typed owner lease; the final strong release invokes the supplied destructor. */
static inline srl_reference_scope* @name@_create_scope(@owner@* srl_owner, void (*srl_release)(void*)) {
  srl_reference_scope* srl_scope = srl_reference_scope_create(srl_owner, srl_release);
  if (srl_scope) { srl_scope->owner_type = "@owner_key@"; } return srl_scope;
}
/* Detach before releasing an owner whose destructor may itself destroy this wrapper. */
static inline void @name@_unbind(@name@* srl_value) {
  if (!srl_value) { return; }
  srl_reference_scope* srl_previous = srl_value->srl_reference;
  srl_value->srl_reference = NULL;
  if (srl_previous) { @release@(srl_previous); }
}
/* Bind a live typed owner without copying the selected field. */
static inline srl_status @name@_bind(@name@* srl_value, srl_reference_scope* srl_scope) {
  if (!srl_value || !srl_scope || !srl_scope->owner || !srl_scope->owners || !srl_scope->owner_type || strcmp(srl_scope->owner_type, "@owner_key@")) { return srl_invalid; }
  srl_status srl_status_value = @retain@(srl_scope);
  if (srl_status_value != srl_ok) { return srl_status_value; }
  srl_reference_scope* srl_previous = srl_value->srl_reference;
  srl_value->srl_reference = srl_scope;
  if (srl_previous) { @release@(srl_previous); }
  return srl_ok;
}
/* Borrow the live field; unbound and expired owners fail. Never free the returned pointer. */
static inline srl_status @name@_read_value(const @name@* srl_value, const @value@** srl_result) {
  if (!srl_value || !srl_result) { return srl_invalid; }
  if (!srl_value->srl_reference) { return srl_invalid; }
  const srl_reference_scope* srl_scope = srl_value->srl_reference;
  if (!srl_scope->owners || !srl_scope->owner || !srl_scope->owner_type || strcmp(srl_scope->owner_type, "@owner_key@")) { return srl_invalid; }
  const @owner@* srl_owner = (const @owner@*)srl_scope->owner;
  *srl_result = &@path@; return srl_ok;
}
/* Hold a temporary strong owner lease throughout one synchronous read callback. */
static inline srl_status @name@_with_value(const @name@* srl_value, void (*srl_callback)(const @value@*, void*), void* srl_context) {
  const @value@* srl_selected = NULL;
  if (!srl_callback || @name@_read_value(srl_value, &srl_selected) != srl_ok) { return srl_invalid; }
  srl_reference_scope* srl_scope = srl_value->srl_reference;
  if (srl_scope) { srl_status srl_status_value = srl_reference_scope_retain(srl_scope); if (srl_status_value != srl_ok) { return srl_status_value; } }
  srl_callback(srl_selected, srl_context);
  if (srl_scope) { srl_reference_scope_release(srl_scope); }
  return srl_ok;
}
/* Report whether the export reference still has a live owner. */
static inline bool @name@_is_bound(const @name@* srl_value) {
  const @value@* srl_selected = NULL;
  return srl_value && srl_value->srl_reference && @name@_read_value(srl_value, &srl_selected) == srl_ok;
}
)SRL", {{"@name@", name}, {"@owner@", type(reference.owner)},
      {"@owner_key@", reference.owner.get_full_name()}, {"@value@", type(reference.value_type)},
      {"@path@", native_value_path(value, "c", "srl_owner", field)},
      {"@retain@", shared ? "srl_reference_scope_retain" : "srl_reference_scope_observe"},
      {"@release@", shared ? "srl_reference_scope_release" : "srl_reference_scope_unobserve"}});
}

} // namespace rohit::serializer::writer::reference_support
