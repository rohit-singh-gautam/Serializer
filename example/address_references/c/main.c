// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include "schema.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static bool owner_released;

/* Keep example checks active in optimized builds. */
static void require(bool condition, const char* message) {
  if (!condition) { fprintf(stderr, "%s\n", message); exit(EXIT_FAILURE); }
}

/* Stop on codec or binding failure without relying on assertions for side effects. */
static void require_status(srl_status status) {
  require(status == srl_ok, "Reference example operation failed");
}

/* The typed scope adopts this stable allocation and destroys it after its final strong lease. */
static void release_owner(void* pointer) {
  owner* storage = (owner*)pointer;
  owner_free(storage);
  free(storage);
  owner_released = true;
}

/* Encode the target schema directly; the readonly translator itself remains scoped. */
static void encode_projection(const live_value_to_wire_translator* projected, void* context) {
  require_status(live_value_to_wire_translator_encode(projected, srl_json, (srl_buffer*)context));
}

/* Borrow the current owner.b storage and translate it during the same synchronous operation. */
static void project_value(const live_value* selected, void* context) {
  require(srl_string_equal(&selected->text, "updated"), "Reference returned a stale field");
  require_status(live_value_with_to_wire(selected, encode_projection, context));
}

/* Demonstrate typed ownership leases, explicit post-load resolution and projected JSON output. */
int main(void) {
  owner* storage = (owner*)calloc(1, sizeof(*storage));
  require(storage != NULL, "Owner allocation failed");
  require_status(owner_init(storage));
  require_status(srl_string_set_cstr(&storage->b.text, "original"));
  storage->b.count = 1;
  srl_reference_scope* scope = shared_b_create_scope(storage, release_owner);
  require(scope != NULL, "Owner scope allocation failed");

  consumer links = {0};
  borrowed_b borrowed = {0};
  weak_b weak = {0};
  require_status(consumer_init(&links));
  require_status(borrowed_b_init(&borrowed));
  require_status(weak_b_init(&weak));
  require_status(shared_b_bind(&links.d, scope, "document-1", 7, 1));
  require_status(borrowed_b_bind(&borrowed, scope, "document-1", 7, 1));
  require_status(weak_b_bind(&weak, scope, "document-1", 7, 1));

  require_status(srl_string_set_cstr(&storage->b.text, "updated"));
  storage->b.count = 2;
  srl_buffer payload = {0};
  require_status(shared_b_with_value(&links.d, project_value, &payload));
  projected_value received = {0};
  require_status(projected_value_decode(&received, payload.data, payload.size, srl_json, NULL));
  require(srl_string_equal(&received.label, "updated") && received.count == 2, "Unexpected projection");
  fwrite(payload.data, 1, payload.size, stdout);
  putchar('\n');
  projected_value_free(&received);
  srl_buffer_free(&payload);

  /* Loading restores only an address. The application supplies an already loaded owner scope. */
  srl_buffer addresses = {0};
  consumer loaded = {0};
  require_status(consumer_encode(&links, srl_json, &addresses));
  require_status(consumer_decode(&loaded, addresses.data, addresses.size, srl_json, NULL));
  require(!shared_b_is_bound(&loaded.d), "A process-local binding was serialized");
  require(shared_b_resolve(&loaded.d, scope, "document-1", 7, 1), "Owner identity did not resolve");
  consumer_free(&loaded);
  srl_buffer_free(&addresses);

  /* Drop the application's initial lease. Shared keeps storage alive; the others only observe. */
  srl_reference_scope_release(scope);
  require(!owner_released && shared_b_is_bound(&links.d), "Shared ownership ended too early");
  consumer_free(&links);
  require(owner_released && !borrowed_b_is_bound(&borrowed) && !weak_b_is_bound(&weak), "Expiry failed");
  borrowed_b_free(&borrowed);
  weak_b_free(&weak);
  return EXIT_SUCCESS;
}
