#include "runtime_equivalence.hpp"

#include <cstddef>

// Expose the unchanged runtime traversal in a small translation unit for assembly comparison.
extern "C" std::size_t serialize_runtime_equivalence(rohit::full_stream& output,
                                                    const runtime_equivalence::message& value) {
  value.serialize_out<rohit::serializer::binary_none>(output);
  return output.current_offset();
}
