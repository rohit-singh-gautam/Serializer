# Managed document features

`document.serializer` is the complete model. Run the `serializer_managed_document_features`
CMake target to demonstrate these generated features:

| Feature | Schema declaration | Executed example |
| --- | --- | --- |
| Schema-declared edit and readonly methods | `set_metrics(...) edit cpp`, `add_pages(...) edit`, `estimated_words() readonly expression(...)` | Native and application-owned template definitions share one atomic transaction; qualification references the external method, undo/redo restores data. |
| Private persistent data | `private string name (1)` | `author().set_name("Ada")` through a checked editor. |
| Independently managed bases | `public managed author_data`, `public managed review_data` | Distinct base/root IDs survive Save/load and shared history. |
| Ordinary multiple bases | `public title_data`, `public language_data` | Encoded nested field scopes allow selective title undo while preserving another author's language edit. |
| Managed owning variants | `managed variant(text_data = text, attachment_data = attachment)` | Switch to a file attachment; undo/redo restores the earlier tag and exact child IDs. |
| Transient fields | `public transient uint64 preview_cache {0}` | Guarded runtime update changes no saved bytes; durable publication resets the cache. |
| Copy/clone policy | Ordinary generated value members | Detached copying preserves runtime fields; explicit reset cloning restores defaults. |
| Concrete managed generics | `instantiate string_annotation = annotation<string>` and `annotation<text_data>` | Generated editors change typed notes and nested paragraph values. |
| Fixed managed arrays | `public managed array[2] text_data paragraphs (1)` | Checked child edits preserve fixed membership and identities through undo. |
| Active managed field lifetimes | `obsolete(2)` and `created(2)` on managed child fields | Inactive children and variants keep identity zero and reject editor access; only current fields participate in identity traversal. |
| Payload lifecycle metadata | `version`, `obsolete(2)`, `created(2) replaced(...)` | Managed Save/load and undo retain the current versioned title. Whole-document schema conversion uses the separate migration API. |

The executable uses direct identity storage. The generated tests cover
independently managed bases, variant conversion and transient traversal with
`managed.separate_values = true`. Virtual/repeated ancestor inheritance, packed/view editing, raw union
editing and recursive owning schemas remain rejected.

`test/managed_document_features_test.cpp` also tests stale alternative editors:
switching away and back never revives an old handle. Alternative replacement
conservatively invalidates every existing alternative editor in that transaction.

`behavior.hpp` supplies the application-owned editor-template definition after the generated schema is included. Tests reject completed editors and prove runtime receivers cannot invoke durable edit methods.

The CMake targets select [codegen.ini](codegen.ini) (cpp.naming = preserve)
because native method bodies use authored identifiers. [behavior.hpp](behavior.hpp)
supplies the external editor method definition; the example invokes the generated
required-definition check before using it.
