# Security policy

## Supported versions

Security fixes target the current `main` branch. Older revisions and release
lines do not have a guaranteed backport policy. Include the exact affected commit
or release in your report, even if you cannot reproduce it on the latest revision.
Consumers should regenerate affected code and rebuild when applying a generator
security fix; updating the compiler alone does not update existing generated code.

## Report a vulnerability privately

Email [rohit@singh.org.in](mailto:rohit@singh.org.in) with the subject
`[Serializer security]` and a short description. This is the maintainer contact
published in the project's [code of conduct](CODE_OF_CONDUCT.md).

Do not disclose suspected vulnerabilities, exploit payloads, or sensitive data in
public issues or pull requests before coordinating with the maintainer.

Please include, where available:

- The affected commit or release, operating system, compiler or runtime version,
  output language, protocol, and relevant build or generator options.
- A minimal schema, input, and reproduction commands, with the expected and
  observed behavior. Use synthetic data and remove credentials and personal data.
- The potential impact and required conditions, including whether untrusted
  schemas, encoded messages, or persisted data can trigger the problem.
- Relevant diagnostics, a reduced proof of concept, and any suggested mitigation.

Reports may concern the schema compiler, generated codecs, runtime decoding,
compression, or managed storage and recovery. Memory-safety failures, resource-limit
bypasses, and unexpected data exposure are examples of security concerns.
Report ordinary defects without a security impact through the
[issue tracker](https://github.com/rohit-singh-gautam/Serializer/issues).

## Handling and disclosure

The maintainer will assess reports and coordinate reproduction, mitigation, fixes,
and disclosure with the reporter through the private contact channel. Response
and remediation times are not guaranteed. If you have not received a reply, follow
up using the same email thread.

Please coordinate public disclosure until a fix or mitigation is available.
Include your preferred attribution, or say if you would like to remain anonymous.
