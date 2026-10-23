# Designs and implementation specifications

[Documentation index](../README.md)

Use [the central pending backlog](../pending.md) for outstanding work and the
[implemented feature catalog](../project/completed_requirements.md) for completed
IDs and evidence. These design documents retain
contracts and rationale, rather than separate completion lists.

These documents preserve design reasoning and implementation requests. Their
status differs: the dimension specification has implementation evidence, XML and
multipart packages have implementation subsets and pending gates, paged indexing remains proposed, and the repository
specification records a dated positioning and packaging brief. Use the current
guides for supported APIs.

| Document | Status and purpose |
| --- | --- |
| [Rebuildable cache attributes and storage](cache_attributes.md) | Owning C++ cache fields, bounded provider/rebuild APIs, optional retention priority, shared `.serializer/` layout, explicit migration and graceful-close retirement; see the dated qualification record for actual scope. |
| [Dimension templates and fixed arrays](generic_dimension_templates.md) | Historical implementation specification with accepted scope and qualification links; see the current [generics guide](../schema/generics.md) for implemented behavior. |
| [Low-memory paged indexing](paged_indexing.md) | Research and proposed disk-backed indexing, recovery, bounded construction, and lazy storage; no paged backend is implemented or measured. |
| [Storage, schema indexes and database integration requirements](database_storage_requirements.md) | Consolidated standalone/database indexing, mandatory C++23 minimum as first priority, data ownership, independent local/database/both index placement, specialized methods, durable publication, lazy storage and acceptance gates; proposed features remain unimplemented. |
| [XML and package support design](xml_support.md) | XML/XSD, HTML boundaries, codec-independent ZIP, OPC requirements PKG01–PKG09 and release gates; see [implemented APIs](../serialization/xml_packages.md) and [pending work](../pending.md). |
| [OPC and 3MF authoring requirements](opc_3mf_support.md) | Application/framework ownership, C++ generated OPC profiles, inline XML/XSD authoring and declaration import, restricted schema/build example and remaining conformance/parity requirements. |
| [Typed managed entity references](managed_entity_references.md) | C++ typed references/static class tags, import/edit operations and previews; strict collaborative identity contract with explicit qualification boundaries. |
| [Repository, extensions, and vcpkg specification](repository_extensions_vcpkg_spec.md) | Dated implementation brief for project wording, stable identities, package metadata, and release validation; it is not a declaration that every requested deliverable has shipped. |
