import {CheckVisibility, InteropMessage, InteropState, Protocol, Limits} from './schema.mjs';

// Type-check the actual generated declarations against native JS calling conventions.
const value = new InteropMessage();
value.bigUnsigned = 18446744073709551615n;
value.states = [InteropState.Ready];
value.counts.set('visits', 7n);
const encoded: Uint8Array = value.encode(Protocol.BINARY_INTEGER);
const decoded: InteropMessage = InteropMessage.decode(encoded, Protocol.BINARY_INTEGER, new Limits());
const exact: bigint = decoded.bigUnsigned;
void exact;
// @ts-expect-error A 64-bit integer must not silently become a JavaScript number.
value.bigUnsigned = 42;
// @ts-expect-error Protocol values are checked at the TypeScript boundary.
value.encode(42);

// @ts-expect-error Private schema fields are not public JavaScript properties.
new CheckVisibility().hidden;
