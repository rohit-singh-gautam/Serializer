// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>

namespace rohit::serializer::writer::reference_support {

// Validate nullable typed addresses independently of their optional nonserialized binding.
inline std::string portable_validation(std::string_view language) {
  if (language == "js") {
    return R"SRL(
  // Reject partial addresses and paths belonging to a different schema field.
  srlValidateReferenceAddress() {
    if (this.@doc@ === '' && this.@id@ === 0n && this.@gen@ === 0n && Array.isArray(this.@fields@) && this.@fields@.length === 0) { return; }
    if (typeof this.@doc@ !== 'string' || this.@doc@.length === 0 || typeof this.@id@ !== 'bigint' || this.@id@ <= 0n || this.@id@ > 18446744073709551615n || typeof this.@gen@ !== 'bigint' || this.@gen@ <= 0n || this.@gen@ > 18446744073709551615n || !this.#srlPathMatches()) { throw new RangeError('Invalid typed reference address'); }
  }
)SRL";
  }
  if (language == "typescript") { return "  /** Validate a null or complete typed address. */\n  srlValidateReferenceAddress(): void;\n"; }
  if (language == "java") {
    return R"SRL(
  /** Reject partial addresses and paths for a different schema field. */
  public void srlValidateReferenceAddress() {
    if ("".equals(@doc@) && @id@ == 0 && @gen@ == 0 && @fields@ != null && @fields@.length == 0) { return; }
    if (@doc@ == null || @doc@.isEmpty() || @id@ == 0 || @gen@ == 0 || !java.util.Arrays.equals(@fields@, new int[] {@ids@})) { throw new IllegalArgumentException("Invalid typed reference address"); }
  }
)SRL";
  }
  if (language == "csharp") {
    return R"SRL(
  // Reject partial addresses and paths for a different schema field.
  public void SrlValidateReferenceAddress() {
    if (@doc@ == "" && @id@ == 0 && @gen@ == 0 && @fields@ != null && @fields@.Count == 0) { return; }
    if (string.IsNullOrEmpty(@doc@) || @id@ == 0 || @gen@ == 0 || !SrlReferencePathMatches()) { throw new System.ArgumentException("Invalid typed reference address"); }
  }
)SRL";
  }
  if (language == "python") {
    return R"SRL(
    def _srl_validate_reference_address(self):
        """Accept null or a complete address for the declared field path."""
        if (self.@doc@, self.@id@, self.@gen@, self.@fields@) == ('', 0, 0, []):
            return
        if not isinstance(self.@doc@, str) or not self.@doc@ or not isinstance(self.@id@, int) or not 0 < self.@id@ < 1 << 64 or not isinstance(self.@gen@, int) or not 0 < self.@gen@ < 1 << 64 or self.@fields@ != [@ids@]:
            raise ValueError('Invalid typed reference address')
)SRL";
  }
  if (language == "kotlin") {
    return R"SRL(
  // Accept null or a complete address for the declared schema path.
  fun srlValidateReferenceAddress() {
    if (@doc@.isEmpty() && @id@ == 0uL && @gen@ == 0uL && @fields@.isEmpty()) { return }
    require(@doc@.isNotEmpty() && @id@ != 0uL && @gen@ != 0uL && @fields@.contentEquals(uintArrayOf(@ids@))) { "Invalid typed reference address" }
  }
)SRL";
  }
  if (language == "swift") {
    return R"SRL(
  /// Accept null or a complete address for the declared schema path.
  public func srlValidateReferenceAddress() throws {
    if @doc@.isEmpty && @id@ == 0 && @gen@ == 0 && @fields@.isEmpty { return }
    guard !@doc@.isEmpty, @id@ != 0, @gen@ != 0, @fields@ == [@ids@] else { throw SerializerError.invalid("Invalid typed reference address") }
  }
)SRL";
  }
  if (language == "rust") {
    return R"SRL(
    /// Accept null or a complete address for the declared schema path.
    pub fn srl_validate_reference_address(&self) -> SrlResult<()> {
        if self.@doc@.is_empty() && self.@id@ == 0 && self.@gen@ == 0 && self.@fields@.is_empty() { return Ok(()); }
        if self.@doc@.is_empty() || self.@id@ == 0 || self.@gen@ == 0 || self.@fields@ != vec![@ids@] { return Err(Error("Invalid typed reference address")); }
        Ok(())
    }
)SRL";
  }
  if (language == "go") {
    return R"SRL(
// srlValidateReferenceAddress accepts null or the complete declared field address.
func (value *@name@) srlValidateReferenceAddress() error {
  if value == nil { return fmt.Errorf("Missing reference address") }
  if value.@doc@ == "" && value.@id@ == 0 && value.@gen@ == 0 && len(value.@fields@) == 0 { return nil }
  if value.@doc@ == "" || value.@id@ == 0 || value.@gen@ == 0 || !value.srlReferencePathMatches() { return fmt.Errorf("Invalid typed reference address") }
  return nil
}
)SRL";
  }
  return {};
}

} // namespace rohit::serializer::writer::reference_support
