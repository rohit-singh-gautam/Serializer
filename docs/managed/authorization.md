# Authorization in managed collaboration

[Managed features overview](getting_started.md) · [Project overview](../../README.md)

The implemented C++ collaboration authority validates who is submitting an edit
and lets the application decide whether to accept it. Serializer supplies session
gates and policy callbacks; the application supplies its permission rules.

This guide describes the current implementation. The broader
[authorization design](managed_state.md#authorization-on-the-design-hierarchy)
includes features that are still proposals.

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

The runtime does not implement these broader policy features:

- User accounts, authentication, or a role-management service.
- Inherited allow/deny policies or automatic field-path permissions.
- Built-in operation classification, read filtering, or policy persistence.
- An authorization gate on every ordinary local `model_store` transaction.

Opening an observer session does not provide filtered reads. If different users
may see different data, the application must define and enforce that boundary.

History, journals, and locks each have separate purposes. None supplies missing
authentication or permission rules. For their relationship, start with the
[managed features overview](getting_started.md).
