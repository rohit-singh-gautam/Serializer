// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <rohit/serializer_creator.hpp>
#include "reference_validation_writer.hpp"
#include "value_reference_portable_writer.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rohit::serializer::writer::reference_support {

// Replace complete generator placeholders without interpreting generated source text.
inline std::string reference_expand(std::string text,
    const std::vector<std::pair<std::string, std::string>>& replacements) {
  for (const auto& [key, value] : replacements) {
    std::size_t offset{};
    while ((offset = text.find(key, offset)) != std::string::npos) {
      text.replace(offset, key.size(), value);
      offset += value.size();
    }
  }
  return text;
}

// Preserve native syntax while deriving durable identities and public owner accessors.
template <typename Type, typename Field>
inline std::string portable_methods(const class_node& value, const std::string& name,
    std::string_view language, Type type, Field field) {
  if (!value.reference) { return {}; }
  if (value.reference->value_projection && (language == "js" || language == "typescript" ||
      language == "java" || language == "csharp" || language == "python" || language == "go")) {
    return value_reference_portable_methods(value, name, language, type, field);
  }
  const auto& reference = *value.reference;
  const bool shared = reference.mode == reference_declaration::mode_type::shared;
  std::string path{language == "swift" ? "holder.value" : "owner"};
  std::vector<std::string> intermediate_paths;
  for (std::size_t index{}; index < reference.source_path.size(); ++index) {
    for (const auto base : reference.source_base_paths.at(index)) {
      path += "." + field(std::string{language == "go" ? "Base" : "base"} + std::to_string(base), access_type::public_access);
      intermediate_paths.push_back(path);
    }
    path += "." + field(reference.source_path[index], access_type::public_access);
    if (index + 1 < reference.source_path.size()) { intermediate_paths.push_back(path); }
  }
  std::string null_condition;
  for (const auto& intermediate : intermediate_paths) {
    if (!null_condition.empty()) { null_condition += language == "python" ? " or " : " || "; }
    null_condition += intermediate + (language == "python" ? " is None" : language == "go" ? " == nil" : " == null");
  }
  std::string null_guard;
  if (!null_condition.empty()) {
    if (language == "js") { null_guard = "if (" + null_condition + ") { throw new Error('Missing reference path owner'); }"; }
    if (language == "java") { null_guard = "if (" + null_condition + ") { throw new IllegalStateException(\"Missing reference path owner\"); }"; }
    if (language == "csharp") { null_guard = "if (" + null_condition + ") { throw new System.InvalidOperationException(\"Missing reference path owner\"); }"; }
    if (language == "python") { null_guard = "if " + null_condition + ":\n            raise RuntimeError('Missing reference path owner')"; }
    if (language == "go") { null_guard = "if " + null_condition + " { return empty, fmt.Errorf(\"Missing reference path owner\") }"; }
  }
  std::string ids;
  for (const auto id : reference.field_path) {
    if (!ids.empty()) { ids += ", "; }
    if (language == "java") { ids += "(int)" + std::to_string(id) + "L"; }
    else { ids += std::to_string(id); }
    if (language == "kotlin") { ids += "u"; }
  }
  std::string owner_type = type(reference.owner);
  std::string value_type = type(reference.value_type);
  std::string boxed_type = value_type;
  if (language == "java") {
    boxed_type = value_type == "boolean" ? "java.lang.Boolean" :
        value_type == "byte" ? "java.lang.Byte" : value_type == "short" ? "java.lang.Short" :
        value_type == "int" ? "java.lang.Integer" : value_type == "long" ? "java.lang.Long" :
        value_type == "float" ? "java.lang.Float" : value_type == "double" ? "java.lang.Double" :
        value_type == "char" ? "java.lang.Character" : value_type;
  }
  const std::vector<std::pair<std::string, std::string>> replacements{
      {"@name@", name}, {"@owner@", owner_type}, {"@value@", value_type},
      {"@boxed@", boxed_type}, {"@path@", path}, {"@ids@", ids},
      {"@null_guard@", null_guard},
      {"@doc@", field("document_id", access_type::public_access)},
      {"@id@", field("owner_id", access_type::public_access)},
      {"@gen@", field("generation", access_type::public_access)},
      {"@fields@", field("field_path", access_type::public_access)},
      {"@shared@", shared ? "true" : "false"}};
  std::string source;
  if (language == "js") {
    source = R"SRL(
  #srlReference = null;
  // Detach the live binding while retaining the durable address.
  unbind() { this.#srlReference = null; }
  // Bind one owner and assign this reference's durable address.
  bind(owner, documentId, ownerId, generation) {
    if (!(owner instanceof @owner@) || typeof documentId !== 'string' || !documentId || typeof ownerId !== 'bigint' || ownerId <= 0n || ownerId >= 1n << 64n || typeof generation !== 'bigint' || generation <= 0n || generation >= 1n << 64n) { throw new TypeError('Invalid reference binding'); }
    const path = [@ids@];
    const holder = @shared@ ? owner : new WeakRef(owner);
    this.@doc@ = documentId; this.@id@ = ownerId; this.@gen@ = generation; this.@fields@ = path;
    this.#srlReference = {holder, documentId, ownerId, generation};
  }
  // Attach a decoded address only when its identity matches the supplied owner.
  resolve(owner, documentId, ownerId, generation) {
    if (this.@doc@ !== documentId || this.@id@ !== ownerId || this.@gen@ !== generation || !this.#srlPathMatches()) { return false; }
    this.bind(owner, documentId, ownerId, generation); return true;
  }
  // Verify the declared field path without trusting mutable address metadata.
  #srlPathMatches() { const expected = [@ids@]; return Array.isArray(this.@fields@) && this.@fields@.length === expected.length && expected.every((id, index) => this.@fields@[index] === id); }
  // Resolve the current field slot; expired or edited bindings fail before access.
  readValue() {
    const binding = this.#srlReference;
    if (!binding || this.@doc@ !== binding.documentId || this.@id@ !== binding.ownerId || this.@gen@ !== binding.generation || !this.#srlPathMatches()) { throw new Error('Unbound or stale reference'); }
    const owner = @shared@ ? binding.holder : binding.holder.deref();
    if (owner == null) { throw new Error('Expired reference owner'); }
    @null_guard@
    return @path@;
  }
  // Run one synchronous read callback; callers must not retain borrowed values.
  withValue(callback) { return callback(this.readValue()); }
  // Report whether the bound owner and durable address are still valid.
  isBound() { try { this.readValue(); return true; } catch (_) { return false; } }
)SRL";
  } else if (language == "typescript") {
    source = R"SRL(
  /** Detach the live binding while preserving its durable address. */
  unbind(): void;
  /** Bind the owner and assign its durable identity and declared field path. */
  bind(owner: @owner@, documentId: string, ownerId: bigint, generation: bigint): void;
  /** Rebind a decoded address only when the supplied identity matches. */
  resolve(owner: @owner@, documentId: string, ownerId: bigint, generation: bigint): boolean;
  /** Read the current field; throws for an unbound, expired, or changed address. */
  readValue(): @value@;
  /** Run a synchronous callback without copying the selected field. */
  withValue<Result>(callback: (value: @value@) => Result): Result;
  /** Test the current binding and address. */
  isBound(): boolean;
)SRL";
  } else if (language == "java") {
    source = R"SRL(
  private java.util.function.Supplier<@owner@> srlReferenceOwner;
  private String srlReferenceDocument;
  private long srlReferenceId, srlReferenceGeneration;
  /** Detach the owner without changing the serialized address. */
  public void unbind() { srlReferenceOwner = null; }
  /** Bind one owner and atomically publish its declared durable address. */
  public void bind(@owner@ owner, String documentId, long ownerId, long generation) {
    java.util.Objects.requireNonNull(owner); java.util.Objects.requireNonNull(documentId);
    if (documentId.isEmpty() || ownerId == 0 || generation == 0) { throw new IllegalArgumentException("Invalid reference identity"); }
    var path = new int[] {@ids@};
    java.util.function.Supplier<@owner@> holder;
    if (@shared@) { holder = () -> owner; }
    else { var weak = new java.lang.ref.WeakReference<@owner@>(owner); holder = weak::get; }
    this.@doc@ = documentId; this.@id@ = ownerId; this.@gen@ = generation; this.@fields@ = path;
    srlReferenceDocument = documentId; srlReferenceId = ownerId; srlReferenceGeneration = generation; srlReferenceOwner = holder;
  }
  /** Resolve a decoded address without altering it on identity mismatch. */
  public boolean resolve(@owner@ owner, String documentId, long ownerId, long generation) {
    if (!java.util.Objects.equals(this.@doc@, documentId) || this.@id@ != ownerId || this.@gen@ != generation || !java.util.Arrays.equals(this.@fields@, new int[] {@ids@})) { return false; }
    bind(owner, documentId, ownerId, generation); return true;
  }
  /** Read the current owner field; stale addresses and expired owners throw. */
  public @value@ readValue() {
    if (srlReferenceOwner == null || !java.util.Objects.equals(this.@doc@, srlReferenceDocument) || this.@id@ != srlReferenceId || this.@gen@ != srlReferenceGeneration || !java.util.Arrays.equals(this.@fields@, new int[] {@ids@})) { throw new IllegalStateException("Unbound or stale reference"); }
    var owner = srlReferenceOwner.get();
    if (owner == null) { throw new IllegalStateException("Expired reference owner"); }
    @null_guard@
    return @path@;
  }
  /** Run one synchronous callback against the selected field. */
  public <Result> Result withValue(java.util.function.Function<@boxed@, Result> callback) { return callback.apply(readValue()); }
  /** Test the owner and durable address without exposing the selected field. */
  public boolean isBound() { try { readValue(); return true; } catch (IllegalStateException error) { return false; } }
)SRL";
  } else if (language == "csharp") {
    source = R"SRL(
  private System.Func<@owner@?>? srlReferenceOwner;
  private string? srlReferenceDocument;
  private ulong srlReferenceId, srlReferenceGeneration;
  // Detach the owner while retaining its durable address.
  public void Unbind() { srlReferenceOwner = null; }
  // Bind the owner and assign the canonical durable address.
  public void Bind(@owner@ owner, string documentId, ulong ownerId, ulong generation) {
    System.ArgumentNullException.ThrowIfNull(owner); System.ArgumentNullException.ThrowIfNull(documentId);
    if (documentId.Length == 0 || ownerId == 0 || generation == 0) { throw new System.ArgumentException("Invalid reference identity"); }
    var path = new System.Collections.Generic.List<uint> { @ids@ };
    @csharp_holder@
    this.@doc@ = documentId; this.@id@ = ownerId; this.@gen@ = generation; this.@fields@ = path;
    srlReferenceDocument = documentId; srlReferenceId = ownerId; srlReferenceGeneration = generation; srlReferenceOwner = holder;
  }
  // Match a decoded address before binding it to one supplied owner.
  public bool Resolve(@owner@ owner, string documentId, ulong ownerId, ulong generation) {
    if (this.@doc@ != documentId || this.@id@ != ownerId || this.@gen@ != generation || !SrlReferencePathMatches()) { return false; }
    Bind(owner, documentId, ownerId, generation); return true;
  }
  // Check every wire field identity against the schema declaration.
  private bool SrlReferencePathMatches() { uint[] expected = { @ids@ }; if (this.@fields@ == null || this.@fields@.Count != expected.Length) { return false; } for (int index = 0; index < expected.Length; ++index) { if (this.@fields@[index] != expected[index]) { return false; } } return true; }
  // Read the current field slot; expiry and address changes fail before access.
  public @value@ ReadValue() {
    if (srlReferenceOwner == null || this.@doc@ != srlReferenceDocument || this.@id@ != srlReferenceId || this.@gen@ != srlReferenceGeneration || !SrlReferencePathMatches()) { throw new System.InvalidOperationException("Unbound or stale reference"); }
    var owner = srlReferenceOwner();
    if (owner == null) { throw new System.InvalidOperationException("Expired reference owner"); }
    @null_guard@
    return @path@;
  }
  // Invoke one synchronous callback against the current field.
  public Result WithValue<Result>(System.Func<@value@, Result> callback) { return callback(ReadValue()); }
  // Check the live binding without propagating expiration failures.
  public bool IsBound() { try { ReadValue(); return true; } catch (System.InvalidOperationException) { return false; } }
)SRL";
    source = reference_expand(std::move(source), {{"@csharp_holder@", shared ?
        "System.Func<@owner@?> holder = () => owner;" :
        "var weak = new System.WeakReference<@owner@>(owner); System.Func<@owner@?> holder = () => weak.TryGetTarget(out var target) ? target : null;"}});
  } else if (language == "python") {
    source = R"SRL(
    def unbind(self):
        """Detach the owner while preserving the durable address."""
        self._srl_reference = None
    def bind(self, owner, document_id, owner_id, generation):
        """Assign the canonical address and bind a supplied owner."""
        if not isinstance(owner, @owner@) or not isinstance(document_id, str) or not document_id or not isinstance(owner_id, int) or not isinstance(generation, int) or not 0 < owner_id < 1 << 64 or not 0 < generation < 1 << 64:
            raise ValueError('Invalid reference binding')
        import weakref
        holder = (lambda: owner) if @shared_python@ else weakref.ref(owner)
        path = [@ids@]
        self.@doc@, self.@id@, self.@gen@, self.@fields@ = document_id, owner_id, generation, path
        self._srl_reference = (holder, document_id, owner_id, generation)
    def resolve(self, owner, document_id, owner_id, generation):
        """Attach a decoded address only when all supplied identity fields match."""
        if (self.@doc@, self.@id@, self.@gen@, self.@fields@) != (document_id, owner_id, generation, [@ids@]):
            return False
        self.bind(owner, document_id, owner_id, generation)
        return True
    def read_value(self):
        """Read the live field slot; unbound, edited, or expired addresses fail."""
        binding = self._srl_reference
        if binding is None or (self.@doc@, self.@id@, self.@gen@, self.@fields@) != (binding[1], binding[2], binding[3], [@ids@]):
            raise RuntimeError('Unbound or stale reference')
        owner = binding[0]()
        if owner is None:
            raise RuntimeError('Expired reference owner')
        @null_guard@
        return @path@
    def with_value(self, callback):
        """Run one synchronous callback; callers must not retain borrowed values."""
        return callback(self.read_value())
    def is_bound(self):
        """Test the live binding without propagating expiration failures."""
        try:
            self.read_value()
            return True
        except RuntimeError:
            return False
)SRL";
    source = reference_expand(std::move(source), {{"@shared_python@", shared ? "True" : "False"}});
  } else if (language == "kotlin") {
    source = R"SRL(
  private var srlReferenceOwner: (() -> @owner@?)? = null
  private var srlReferenceDocument = ""
  private var srlReferenceId = 0uL
  private var srlReferenceGeneration = 0uL
  // Detach the owner while retaining its durable address.
  fun unbind() { srlReferenceOwner = null }
  // Bind an owner and publish the canonical durable field address.
  fun bind(owner: @owner@, documentId: String, ownerId: ULong, generation: ULong) {
    require(documentId.isNotEmpty() && ownerId != 0uL && generation != 0uL) { "Invalid reference identity" }
    val path = uintArrayOf(@ids@)
    val holder: () -> @owner@? = if (@shared@) { { owner } } else { val weak = java.lang.ref.WeakReference(owner); { weak.get() } }
    this.@doc@ = documentId; this.@id@ = ownerId; this.@gen@ = generation; this.@fields@ = path
    srlReferenceDocument = documentId; srlReferenceId = ownerId; srlReferenceGeneration = generation; srlReferenceOwner = holder
  }
  // Resolve a decoded address only when its supplied owner identity matches.
  fun resolve(owner: @owner@, documentId: String, ownerId: ULong, generation: ULong): Boolean {
    if (this.@doc@ != documentId || this.@id@ != ownerId || this.@gen@ != generation || !this.@fields@.contentEquals(uintArrayOf(@ids@))) { return false }
    bind(owner, documentId, ownerId, generation); return true
  }
  // Read the live field; owner expiry and edited address metadata fail.
  fun readValue(): @value@ {
    if (this.@doc@ != srlReferenceDocument || this.@id@ != srlReferenceId || this.@gen@ != srlReferenceGeneration || !this.@fields@.contentEquals(uintArrayOf(@ids@))) { error("Unbound or stale reference") }
    val owner = srlReferenceOwner?.invoke() ?: error("Expired or unbound reference owner")
    return @path@
  }
  // Run one synchronous callback against the current field.
  fun <Result> withValue(callback: (@value@) -> Result): Result = callback(readValue())
  // Check the live owner and unchanged durable address.
  fun isBound(): Boolean = try { readValue(); true } catch (error: IllegalStateException) { false }
)SRL";
  } else if (language == "go") {
    source = R"SRL(
// @name@Scope tracks borrowed validity explicitly; call Close before discarding its owner.
// Scope and reference access require external synchronization.
type @name@Scope struct { owner @owner@ }
// New@name@Scope starts a checked borrow scope for one owner.
func New@name@Scope(owner @owner@) *@name@Scope { return &@name@Scope{owner: owner} }
// Close invalidates every borrowed reference and releases the scope's owner pointer.
func (scope *@name@Scope) Close() { if scope != nil { scope.owner = nil } }
// Unbind detaches live state while preserving its durable address.
func (value *@name@) Unbind() { value.srlReferenceOwner = nil; value.srlReferenceScope = nil }
// Bind assigns the canonical address; borrowed bindings require an open scope.
func (value *@name@) Bind(@go_argument@, documentId string, ownerId uint64, generation uint64) error {
  @go_owner@
  if owner == nil || documentId == "" || ownerId == 0 || generation == 0 { return fmt.Errorf("Invalid reference identity") }
  path := []uint32{@ids@}
  value.@doc@ = documentId; value.@id@ = ownerId; value.@gen@ = generation; value.@fields@ = path
  value.srlReferenceDocument = documentId; value.srlReferenceId = ownerId; value.srlReferenceGeneration = generation
  @go_publish@
  return nil
}
// Resolve preserves an existing decoded address on an identity mismatch.
func (value *@name@) Resolve(@go_argument@, documentId string, ownerId uint64, generation uint64) bool {
  if value.@doc@ != documentId || value.@id@ != ownerId || value.@gen@ != generation || !value.srlReferencePathMatches() { return false }
  return value.Bind(@go_forward@, documentId, ownerId, generation) == nil
}
// srlReferencePathMatches checks all schema-defined field identities.
func (value *@name@) srlReferencePathMatches() bool { expected := []uint32{@ids@}; if len(value.@fields@) != len(expected) { return false }; for index, id := range expected { if value.@fields@[index] != id { return false } }; return true }
// ReadValue reads the current field slot without caching a stale child address.
func (value *@name@) ReadValue() (@value@, error) {
  var empty @value@
  if value == nil || value.@doc@ != value.srlReferenceDocument || value.@id@ != value.srlReferenceId || value.@gen@ != value.srlReferenceGeneration || !value.srlReferencePathMatches() { return empty, fmt.Errorf("Unbound or stale reference") }
  owner := value.srlReferenceOwner
  if !@shared@ { if value.srlReferenceScope == nil { return empty, fmt.Errorf("Unbound reference") }; owner = value.srlReferenceScope.owner }
  if owner == nil { return empty, fmt.Errorf("Expired reference owner") }
  @null_guard@
  return @path@, nil
}
// WithValue invokes a synchronous callback while the selected owner is available.
func (value *@name@) WithValue(callback func(@value@)) error { selected, err := value.ReadValue(); if err != nil { return err }; callback(selected); return nil }
// IsBound tests the owner and unchanged address without propagating expiry errors.
func (value *@name@) IsBound() bool { _, err := value.ReadValue(); return err == nil }
)SRL";
    source = reference_expand(std::move(source), {
        {"@go_argument@", shared ? "owner " + owner_type : "scope *" + name + "Scope"},
        {"@go_owner@", shared ? "" : "var owner " + owner_type + "; if scope != nil { owner = scope.owner }"},
        {"@go_publish@", shared ? "value.srlReferenceOwner = owner; value.srlReferenceScope = nil" :
            "value.srlReferenceOwner = nil; value.srlReferenceScope = scope"},
        {"@go_forward@", shared ? "owner" : "scope"}});
  } else if (language == "rust") {
    source = R"SRL(
    /// Detach the owner without changing the durable address.
    pub fn unbind(&mut self) { self.srl_reference.0 = None; }
    /// Bind an owner and assign its canonical durable address.
    pub fn bind(&mut self, owner: &std::rc::Rc<std::cell::RefCell<@owner@>>, document_id: &str, owner_id: u64, generation: u64) -> SrlResult<()> {
        if document_id.is_empty() || owner_id == 0 || generation == 0 { return Err(Error("Invalid reference identity")); }
        let address = (document_id.to_owned(), owner_id, generation, vec![@ids@]);
        let binding = (address.clone(), std::rc::Rc::downgrade(owner), if @shared@ { Some(std::rc::Rc::clone(owner)) } else { None });
        self.@doc@ = address.0; self.@id@ = address.1; self.@gen@ = address.2; self.@fields@ = address.3;
        self.srl_reference.0 = Some(binding);
        Ok(())
    }
    /// Resolve a decoded address without changing it on an identity mismatch.
    pub fn resolve(&mut self, owner: &std::rc::Rc<std::cell::RefCell<@owner@>>, document_id: &str, owner_id: u64, generation: u64) -> bool {
        if self.@doc@ != document_id || self.@id@ != owner_id || self.@gen@ != generation || self.@fields@ != vec![@ids@] { return false; }
        self.bind(owner, document_id, owner_id, generation).is_ok()
    }
    /// Borrow the current slot only during the synchronous callback; conflicting edits fail.
    pub fn with_value<Result>(&self, callback: impl FnOnce(&@value@) -> Result) -> SrlResult<Result> {
        let binding = self.srl_reference.0.as_ref().ok_or(Error("Unbound reference"))?;
        if self.@doc@ != binding.0.0 || self.@id@ != binding.0.1 || self.@gen@ != binding.0.2 || self.@fields@ != binding.0.3 || self.@fields@ != vec![@ids@] { return Err(Error("Stale reference address")); }
        let owner_handle = binding.1.upgrade().ok_or(Error("Expired reference owner"))?;
        let owner = owner_handle.try_borrow().map_err(|_| Error("Reference owner is mutably borrowed"))?;
        Ok(callback(&@path@))
    }
    /// Copy the current value without allowing a borrowed reference to escape.
    pub fn read_value(&self) -> SrlResult<@value@> { self.with_value(|value| value.clone()) }
    /// Check owner availability and unchanged address metadata.
    pub fn is_bound(&self) -> bool { self.with_value(|_| ()).is_ok() }
)SRL";
  } else if (language == "swift") {
    source = R"SRL(
  private var srlReference: ((String, UInt64, UInt64, [UInt32]), () -> SrlReferenceOwner<@owner@>?)? = nil
  /// Detach the owner while retaining the durable address.
  public mutating func unbind() { srlReference = nil }
  /// Bind stable boxed owner storage and publish the durable address.
  public mutating func bind(_ owner: SrlReferenceOwner<@owner@>, _ documentId: String, _ ownerId: UInt64, _ generation: UInt64) throws {
    guard !documentId.isEmpty, ownerId != 0, generation != 0 else { throw SerializerError.invalid("Invalid reference identity") }
    let path: [UInt32] = [@ids@]
    let holder: () -> SrlReferenceOwner<@owner@>?
    if @shared@ { holder = { owner } } else { holder = { [weak owner] in owner } }
    self.@doc@ = documentId; self.@id@ = ownerId; self.@gen@ = generation; self.@fields@ = path
    srlReference = ((documentId, ownerId, generation, path), holder)
  }
  /// Attach a decoded address only when its owner identity and path match.
  public mutating func resolve(_ owner: SrlReferenceOwner<@owner@>, _ documentId: String, _ ownerId: UInt64, _ generation: UInt64) -> Bool {
    guard self.@doc@ == documentId, self.@id@ == ownerId, self.@gen@ == generation, self.@fields@ == [@ids@] else { return false }
    do { try bind(owner, documentId, ownerId, generation); return true } catch { return false }
  }
  /// Resolve the current field, failing before access when the owner or address expired.
  public func readValue() throws -> @value@ {
    guard let binding = srlReference, self.@doc@ == binding.0.0, self.@id@ == binding.0.1, self.@gen@ == binding.0.2, self.@fields@ == binding.0.3, self.@fields@ == [@ids@], let holder = binding.1() else { throw SerializerError.invalid("Unbound or stale reference") }
    return @path@
  }
  /// Run a synchronous callback using the current selected value.
  public func withValue<Result>(_ callback: (@value@) throws -> Result) throws -> Result { return try callback(readValue()) }
  /// Test owner availability and the unchanged durable address.
  public func isBound() -> Bool { return (try? readValue()) != nil }
)SRL";
  }
  source += portable_validation(language);
  return reference_expand(std::move(source), replacements);
}

// Emit Go's live fields inside its model struct, outside the wire member traversal.
template <typename Type>
inline std::string go_binding_fields(const class_node& value, const std::string& name, Type type) {
  if (!value.reference) { return {}; }
  if (value.reference->value_projection) {
    return value.reference->mode == reference_declaration::mode_type::shared ?
        "  srlReferenceOwner " + type(value.reference->owner) + "\n" :
        "  srlReferenceScope *" + name + "Scope\n";
  }
  return "  srlReferenceOwner " + type(value.reference->owner) + "\n  srlReferenceScope *" + name +
      "Scope\n  srlReferenceDocument string\n  srlReferenceId uint64\n  srlReferenceGeneration uint64\n";
}

// Keep C's live control block and checked address snapshot outside encoded fields.
inline std::string c_binding_fields(const class_node& value) {
  if (!value.reference) { return {}; }
  return "  srl_reference_scope* srl_reference;\n  srl_string srl_reference_document;\n"
      "  uint64_t srl_reference_id, srl_reference_generation;\n";
}

// Emit C's explicit reference lifecycle, retaining the owner only for shared bindings.
template <typename Type, typename Field>
inline std::string c_methods(const class_node& value, const std::string& name,
    Type type, Field field) {
  if (!value.reference) { return {}; }
  const auto& reference = *value.reference;
  const bool shared = reference.mode == reference_declaration::mode_type::shared;
  // C typedefs share the local identifier namespace; the reserved name avoids owner-type shadowing.
  std::string path{"srl_owner"}, ids;
  for (std::size_t index{}; index < reference.source_path.size(); ++index) {
    for (const auto base : reference.source_base_paths.at(index)) {
      path += (path == "srl_owner" ? "->" : ".") + std::string{"base"} + std::to_string(base);
    }
    path += (path == "srl_owner" ? "->" : ".") + field(reference.source_path[index]);
  }
  for (const auto id : reference.field_path) {
    if (!ids.empty()) { ids += ", "; }
    ids += std::to_string(id);
  }
  return reference_expand(R"SRL(
/* Create a typed owner scope; release adopts the supplied stable owner allocation. */
static inline srl_reference_scope* @name@_create_scope(@owner@* owner, void (*release)(void*)) {
  srl_reference_scope* scope = srl_reference_scope_create(owner, release);
  if (scope) { scope->owner_type = "@owner_key@"; } return scope;
}
/* Detach the live control block while preserving the durable address. */
static inline void @name@_unbind(@name@* value) {
  if (!value) { return; }
  srl_reference_scope* previous = value->srl_reference;
  value->srl_reference = NULL; srl_string_free(&value->srl_reference_document);
  if (previous) { @release@(previous); }
}
/* Check the complete schema-defined path without trusting mutable metadata. */
static inline bool @name@_srl_reference_path(const @name@* value) {
  static const uint32_t expected[] = {@ids@};
  return value && value->@fields@.size == sizeof(expected)/sizeof(expected[0]) && value->@fields@.capacity >= value->@fields@.size &&
      value->@fields@.data && !memcmp(value->@fields@.data, expected, sizeof(expected));
}
/* Accept null or a complete typed field address independently of live bindings. */
static inline srl_status @name@_validate_address(const @name@* value) {
  if (!value) { return srl_invalid; }
  if (!value->@doc@.size && !value->@id@ && !value->@gen@ && !value->@fields@.size) { return srl_ok; }
  if (!value->@doc@.size || !value->@doc@.data || !value->@id@ || !value->@gen@ || !@name@_srl_reference_path(value)) { return srl_invalid; }
  return srl_ok;
}
/* Atomically bind an existing owner scope and assign its canonical durable address. */
static inline srl_status @name@_bind_bytes(@name@* value, srl_reference_scope* scope,
    const char* document_id, size_t document_size, uint64_t owner_id, uint64_t generation) {
  if (!value || !scope || !scope->owner || !scope->owners || !scope->owner_type || strcmp(scope->owner_type, "@owner_key@") || !document_id || !document_size || !owner_id || !generation) { return srl_invalid; }
  srl_string document = {0}, snapshot = {0};
  static const uint32_t expected[] = {@ids@};
  uint32_t* path = (uint32_t*)malloc(sizeof(expected));
  if (!path) { return srl_allocation; }
  srl_status status = srl_string_set(&document, document_id, document_size);
  if (status == srl_ok) { status = srl_string_set(&snapshot, document_id, document_size); }
  if (status == srl_ok) { status = @retain@(scope); }
  if (status != srl_ok) { free(path); srl_string_free(&document); srl_string_free(&snapshot); return status; }
  memcpy(path, expected, sizeof(expected));
  srl_reference_scope* previous = value->srl_reference;
  srl_string previous_snapshot = value->srl_reference_document;
  value->srl_reference = NULL; memset(&value->srl_reference_document, 0, sizeof(value->srl_reference_document));
  srl_string_free(&previous_snapshot); srl_string_free(&value->@doc@); free(value->@fields@.data);
  value->@doc@ = document; value->@id@ = owner_id; value->@gen@ = generation;
  value->@fields@.data = path; value->@fields@.size = sizeof(expected)/sizeof(expected[0]); value->@fields@.capacity = value->@fields@.size;
  value->srl_reference = scope; value->srl_reference_document = snapshot;
  value->srl_reference_id = owner_id; value->srl_reference_generation = generation;
  if (previous) { @release@(previous); }
  return srl_ok;
}
/* Bind a conventional NUL-terminated document identity; use bind_bytes for embedded NULs. */
static inline srl_status @name@_bind(@name@* value, srl_reference_scope* scope,
    const char* document_id, uint64_t owner_id, uint64_t generation) {
  return document_id ? @name@_bind_bytes(value, scope, document_id, strlen(document_id), owner_id, generation) : srl_invalid;
}
/* Resolve a decoded address without changing either metadata or binding on failure. */
static inline bool @name@_resolve_bytes(@name@* value, srl_reference_scope* scope,
    const char* document_id, size_t document_size, uint64_t owner_id, uint64_t generation) {
  if (!value || !document_id || value->@doc@.size != document_size || (document_size && (!value->@doc@.data || memcmp(value->@doc@.data, document_id, document_size))) || value->@id@ != owner_id || value->@gen@ != generation || !@name@_srl_reference_path(value)) { return false; }
  return @name@_bind_bytes(value, scope, document_id, document_size, owner_id, generation) == srl_ok;
}
/* Resolve a conventional NUL-terminated document identity. */
static inline bool @name@_resolve(@name@* value, srl_reference_scope* scope,
    const char* document_id, uint64_t owner_id, uint64_t generation) {
  return document_id && @name@_resolve_bytes(value, scope, document_id, strlen(document_id), owner_id, generation);
}
/* Borrow the current field until the next owner edit or scope release; never free the result. */
static inline srl_status @name@_read_value(const @name@* srl_value, const @value@** srl_result) {
  if (!srl_value || !srl_result || !srl_value->srl_reference || !srl_value->srl_reference->owners || !srl_value->srl_reference->owner || !srl_value->srl_reference->owner_type || strcmp(srl_value->srl_reference->owner_type, "@owner_key@") || srl_value->@id@ != srl_value->srl_reference_id || srl_value->@gen@ != srl_value->srl_reference_generation || srl_value->@doc@.size != srl_value->srl_reference_document.size || (srl_value->@doc@.size && (!srl_value->@doc@.data || memcmp(srl_value->@doc@.data, srl_value->srl_reference_document.data, srl_value->@doc@.size))) || !@name@_srl_reference_path(srl_value)) { return srl_invalid; }
  const @owner@* srl_owner = (const @owner@*)srl_value->srl_reference->owner;
  *srl_result = &@path@; return srl_ok;
}
/* Invoke a synchronous callback while a temporary strong owner lease is held. */
static inline srl_status @name@_with_value(const @name@* value,
    void (*callback)(const @value@*, void*), void* context) {
  const @value@* selected = NULL;
  if (!callback || @name@_read_value(value, &selected) != srl_ok) { return srl_invalid; }
  srl_reference_scope* scope = value->srl_reference;
  srl_status status = srl_reference_scope_retain(scope);
  if (status != srl_ok) { return status; }
  callback(selected, context); srl_reference_scope_release(scope); return srl_ok;
}
/* Report whether this address still resolves through a live owner scope. */
static inline bool @name@_is_bound(const @name@* value) { const @value@* selected = NULL; return @name@_read_value(value, &selected) == srl_ok; }
)SRL", {{"@name@", name}, {"@value@", type(reference.value_type)},
      {"@owner@", type(reference.owner)}, {"@path@", path}, {"@ids@", ids},
      {"@owner_key@", reference.owner.get_full_name()},
      {"@doc@", field("document_id")}, {"@id@", field("owner_id")},
      {"@gen@", field("generation")}, {"@fields@", field("field_path")},
      {"@retain@", shared ? "srl_reference_scope_retain" : "srl_reference_scope_observe"},
      {"@release@", shared ? "srl_reference_scope_release" : "srl_reference_scope_unobserve"}});
}

// Emit a Rust binding whose transport-independent state is ignored by value equality.
inline std::string portable_runtime(std::string_view language) {
  if (language == "c") {
    return R"SRL(
/* Explicit owner lease; callers synchronize access and never shallow-copy this control block. */
typedef struct srl_reference_scope { void* owner; size_t owners, observers; void (*release)(void*); bool releasing; const char* owner_type; } srl_reference_scope;
/* Adopt one stable owner pointer; release runs exactly once after the final strong lease. */
static inline srl_reference_scope* srl_reference_scope_create(void* owner, void (*release)(void*)) {
  if (!owner) { return NULL; }
  srl_reference_scope* scope = (srl_reference_scope*)calloc(1, sizeof(*scope));
  if (scope) { scope->owner = owner; scope->owners = 1; scope->release = release; } return scope;
}
/* Retain a live owner, detecting count overflow before mutation. */
static inline srl_status srl_reference_scope_retain(srl_reference_scope* scope) {
  if (!scope || !scope->owners || scope->owners == SIZE_MAX) { return srl_invalid; } ++scope->owners; return srl_ok;
}
/* Drop one owner lease, invalidate borrowed handles, and eventually release the control block. */
static inline void srl_reference_scope_release(srl_reference_scope* scope) {
  if (!scope || !scope->owners) { return; }
  if (--scope->owners == 0) { void* owner = scope->owner; scope->owner = NULL; scope->releasing = true; if (scope->release) { scope->release(owner); } scope->releasing = false; }
  if (!scope->owners && !scope->observers) { free(scope); }
}
/* Keep only the expiry control block alive without retaining its owner. */
static inline srl_status srl_reference_scope_observe(srl_reference_scope* scope) {
  if (!scope || !scope->owners || scope->observers == SIZE_MAX) { return srl_invalid; } ++scope->observers; return srl_ok;
}
/* Release one non-owning observation without touching a live owner. */
static inline void srl_reference_scope_unobserve(srl_reference_scope* scope) {
  if (!scope || !scope->observers) { return; } --scope->observers; if (!scope->owners && !scope->observers && !scope->releasing) { free(scope); }
}
)SRL";
  }
  if (language == "rust") {
    return R"SRL(
/// Live reference state is excluded from durable value equality and debug output.
#[derive(Clone)]
struct SrlReferenceBinding<T>(Option<T>);
impl<T> std::fmt::Debug for SrlReferenceBinding<T> {
    /// Print transport metadata without exposing a borrowed owner graph.
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result { formatter.write_str("ReferenceBinding") }
}
impl<T> PartialEq for SrlReferenceBinding<T> {
    /// Compare references by their ordinary durable address fields.
    fn eq(&self, _other: &Self) -> bool { true }
}
)SRL";
  }
  if (language == "swift") {
    return R"SRL(
/// Stable identity for a generated value owner; externally synchronize mutation.
public final class SrlReferenceOwner<Value> {
    public var value: Value
    /// Move one owning model into stable reference storage.
    public init(_ value: Value) { self.value = value }
}
)SRL";
  }
  return {};
}

// Describe Rust's hidden owner lease separately from encoded schema fields.
template <typename Type>
inline std::string rust_binding_type(const class_node& value, Type type) {
  const auto owner = type(value.reference->owner);
  return "SrlReferenceBinding<((String, u64, u64, Vec<u32>), std::rc::Weak<std::cell::RefCell<" + owner +
      ">>, Option<std::rc::Rc<std::cell::RefCell<" + owner + ">>>) >";
}

} // namespace rohit::serializer::writer::reference_support
