# Database storage and document sinks

Status: integration guidance and proposed adapter design. Serializer implements
JSON and binary codecs, but this repository does not implement database clients,
document sinks, query generation, or database migrations. The databases below are
candidate storage targets based on their documented data models, not a list of
tested Serializer integrations. Provider documentation was reviewed on 2026-09-28;
check the selected server edition, version, and SDK before implementation.

Serializer generates application types in C++, Java, JavaScript/TypeScript, Go,
C#, Rust, Python, Swift, Kotlin, and C. An application can pass their encoded
output to a database client available for its language and platform. Support for
a generated language does not imply availability of every provider's SDK there.
Start with the [usage guide](usage.md) and preserve the
[wire-format contract](wire_format.md).

## Choose a storage representation

| Representation | Application integration | Query behavior |
| --- | --- | --- |
| JSON document or SQL JSON column | Encode Serializer JSON, validate provider restrictions, and submit through a driver or HTTP API. Reverse any storage mapping on reads. | The database can query supported JSON fields, subject to indexes and its query model. |
| Typed document | Map Serializer values to BSON or the provider's typed values, preserving integer widths and collection shapes. | Native fields can be queried; database-specific types need an explicit mapping. |
| Opaque payload | Store the exact JSON text or binary message in a string/blob/byte value. Fetch it before decoding. | Payload internals are opaque; maintain separate indexed metadata when needed. |

SQL databases with JSON columns and NoSQL document databases can both use these
patterns. A document sink does not translate arbitrary SQL into NoSQL queries.
Serializer also does not currently flatten an object into relational tables.

For example, an application might store an external record ID, a schema revision,
and a JSON payload together. The external ID and revision belong to its storage
contract; `stable_ids` identifies schema fields, not database records.

## Document databases and services

Every row requires application integration or a future adapter. Links lead to the
provider's storage contract; the proposed mapping is Serializer integration advice.

| Database | Candidate mapping | Requirements and limitations |
| --- | --- | --- |
| MongoDB | Generated object to BSON document; alternatively an opaque payload field. | BSON is not Serializer binary. Select integer, binary, date, and ObjectId mappings explicitly; keep `_id` separate from the payload contract. See [BSON types](https://www.mongodb.com/docs/manual/reference/bson-types/). |
| Firebase Cloud Firestore | Generated object to typed document fields, with nested objects as maps. | Native integers are signed 64-bit. Standard edition forbids arrays directly inside arrays; the documented Enterprise behavior differs. Pin the edition/API and validate its types and limits. See [Firestore data types](https://firebase.google.com/docs/firestore/manage-data/data-types). |
| Firebase Realtime Database | JSON subtree at an application-selected path. | Treat it as a JSON tree with path/key restrictions, not a collection API. Define replacement versus patch behavior and test null and empty-container round trips. See [data structure](https://firebase.google.com/docs/database/web/structure-data) and [REST writes](https://firebase.google.com/docs/database/rest/save-data). |
| Couchbase | JSON document keyed by the application's record ID. | Nested objects and arrays fit its document model; select indexed fields from the actual Serializer JSON shape. See [document data model](https://docs.couchbase.com/server/current/learn/data/document-data-model.html). |
| Apache CouchDB | JSON payload within a document envelope. | Keep `_id` and `_rev` metadata outside the decoded payload. Updates need revision/conflict handling. See [document API](https://docs.couchdb.org/en/stable/api/document/common.html). |
| Azure Cosmos DB for NoSQL | JSON item with an application ID and partition-key mapping. | Choose document boundaries and partitioning for the workload. Other Cosmos DB APIs need their own adapter contract. See [data modeling](https://learn.microsoft.com/en-us/azure/cosmos-db/modeling-data). |
| Amazon DynamoDB | Typed map/list/scalar attributes, or a binary/string payload attribute. | The low-level attribute representation is not ordinary Serializer JSON. Preserve numeric tokens: DynamoDB numbers support up to 38 digits and travel as strings in its API. Define partition/sort keys separately. See [data types](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/HowItWorks.NamingRulesDataTypes.html). |

“Firebase” must resolve to a specific database product. Similarly, a service that
offers a MongoDB-compatible API still needs qualification against that service;
protocol compatibility alone does not establish identical types, transactions,
limits, or error behavior.

## SQL databases with JSON storage

Bind the encoded JSON as a parameter through the database driver. Use separate
columns for primary keys, concurrency tokens, and frequently queried metadata
where useful. Keep table/column selection in trusted application configuration;
do not concatenate payloads into SQL statements.

| Database | Candidate mapping | Requirements and limitations |
| --- | --- | --- |
| PostgreSQL | `jsonb` for queryable documents; `json` for preserving input JSON text. | `jsonb` normalizes object representation and rejects `\u0000`; its numeric and encoding constraints apply. Neither is Serializer binary. See [JSON types](https://www.postgresql.org/docs/current/datatype-json.html). |
| MySQL | Native `JSON` column. | The server validates and converts JSON into its internal representation. Use driver parameters and verify number handling across the client/server round trip. See [JSON data type](https://dev.mysql.com/doc/refman/8.4/en/json.html). |
| MariaDB | `JSON` column, implemented as a `LONGTEXT` alias with JSON validation. | Do not assume MySQL's internal binary JSON representation or identical behavior. See [JSON data type](https://mariadb.com/docs/server/reference/data-types/string-data-types/json). |
| SQLite | JSON text in `TEXT`; optionally use SQLite functions to produce SQLite JSONB. | JSONB is SQLite's own format, not PostgreSQL JSONB or a Serializer binary protocol. Check the SQLite library's available JSON functions. See [JSON functions](https://sqlite.org/json1.html). |
| SQL Server / Azure SQL | JSON text in `nvarchar`, or native `json` where the deployment supports it. | Choose the storage type for the deployed version/service; do not assume native `json` exists on older SQL Server releases. See [JSON storage](https://learn.microsoft.com/en-us/sql/relational-databases/json/store-json-documents-in-sql-tables?view=sql-server-ver17) and [native JSON type availability](https://learn.microsoft.com/en-us/sql/t-sql/data-types/json-data-type). |
| CockroachDB | `JSONB` column alongside relational keys. | Define indexes and validate its supported value representation; sharing a type name does not establish PostgreSQL behavioral equivalence. See [JSONB](https://www.cockroachlabs.com/docs/stable/jsonb). |

The following is an illustrative PostgreSQL storage layout, not generated SQL or
an executed example:

```sql
CREATE TABLE serializer_documents (
  document_id text PRIMARY KEY,
  schema_revision integer NOT NULL,
  revision bigint NOT NULL,
  payload jsonb NOT NULL
);

INSERT INTO serializer_documents
  (document_id, schema_revision, revision, payload)
VALUES ($1, $2, $3, $4::jsonb);

UPDATE serializer_documents
SET payload = $1::jsonb, revision = revision + 1
WHERE document_id = $2 AND revision = $3
RETURNING revision;
```

The application binds the parameters. No row returned by the update means the
record was absent or the expected revision did not match; it is not a successful
replacement. Schema revision and concurrency revision have different purposes.
This example is PostgreSQL-specific and is not portable SQL for the whole table.

## Key-value, embedded, and search storage

| Database | Candidate mapping | Requirements and limitations |
| --- | --- | --- |
| Redis | Exact JSON/binary payload in a Redis string, or a JSON value where JSON commands are available. | Choose retention, eviction, and persistence policies for the application's requirements. A string payload does not expose its fields as JSON. See [strings](https://redis.io/docs/latest/develop/data-types/strings/) and [JSON](https://redis.io/docs/latest/develop/data-types/json/). |
| RocksDB | Record key to encoded JSON or binary bytes. | Suitable for an embedded key-value adapter. Secondary indexes, document queries, and schema metadata remain application responsibilities. See [basic operations](https://github.com/facebook/rocksdb/wiki/Basic-Operations). |
| Apache Cassandra | Payload in a `text`/`blob` column, or an explicitly designed mapping to CQL columns. | `INSERT JSON` and `SELECT JSON` operate on the table's schema; they do not turn Cassandra into an arbitrary document store. See [CQL JSON support](https://cassandra.apache.org/doc/latest/cassandra/developing/cql/json.html). |
| Elasticsearch | JSON search projection with explicit field mappings. | Prefer a rebuildable projection when another database owns the record. Verify indexed types and array/object query semantics rather than assuming every Serializer shape is directly searchable. See [mapping](https://www.elastic.co/docs/manage-data/data-store/mapping). |

Other stores accepting byte values can also retain Serializer messages without
understanding them. For example, PostgreSQL offers
[`bytea`](https://www.postgresql.org/docs/current/datatype-binary.html).
Byte storage alone provides no field queries or schema compatibility checks.
Record the protocol, schema revision, and any compression choice alongside the
payload; native Serializer messages do not supply automatic protocol detection.
Base64 is an explicit transport mapping when an API accepts only text, with
additional size overhead. Compressed messages likewise need opaque storage.

## Preserve the Serializer value contract

Database acceptance of valid JSON is insufficient to guarantee a lossless round
trip. Establish these policies before calling a storage integration supported:

- **Numbers:** preserve full signed/unsigned widths through every SDK conversion.
  An intermediate JavaScript `Number` cannot exactly retain all integers above
  `2^53 - 1`; native signed 64-bit fields cannot hold all `uint64` values. Use a
  documented reversible representation or reject unsupported values before the
  write. A decimal string mapping must be reversed before ordinary numeric decode.
- **Maps:** Serializer JSON represents maps as arrays of `{"key": ..., "value": ...}`
  entries. Keep this shape by default, including non-string keys. Converting it
  into an object changes the storage contract and requires a reversible mapping.
- **Arrays and strings:** test nested/empty collections, Unicode, embedded NULs,
  and provider field/path restrictions. In particular, a Serializer JSON string
  containing `\u0000` cannot be stored unchanged as PostgreSQL `jsonb`. Provider
  restrictions must produce a clear rejection or an explicit mapping.
- **Presence:** distinguish a missing record, missing field, null, empty object,
  and empty collection. A provider patch or deletion operation is not a Serializer
  decode operation. Ordinary keyed decoding retains destination values for absent
  fields; load into a fresh object when schema defaults are intended.
- **Special types:** dates, UUIDs, ObjectIds, decimal types, references, and native
  byte fields need agreed schema representations. JSON `array uint8` remains an
  array of numbers unless an adapter explicitly maps it to a native byte value.
  Native JSON output rejects non-finite floating-point values even if a provider
  can store them.
- **Metadata:** keep database IDs, revisions, partition keys, and timestamps in
  columns or an envelope outside the payload. Extract only the agreed payload on
  reads. Do not feed provider-added fields or Extended JSON wrappers directly to
  the ordinary Serializer decoder and assume they have the same meaning.
- **Normalization:** compare decoded values for semantic storage. Keep exact bytes
  in opaque storage when signatures, hashes, or byte-for-byte reproduction matter.
  Do not rely on JSON document stores to retain whitespace or object-key order.
- **Evolution:** preserve wire names and IDs, track storage/schema revisions, and
  plan migrations with the [compatibility and migration guide](../migration.md#optional-compatibility-checks-and-exact-decoding).
  Database flexibility does not make every old/new Serializer schema compatible.

For C++ reads, fetch one complete bounded payload, reverse the storage mapping,
and use [`deserialize_exact<Value, Protocol>`](usage.md#decode-one-exact-message-into-a-fresh-value)
with explicit limits. Keep the current application value until decoding succeeds.
Other generated runtimes need their documented complete-message APIs and limits;
the C++ helper name is not a cross-language database interface.

## Proposed document sink boundary

This section proposes a future design; the names below are operation descriptions,
not implemented C++ functions, generator switches, or schema syntax.

```text
generated value -> codec / reversible document mapping -> validated payload
                                                        |
                                              provider adapter
                                                        |
                                                     database
```

The adapter should accept a caller-owned client/session and an explicit target,
record key, and payload. Keep credentials, connections, transaction ownership,
and provider dependencies in the host application or optional adapter package.
One write should stage and validate the complete payload before any remote change.
Bound encoded size as well as decoded size; a database may count envelope and
index overhead differently from Serializer's payload bytes.

Start with distinct insert, replace, upsert, and delete operations. Pair the sink
with a read adapter for round-trip verification. Specify existence requirements,
expected-revision conditions, durability/acknowledgment policy, and whether the
operation participates in a caller transaction. Expose backend capabilities;
do not promise the same transaction scope across all providers.

A serialization failure must send no write. A timeout after sending a request can
mean the write committed but its acknowledgment was lost. Report that uncertainty;
retry only under an explicit idempotency policy. Batches need per-item outcomes
unless the backend actually guarantees atomic execution. Partial updates, query
translation, change streams, and automatic synchronization are separate features.
This proposal does not implement the [managed-state designs](managed/README.md).

## Suggested implementation order

This is an engineering recommendation based on the mappings above, not a benchmark
or a declaration of existing support:

1. **SQLite:** establish local JSON and opaque-binary round trips with a small
   optional dependency and reproducible integration tests.
2. **PostgreSQL:** exercise queryable JSON, parameter binding, revision-checked
   replacement, and a binary alternative under a server-backed integration test.
3. **MongoDB:** establish typed BSON mapping and native document operations.
4. **Firestore or Realtime Database:** select the actual Firebase product and
   edition, then implement its types, identity, and write semantics explicitly.
5. **Additional adapters:** add Couchbase, CouchDB, Cosmos DB, DynamoDB, or other
   backends according to a consuming application's needs.

## Verification required before claiming support

Qualify each backend, server edition/version, SDK, and supported language
combination independently. At minimum, cover:

- Generated records with nested objects, enums, union alternatives, maps, arrays,
  schema defaults, Unicode, and integer/floating-point boundaries.
- Empty values, missing/null distinctions, unsupported values, size/depth limits,
  invalid input, trailing input, and provider-added metadata on reads.
- Insert conflicts, absent records, conditional-write conflicts, replacements,
  explicit deletion, transaction rollback where supported, and timeout/retry outcomes.
- Old/new schema reads and the distinction between semantic JSON round trips and
  exact opaque-byte preservation.

Use isolated test databases or supported emulators and report their versions.
Mock tests can validate adapter contracts but cannot prove provider behavior.
For this documentation change, provider references and repository codec contracts
were reviewed; no database adapter, live database test, or SQL example execution
has been completed.
