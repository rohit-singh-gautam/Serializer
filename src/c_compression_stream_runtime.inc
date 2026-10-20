// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"serializer(
/* Borrowed callbacks return a status and an actual byte count. A zero read means
   EOF; writes must consume the complete supplied chunk. No close/flush callback
   is owned. Payload pointers are borrowed only until the callback returns. */
typedef @c@_code (*@c@_source)(void*, uint8_t*, size_t, size_t*);
typedef @c@_code (*@c@_sink)(void*, const uint8_t*, size_t, size_t*);
typedef struct @c@_output {
  @c@_encode_options options;
  @c@_sink sink;
  void* sink_context;
  srl_buffer staged;
  @c@_code failure;
  bool busy, finished, closed, initialized;
} @c@_output;
typedef struct @c@_input {
  @c@_decode_options options;
  srl_buffer decoded;
  size_t position;
  @c@_code failure;
  bool busy, finished, closed, initialized;
} @c@_input;

/* Latch the first failure; a later invalid operation never replaces its category. */
static inline @c@_code @c@_first(@c@_code* failure, @c@_code observed) {
  if (*failure == @c@_ok) { *failure = observed; }
  return *failure;
}
/* Initialize zeroed storage once, borrowing a sink without writing a header. */
static inline @c@_code @c@_output_init(@c@_output* output, @c@_sink sink,
                                       void* context, @c@_encode_options options) {
  if (!output || !sink) { return @c@_invalid_options; }
  if (output->busy || output->initialized || output->closed) { return @c@_first(&output->failure, @c@_invalid_state); }
  @c@_code status = @c@_admit_encode(options);
  if (status != @c@_ok) { return status; }
  memset(output, 0, sizeof(*output));
  output->sink = sink; output->sink_context = context; output->options = options; output->initialized = true;
  return @c@_ok;
}
/* Check before/after every policy callback, including a swallowed nested-operation failure. */
static inline bool @c@_output_cancel(void* context) {
  @c@_output* output = (@c@_output*)context;
  if (output->failure != @c@_ok || output->closed) { return true; }
  const bool cancelled = output->options.is_cancelled &&
      output->options.is_cancelled(output->options.context);
  if (cancelled) { @c@_first(&output->failure, @c@_cancelled); }
  return output->failure != @c@_ok || output->closed;
}
/* Release deferred storage only after an active callback/provider has stopped borrowing it. */
static inline @c@_code @c@_output_leave(@c@_output* output, @c@_code status) {
  output->busy = false;
  @c@_first(&output->failure, status);
  if (output->closed) { srl_buffer_free(&output->staged); }
  return output->failure;
}
/* Stage complete serialized bytes with bounded cancellation and no borrowed sink side effects. */
static inline @c@_code @c@_output_write(@c@_output* output, const uint8_t* bytes, size_t size) {
  if (!output) { return @c@_invalid_options; }
  if (!output->initialized || output->busy || output->finished || output->closed || output->failure != @c@_ok) {
    return @c@_first(&output->failure, @c@_invalid_state);
  }
  if (size && !bytes) { return @c@_first(&output->failure, @c@_invalid_options); }
  output->busy = true;
  if (@c@_output_cancel(output)) { return @c@_output_leave(output, output->failure); }
  @c@_code status = @c@_ok;
  if (output->staged.size > output->options.max_input_bytes ||
      size > output->options.max_input_bytes-output->staged.size) { status = @c@_resource_limit; }
  for (size_t offset = 0; status == @c@_ok && offset < size;) {
    if (@c@_output_cancel(output)) { break; }
    const size_t remaining = size-offset;
    const size_t count = remaining < @c@_batch_bytes ? remaining : @c@_batch_bytes;
    status = @c@_append(&output->staged, bytes+offset, count, output->options.max_input_bytes);
    offset += count;
  }
  if (status == @c@_ok) { (void)@c@_output_cancel(output); }
  return @c@_output_leave(output, status);
}
/* Explicitly finalize one frame; a failure may leave an external prefix but prevents retries. */
static inline @c@_code @c@_output_finish(@c@_output* output) {
  if (!output) { return @c@_invalid_options; }
  if (!output->initialized || output->busy || output->finished || output->closed || output->failure != @c@_ok) {
    return @c@_first(&output->failure, @c@_invalid_state);
  }
  output->busy = true;
  srl_buffer frame = {0};
  @c@_encode_options policy = output->options;
  policy.is_cancelled = @c@_output_cancel; policy.context = output;
  @c@_code status = @c@_compress(output->staged.data, output->staged.size, policy, &frame);
  for (size_t offset = 0; status == @c@_ok && offset < frame.size;) {
    if (@c@_output_cancel(output)) { break; }
    const size_t remaining = frame.size-offset;
    const size_t count = remaining < @c@_batch_bytes ? remaining : @c@_batch_bytes;
    size_t written = 0;
    status = output->sink(output->sink_context, frame.data+offset, count, &written);
    @c@_first(&output->failure, status);
    if (output->failure != @c@_ok) { break; }
    if (written != count) { status = @c@_transport; break; }
    offset += count;
    if (@c@_output_cancel(output)) { break; }
  }
  if (status == @c@_ok) { (void)@c@_output_cancel(output); }
  srl_buffer_free(&frame);
  output->finished = status == @c@_ok && output->failure == @c@_ok;
  return @c@_output_leave(output, status);
}
/* Abandon without a trailer, flushing or closing the borrowed sink; safe during a callback. */
static inline void @c@_output_close(@c@_output* output) {
  if (!output) { return; }
  output->closed = true;
  if (output->busy) { @c@_first(&output->failure, @c@_invalid_state); }
  else { srl_buffer_free(&output->staged); }
}
/* Retain input cancellation and lifecycle state through source staging and provider work. */
static inline bool @c@_input_cancel(void* context) {
  @c@_input* input = (@c@_input*)context;
  if (input->failure != @c@_ok || input->closed) { return true; }
  const bool cancelled = input->options.is_cancelled && input->options.is_cancelled(input->options.context);
  if (cancelled) { @c@_first(&input->failure, @c@_cancelled); }
  return input->failure != @c@_ok || input->closed;
}
/* Leave one active input operation and honor deferred closure after all borrowed calls end. */
static inline @c@_code @c@_input_leave(@c@_input* input, @c@_code status) {
  input->busy = false;
  @c@_first(&input->failure, status);
  if (input->closed) { srl_buffer_free(&input->decoded); }
  return input->failure;
}
/* Read to real EOF with a one-byte over-limit probe, then validate exactly one complete frame.
   Initialize zeroed storage once; close before reinitializing. Source remains caller-owned. */
static inline @c@_code @c@_input_init(@c@_input* input, @c@_source source,
                                      void* context, @c@_decode_options options) {
  if (!input || !source) { return @c@_invalid_options; }
  if (input->busy || input->initialized || input->closed) { return @c@_first(&input->failure, @c@_invalid_state); }
  @c@_code status = @c@_admit_decode(options);
  if (status != @c@_ok) { return status; }
  memset(input, 0, sizeof(*input)); input->options = options; input->busy = true; input->initialized = true;
  srl_buffer staged = {0}; uint8_t chunk[@c@_batch_bytes];
  for (;;) {
    if (@c@_input_cancel(input)) { break; }
    const size_t remaining = options.max_input_bytes-staged.size;
    const size_t requested = remaining < sizeof(chunk) ? remaining+1 : sizeof(chunk);
    size_t count = 0;
    status = source(context, chunk, requested, &count);
    @c@_first(&input->failure, status);
    if (input->failure != @c@_ok) { break; }
    if (@c@_input_cancel(input)) { break; }
    if (count > requested) { status = @c@_transport; break; }
    if (!count) { break; }
    status = @c@_append(&staged, chunk, count, options.max_input_bytes);
    if (status != @c@_ok) { break; }
  }
  if (status == @c@_ok && input->failure == @c@_ok) {
    @c@_decode_options policy = options;
    policy.is_cancelled = @c@_input_cancel; policy.context = input;
    status = @c@_decompress(staged.data, staged.size, policy, &input->decoded);
  }
  srl_buffer_free(&staged);
  return @c@_input_leave(input, status);
}
/* Borrow an exact decoded prefix; input and destination spans must not overlap. */
static inline @c@_code @c@_input_read(@c@_input* input, uint8_t* bytes, size_t capacity, size_t* count) {
  if (!input) { return @c@_invalid_options; }
  if (count) { *count = 0; }
  if (!input->initialized || input->busy || input->closed || input->finished || input->failure != @c@_ok) {
    return @c@_first(&input->failure, @c@_invalid_state);
  }
  if (!count || (capacity && !bytes)) { return @c@_first(&input->failure, @c@_invalid_options); }
  input->busy = true;
  if (!@c@_input_cancel(input)) {
    const size_t remaining = input->decoded.size-input->position;
    const size_t available = remaining < capacity ? remaining : capacity;
    for (size_t offset = 0; offset < available;) {
      if (@c@_input_cancel(input)) { break; }
      const size_t tail = available-offset;
      const size_t batch = tail < @c@_batch_bytes ? tail : @c@_batch_bytes;
      memcpy(bytes+offset, input->decoded.data+input->position, batch);
      input->position += batch; offset += batch; *count += batch;
    }
    (void)@c@_input_cancel(input);
  }
  return @c@_input_leave(input, @c@_ok);
}
/* Require complete native consumption before accepting the expanded message. */
static inline @c@_code @c@_input_finish(@c@_input* input) {
  if (!input) { return @c@_invalid_options; }
  if (!input->initialized || input->busy || input->closed || input->finished || input->failure != @c@_ok) {
    return @c@_first(&input->failure, @c@_invalid_state);
  }
  input->busy = true;
  @c@_code status = @c@_ok;
  if (!@c@_input_cancel(input) && input->position != input->decoded.size) { status = @c@_invalid_data; }
  input->finished = status == @c@_ok && input->failure == @c@_ok;
  return @c@_input_leave(input, status);
}
/* Release owned expanded bytes without closing/rewinding the borrowed source. */
static inline void @c@_input_close(@c@_input* input) {
  if (!input) { return; }
  input->closed = true;
  if (input->busy) { @c@_first(&input->failure, @c@_invalid_state); }
  else { srl_buffer_free(&input->decoded); }
}
)serializer"
