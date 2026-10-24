// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <rohit/opc_signatures.hpp>
#include <rohit/xml.hpp>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <msopc.h>
#include <ncrypt.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <wrl/client.h>
#endif

namespace rohit::serializer::package::signatures {
namespace {
#if defined(_WIN32)
using Microsoft::WRL::ComPtr;
constexpr auto signature_method = L"http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";
constexpr auto digest_method = L"http://www.w3.org/2001/04/xmlenc#sha256";

// Translate provider failures without disclosing certificate/key or package payload contents.
void check(HRESULT result) {
  if (FAILED(result)) {
    const auto message = "Windows OPC signature provider rejected the operation (HRESULT " +
        std::to_string(static_cast<std::uint32_t>(result)) + ")";
    throw error{error_code::integrity, message.c_str()};
  }
}

// Balance initialization on this thread while accepting an existing caller-selected apartment.
struct apartment {
  HRESULT result{CoInitializeEx(nullptr, COINIT_MULTITHREADED)};
  // Admit the current COM apartment without taking ownership of a caller's initialization.
  apartment() { if (result != RPC_E_CHANGED_MODE) { check(result); } }
  // Release only the initialization acquired by this operation.
  ~apartment() { if (SUCCEEDED(result)) { CoUninitialize(); } }
  apartment(const apartment&) = delete;
  apartment& operator=(const apartment&) = delete;
};

// Release a provider-allocated wide string on every failure path.
struct task_string {
  wchar_t* value{};
  // Begin with no owned provider string.
  task_string() = default;
  task_string(const task_string&) = delete;
  task_string& operator=(const task_string&) = delete;
  // Free the COM task allocation, including a null result.
  ~task_string() { CoTaskMemFree(value); }
};

// Convert strict UTF-8 package identities without accepting lossy Windows replacements.
std::wstring wide(std::string_view value) {
  if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw error{error_code::resource_limit, "Signature identity exceeds platform limit"};
  }
  if (value.empty()) { return {}; }
  const auto length = static_cast<int>(value.size());
  const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), length, nullptr, 0);
  if (size == 0) { throw error{error_code::invalid_name, "Signature identity is not UTF-8"}; }
  std::wstring result(static_cast<std::size_t>(size), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), length, result.data(), size) != size) {
    throw error{error_code::invalid_name, "Signature identity conversion failed"};
  }
  return result;
}

// Obtain canonical URI spelling and immediately release the provider's BSTR.
std::string identity(IOpcUri* uri) {
  BSTR raw{};
  check(uri->GetRawUri(&raw));
  const std::unique_ptr<wchar_t, decltype(&SysFreeString)> retained{raw, SysFreeString};
  const auto length = static_cast<int>(SysStringLen(raw));
  const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, raw, length, nullptr, 0, nullptr, nullptr);
  if (size <= 0) { throw error{error_code::invalid_name, "Signature URI conversion failed"}; }
  std::string result(static_cast<std::size_t>(size), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, raw, length, result.data(), size, nullptr, nullptr) != size) {
    throw error{error_code::invalid_name, "Signature URI conversion failed"};
  }
  return result;
}

// Keep the borrowed input and provider objects together for the complete synchronous operation.
struct opened_package {
  apartment initialized{};
  ComPtr<IOpcFactory> factory{};
  ComPtr<IStream> input{};
  ComPtr<IOpcPackage> package{};
  ComPtr<IOpcDigitalSignatureManager> manager{};
  // Validate ZIP/OPC integrity and expansion bounds before passing bytes to the OS provider.
  opened_package(std::span<const std::uint8_t> bytes, const limits& configured) {
    if (bytes.empty()) { throw error{error_code::invalid_archive, "Signed package input is empty"}; }
    if (bytes.size() > configured.max_archive_bytes || bytes.size() > std::numeric_limits<UINT>::max()) {
      throw error{error_code::resource_limit, "Signed package exceeds input byte limit"};
    }
    std::istringstream source{std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()}, std::ios::binary};
    opc_reader checked{source, std::make_shared<budget>(configured)};
    checked.validate_all();
    for (const auto& part : checked.parts()) {
      if (part.content_type != "application/vnd.openxmlformats-package.digital-signature-xmlsignature+xml") { continue; }
      const auto signature_xml = checked.read(part.name);
      xml::limits bounds;
      bounds.max_input_bytes = static_cast<std::size_t>(std::min<std::uint64_t>(
          bounds.max_input_bytes, configured.max_metadata_bytes));
      static_cast<void>(xml::parse(std::string_view{
          reinterpret_cast<const char*>(signature_xml.data()), signature_xml.size()}, bounds));
    }
    check(CoCreateInstance(__uuidof(OpcFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
    input.Attach(SHCreateMemStream(bytes.data(), static_cast<UINT>(bytes.size())));
    if (!input) { throw error{error_code::resource_limit, "Cannot allocate OPC signature input stream"}; }
    check(factory->ReadPackageFromStream(input.Get(), OPC_VALIDATE_ON_LOAD, &package));
    check(factory->CreateDigitalSignatureManager(package.Get(), &manager));
  }
  // Serialize to private storage and publish an owning result only after completion and admission.
  std::vector<std::uint8_t> bytes(const limits& configured) const {
    ComPtr<IStream> output;
    check(CreateStreamOnHGlobal(nullptr, TRUE, &output));
    // This owning in-memory profile is UINT bounded; emit ZIP32 rather than OS-specific ZIP64 sentinels.
    check(factory->WritePackageToStream(package.Get(), OPC_WRITE_FORCE_ZIP32, output.Get()));
    STATSTG status{};
    check(output->Stat(&status, STATFLAG_NONAME));
    if (status.cbSize.QuadPart > configured.max_archive_bytes ||
        status.cbSize.QuadPart > std::numeric_limits<ULONG>::max()) {
      throw error{error_code::resource_limit, "Signed package exceeds output byte limit"};
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(status.cbSize.QuadPart));
    check(output->Seek({}, STREAM_SEEK_SET, nullptr));
    ULONG read_bytes{};
    check(output->Read(result.data(), static_cast<ULONG>(result.size()), &read_bytes));
    if (read_bytes != result.size()) { throw error{error_code::io, "Truncated OPC signature output"}; }
    return result;
  }
};

// Enumerate bounded signatures before performing a mutation that invalidates the enumerator.
std::vector<ComPtr<IOpcDigitalSignature>> signatures(opened_package& source, const limits& configured) {
  ComPtr<IOpcDigitalSignatureEnumerator> iterator;
  check(source.manager->GetSignatureEnumerator(&iterator));
  std::vector<ComPtr<IOpcDigitalSignature>> result;
  BOOL next{};
  for (check(iterator->MoveNext(&next)); next; check(iterator->MoveNext(&next))) {
    if (result.size() >= configured.max_entries) { throw error{error_code::resource_limit, "Signature count limit exceeded"}; }
    check(iterator->GetCurrent(&result.emplace_back()));
  }
  return result;
}

// Require exact supported algorithm URIs instead of silently falling back to legacy SHA-1.
void algorithm(const wchar_t* actual, const wchar_t* expected) {
  if (!actual || std::wstring_view{actual} != expected) {
    throw error{error_code::unsupported_feature, "Unsupported OPC signature or digest algorithm"};
  }
}

// Recover the signature's actual authenticated coverage and reject unsupported transforms/selectors.
coverage signature_coverage(IOpcDigitalSignature* signature, const limits& configured) {
  coverage result;
  ComPtr<IOpcSignaturePartReferenceEnumerator> parts;
  check(signature->GetSignaturePartReferenceEnumerator(&parts));
  BOOL next{};
  for (check(parts->MoveNext(&next)); next; check(parts->MoveNext(&next))) {
    if (result.parts.size() >= configured.max_references) { throw error{error_code::resource_limit, "Signature reference limit exceeded"}; }
    ComPtr<IOpcSignaturePartReference> reference;
    check(parts->GetCurrent(&reference));
    task_string digest;
    check(reference->GetDigestMethod(&digest.value));
    algorithm(digest.value, digest_method);
    OPC_CANONICALIZATION_METHOD transform{};
    check(reference->GetTransformMethod(&transform));
    if (transform != OPC_CANONICALIZATION_NONE) { throw error{error_code::unsupported_feature, "Only byte-exact signed part digests are supported"}; }
    ComPtr<IOpcPartUri> name;
    check(reference->GetPartName(&name));
    result.parts.push_back(identity(name.Get()));
  }
  ComPtr<IOpcSignatureRelationshipReferenceEnumerator> relationships;
  check(signature->GetSignatureRelationshipReferenceEnumerator(&relationships));
  for (check(relationships->MoveNext(&next)); next; check(relationships->MoveNext(&next))) {
    if (result.relationship_sources.size() >= configured.max_references - result.parts.size()) {
      throw error{error_code::resource_limit, "Signature reference limit exceeded"};
    }
    ComPtr<IOpcSignatureRelationshipReference> reference;
    check(relationships->GetCurrent(&reference));
    task_string digest;
    check(reference->GetDigestMethod(&digest.value));
    algorithm(digest.value, digest_method);
    OPC_RELATIONSHIPS_SIGNING_OPTION selection{};
    OPC_CANONICALIZATION_METHOD transform{};
    check(reference->GetRelationshipSigningOption(&selection));
    check(reference->GetTransformMethod(&transform));
    if (selection != OPC_RELATIONSHIP_SIGN_PART || transform != OPC_CANONICALIZATION_C14N) {
      throw error{error_code::unsupported_feature, "Only whole relationship parts with inclusive C14N are supported"};
    }
    ComPtr<IOpcUri> name;
    check(reference->GetSourceUri(&name));
    result.relationship_sources.push_back(identity(name.Get()));
  }
  return result;
}

// Reject a cryptographically valid signature that omits content the application requires.
void require_coverage(const coverage& actual, const coverage& required) {
  for (const auto& name : required.parts) {
    if (std::find(actual.parts.begin(), actual.parts.end(), name) == actual.parts.end()) {
      throw error{error_code::integrity, "Required package part is not signed", name};
    }
  }
  for (const auto& name : required.relationship_sources) {
    if (std::find(actual.relationship_sources.begin(), actual.relationship_sources.end(), name) == actual.relationship_sources.end()) {
      throw error{error_code::integrity, "Required relationship source is not signed", name};
    }
  }
}
#endif
} // namespace

// Keep provider availability explicit on every platform without introducing a fallback trust policy.
bool available() noexcept {
#if defined(_WIN32)
  return xml::available();
#else
  return false;
#endif
}

// Authenticate selected OPC coverage with a caller-pinned certificate before returning any evidence.
std::vector<verified_signature> verify(std::span<const std::uint8_t> package_bytes,
    std::span<const std::uint8_t> trusted_certificate_der, const coverage& required, limits configured) {
#if defined(_WIN32)
  if (trusted_certificate_der.empty() || trusted_certificate_der.size() > configured.max_metadata_bytes ||
      trusted_certificate_der.size() > std::numeric_limits<DWORD>::max()) {
    throw error{error_code::semantic, "An admitted pinned signing certificate is required"};
  }
  const std::unique_ptr<const CERT_CONTEXT, decltype(&CertFreeCertificateContext)> certificate{
      CertCreateCertificateContext(X509_ASN_ENCODING, trusted_certificate_der.data(),
          static_cast<DWORD>(trusted_certificate_der.size())), CertFreeCertificateContext};
  if (!certificate || CertVerifyTimeValidity(nullptr, certificate->pCertInfo) != 0) {
    throw error{error_code::semantic, "Pinned signing certificate is invalid or outside its validity interval"};
  }
  opened_package source{package_bytes, configured};
  const auto selected = signatures(source, configured);
  if (selected.empty()) { throw error{error_code::integrity, "Package has no signature"}; }
  std::vector<verified_signature> result;
  for (const auto& signature : selected) {
    ComPtr<IOpcSignatureReferenceEnumerator> custom;
    check(signature->GetCustomReferenceEnumerator(&custom));
    BOOL has_custom{};
    check(custom->MoveNext(&has_custom));
    if (has_custom) { throw error{error_code::unsupported_feature, "Custom OPC signature references are unsupported"}; }
    ComPtr<IOpcSignatureReference> package_reference;
    check(signature->GetPackageObjectReference(&package_reference));
    task_string package_digest;
    check(package_reference->GetDigestMethod(&package_digest.value));
    algorithm(package_digest.value, digest_method);
    task_string method;
    check(signature->GetSignatureMethod(&method.value));
    algorithm(method.value, signature_method);
    OPC_CANONICALIZATION_METHOD canonicalization{};
    check(signature->GetCanonicalizationMethod(&canonicalization));
    if (canonicalization != OPC_CANONICALIZATION_C14N) {
      throw error{error_code::unsupported_feature, "Only inclusive signature canonicalization is supported"};
    }
    auto signed_content = signature_coverage(signature.Get(), configured);
    require_coverage(signed_content, required);
    OPC_SIGNATURE_VALIDATION_RESULT validation{};
    check(source.manager->Validate(signature.Get(), certificate.get(), &validation));
    if (validation != OPC_SIGNATURE_VALID) { throw error{error_code::integrity, "Package signature is invalid for the pinned signing key"}; }
    ComPtr<IOpcPartUri> name;
    check(signature->GetSignaturePartName(&name));
    result.push_back({identity(name.Get()), std::move(signed_content)});
  }
  return result;
#else
  static_cast<void>(package_bytes); static_cast<void>(trusted_certificate_der);
  static_cast<void>(required); static_cast<void>(configured);
  throw error{error_code::unavailable, "OPC signatures require the Windows Packaging API provider"};
#endif
}

// Import one ephemeral signing identity and use the explicitly selected OS canonicalization profile.
signed_package sign(std::span<const std::uint8_t> package_bytes,
    std::span<const std::uint8_t> certificate_pfx, std::wstring_view password,
    const coverage& selected, limits configured) {
#if defined(_WIN32)
  if (selected.parts.empty() || selected.parts.size() > configured.max_references ||
      selected.relationship_sources.size() > configured.max_references - selected.parts.size()) {
    throw error{error_code::semantic, "Signing requires a nonempty admitted part selection"};
  }
  if (certificate_pfx.empty() || certificate_pfx.size() > configured.max_metadata_bytes ||
      certificate_pfx.size() > std::numeric_limits<DWORD>::max()) {
    throw error{error_code::resource_limit, "Signing certificate exceeds metadata limit"};
  }
  opened_package source{package_bytes, configured};
  if (!signatures(source, configured).empty()) { throw error{error_code::invalid_state, "Remove existing signatures explicitly before signing again"}; }
  CRYPT_DATA_BLOB blob{static_cast<DWORD>(certificate_pfx.size()), const_cast<BYTE*>(certificate_pfx.data())};
  std::wstring secret{password};
  const auto store = PFXImportCertStore(&blob, secret.c_str(), PKCS12_NO_PERSIST_KEY | PKCS12_ALWAYS_CNG_KSP);
  SecureZeroMemory(secret.data(), secret.size() * sizeof(wchar_t));
  if (!store) { throw error{error_code::semantic, "Cannot import the in-memory signing identity"}; }
  const std::unique_ptr<void, decltype([](void* value) { CertCloseStore(value, 0); })> retained_store{store};
  const CERT_CONTEXT* selected_certificate{};
  const CERT_CONTEXT* current{};
  DWORD private_key_error{static_cast<DWORD>(CRYPT_E_NOT_FOUND)};
  while ((current = CertEnumCertificatesInStore(store, current)) != nullptr) {
    HCRYPTPROV_OR_NCRYPT_KEY_HANDLE key{};
    DWORD key_spec{};
    BOOL caller_free{};
    // NO_PERSIST imports carry a cached key context rather than a persisted provider/key name.
    if (!CryptAcquireCertificatePrivateKey(current, CRYPT_ACQUIRE_SILENT_FLAG | CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG | CRYPT_ACQUIRE_CACHE_FLAG,
        nullptr, &key, &key_spec, &caller_free)) { private_key_error = GetLastError(); continue; }
    if (caller_free) {
      if (key_spec == CERT_NCRYPT_KEY_SPEC) { NCryptFreeObject(key); }
      else { CryptReleaseContext(key, 0); }
    }
    if (selected_certificate) {
      CertFreeCertificateContext(current);
      CertFreeCertificateContext(selected_certificate);
      throw error{error_code::semantic, "Signing PFX contains multiple private-key identities"};
    }
    selected_certificate = CertDuplicateCertificateContext(current);
  }
  const std::unique_ptr<const CERT_CONTEXT, decltype(&CertFreeCertificateContext)> certificate{
      selected_certificate, CertFreeCertificateContext};
  if (!certificate) {
    const auto message = "Cannot acquire an ephemeral signing key (Windows error " +
        std::to_string(private_key_error) + ")";
    throw error{error_code::semantic, message.c_str()};
  }
  if (CertVerifyTimeValidity(nullptr, certificate->pCertInfo) != 0) {
    throw error{error_code::semantic, "Signing certificate is outside its validity interval"};
  }
  ComPtr<IOpcSigningOptions> options;
  check(source.manager->CreateSigningOptions(&options));
  check(options->SetSignatureMethod(signature_method));
  check(options->SetDefaultDigestMethod(digest_method));
  check(options->SetCertificateEmbeddingOption(OPC_CERTIFICATE_IN_SIGNATURE_PART));
  ComPtr<IOpcSignaturePartReferenceSet> parts;
  check(options->GetSignaturePartReferenceSet(&parts));
  for (const auto& name : selected.parts) {
    ComPtr<IOpcPartUri> uri;
    check(source.factory->CreatePartUri(wide(name).c_str(), &uri));
    ComPtr<IOpcSignaturePartReference> reference;
    check(parts->Create(uri.Get(), digest_method, OPC_CANONICALIZATION_NONE, &reference));
  }
  ComPtr<IOpcSignatureRelationshipReferenceSet> relationships;
  check(options->GetSignatureRelationshipReferenceSet(&relationships));
  for (const auto& name : selected.relationship_sources) {
    ComPtr<IOpcUri> uri;
    if (name == "/") { check(source.factory->CreatePackageRootUri(&uri)); }
    else {
      ComPtr<IOpcPartUri> part;
      check(source.factory->CreatePartUri(wide(name).c_str(), &part));
      check(part.As(&uri));
    }
    ComPtr<IOpcSignatureRelationshipReference> reference;
    check(relationships->Create(uri.Get(), digest_method, OPC_RELATIONSHIP_SIGN_PART, nullptr,
        OPC_CANONICALIZATION_C14N, &reference));
  }
  ComPtr<IOpcDigitalSignature> signature;
  check(source.manager->Sign(certificate.get(), options.Get(), &signature));
  signed_package result{source.bytes(configured), {certificate->pbCertEncoded,
      certificate->pbCertEncoded + certificate->cbCertEncoded}};
  static_cast<void>(verify(result.bytes, result.certificate_der, selected, configured));
  return result;
#else
  static_cast<void>(package_bytes); static_cast<void>(certificate_pfx); static_cast<void>(password);
  static_cast<void>(selected); static_cast<void>(configured);
  throw error{error_code::unavailable, "OPC signatures require the Windows Packaging API provider"};
#endif
}

// Make signature invalidation an explicit edit operation rather than an opaque preservation promise.
std::vector<std::uint8_t> remove(std::span<const std::uint8_t> package_bytes, limits configured) {
#if defined(_WIN32)
  opened_package source{package_bytes, configured};
  const auto selected = signatures(source, configured);
  for (const auto& signature : selected) {
    ComPtr<IOpcPartUri> name;
    check(signature->GetSignaturePartName(&name));
    check(source.manager->RemoveSignature(name.Get()));
  }
  return source.bytes(configured);
#else
  static_cast<void>(package_bytes); static_cast<void>(configured);
  throw error{error_code::unavailable, "OPC signatures require the Windows Packaging API provider"};
#endif
}
} // namespace rohit::serializer::package::signatures
