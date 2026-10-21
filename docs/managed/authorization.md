# Authorization in managed collaboration

[Managed features overview](getting_started.md) · [Project overview](../../README.md)

The C++ store and collaboration authority validate complete proposed changes
before publication. Serializer supplies inherited rules, generated ownership
traversal, session gates and policy callbacks; the application selects the trusted
principal and supplies its permission rules.

Schema selectors, generated permission-policy fields and other-language managed
engines remain proposals in the
[authorization design](managed_state.md#authorization-on-the-design-hierarchy).

## Inherited ownership policies

Include `<rohit/managed_authorization.hpp>`. An immutable `authorization_policy`
contains `authorization_rule` values addressed by a document-scoped entity ID and
an optional stable field path. Rules separately allow or deny `content`,
`structure` and `policy` permissions. `authorization_scope::target` covers the
target entity's selected field scope; `subtree` additionally covers its actual
owned descendants. References never convey ownership or permission. Ordinary
collection entries are authorized as a complete collection; their indexes and
keys are not permission identities.

Unmatched operations deny. The default `authorization_precedence::deny_overrides`
follows the design contract: any applicable explicit denial wins, including an
ancestor denial. A grant on one child makes that subtree editable while the
unmatched rest of the document remains read-only. A denial on a nested child
protects it inside a granted subtree.

Applications requiring an editable exception beneath an **explicitly denied**
ancestor can opt into `authorization_precedence::nearest_rule`. Rules on a nearer
entity take precedence, then longer field paths within that entity; equally
specific denials win regardless of rule order. This explicit policy choice must
agree at local and authority boundaries.

`authorize_managed_candidate<Traits>(policy, before, candidate, max_snapshot_bytes)`
compares complete generated durable field and owning-membership encodings.
It requires content permission for changed/new/deleted child payloads and
structure permission for owning membership, keys and variant-alternative changes.
Moves check unchanged fields against **both** ownership trees, preventing a move
from bypassing a protected child's ancestor restrictions. Handwritten traits must
implement the generated `visit_collaboration_fields` contract to use this helper.
Each encoding traversal uses the supplied byte bound; its maps are not a hard
process-allocation limit.

The standalone helper compares authorization only. Its inputs must already be
validated same-document candidates, including their identity, ownership, schema
and reference invariants. It does not replace those checks or establish a
document namespace from payload IDs. The native store and authority perform
their invariant/domain checks before invoking the helper through a policy hook.

## Enforce the store publication boundary

`model_store::set_authorization_policy` accepts an owner-thread callback receiving
an `authorization_operation`, immutable complete `before` and `candidate` values,
and a flag indicating replacement of the document namespace. False or exceptions
reject publication. Callbacks cannot reenter store mutation.
The supplied references are borrowed only for the synchronous callback duration;
do not retain them or derive mutable aliases. A candidate has not been accepted
and may be destroyed after denial or a later preparation failure. Use separately
owned copies when the host needs to retain inspection data.

```cpp
using store_type = rohit::managed::model_store<document>;
namespace managed = rohit::managed;
store_type store{document{}};
const auto root_id = store.read()->persistent_id;
auto policy = std::make_shared<const managed::authorization_policy>(
    std::vector<managed::authorization_rule>{
      {{root_id, {}}, managed::authorization_permission::content},
      {{root_id, {}}, managed::authorization_permission::structure}
    });
store.set_authorization_policy([policy](auto operation, const auto& before,
                                         const auto& candidate, bool replaced_namespace) {
  if (operation == managed::authorization_operation::replicate) {
    return true; // Bound transport/authority validates trusted accepted changes.
  }
  if (replaced_namespace || operation == managed::authorization_operation::runtime) {
    return false; // This example grants only durable edits in this document.
  }
  return managed::authorize_managed_candidate<store_type::traits_type>(
      *policy, before, candidate, managed::store_options::default_snapshot_bytes);
});
```

The gate runs before journal I/O or accepted-state/history swaps for durable
transactions (including editable application and independent import), Undo/Redo,
tree checkout, memory load, recovery, migration, discard and guarded runtime
updates. Denial preserves accepted values, pins, history cursors and journal
publication. Allocated/reserved identities remain burned under the existing
nonreuse contract. Private previews may be edited without live permissions; final
acceptance always passes the live gate.

Operations distinguish `edit`, `history`, `replace`, `runtime` and `replicate`.
Accepted-change refresh, initial trusted join and epoch reconnect use `replicate`;
loading a client checkpoint uses `replace`. Current permissions apply to Undo/Redo.
The durable comparison helper does not inspect transient fields; the application
must decide `runtime` permission separately.

The policy setter is a **trusted host administration API**. Optional legacy
policies can be cleared; a profile requiring authorization rejects clearing its
provider or downgrading authorization on the live store. Principal binding,
permission-policy persistence and administration remain
application-owned. The `policy` permission is available to explicit host checks;
ordinary schema fields are not automatically recognized as administrator policy
fields. Replacing policies requires the host's current administration gate.
There is no new schema syntax or persisted policy metadata. The application-owned
parts are explicitly **will not fix in Serializer**, as recorded in
[application responsibilities](../project/application_responsibilities.md#security-and-authorization).
They do not require a second policy database or authentication engine inside the runtime.

Use the same candidate helper inside the authority `change_policy` below,
choosing rules for the separately authenticated session. Local enforcement does
not replace authority enforcement.

## Bind requests to a trusted session

Applications create, authenticate, and retire sessions. A session ID is an opaque
identifier, not proof of identity or a user role. IDs may be unsigned integers,
strings, or supported custom types.

At the authority boundary, supply the session established by the authenticated
connection separately from the session claimed in the incoming record. Passing
the incoming record's own session as the trusted argument defeats that check.

The application also owns the transport and authority lifecycle. See
[application-owned sessions](collaboration_runtime.md#application-owned-sessions)
for supported types, reconnection, and session retirement.

## Set session-level permissions

`open_session(id, writable)` establishes whether a session may author accepted
changes. `set_session_writable` updates that gate. A read-only session can observe
state and publish advisory presence, but cannot publish edits or acquire locks.

This gate applies at the authority. A client may still edit its own local draft;
denied publication retains the draft and leaves the accepted model unchanged.
Reflect read-only status in the UI to avoid presenting an edit as publishable.

The [collaboration example](../../example/managed/collaboration/README.md#changes-conflicts-and-read-only-synchronization)
demonstrates a denied viewer edit and explicit draft cleanup.

## Check a proposed change

Supply a `change_policy(session, before, candidate, affected_ids)` callback when
the application needs finer control than a writable flag.

The authority calls the policy on the actual model difference inside its
acceptance boundary. The callback sees the prior state, proposed state, and
affected entity IDs. It can consult application-owned rules to decide whether
the entire candidate is allowed.

Returning false rejects the candidate. An exception also denies acceptance.
The hook does not partially apply an edit or automatically redact individual
fields; such rules belong in the application's policy logic.

Policy callbacks run on the authority's owner thread and must not reenter the
authority. Synchronize permission changes with that thread so acceptance uses
the intended current policy.

## Check locks and undo requests

`lock_policy(session, target, scope)` gates lease acquisition and renewal.
Releasing a lock remains possible after editing permission is revoked.

Presence is advisory. Cached lock records and presence indicators do not grant
permission to edit; the authority evaluates a submitted change and its grants.

Collaborative undo and redo are new requests. They pass current authorization
and lock checks as well as conflict checks. A previously accepted edit does not
give its author permanent permission to reverse it.

See [host authorization hooks](collaboration_runtime.md#host-authorization-hooks)
and [conditional collaborative undo](collaboration_runtime.md#conditional-collaborative-undo-and-redo)
for the full acceptance contract.

## Know the implementation boundary

User accounts, authentication, trusted principal/session binding, read filtering,
policy storage and application permission administration are application
responsibilities, with an explicit will-not-fix disposition. Install a policy
before exposing edit/import APIs and use `configure_capabilities` when enforcement
must be mandatory. Serializer enforces inherited content/structure rules and
the complete candidate at local and trusted-authority boundaries; the application
requests the separate `policy` permission before administering its own rules.

Opening an observer session does not provide filtered reads. If different users
may see different data, the application must define and enforce that boundary.

History, journals, and locks each have separate purposes. None supplies missing
authentication or permission rules. For their relationship, start with the
[managed features overview](getting_started.md).
