#pragma once

// Cross-endian qualification must execute a real big-endian target build.
#ifdef __cplusplus
#include <bit>
static_assert(std::endian::native == std::endian::big,
              "Cross-endian qualification requires a big-endian CPU target");
#else
_Static_assert(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__,
               "Cross-endian qualification requires a big-endian CPU target");
#endif
