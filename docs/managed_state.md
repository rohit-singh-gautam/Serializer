# Managed state, authorization, and collaboration proposal

Status: design direction; not implemented in any language backend. This document
extends the [transactional history proposal](history.md) with constraints to keep
in mind during implementation. It does not add authentication, collaboration,
distributed commits, or external-effect adapters to Serializer today.

The initial local history implementation can stay small. It should expose clear
validation, identity, persistence, and publication boundaries so these features
can be added without allowing writes to bypass transaction rules. All contracts
below apply across supported generated languages; API syntax and resource cleanup
can remain idiomatic for each language.

This is a generic facility. The [application examples](managed_examples.md) use a
cylinder-with-hole difference, an accounting ledger, a wordpad-style editor, and
other data models. Graphics ownership trees illustrate the rules without defining
the subsystem's domain. See the history proposal for
[RAII completion and failure reporting](history.md#scoped-completion-and-raii),
[compact identity encoding](history.md#compact-records-without-losing-identity),
[deleted-entity retention](history.md#deleted-from-the-model-retained-in-history),
and [storage/performance priorities](history.md#storage-and-performance-priorities).

## Naming the broader capability

Recommend **Serializer managed state** for the optional subsystem, `model_store`
for its owning runtime, and `managed` for schema opt-in. Keep **history** as the
name of the optional revision/undo component. Identity, authorization, and
transactions remain useful when history recording is disabled.

| Candidate | Fit |
| --- | --- |
| `managed` / managed state | Recommended: expresses store-controlled identity and mutation without tying them to one optional capability. |
| `model` / `model_store` | Describes the application data owned by the runtime; works for designs, projects, and other domains. |
| `document` / `document_store` | Suitable for document applications, but can imply file or document-database storage. |
| `history` / `history_store` | Clear for revision retention; narrower than authorization and collaboration. |
| `accounting` | Suggests bookkeeping or financial records; retain it only as an application's chosen root type. |

Use this naming consistently in the proposals. `history` names an optional
feature, not a separate schema modifier; `managed` names participation in the
broader subsystem. All syntax below remains unimplemented.

```text
serializer version 1;

class component stable_ids managed {
  public string name (1);
  public double height (2);
}

class assembly stable_ids {
  public string name (1);
  public managed map(uint64) component components (2);
}

class design stable_ids {
  public managed map(uint64) assembly assemblies (1);
  public exclude(history) uint64 selected_component_id (2);
  public transient bool calculation_cache_valid;
}
```

No class-level marker is needed on `assembly` or `design`: their own managed
members imply capability. The leaf `component` declares it explicitly. On a member,
`managed` identifies owned entities only when the parent occurrence is managed.
Unmarked members remain owner-tracked values, and normal construction still
creates ordinary objects. Value-field exclusions and `transient` have separate
persistence and participation rules described below.

Generated `tracked_` editor names can remain: they describe the access interface.
Runtime permissions belong in policies, not in per-user `.serializer` declarations.
The word managed describes control by the store, independently of a language's
memory-management model.

## Inferred generation and explicit root creation

Declare managed capability on a class, or infer it when that class directly
declares a managed member. Every managed member must target a class that already
qualifies by one of these rules. Referencing a type through a managed member does
not grant capability to that type. Validate declarations before generating
companions; an invalid target makes the schema invalid.

Capability does not put a history manager, ID, or access context inside every
ordinary instance of the class. It permits the generated managed representation
to be used when explicitly selected at a member or store boundary.

For example, a normal `project` construction still makes an ordinary map of tasks.
Creating a `model_store<project>` explicitly makes that instance a managed root.
The store owns identities, transaction state, configured policies, and optional
history. A managed task's changes feed that store; they do not create a second
history manager in the task or its class.

Root selection is independent of the type's other uses. The same project type
can be a root in one store, an explicitly managed child in another, or an ordinary
value. An unmarked member containing a management-capable type remains an ordinary
value occurrence; inference must not promote descendants across that boundary.

A standalone root that has only scalar/value members must declare the bare
class-level `managed` marker. A class with its own valid managed members already
qualifies and needs no repeated class marker. Constructing a store or referencing
a type from another schema cannot silently upgrade an ineligible type. For the
initial proposal, feature selectors apply to members; root feature activation
belongs to store options. Do not introduce competing class-level feature defaults.

### Managed leaf types require declared capability

A type does not need managed children in order to be a managed entity itself,
but a leaf type must declare that capability explicitly. The member marker selects
the managed representation; it does not authorize generation for an otherwise
ineligible payload type. Under this revised proposal, the following is invalid:

```text
class task stable_ids {
  public uint32 taskid (1);
}

class project stable_ids {
  public managed map(uint64) task tasks (1);
}
```

Proposed diagnostic:

```text
project.tasks requires a managed-capable element type.
task has no class-level managed declaration and declares no managed members.
Declare task managed, or remove managed from project.tasks.
```

The correction is:

```text
class task stable_ids managed {
  public uint32 taskid (1);
}

class project stable_ids {
  public managed map(uint64) task tasks (1);
}
```

The class author declares that task supports the companion representation; the
member author chooses it for this collection. Project qualifies through its own
managed member, so its class header remains unmarked. This makes the two levels
meaningful without forcing a redundant class marker on every container.

The generator can now produce ordinary `project` / `task` classes and their
`tracked_project` / `tracked_task` companions. When the project is managed, task
scalar edits are controlled by the task editor and participate in selected
features. Scalars do not acquire their own entity IDs. The task class marker
never forces ordinary task instances or unmarked task members to use management.

This replaces the earlier idea of inferring target capability from an incoming
managed reference. Requiring target eligibility keeps capability in the defining
schema, so a consuming schema cannot silently opt a plain leaf class into generated
management. Use the same validation in every language backend.

Resolve the schema type graph before validating targets, so declaration order
does not determine capability. Imported types must advertise the same declared
or inferred capability through their schema metadata and have a compatible
companion generated or provided by their owning module. Report missing capability,
unsupported ownership/features, and unavailable external adapters distinctly.
Validate managed member declarations even when some runtime occurrences remain plain.

`taskid` remains an ordinary application field. Its name and numeric type do not
make it the managed identity. In this example, the store-controlled `uint64` map
key and document identity supply that identity under the history proposal's
collection rules; explicit binding would be needed to use a payload field instead.

### Plain containment does not propagate managed support

Distinguish a type that has a generated managed companion from a particular
occurrence that uses it. A managed member makes the declaring type eligible for
generation; an ordinary member whose type has that eligibility does not pass it
to its containing class.

Using the task type from the history example:

```text
class project stable_ids {
  public managed map(uint64) task tasks (1);
}

class project_archive stable_ids {
  public project saved_project (1);
}

class workspace stable_ids {
  public project draft (1);
  public managed project active (2);
}
```

`project` needs companion generation because it declares `tasks` as managed.
`project_archive` gains no inferred capability merely by containing an ordinary
project. It must declare class-level `managed` or introduce its own valid managed
member before it can be used as a managed target or root. `workspace` qualifies
because its own `active` member is managed and project is an eligible target.

| Access context | `draft` and its tasks | `active` and its tasks |
| --- | --- | --- |
| Ordinary `workspace` | Ordinary project and task values | Ordinary project and task values |
| Managed workspace through its store | Ordinary project and task values | Managed project and managed task entities |

The plain `project` representation contains no hidden managed task collection,
entity registry, or transaction context. Its declared map keys remain ordinary
payload data. Its nested managed annotations are inactive for that occurrence;
generating a companion for another occurrence cannot change its representation.

For a managed workspace, `draft` is still an owned value. A scoped edit may record
its changes in the workspace's history and must obey the workspace's permissions,
without giving that project or its tasks independent managed identities. Plain
representation does not mean `exclude(history)`; that exclusion remains explicit.

Separate capability, member validation, and occurrence binding:

```text
supports_managed(type) = has_class_managed_marker(type)
                     or directly_declares_managed_member(type)

managed_member_is_valid = supports_managed(target_type)
                     and ownership_and_features_are_supported

is_managed_occurrence = is_store_root
                    or (parent_is_managed and member_is_managed)
```

Compute capability from each type's own declaration, then validate every managed
member target before generation. An incoming edge never changes that predicate.
Ordinary containment neither infers capability nor activates a child occurrence.
Removing `active` from the workspace example leaves only plain containment, so
workspace would need an explicit class marker to remain eligible. These rules
apply equally across all supported output languages.

## Feature selection with one keyword

Recommend a positive list or an explicit set exclusion:

| Proposed member syntax | Optional feature participation |
| --- | --- |
| `managed` | All features in the selected, versioned feature profile. |
| `managed(all)` | Explicit spelling of the same selection. |
| `managed(history)` | History only, plus the mandatory managed foundation. |
| `managed(history, collaboration)` | The named features, when supported together by the profile/backend. |
| `managed(all except history)` | All profile features except history. |
| `managed(all except history, collaboration)` | All profile features except both named features. |

Prefer `except` to `!` or `~`: this is subtraction from a feature set, not Boolean
negation or a bitwise operation. `managed(!history)` would also leave the source
set implicit. If symbolic syntax were required, `!` would convey exclusion more
readily than `~`, but the recommended grammar accepts neither alias.

Use these distinct grammar forms: a bare modifier; a nonempty list of feature
names; `all`; or `all except` followed by a nonempty exclusion list. Reject empty
lists, unknown/duplicate names, mixed positive and negative lists, and contradictory
dependencies. `except` is syntax inside this modifier, not a new field modifier.
Every comma-separated name following `except` is excluded: the last row means
`profile_features - {history, collaboration}`, not an exclusion followed by an
inclusion. Listing positive names without `all except` selects only those names.

```text
class project stable_ids {
  public managed(history) map(uint64) task tasks (1);
  public managed(all except history) map(uint64) task reference_tasks (2);
}
```

Both collections contain independently identified entities in a managed project.
Changes to `tasks` can enter undo history; changes to `reference_tasks` persist
without being restored by history. Normal project construction contains only
ordinary task values in both collections. As with the earlier examples, the
`task` type is defined in the [history proposal](history.md).

### Define what all includes

`all` must expand against a named, versioned feature profile selected for schema
generation. Record that selection with generation configuration and persisted
managed metadata. Every language backend must use the same feature set for the
same profile. An unavailable selected feature must be diagnosed; do not silently
redefine `all` to mean whatever that backend happens to implement.

Pin the profile rather than implicitly adding future features on a compiler
upgrade. Moving to a new profile is an explicit configuration/migration decision.
The exact profile registry and configuration spelling remain to be designed;
there is no corresponding compiler switch today. The table illustrates proposed
selectors, not a claim that collaboration or every combination is implemented.

Schema selectors define which features a member can participate in. Runtime store
options choose which are active and provide any required policies/adapters. Bare
`managed` does not itself start networking, perform file writes, or enable history
recording. A history mode still selects disabled, linear, or tree behavior.
Explicitly requested runtime features without generated support must fail clearly.

Independent identity, ownership validation, scoped mutation, and atomic local
transaction publication are the managed foundation, not optional feature bits.
`managed(history)` still needs them. Authorization enforced by the store is also
a mandatory gate on every affected entity, regardless of its feature selector.
It cannot be disabled with `managed(all except authorization)`; reject attempts
to use selectors to bypass it. Per-target grants live in the authorization policy.

Not every future capability is a member feature. Branch merging depends on
history ancestry; distributed commits and external-effect delivery depend on
store/workflow adapters and durable protocols. Maintain a feature/dependency
registry and validate combinations. Excluding a required dependency must produce
an error, not silently turn it back on. Define member eligibility separately from
these operation-level guarantees before adding their names to the grammar.

### Propagation, history exclusion, and value fields

A child's effective optional features are limited by its enclosing managed
occurrence, its member selector, and runtime activation. A bare managed child
inherits the available profile features within that limit. A narrower selection
cannot be widened by a descendant: an explicit request for history inside a
history-excluded subtree is contradictory and must be diagnosed for that managed
occurrence. Inactive annotations in ordinary values remain harmless metadata.

Excluding history from a managed member must exclude its payload, ownership
membership changes, and managed descendants from the enclosing owner's history
projection too. A whole-parent snapshot cannot restore that member incidentally.
Keep its latest persistent values and IDs in current state across undo/redo;
mark persistent changes dirty and notify without creating a history node for them.
Transaction abort still discards those candidate edits, and authorization still
applies. A scalar/value member without an exclusion continues to participate in
its owner's enabled history as before.

If history removes the enclosing owner, the excluded subtree becomes invisible.
Retain its current values, identities, and membership while a retained revision
can restore that owner. This extends the excluded-state overlay described in the
history proposal to managed subtrees. A backend must reject this combination until
it supports correct lifecycle restoration and pruning; skipping setter hooks is
insufficient. Changes to selectors/profiles need explicit history migration rules.

`exclude(history)` expresses a persistent ordinary value excluded from undo;
`managed(all except history)` expresses an independently identified managed entity
excluded from undo. They are not aliases. Scalar caches or a plain saved viewport
do not need an entity identity merely to select a persistence policy. Keep
`transient` for data omitted from serialization as well as history. Using one
keyword for entity management does not collapse these separate value policies.

## Excluding features from ordinary values

Recommend `exclude(history, collaboration)` for a value member that should not
participate in those optional features. This generalizes the earlier proposed
`no_history` spelling to a named feature list. The underscore was a naming choice,
not a semantic requirement. A spelling such as `no history` could be designed as
two grammar tokens, but changing spaces or punctuation would not define the
missing multi-feature semantics by itself.

| Candidate syntax | Assessment |
| --- | --- |
| `exclude(history, collaboration)` | Recommended: explicitly lists the excluded features and scales beyond history. |
| `without(history, collaboration)` | Readable alternative, but choose one canonical spelling rather than aliases. |
| `no managed(history, collaboration)` | Ambiguous about whether it disables the managed representation or only the named features. |
| `no_history` / `nohistory` | Specific to one feature and would require more names as the feature set grows. |

The recommended grammar uses `exclude` followed by a nonempty comma-separated
list of optional feature names. Reject unknown or duplicate names and attempts to
exclude mandatory controls. Do not add the old spelling as a compatibility alias:
none of these proposal keywords has shipped. As with `managed(...)`, each selected
feature must have a defined contract and support in the chosen profile/backend.

```text
class project stable_ids {
  public managed map(uint64) task tasks (1);
  public exclude(history) uint64 selected_task_id (2);
  public exclude(history, collaboration) double scroll_offset (3);
  public transient bool calculation_cache_valid;
}
```

`exclude(...)` applies to an ordinary member's value subtree, including when the
member type has a managed companion available. It allocates no entity ID, does
not request a managed representation, and does not infer managed capability for
the containing class. In this example, `tasks` supplies project's capability.
Ordinary codecs still save excluded values with their usual field IDs; exclusions
affect the named feature projections, not normal serialization.

Exclusions compose with those of enclosing values. An excluded feature cannot be
reactivated inside the same subtree. Features not named continue to participate
under the owner's settings: `exclude(collaboration)` can still allow local undo,
whereas `exclude(history)` can still allow collaboration. Use
`managed(all except history, collaboration)` when independent managed identity is
also required. For the initial grammar, select only one of `managed(...)`,
`exclude(...)`, or `transient` for a member; reject combinations rather than
establishing an implicit override order. Class-level `managed` remains a separate
capability declaration.

Excluding history preserves current values through history navigation as described
in the history proposal. Excluding collaboration needs a separate synchronization
projection: omit the value from outgoing changes and shared snapshots, and reject
incoming writes to it. Preserve the receiving instance's local value; initialize
it to defaults or application-supplied local state when its owner first appears.
Local save/load still includes it. Owner deletion can still remove the containing
object; this is not an independent lifetime guarantee for an excluded child.

A local history record may contain a field excluded only from collaboration.
Therefore, never transmit whole local history records as shared records without
applying the synchronization projection. Diffing, merging, notifications sent to
peers, and shared checkpoints must follow the same exclusion. This is an additional
contract for a future collaboration adapter, not an implemented guarantee. An
exclusion also does not hide data already sent through another channel.

The store's transaction and authorization rules remain mandatory for edits through
its APIs, including excluded values. `exclude(authorization)` cannot bypass an
enforced policy. Use `transient` when a field should be omitted from normal saving
as well; feature exclusion alone does not make a field transient.

## Keep the responsibilities separate

| Component | Responsibility |
| --- | --- |
| Serializer codecs | Encode declared persistent fields and decode bounded input. |
| Model store | Own entities, resolve IDs, stage candidates, and publish valid state atomically. |
| Allocation resource | Supply and reclaim storage according to runtime lifetime decisions. |
| Authorization policy | Decide whether an application-supplied context may perform a particular operation on a target. |
| History component | Retain revisions, navigate alternatives, and later prepare merges. |
| Collaboration adapter | Exchange proposed changes, track acknowledgements, and resolve concurrent submissions. |
| Persistence adapter | Durably record model state and any required journals or effect intents. |
| Application | Authenticate users, issue invitations, assign permissions, supply domain invariants, and integrate external systems. |

Use one controlled publication path for local edits, incoming changes, merge
results, and history navigation. Each prepares a candidate and a complete semantic
change set, then performs authorization and domain validation before publication.
The history component records accepted changes; it does not decide who is allowed
to make them. The serialization core need not depend on any of these modules.

The [ownership and allocation contract](history.md#ownership-and-custom-allocation)
keeps lifetime control in the store while allowing runtime storage providers.
State, history, and scratch resources may be configured separately; payload
allocation support must be declared per backend. Resource pointers are never
persistent identity or serialized data. Reference counts are an optional internal
retention technique, not another schema field or mandatory entity property.

Physical packing never changes semantic boundaries. A parent and its managed
children can share one encoded record, with IDs recovered from a versioned identity
table and relative field paths. Reconstruct canonical identities and the complete
change set before permission checks or merge decisions. Normal scoped completion
must run the same authorization and validation path as explicit commit; failure
is reported without publication, including when completion is attempted by an
RAII destructor. Deleted historical values are immutable until explicitly restored
through a validated transaction or history navigation.

## Authorization on the design hierarchy

Three structures must be distinguished:

- The **ownership tree** describes a design, its assemblies, and their components.
  This is the natural scope for "may edit this part of the design."
- The **revision graph** describes alternative states and their ancestry. Branch
  publication or merging can have additional permissions.
- The **reference graph** connects entities that refer to one another. A reference
  does not convey ownership or extend a permission grant to its target.

Permissions should normally attach to stable owner/entity IDs and stable field
paths. Avoid display names, memory addresses, and collection indexes as permission
identities. The path of a value-owned field starts at its nearest identified owner.
Restricting an individual variable collection entry requires stable addressing;
until supported, authorize the collection as a whole or reject that policy scope.

Use the history proposal's [entity IDs and field paths](history.md#entity-ids-and-field-paths)
for canonical targets. An embedded point's x is cylinder ID plus fields 3/1;
an independently managed point's x is point ID plus field 1. Resolve the actual
entity boundary before applying policy so an alternate parent path cannot evade
the point's restrictions.

### Editable subtrees and read-only exceptions

Consider this ownership tree:

```text
design
  arm_assembly
    bracket
    calibrated_sensor
  base_assembly
```

Both requested policy styles are possible:

| Policy | Explicit grants and restrictions | Result |
| --- | --- | --- |
| Edit one part | Grant read on the design and mutations on the arm subtree; unmatched mutations are denied. | The arm is editable; the base is read-only. |
| Protect one part | Grant read and mutations on the design; explicitly deny mutations on the arm subtree. | The base is editable; the arm is read-only. |
| Protect a nested part | Grant mutations on the arm; explicitly deny mutations on the calibrated sensor. | The bracket is editable; the sensor is read-only. |

Recommended initial resolution rules:

1. Select rules for the application's access context and the requested operation.
2. Match exact targets, declared subtrees, or field paths. Inheritance follows
   ownership, never reference links.
3. An applicable explicit denial wins over a grant. Otherwise an applicable
   grant permits the action; an unmatched action is denied.
4. Combine object permissions with any branch-level restriction by intersection:
   branch access cannot grant access to a protected object.

The unmatched default is not an explicit denial on the root. This distinction
lets an explicit grant on the arm work when the rest of the design has no write
grant. Conversely, a descendant grant cannot override an explicit ancestor denial
under these rules; an authorized policy administrator must change that denial.
"Read-only" denies mutation operations while preserving permitted reads.

A subtree rule anchored to an entity follows that entity and its owned descendants.
Permissions inherited from another ancestor depend on the actual ownership path.
Moves therefore require checks against both existing and proposed ownership, so
moving a protected component cannot be used to evade its restriction. Define
whether newly inserted children inherit a grant; the proposed subtree scope
includes them. Duplication creates new IDs and does not copy administrator grants.

### Separate content edits from structural and policy operations

Use operation-specific permissions rather than one mutable `editable` flag:

| Operation | Required scope |
| --- | --- |
| Set a scalar or edit an owned value | The owning entity and affected member paths. |
| Insert an entity | Insertion into the destination ownership collection. |
| Remove an entity | Removal from the owning collection and deletion of the affected subtree, including protected descendants. |
| Move an entity | Removal from the source, insertion at the destination, and permission for affected entities under both ownership paths. |
| Replace a parent or import a subtree | Every effective field, entity, and membership change; parent access alone is insufficient. |
| Undo, redo, or checkout | Permission to perform the navigation and every resulting content/structure mutation. |
| Merge or publish a branch | Branch operation permission plus all effective changes in the accepted result. |
| Change permissions | A separate policy-administration authority supplied by the application. |

An application can offer an "edit subtree" convenience grant that expands into
defined content and structural operations. It must state which ones; editing a
dimension need not confer deletion or permission-administration rights. Check
semantic changes rather than the physical size of a snapshot. A root snapshot
implementation can still permit a small edit without granting root-wide writes.
Derived persistent changes, reference repairs, and cascading deletions are part
of the effective change set and must be checked too.

### Access context and enforcement boundaries

The application supplies an opaque `access_context` when starting an operation.
It may represent an already authenticated user, a role/capability selected by a
trusted host, or a local editor session. Serializer does not implement login,
invitations, passwords, or identity verification. Entity IDs identify model
objects; they are never proof of a caller's authority.

An illustrative policy hook could have this language-neutral contract:

```text
authorize(access_context, operation, target, policy_version) -> allow | deny

target = entity_id + field_path + ownership_change
```

The trusted application creates and binds the context. An untrusted caller cannot
obtain authority by submitting a string that says "owner" or selecting another
principal ID. All derived editors retain the transaction's bound context. Helpers
do not pass it separately on every setter, and editor conversion cannot expand it.

Check a controlled setter or structural action before applying it to the candidate.
At prepare/commit, authorize the complete effective change set again, including
`exclude(history)` writes. Whole-value callbacks require permission for the mutable
region they expose; if that region contains restricted descendants, use narrower
editors or reject the callback. Do not expose a live mutable parent and rely on
checking only its ID. Final-diff checking remains necessary for generated and
adapter-based bulk operations.

Policy revocation must take effect at the publication boundary. Pin a policy
version while authorizing and publishing, or use an equivalent atomic version
check coordinated with the authority. If permissions or the relevant base state
changed, revalidate or reject. A cached editor, old revision, or transaction opened
before revocation is not a permanent grant. A remote policy service needs its own
version/lease/fencing contract; an earlier network lookup alone cannot close the
race between authorization and publication.

On denial or policy-provider failure, reject the operation and abort its candidate.
Publish no partial state, history node, committed-change notification, or external
effect. Allocated object numbers may still be consumed, as in ordinary aborts;
never reuse them to make failure appear perfectly invisible. Return a structured
failure that the application can present without exposing unreadable data.

Make policy enforcement an explicit store option. An explicitly unrestricted local
store supports existing single-user workflows. Once enforcement is selected,
missing context, missing grants, or a failed policy provider denies the operation;
do not silently fall back to unrestricted access. Disabling history recording
must not disable authorization.

This is an API-level boundary. Direct writes to ordinary objects or process memory
are outside it, as are callers that can replace the policy provider or runtime.
For collaboration, the trusted receiver must authorize and validate incoming
changes independently, even if a client reports successful checks. This separation
of authentication, authorization, and trusted enforcement follows the general
[OWASP authorization guidance](https://cheatsheetseries.owasp.org/cheatsheets/Authorization_Cheat_Sheet.html).

### Policies, history, and read access

Keep active grants and revocations outside undoable document state. Undoing an old
design must not resurrect a revoked invitation. Persist policy references and
versions only as application-controlled metadata; a model file's embedded grant
is not automatically trusted on import. Offline caches cannot guarantee that
another authority has not revoked access while disconnected.

Permissions can also restrict revision/branch operations, but a revision-wide
undo that changes both editable and protected entities fails as a whole. Do not
silently apply the permitted subset and call it the same undo step. A separately
designed selective revert can propose a new transaction with an explicit scope.
Merge conflict resolutions are subject to the same current permissions.

Read-only means readable. Hiding part of the model is a separate requirement. If
read restrictions are later supported, enforce them on lookup, snapshots, value
cloning, history inspection, diffs, notifications, and exports. A full document
already handed to a client cannot be made secret by marking an API read-only.
Keep initial claims limited to controlled mutation unless filtered reads and
replication are actually implemented and tested.

## Branch merging

A merge combines changes from two revisions relative to a common ancestor. For
example, one branch changes a component's name and another changes its height.
A three-way merge can preserve both. If both change height differently, or one
deletes the component while the other edits it, the result needs a defined conflict
policy. Git's documentation provides a familiar description of
[three-way merging](https://git-scm.com/book/en/v2/Git-Branching-Basic-Branching-and-Merging);
Serializer would merge typed model values rather than lines of source text.

Prepare a merge candidate without changing either input revision. Compare stable
entity IDs and schema paths, and retain the base/left/right values for unresolved
conflicts. Equal changes can coalesce. Independent field changes still require
domain validation: individually valid dimension edits can violate a combined
geometric constraint. Do not automatically resolve numeric conflicts by taking
the larger value or the latest wall-clock timestamp.

The initial merge design should cover scalar/value changes and identified map
entries. Ordering, text, shared references, and ownership moves need explicit
merge policies. Without stable element identity, an index-based list diff cannot
reliably distinguish moves from replacements. When ancestry or an appropriate
schema migration is unavailable, fail clearly rather than guess a merge base.

Record a successful merge as a new revision with both parents. The revision
structure then becomes a directed acyclic graph, not a strict tree. Use versioned
records that can later represent multiple parents, reject cycles, and specify
deterministic parent ordering. A practical first rule is that undo follows the
first, target-branch parent; additional parents preserve merge ancestry. A merge
delta can be defined relative to that first parent, so replay never depends on
running an application callback again. Multiple merge bases need a later explicit
policy; the first implementation can reject that case.

Exclude `exclude(history)` state from branch merging and retain the target store's
current overlay; invalidate transient caches. Importing an entity absent from
the target may require initial excluded values from its source. Define that as
an explicit import policy rather than treating excluded edits as history. Policy
grants, audit records, and effect-delivery status must not be merged as ordinary
design fields. Reauthorize the complete resolved candidate before publication.

Implementation consideration now: give revisions stable IDs, keep change data
separate from callbacks, retain necessary ancestry/checkpoints, and version the
envelope so merge-aware formats can be negotiated. Initial tree-only readers
must reject unsupported merge records; adding a parent list alone does not
implement merge semantics.

## Collaborative editing

A first collaboration layer can use one authoritative store. Clients submit
proposed changes against a base revision, and the authority authorizes, validates,
orders, and acknowledges accepted transactions. This extends the single-writer
publication model without immediately requiring a peer-to-peer conflict algorithm.

Proposed request metadata should include document identity, a unique operation
ID, base revision, schema/feature version, and a typed change description. The
authenticated transport supplies the access context; claimed author metadata is
only provenance until the host verifies it. Repeated delivery of an operation
must return its recorded result rather than apply it twice. Reject reuse of one
operation ID with a different payload.

When the base is stale, explicitly reject, rebase with validation, or prepare a
merge. Do not overwrite the authority's current model with a client's complete
snapshot. Track local pending changes separately from acknowledged revisions,
including failures after reconnect. Permission and invariant checks occur against
the authority's current policy and state. A revoked offline editor may retain a
local draft, but the authority can reject its later submission.

Preserve atomic user transactions in synchronization. Check coordinate or shape
invariants on complete submitted edits rather than publishing a stream of partial
setters. Separate presence, cursor positions, and previews from durable model
changes. `exclude(history)` means excluded from undo, not automatically private, local,
or synchronized: the application chooses whether it is document-wide, per-user,
or per-device state. Never let one user's selection overwrite another's merely
because both values can be serialized.

Online clients can obtain entity numbers from the authority. Offline creation
requires disjoint preallocated ranges or a negotiated globally unique ID scheme;
independent counters within one document can collide. An actor identifier in
metadata does not fix collisions in an existing `map(uint64)` identity registry.
Changing the identity representation requires schema/envelope compatibility work.

In a shared model, "undo my action" should normally create a validated new change
that reverses that action where still applicable. Rewinding the shared head could
erase other people's later edits. Define selective undo, dependency conflicts,
ownership changes, and current permissions before exposing that operation.
Keep a durable audit trail separate from a user's prunable undo history when the
application needs attribution or policy-administration records.

A CRDT or operation-transformation adapter is a separate strategy, not a property
automatically acquired by retaining a history tree. Convergence does not ensure
domain validity or authorized writes. For example, Automerge exposes concurrent
property conflicts even while choosing a consistent visible value; see its
[conflict semantics](https://automerge.org/docs/reference/documents/conflicts/).
Choose supported field operations and conflict policies deliberately. Independent
replicas also need a defined authority and revocation model before promising
collaboration over protected subtrees.

Implementation consideration now: distinguish transaction IDs from revision IDs,
carry base versions and provenance, preserve transaction boundaries, and route
incoming changes through the same authorization/validation pipeline. Keep transport
and identity providers replaceable rather than putting network login into codecs.

## Distributed transactions

A local store transaction covers one model publication boundary. Applying changes
to two stores or services creates a different failure problem: one may commit
while the other becomes unavailable. Branch synchronization and merge do not
provide an atomic commit across those participants.

| Approach | Required contract |
| --- | --- |
| One aggregate/store | Prefer when the invariant can share one local transaction boundary. |
| Coordinated atomic commit | Every participant must support durable prepare/commit/abort, recovery, and a coordinator decision; unresolved outcomes can block progress. |
| Saga/workflow | Commit local steps and track retries or application-defined compensation; intermediate states are visible and global isolation is not implied. |

Two-phase commit would require actual durable prepared state and recovery at all
participants, not just a loop calling the existing in-memory `commit()`. Reserve
an explicit integration interface if needed; do not claim support from naming a
local method `prepare`. A timeout can leave an outcome unknown. It must not cause
the framework to report a global abort while a participant may have committed.

A saga coordinates local transactions and compensating actions, as described in
the [Azure saga pattern](https://learn.microsoft.com/en-us/azure/architecture/patterns/saga).
For Serializer, an optional adapter would need durable workflow/step IDs, recorded
outcomes, idempotent retry rules, authorization for every participant, and a way
to resume or report unresolved work. Compensation can fail and may require user
or operator intervention; it is not ordinary undo of an in-memory snapshot.

Implementation consideration now: label local commit and durable save separately,
keep transaction outcome explicit, and reject cross-store atomicity requests until
a coordinator contract exists. Do not make every ordinary edit pay for distributed
coordination.

## External effects: files and network requests

Undoing a model snapshot cannot automatically restore an overwritten file, retract
a delivered message, or make an external service forget a completed request.
Treat these as explicit effects whose lifecycle is separate from revision replay.

| Effect | Possible application policy |
| --- | --- |
| Export a design to a file | Leave the export unchanged on model undo, or explicitly write a new export. |
| Replace an existing file | Retain previous content and use an expected-version/content check before a later restore. |
| Update a remote resource | Use a version precondition and idempotency key; define any inverse request through that service's API. |
| Irreversible action | Show its committed status and require a new corrective action rather than promise reversal. |

Avoid performing effects inside candidate-edit callbacks, which may fail or abort.
For reliable delivery, a persistence adapter can store an effect intent atomically
with the durable model commit, then dispatch it afterward. This requires a real
shared durable transaction or envelope/journal protocol; an in-memory notification
after commit is not a durable outbox. The initial history library provides no such
guarantee by itself.

Give each logical effect a stable ID and record pending, in-flight, succeeded,
failed, or unknown outcomes outside undoable model state. Retries use the same
idempotency key where the destination supports it. A crash after remote success
but before local acknowledgement can require reconciliation; do not promise
exactly-once execution for an arbitrary network endpoint or filesystem operation.
Never resend an effect merely because its revision was replayed, checked out,
merged, or redone.

An undo request can cancel an intent only if delivery has not started under the
dispatcher's synchronization rules. Otherwise it may propose a separate compensating
action with a new effect ID linked to the original. Validate its permissions and
current preconditions. For a file, do not overwrite another writer's later work
while attempting to restore old contents. Record failed or unknown compensation
and make the outcome visible rather than marking the original effect "undone."
Pruning undo history must not discard pending intents or delivery evidence needed
for recovery.

Compensation is application-specific and can leave a different state from the
original one. Its failure and retry behavior needs an explicit contract; see
[compensating transactions](https://learn.microsoft.com/en-us/azure/architecture/patterns/compensating-transaction).

Implementation consideration now: separate replay from effect dispatch, keep
effect status out of the undo projection, and make durability and external outcomes
visible through distinct APIs. Versioned effect descriptors can identify handlers
registered by the application; never execute arbitrary code supplied by a model
file as a restore or compensation callback.

## Boundaries to preserve in the first implementation

The initial history implementation does not need to ship every feature above.
It should preserve these extension points and reject unsupported modes explicitly:

1. Stable document/entity/revision IDs and schema paths, with independent operation
   IDs available for future collaboration and retry tracking.
2. Private candidates, complete semantic change descriptions, validation, and one
   controlled publication boundary. Authorization must cover every mutation path.
3. Optional access contexts and versioned policy hooks, independent of recording
   mode and identity-provider choice. Leave plain objects unchanged.
4. Separate history, persistence, policy, audit, and effect state. Do not assume
   everything that can be serialized should rewind together.
5. Versioned envelopes and explicit feature negotiation for new identity formats,
   merge ancestry, collaborative operations, and durable effect records.
6. Common schema-level semantics with per-backend conformance tests; no backend
   may claim authorization simply because it emits a read-only UI or parses a keyword.

Keep policies and optional adapters composable. Applications that only need local
undo should not need a network stack, identity service, distributed coordinator,
or durable effect dispatcher.

## Future verification requirements

These are acceptance scenarios for later implementation, not tests run here:

- Infer capability from a class's own managed members or its explicit class marker,
  without changing ordinary construction or promoting unmarked child occurrences.
- Reject a managed collection of an unmarked scalar-only task type. Adding the
  task class marker makes it eligible while preserving normal construction and
  leaving `taskid` ordinary data. Never infer target capability from incoming uses.
- Validate the same target-capability rules for imported types, declaration order,
  standalone roots, and each language backend; report unsupported adapters separately.
- A plain-only wrapper around a type with managed members gains no inferred
  companion. In a mixed workspace, only the marked project occurrence activates
  managed tasks; plain project values can still be edited through their owner.
- Expand positive and multiple-exclusion selectors consistently across language
  backends using the pinned profile. Reject invalid grammar and dependencies;
  upgrading the compiler alone cannot expand the profile or bypass authorization.
- `exclude(history, collaboration)` keeps values serialized and ordinary while
  omitting them from the named feature projections. Test local history with
  collaboration-only exclusion, preserved receiver-local values, new-owner defaults,
  and rejection of incoming excluded writes or mandatory-control exclusions.
- Preserve excluded managed IDs and membership through owner undo/restoration,
  save/reload, and pruning without capturing them in parent history snapshots.
- Grant only an assembly, grant everything except an assembly, and protect a child
  inside an editable assembly; verify inheritance and explicit-denial precedence.
- Reject a forbidden scalar edit, `exclude(history)` edit, callback, parent replacement,
  cascading deletion, import, move, merge, and undo without publishing partial state.
- Revoke permission during a transaction; stale handles and stale policy decisions
  cannot commit. Policy failures and missing context fail closed in enforced mode.
- Preserve revocations across undo and reload; untrusted actor claims and imported
  policy metadata cannot create grants. Reference links do not extend scope.
- Verify protected descendants and both ownership paths on move; scoped grants
  cannot be widened by copying an editor or switching branches.
- Verify identical authorization decisions across language backends and history
  modes, including snapshot-based storage and ordinary-value callback boundaries.
- Merge independent edits, equal edits, conflicting values, deletion/edit conflicts,
  invalid combined geometry, and unsupported ancestry without losing either input.
- Preserve IDs through merge, prevent cycles, define undo at merge nodes, and retain
  required ancestors during pruning and cross-language persistence.
- Handle stale and duplicate submissions, mismatched operation IDs, disconnection,
  offline ID allocation, revocation, rejected drafts, and selective collaborative undo.
- Inject failures before/after durable commit, effect dispatch, acknowledgement,
  and compensation. Verify retry/recovery behavior and explicit unknown outcomes.
- Verify that replay does not dispatch effects and pruning does not lose required
  policy, audit, workflow, or delivery records.

Only documentation is added by this proposal. No grammar, generator, runtime,
network service, policy engine, distributed protocol, or platform adapter has been
implemented or qualified. Update the main history proposal, usage guide, integration
skill, and backend support records as each capability becomes available.
