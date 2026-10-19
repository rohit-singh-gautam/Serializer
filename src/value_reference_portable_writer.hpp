// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::reference_support {

// Substitute generated native spellings without interpreting application source.
inline std::string value_reference_expand(std::string source,
    const std::vector<std::pair<std::string, std::string>>& replacements) {
  for (const auto& [key, value] : replacements) {
    std::size_t offset{};
    while ((offset = source.find(key, offset)) != std::string::npos) {
      source.replace(offset, key.size(), value);
      offset += value.size();
    }
  }
  return source;
}

// Emit value-transparent lifetime bindings; codecs access the selected storage directly.
template <typename Type, typename Field>
inline std::string value_reference_portable_methods(const class_node& record,
    const std::string& name, std::string_view language, Type type, Field field) {
  const auto& reference = *record.reference;
  const auto& selected = record.member_list.front();
  const bool shared = reference.mode == reference_declaration::mode_type::shared;
  auto native = type(reference.value_type);
  if (selected.modifier == member::modifier_type::array) {
    native = language == "go" ? "[]" + native : language == "csharp" ?
        "System.Collections.Generic.List<" + native + ">" :
        language == "python" ? "list[" + native + "]" : native + "[]";
  }
  auto boxed = native;
  if (language == "java") {
    boxed = native == "boolean" ? "Boolean" : native == "byte" ? "Byte" :
        native == "short" ? "Short" : native == "int" ? "Integer" :
        native == "long" ? "Long" : native == "float" ? "Float" :
        native == "double" ? "Double" : native == "char" ? "Character" : native;
  }
  std::string path{"owner"};
  for (std::size_t index{}; index < reference.source_path.size(); ++index) {
    for (const auto base : reference.source_base_paths.at(index)) {
      path += "." + field(std::string{language == "go" || language == "csharp" ? "Base" : "base"} +
          std::to_string(base), access_type::public_access);
    }
    path += "." + field(reference.source_path[index], access_type::public_access);
  }
  std::string source;
  if (language == "js") {
    source = R"SRL(
  #srlValueOwner = null;
  // Bind the owner without copying its selected field or storing wire metadata.
  bind(owner) {
    if (!(owner instanceof @owner@)) { throw new TypeError('Invalid reference owner'); }
    this.#srlValueOwner = @holder@;
  }
  // Release the live owner; subsequent access requires another binding.
  unbind() { this.#srlValueOwner = null; }
  // Read the original storage; arrays and records retain their native identity.
  readValue() {
    if (this.#srlValueOwner === null) { throw new Error('Unbound export reference'); }
    const owner = @get@;
    if (owner == null) { throw new Error('Expired reference owner'); }
    return @path@;
  }
  // Consume the selected value synchronously without materializing another model.
  withValue(callback) { return callback(this.readValue()); }
  // Report availability of the selected live binding.
  isBound() { return this.#srlValueOwner !== null && @alive@; }
)SRL";
  } else if (language == "typescript") {
    source = R"SRL(
  /** Bind the owner without copying its field. */
  bind(owner: @owner@): void;
  /** Release the owner; subsequent access requires another binding. */
  unbind(): void;
  /** Read the original native field handle. */
  readValue(): @value@;
  /** Consume the selected value synchronously. */
  withValue<Result>(callback: (value: @value@) => Result): Result;
  /** Test live binding availability. */
  isBound(): boolean;
)SRL";
  } else if (language == "java") {
    source = R"SRL(
  private @holder_type@ srlValueOwner;
  /** Bind the owner without copying its selected storage. */
  public void bind(@owner@ owner) {
    java.util.Objects.requireNonNull(owner);
    srlValueOwner = @holder@;
  }
  /** Release the owner; subsequent access requires another binding. */
  public void unbind() { srlValueOwner = null; }
  /** Read the original field handle. */
  public @value@ readValue() {
    if (srlValueOwner == null) { throw new IllegalStateException("Unbound export reference"); }
    @owner@ owner = @get@;
    if (owner == null) { throw new IllegalStateException("Expired reference owner"); }
    return @path@;
  }
  /** Consume the selected value synchronously. */
  public <Result> Result withValue(java.util.function.Function<@boxed@, Result> callback) { return callback.apply(readValue()); }
  /** Test live owner availability. */
  public boolean isBound() { return srlValueOwner != null && @alive@; }
)SRL";
  } else if (language == "csharp") {
    source = R"SRL(
  private @holder_type@? srlValueOwner;
  // Bind the owner without copying its selected storage.
  public void Bind(@owner@ owner) {
    System.ArgumentNullException.ThrowIfNull(owner);
    srlValueOwner = @holder@;
  }
  // Release the live owner; subsequent access requires another binding.
  public void Unbind() { srlValueOwner = null; }
  // Read the original native field handle.
  public @value@ ReadValue() {
    if (srlValueOwner == null) { throw new System.InvalidOperationException("Unbound export reference"); }
    @owner@ owner = @get@;
    return @path@;
  }
  // Consume the selected field synchronously.
  public Result WithValue<Result>(System.Func<@value@, Result> callback) { return callback(ReadValue()); }
  // Test live binding availability.
  public bool IsBound() { return srlValueOwner != null && @alive@; }
)SRL";
  } else if (language == "python") {
    source = R"SRL(
    def bind(self, owner):
        """Bind the owner without copying its selected field."""
        if not isinstance(owner, @owner@):
            raise TypeError('Invalid reference owner')
        self._srl_reference = @holder@
    def unbind(self):
        """Release the live owner; subsequent access requires another binding."""
        self._srl_reference = None
    def read_value(self):
        """Read the original native field handle."""
        if self._srl_reference is None:
            raise RuntimeError('Unbound export reference')
        owner = @get@
        if owner is None:
            raise RuntimeError('Expired reference owner')
        return @path@
    def with_value(self, callback):
        """Consume the selected field synchronously without an intermediate model."""
        return callback(self.read_value())
    def is_bound(self):
        """Test live binding availability."""
        return self._srl_reference is not None and @alive@
)SRL";
  } else if (language == "go") {
    source = R"SRL(
// @name@Scope explicitly bounds borrowed and weak owner access; callers close it before dropping the owner.
type @name@Scope struct { owner @owner@ }
// New@name@Scope starts a checked owner scope without copying its storage.
func New@name@Scope(owner @owner@) *@name@Scope { return &@name@Scope{owner: owner} }
// Close invalidates every reference using this borrow scope.
func (scope *@name@Scope) Close() { if scope != nil { scope.owner = nil } }
// Bind selects one owner without any persistent identity or field copy.
func (value *@name@) Bind(@go_argument@) error {
  @go_owner@
  if owner == nil { return fmt.Errorf("Invalid reference owner") }
  @go_publish@
  return nil
}
// Unbind releases the live owner; subsequent access requires another binding.
func (value *@name@) Unbind() { @go_unbind@ }
// ReadValue returns a native record pointer, slice header, or scalar without copying payload_record buffers.
func (value *@name@) ReadValue() (@value@, error) {
  var empty @value@
  if value == nil { return empty, fmt.Errorf("Nil reference") }
  @go_read_owner@
  if owner == nil { return empty, fmt.Errorf("Unbound export reference") }
  return @path@, nil
}
// WithValue consumes selected storage synchronously without copying its elements.
func (value *@name@) WithValue(callback func(@value@)) error { selected, err := value.ReadValue(); if err != nil { return err }; callback(selected); return nil }
// IsBound tests a live owner binding.
func (value *@name@) IsBound() bool { return value != nil && @go_alive@ }
)SRL";
  }
  const auto owner = type(reference.owner);
  const std::string holder_name = language == "js" ? "this.#srlValueOwner" :
      language == "python" ? "self._srl_reference" : "srlValueOwner";
  const std::string weak_holder = language == "js" ? "new WeakRef(owner)" :
      language == "python" ? "__import__('weakref').ref(owner)" :
      language == "java" ? "new java.lang.ref.WeakReference<>(owner)" :
      "new System.WeakReference<" + owner + ">(owner)";
  const std::string get = shared ? holder_name : language == "js" ? holder_name + ".deref()" :
      language == "python" ? holder_name + "()" : language == "java" ? holder_name + ".get()" :
      "srlValueOwner.TryGetTarget(out var selected) ? selected : throw new System.InvalidOperationException(\"Expired reference owner\")";
  return value_reference_expand(std::move(source), {
      {"@owner@", owner}, {"@value@", native}, {"@boxed@", boxed}, {"@name@", name},
      {"@storage@", field(selected.name, selected.access)}, {"@path@", path},
      {"@holder_type@", shared ? owner : language == "java" ?
          "java.lang.ref.WeakReference<" + owner + ">" : "System.WeakReference<" + owner + ">"},
      {"@holder@", shared ? "owner" : weak_holder}, {"@get@", get},
      {"@alive@", shared ? (language == "python" ? "True" : "true") :
          language == "js" ? holder_name + ".deref() != null" :
          language == "python" ? holder_name + "() is not None" :
          language == "java" ? holder_name + ".get() != null" : "srlValueOwner.TryGetTarget(out _)"},
      {"@go_argument@", shared ? "owner " + owner : "scope *" + name + "Scope"},
      {"@go_owner@", shared ? "" : "var owner " + owner + "; if scope != nil { owner = scope.owner }"},
      {"@go_publish@", shared ? "value.srlReferenceOwner = owner" : "value.srlReferenceScope = scope"},
      {"@go_unbind@", shared ? "value.srlReferenceOwner = nil" : "value.srlReferenceScope = nil"},
      {"@go_read_owner@", shared ? "owner := value.srlReferenceOwner" :
          "if value.srlReferenceScope == nil { return empty, fmt.Errorf(\"Unbound export reference\") }; owner := value.srlReferenceScope.owner"},
      {"@go_alive@", shared ? "value.srlReferenceOwner != nil" :
          "(value.srlReferenceScope != nil && value.srlReferenceScope.owner != nil)"}});
}

} // namespace rohit::serializer::writer::reference_support
