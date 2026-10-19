// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool owner_released;
static bool payload_released;

/* Keep validation active in optimized builds. */
static void require(bool condition) { if (!condition) { abort(); } }

/* Require successful native binding and codec operations. */
static void checked(srl_status status) { require(status == srl_ok); }

/* Compare complete ordinary bytes independently of JSON formatting assumptions. */
static void same_bytes(const srl_buffer* first, const srl_buffer* second) {
  require(first->size == second->size && (!first->size || !memcmp(first->data, second->data, first->size)));
}

/* Dispose of adopted source storage only after the final shared owner lease. */
static void release_source(void* pointer) {
  owner* source = (owner*)pointer;
  owner_free(source);
  free(source);
  owner_released = true;
}

/* Dispose of an independent payload after the final referring parent is released. */
static void release_payload(void* pointer) {
  payload* value = (payload*)pointer;
  payload_free(value);
  free(value);
  payload_released = true;
}

typedef struct projection_context {
  const owner* source;
  const srl_buffer* expected;
  srl_protocol protocol;
} projection_context;

/* The translator selects the original payload and writes its ordinary wire shape directly. */
static void inspect_projection(const owner_as_consumer_translator* view, void* raw) {
  const projection_context* context = (const projection_context*)raw;
  require(owner_as_consumer_translator_get_d(view) == &context->source->b);
  srl_buffer bytes = {0};
  checked(owner_as_consumer_translator_encode(view, context->protocol, &bytes));
  same_bytes(&bytes, context->expected);
  srl_buffer_free(&bytes);
}

/* Export source buffers through references and load values into ordinary owning targets. */
int main(void) {
  /* Unrelated parent records can retain the same independent payload lease. */
  payload* independent = (payload*)calloc(1, sizeof(*independent));
  require(independent != NULL);
  checked(payload_init(independent));
  srl_reference_scope* independent_scope = shared_owner_b_value_reference_create_scope(independent, release_payload);
  require(independent_scope != NULL);
  shared_owner first = {0};
  shared_value_consumer second = {0};
  checked(shared_owner_init(&first));
  checked(shared_value_consumer_init(&second));
  checked(shared_owner_b_value_reference_bind(&first.b, independent_scope));
  checked(shared_value_consumer_d_value_reference_bind(&second.d, independent_scope));
  const payload* first_value = NULL;
  const payload* second_value = NULL;
  checked(shared_owner_b_value_reference_read_value(&first.b, &first_value));
  checked(shared_value_consumer_d_value_reference_read_value(&second.d, &second_value));
  require(first_value == independent && second_value == independent);
  srl_reference_scope_release(independent_scope);
  shared_owner_free(&first);
  require(!payload_released && shared_value_consumer_d_value_reference_is_bound(&second.d));
  shared_value_consumer_free(&second);
  require(payload_released);

  owner* source = (owner*)calloc(1, sizeof(*source));
  require(source != NULL);
  checked(owner_init(source));
  const float values[] = {0, 1, 2};
  const uint32_t codes[] = {0, 1, 2};
  source->b.values.data = (float*)malloc(sizeof(values));
  source->b.codes.data = (uint32_t*)malloc(sizeof(codes));
  require(source->b.values.data != NULL && source->b.codes.data != NULL);
  memcpy(source->b.values.data, values, sizeof(values));
  memcpy(source->b.codes.data, codes, sizeof(codes));
  source->b.values.size = source->b.values.capacity = sizeof(values) / sizeof(values[0]);
  source->b.codes.size = source->b.codes.capacity = sizeof(codes) / sizeof(codes[0]);
  srl_reference_scope* scope = shared_consumer_d_value_reference_create_scope(source, release_source);
  require(scope != NULL);
  borrowed_consumer borrowed = {0};
  shared_consumer shared = {0};
  weak_consumer weak = {0};
  checked(borrowed_consumer_init(&borrowed));
  checked(shared_consumer_init(&shared));
  checked(weak_consumer_init(&weak));
  /* The basic borrow is just a const payload pointer; its source needs no reference machinery. */
  checked(borrowed_consumer_d_value_reference_bind(&borrowed.d, source));
  checked(shared_consumer_d_value_reference_bind(&shared.d, scope));
  checked(weak_consumer_d_value_reference_bind(&weak.d, scope));
  source->b.values.data[0] = 9;
  const payload* selected = NULL;
  checked(shared_consumer_d_value_reference_read_value(&shared.d, &selected));
  require(selected == &source->b && selected->values.data == source->b.values.data);

  /* Only this verification baseline explicitly copies the payload through its ordinary decoder. */
  owning_consumer expected = {0};
  checked(owning_consumer_init(&expected));
  srl_buffer payload_bytes = {0};
  checked(payload_encode(&source->b, srl_json, &payload_bytes));
  checked(payload_decode(&expected.d, payload_bytes.data, payload_bytes.size, srl_json, NULL));
  srl_buffer_free(&payload_bytes);
  for (int index = srl_json; index <= srl_binary_string; ++index) {
    const srl_protocol protocol = (srl_protocol)index;
    srl_buffer bytes = {0}, actual = {0};
    checked(owning_consumer_encode(&expected, protocol, &bytes));
    checked(borrowed_consumer_encode(&borrowed, protocol, &actual)); same_bytes(&bytes, &actual);
    checked(shared_consumer_encode(&shared, protocol, &actual)); same_bytes(&bytes, &actual);
    checked(weak_consumer_encode(&weak, protocol, &actual)); same_bytes(&bytes, &actual);
    projection_context context = {source, &bytes, protocol};
    checked(owner_with_as_consumer(source, inspect_projection, &context));
    /* Output-only references reject input; ordinary consumers own decoded buffers. */
    shared_consumer rejected = {0};
    checked(shared_consumer_init(&rejected));
    require(shared_consumer_decode(&rejected, bytes.data, bytes.size, protocol, NULL) != srl_ok);
    shared_consumer_free(&rejected);
    owning_consumer decoded = {0};
    checked(owning_consumer_init(&decoded));
    checked(owning_consumer_decode(&decoded, bytes.data, bytes.size, protocol, NULL));
    require(decoded.d.values.data != source->b.values.data && decoded.d.values.data[0] == 9);
    source->b.values.data[0] = 10;
    checked(owning_consumer_encode(&decoded, protocol, &actual)); same_bytes(&bytes, &actual);
    source->b.values.data[0] = 9;
    if (protocol == srl_json) { fwrite(bytes.data, 1, bytes.size, stdout); putchar('\n'); }
    owning_consumer_free(&decoded);
    srl_buffer_free(&actual);
    srl_buffer_free(&bytes);
  }
  owning_consumer_free(&expected);
  srl_reference_scope_release(scope);
  require(!owner_released && shared_consumer_d_value_reference_is_bound(&shared.d));
  borrowed_consumer_free(&borrowed);
  shared_consumer_free(&shared);
  require(owner_released && !weak_consumer_d_value_reference_is_bound(&weak.d));
  srl_buffer expired = {0};
  require(weak_consumer_encode(&weak, srl_json, &expired) != srl_ok);
  srl_buffer_free(&expired);
  weak_consumer_free(&weak);
  return 0;
}
