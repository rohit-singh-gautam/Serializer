# Collaboration examples

These five C++20 programs use the real managed ledger schema, generated editors,
automatic proposal encoding, and the implemented
[collaboration runtime](../../../docs/managed/collaboration_runtime.md).
Start with [local_sync.cpp](local_sync.cpp): a session attaches to the existing
`model_store`, and normal transactions update the local document immediately.
The store journals each commit and queues it for separate synchronization:

```cpp
rohit::managed::model_store<ledger_example::ledger> store{ledger_example::ledger{}};
store.collaborate(std::uint64_t{10}).bind(connection);
store.synchronize(); // Join first; connection supplies the trusted authority.
store.execute_transaction([](auto& edit) {
  edit.root().set_name("Office expenses");
}).throw_if_failed();
store.synchronize(); // Or drive synchronize_if_due(now_ms) from the owner event loop.
```

`send_pending()` sends one transaction; `receive_changes()` separately incorporates
accepted work. Every client both sends and receives. The journal persists per commit;
the network interval is configurable from seconds to minutes. Client recovery retains
the session, local undo/redo and exact outstanding request. See the
[integration guide](../../../docs/managed/local_collaboration.md) for conflict
resolution, session policies, ID mappings, transport and lifetime requirements.

Expected `local_sync` output:

```text
Local: Office expenses; server: Expenses
Synchronized: Office expenses, amount = 2000
Local undo: Expenses
Recovered redo: Office expenses
```

`main.cpp`, `locks.cpp`, and `undo.cpp` also use `model_store` for document edits.
The `sessions.cpp` example separately illustrates the lower-level protocol with
application-defined session ID types.

`propose` in that lower-level API constructs a request from an isolated draft;
it does not commit an edit to a client store. Delivery means submitting a request
to the authority and returning accepted changes to clients. A `deliver` helper
would perform those transport steps; it is not a public Serializer API. Store applications
call `synchronize()` to handle these steps through their bound transport.

Build from a configured repository build:

```sh
cmake -S . -B out/collaboration -DSERIALIZER_BUILD_MANAGED=ON -DSERIALIZER_BUILD_TESTS=OFF
cmake --build out/collaboration --config Release --target managed_collaboration_local_sync managed_collaboration_changes managed_collaboration_locks managed_collaboration_sessions managed_collaboration_undo
```

Run the executables in `out/collaboration/example/managed/collaboration` (add
`Release/` for a multi-config generator and `.exe` on Windows). GoogleTest is not
needed for these examples. With tests enabled they are also registered in CTest.
For installed-package use, configure this example directory against an installed
Serializer built with managed support.

## Conditional collaborative undo and redo

`undo.cpp` creates separate Alice and Bob stores, attaches their sessions, joins
the shared baseline, and calls `create_journal` on each store. All document edits
use `execute_transaction`, `undo`, or `redo`. Each operation commits locally;
`synchronize()` exchanges pending work with the authority.

Alice renames the ledger and Bob changes its amount. Alice's undo/redo preserves
Bob's amount. Later, Bob changes the same name while Alice has not yet received it.
Alice's next undo succeeds locally, but synchronization detects the conflict and
retains her local inverse without overwriting Bob's accepted name. Her
`collaboration().acknowledged_read()` exposes accepted state separately. Receiving
Bob's conflicting edit before calling `undo()` would make undo throw immediately.

```text
Undo: Expenses, amount = 2000
Redo: Office expenses
Conflicting undo kept local: Expenses; server: Bob's name
Recorded transactions: 5
```

Both clients use the ordinary store journal API for edits, undo and redo. Internally,
the journal adapter records model operations together with session/outbox metadata
in one frame using the existing file, flush, CRC and recovery mechanisms. The extra
payload lets recovery restore both the document and its synchronization position.
There is no application-owned store wrapper or second journal to manage.
`save_journal()` preserves pending work, including unresolved conflicts. See
[journal recovery](../../../docs/managed/local_collaboration.md#undo-identities-and-recovery)
and `local_sync.cpp` for recovery into a fresh attached store.

Both peers need history-bearing protocols 5/6 or 7/8 and regenerated managed headers.
Authority history lasts for its epoch; its document journal persists undo's result
but does not restore collaborative stacks on restart. See the
[complete contract](../../../docs/managed/collaboration_runtime.md#conditional-collaborative-undo-and-redo).

## Changes, conflicts, and read-only synchronization

[main.cpp](main.cpp) names three logical participants:

| Constant | ID | Client store |
| --- | --- | --- |
| `local_session` | 10 | `local`, an editing client |
| `remote_session` | 20 | `remote`, another editing client |
| `viewer_session` | 30 | `viewer`, with read-only authority permission |

Local/remote describe client perspectives; a session ID encodes no location or role.
Each client owns a `model_store` and binds its session to an in-process transport.
The authority represents the server and enforces read-only permission through
`open_session(viewer_session, false)`.

Two stores edit the same name before synchronization. The local transaction also
inserts an entry. Local synchronization accepts both changes atomically; another
poll with an empty queue leaves the sequence and identity unchanged. Remote
synchronization detects the competing name change. Disjoint fields would merge
instead. The example explicitly archives the remote draft with `discard_pending()`,
adopts accepted state, and reapplies the desired name in a new store transaction.
All three stores then synchronize to the resolved document.

The viewer can make a local edit, but the authority denies its publication. The
example verifies both the retained local draft and unchanged accepted state, then
explicitly discards the denied draft. Applications should also disable editing in
a read-only UI; server permission checks remain necessary.

```text
Local session: 10, remote session: 20, read-only viewer: 30
Accepted sequence: 2
Store: Renamed ledger, Supplies = 1500
Repeated sync preserved IDs; same-field conflict resolved; read-only edit denied
```

This example tests repeated polling, not a lost transport reply. Exact outstanding
request retries and journal recovery after lost replies are covered by
`test/managed_local_collaboration_test.cpp`. Applications call `synchronize()` again
after reconnect; they do not rerun the transaction callback.

## Presence, subtree leases, and fencing

[locks.cpp](locks.cpp) attaches two stores and advertises the holder's editing
presence. Host coordination acquires a root subtree lease for session 10. The
competing store commits an amount locally, but synchronization is blocked by that
lease and leaves the authority unchanged. The holder supplies its grant through
`holder.collaboration().set_grants(...)`, then changes the amount with
`execute_transaction` and `synchronize`.

Lock requests use authority IDs obtained through `session.remote_id(local_id)`.
Reserve each new lock operation with `session.reserve_operation_id()` after joining:
locks and document submissions share the same session operation counter. Reservation
is journaled when a journal is attached; without one it remains in memory. The host
retains lock request payloads and uses the same reserved ID for exact retries.
Presence and lease operations are coordination APIs, not document modifications.

Advancing the authority clock expires the lease. Renewing the old generation is
rejected, refreshing the lock cache observes no grants, and closing the session
clears its presence.

```text
Subtree lock blocked competing edit; holder changed amount to 1800
Expiry fenced old grant; snapshot refreshed cache; session end cleared presence
```

These examples run synchronously in one process. A real host supplies authenticated
connections, monotonic time, transport and authority fencing; no network service is
started. The epoch and session numbers are deterministic teaching inputs. The
runtime offers host policy hooks, while inherited subtree permissions and
field-level authorization remain unimplemented. See the
[policy limitations](../../../docs/managed/collaboration_runtime.md#host-authorization-hooks).

## Application-owned session types

[sessions.cpp](sessions.cpp) chooses session IDs in application code and sends the
complete values through the real codecs. It runs the same flow with strings and
`uint32_t`; the other examples retain the `uint64_t` default:

```cpp
using sessions = rohit::managed::collaboration_session<std::string>;
sessions::authority<ledger_example::ledger> authority{ledger_example::ledger{"Shared"}, 1};
authority.open_session("desktop-session");
sessions::replica<ledger_example::ledger> replica;
replica.synchronize(authority.snapshot(), authority.context());
auto proposal = replica.propose("desktop-session", 1, [](auto& edit) {
  edit.root().set_name("Updated");
});
```

Expected output from `managed_collaboration_sessions`:

```text
Local session: desktop-session, remote session: mobile-session, accepted origin: desktop-session
Local session: 100, remote session: 200, accepted origin: 100
```

The application owns ID creation, storage, transport binding, reconnect decisions,
and when to call `close_session`. Serializer imposes no session registry or numeric
aliases. A custom value type can supply validation, ordering, and bounded encoding
through a policy; see [application-owned sessions](../../../docs/managed/collaboration_runtime.md#application-owned-sessions).
