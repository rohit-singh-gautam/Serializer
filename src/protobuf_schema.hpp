#pragma once

#include <rohit/serializer_creator.hpp>

namespace rohit::serializer::writer {
// Reject schema constructs that cannot preserve their wire identity in the Protobuf mapping.
void validate_protobuf_schema(const std::vector<std::unique_ptr<syntax_node>>& statements);
} // namespace rohit::serializer::writer
