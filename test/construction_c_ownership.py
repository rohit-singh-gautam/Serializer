"""Qualify C construction allocation failures, owned moves, and parameter-only wrappers."""
import argparse
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("examples", ROOT / "example/run.py")
examples = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(examples)

CONSUMER = r'''#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static size_t allocation_count, failure_at, live_allocations;

/* Inject one allocation failure and track every generated ownership release. */
static void* checked_malloc(size_t size) {
  ++allocation_count;
  if (failure_at == allocation_count) { return NULL; }
  void* result = malloc(size);
  if (result) { ++live_allocations; }
  return result;
}

/* Preserve realloc ownership and account for newly acquired storage. */
static void* checked_realloc(void* previous, size_t size) {
  ++allocation_count;
  if (failure_at == allocation_count) { return NULL; }
  const bool allocated = previous != NULL;
  void* result = realloc(previous, size);
  if (result && !allocated) { ++live_allocations; }
  return result;
}

/* Count a release once; zeroed strings require no allocation accounting. */
static void checked_free(void* value) {
  if (value) { --live_allocations; }
  free(value);
}

#define malloc checked_malloc
#define realloc checked_realloc
#define free checked_free
#include "schema.h"
#undef malloc
#undef realloc
#undef free

/* Inspect both text and pointer identity without borrowing beyond the current call. */
static bool equal_text(const srl_string* value, const char* expected) {
  const size_t size = strlen(expected);
  return value->size == size && value->data && !memcmp(value->data, expected, size);
}

/* Fail each candidate allocation, then verify one successful move without copying. */
static int verify_construction(bool constructor) {
  typedef srl_status (*construct_fn)(construction_owned*, srl_string*);
  const construct_fn construct = constructor ? construction_owned_create : construction_owned_from_text;
  for (size_t failure = 1; failure <= 3; ++failure) {
    construction_owned destination = {0};
    srl_string input = {0};
    failure_at = 0;
    if (srl_string_set(&destination.transferred, "old", 3) != srl_ok ||
        srl_string_set(&destination.retained, "existing", 8) != srl_ok ||
        srl_string_set(&input, "owned input", 11) != srl_ok) { return 1; }
    char* const previous_transferred = destination.transferred.data;
    char* const previous_retained = destination.retained.data;
    char* const previous_input = input.data;
    const size_t previous_live = live_allocations;
    allocation_count = 0;
    failure_at = failure;
    const srl_status status = construct(&destination, &input);
    failure_at = 0;
    if (failure <= 2) {
      if (status != srl_allocation || destination.transferred.data != previous_transferred ||
          destination.retained.data != previous_retained || input.data != previous_input ||
          !equal_text(&destination.transferred, "old") ||
          !equal_text(&destination.retained, "existing") || !equal_text(&input, "owned input") ||
          live_allocations != previous_live) { return 2; }
    } else {
      if (status != srl_ok || allocation_count != 2 || destination.transferred.data != previous_input ||
          !equal_text(&destination.transferred, "owned input") ||
          !equal_text(&destination.retained, "constructed") || input.data || input.size ||
          live_allocations != 2) { return 3; }
    }
    construction_owned_free(&destination);
    srl_string_free(&input);
    if (live_allocations != 0) { return 4; }
  }
  return 0;
}

/* Exercise lifecycle APIs on every parameter-only wrapper emitted by the factory. */
int main(void) {
  if (verify_construction(false) || verify_construction(true)) { return 1; }
  construction_owned unused = {0};
  srl_optional_8_uint32_t ignored_optional = {0};
  srl_array_8_uint32_t ignored_sequence = {0};
  srl_map_10_srl_string_8_uint32_t ignored_map = {0};
  if (construction_owned_unused(&unused, &ignored_optional, &ignored_sequence, &ignored_map) != srl_ok ||
      !equal_text(&unused.retained, "seed")) { return 2; }
  construction_owned_free(&unused);
  return live_allocations != 0;
}
'''


def main():
    """Generate the focused schema and run its C11 consumer under the selected SDK host."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--build", type=Path, default=ROOT / "out/construction-c-ownership")
    parser.add_argument("--wsl-languages", default="")
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    args.build = args.build.resolve()
    args.cpp_library = None
    args.sanitize = False
    generated = args.build / "generated"
    source = args.build / "consumer"
    generated.mkdir(parents=True, exist_ok=True)
    source.mkdir(parents=True, exist_ok=True)
    examples.run([args.compiler, "--input", ROOT / "test/resources/construction_c_ownership.serializer",
                  "--language", "c", "--c.output", generated / "schema.h"])
    (source / "main.c").write_text(CONSUMER, encoding="utf-8")
    runner = examples.Runner(args)
    examples.run(runner.build("c", "construction_c_ownership", generated, source))
    print("PASS: C construction allocation failures, move ownership and parameter-only wrappers", flush=True)


if __name__ == "__main__":
    main()
