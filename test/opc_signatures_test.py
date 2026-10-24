# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: GPL-3.0-or-later
"""Independently qualify native OPC signatures with ZIP producers and .NET Framework Packaging."""
import argparse
from pathlib import Path
import subprocess
import zipfile

MANAGED = r'''
param([string]$Mode, [string]$Directory, [string]$Package)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName WindowsBase
if ($Mode -eq 'certificate') {
  $rsa = [System.Security.Cryptography.RSACng]::new(2048)
  $request = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
    'CN=Serializer independent qualification', $rsa,
    [System.Security.Cryptography.HashAlgorithmName]::SHA256,
    [System.Security.Cryptography.RSASignaturePadding]::Pkcs1)
  $certificate = $request.CreateSelfSigned([DateTimeOffset]::UtcNow.AddMinutes(-5), [DateTimeOffset]::UtcNow.AddDays(1))
  try {
    [IO.File]::WriteAllBytes((Join-Path $Directory 'signer.pfx'), $certificate.Export(
      [System.Security.Cryptography.X509Certificates.X509ContentType]::Pfx, 'serializer-test'))
    [IO.File]::WriteAllBytes((Join-Path $Directory 'signer.cer'), $certificate.Export(
      [System.Security.Cryptography.X509Certificates.X509ContentType]::Cert))
  } finally { $certificate.Dispose(); $rsa.Dispose() }
} else {
  $access = if ($Mode -eq 'sign') { [IO.FileAccess]::ReadWrite } else { [IO.FileAccess]::Read }
  $archive = [System.IO.Packaging.Package]::Open($Package, [IO.FileMode]::Open, $access)
  try {
    $manager = [System.IO.Packaging.PackageDigitalSignatureManager]::new($archive)
    if ($Mode -eq 'sign') {
      $manager.HashAlgorithm = 'http://www.w3.org/2001/04/xmlenc#sha256'
      $manager.CertificateOption = [System.IO.Packaging.CertificateEmbeddingOption]::InSignaturePart
      $certificate = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new(
        (Join-Path $Directory 'signer.pfx'), 'serializer-test',
        [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::EphemeralKeySet)
      try { $null = $manager.Sign([Uri[]]@([Uri]::new('/model.bin', [UriKind]::Relative)), $certificate) }
      finally { $certificate.Dispose() }
    } else {
      $result = $manager.VerifySignatures($false)
      if ($result -ne [System.IO.Packaging.VerifyResult]::Success) { throw "Independent signature verification failed: $result" }
    }
  } finally { $archive.Close() }
}
'''


def run(command, success=True):
    """Require real provider success or failure without treating an unavailable SDK as a pass."""
    result = subprocess.run([str(value) for value in command], capture_output=True, text=True, timeout=120)
    if (result.returncode == 0) != success:
        raise AssertionError(str(command) + '\n' + result.stdout + result.stderr)
    return result


def rewrite(source, destination, replacements):
    """Rebuild ZIP CRCs independently so signature failures cannot be attributed to broken ZIP integrity."""
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED) as edited:
        for item in original.infolist():
            data = original.read(item.filename)
            edited.writestr(item.filename, replacements.get(item.filename, data))


def main():
    """Create isolated test identities and clean private-key artifacts even after failed verification."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    args.build = args.build.resolve()
    args.build.mkdir(parents=True, exist_ok=True)
    script = args.build / 'managed-provider.ps1'
    script.write_text(MANAGED)
    managed = ['powershell', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', script]
    try:
        run([*managed, 'certificate', args.build, ''])
        unsigned = args.build / 'unsigned.opc'
        types = b'<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="bin" ContentType="application/octet-stream"/><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/></Types>'
        relationships = b'<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="urn:serializer:signature-test" Target="model.bin"/></Relationships>'
        with zipfile.ZipFile(unsigned, 'w', zipfile.ZIP_DEFLATED) as output:
            output.writestr('[Content_Types].xml', types)
            output.writestr('_rels/.rels', relationships)
            output.writestr('model.bin', b'original signed content')
        signed = args.build / 'signed.opc'
        run([args.probe, 'verify', unsigned, args.build / 'signer.cer'], False)
        run([args.probe, 'sign', unsigned, args.build / 'signer.pfx', signed])
        run([args.probe, 'verify', signed, args.build / 'signer.cer'])
        run([*managed, 'verify', args.build, signed])
        for name, replacements in {
            'payload': {'model.bin': b'edited signed content'},
            'type': {'[Content_Types].xml': types.replace(b'application/octet-stream', b'text/plain')},
            'relationship': {'_rels/.rels': relationships.replace(b'urn:serializer:signature-test', b'urn:serializer:edited')},
        }.items():
            edited = args.build / (name + '.opc')
            rewrite(signed, edited, replacements)
            run([args.probe, 'verify', edited, args.build / 'signer.cer'], False)
            run([*managed, 'verify', args.build, edited], False)
        managed_signed = args.build / 'managed.opc'
        managed_signed.write_bytes(unsigned.read_bytes())
        run([*managed, 'sign', args.build, managed_signed])
        run([args.probe, 'verify-part', managed_signed, args.build / 'signer.cer'])
        run([args.probe, 'verify', managed_signed, args.build / 'signer.cer'], False)
        removed = args.build / 'removed.opc'
        run([args.probe, 'remove', signed, removed])
        run([args.probe, 'verify', removed, args.build / 'signer.cer'], False)
        resigned = args.build / 'resigned.opc'
        run([args.probe, 'sign', removed, args.build / 'signer.pfx', resigned])
        run([*managed, 'verify', args.build, resigned])
        wrong = args.build / 'wrong'
        wrong.mkdir(exist_ok=True)
        run([*managed, 'certificate', wrong, ''])
        run([args.probe, 'verify', signed, wrong / 'signer.cer'], False)
        print('PASS: native/.NET producers and validators, pinned trust, signed coverage, edit rejection, explicit removal and resigning')
    finally:
        for key in (args.build / 'signer.pfx', args.build / 'wrong/signer.pfx'):
            key.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
