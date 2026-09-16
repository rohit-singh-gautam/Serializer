#pragma once

namespace rohit::serializer::detail {

// Check CPU instructions and OS register-state support before entering an AVX2 object file.
bool supports_avx2() noexcept;

} // namespace rohit::serializer::detail
