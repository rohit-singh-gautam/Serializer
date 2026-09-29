#include <gtest/gtest.h>
#include <rohit/serializer_creator.hpp>
#include <rohit/stream.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace {
namespace serializer = rohit::serializer;
namespace writer = serializer::writer;

// Parse through the same resolver as the command-line compiler.
auto parse_managed(std::string_view source) {
  const auto input = rohit::make_constant_stream(source.data(), source.size());
  return serializer::parser::parse(input);
}

constexpr std::string_view schema =
    "namespace sample { class point stable_ids managed { public int32 x (1); public int32 y (2); } "
    "class shape stable_ids { public managed point position (3); public point offset (4); } }";
} // namespace

// Incoming references never grant eligibility to unmarked leaf classes or primitive values.
TEST(managed_schema, validates_occurrence_eligibility) {
  for (const auto source :
       {"class leaf {} class root { public managed leaf value; }",
        "class leaf {} class root { public managed map(uint64) leaf values; }",
        "class root { public managed uint32 value; }",
        "class leaf managed {} class root { public managed union(leaf = item, uint32 = count) "
        "value; }",
        "class leaf managed {} class root { public managed(history) leaf value; }"}) {
    EXPECT_THROW(parse_managed(source), rohit::exception::base_parser) << source;
  }
  auto parsed = parse_managed(schema);
  const auto& space = static_cast<const serializer::namespace_node&>(*parsed.front());
  EXPECT_TRUE(static_cast<const serializer::class_node&>(*space.statements[0]).supports_managed());
  EXPECT_TRUE(static_cast<const serializer::class_node&>(*space.statements[1]).supports_managed());
}

// All naming profiles preserve runtime metadata and emit automatic companions and typed editors.
TEST(managed_schema, generates_companions_for_every_cpp_profile) {
  const auto parsed = parse_managed(schema);
  for (const auto profile :
       {writer::coding_standard::serializer, writer::coding_standard::core,
        writer::coding_standard::google, writer::coding_standard::llvm,
        writer::coding_standard::gnu, writer::coding_standard::cert, writer::coding_standard::misra,
        writer::coding_standard::autosar, writer::coding_standard::qt}) {
    writer::cpp_options options;
    options.format = false;
    options.standard = profile;
    options.managed_separate_values = true;
    const auto generated = writer::cpp::generate(parsed, options);
    EXPECT_NE(generated.find("persistent_id{}"), std::string::npos);
    EXPECT_EQ(generated.find("persistent_id_{}"), std::string::npos);
    EXPECT_NE(generated.find("model_traits<"), std::string::npos);
    EXPECT_NE(generated.find("::visit_entities(storage.value."), std::string::npos);
    EXPECT_NE(generated.find("requires (!Access::is_managed)"), std::string::npos);
  }
}

// A central type selection changes authoritative storage and the envelope's model binding together.
TEST(managed_schema, configurable_identity_width) {
  const auto parsed = parse_managed(schema);
  writer::cpp_options options;
  options.format = false;
  options.managed_id_type = "uint64";
  const auto generated = writer::cpp::generate(parsed, options);
  EXPECT_NE(generated.find("::std::uint64_t persistent_id{}"), std::string::npos);
  EXPECT_NE(generated.find("using id_type = ::std::uint64_t"), std::string::npos);
  options.managed_id_type = "uint16";
  EXPECT_THROW(writer::cpp::generate(parsed, options), std::invalid_argument);
}

// Default output identifies the public schema class; ordinary/storage companions are opt-in.
TEST(managed_schema, direct_identity_is_default_and_reserves_metadata) {
  writer::cpp_options options;
  options.format = false;
  const auto generated = writer::cpp::generate(parse_managed(schema), options);
  EXPECT_NE(generated.find("using storage_type = ::sample::shape;"), std::string::npos);
  EXPECT_NE(generated.find("managed.direct.v1:"), std::string::npos);
  EXPECT_EQ(generated.find("class managed_shape_storage"), std::string::npos);
  for (const auto source : {"class item managed { public uint32 persistent_id (1); }",
                            "class item managed { public uint32 data (1073741823); }"}) {
    EXPECT_THROW(writer::cpp::generate(parse_managed(source), options), std::invalid_argument);
  }
}

// Unsupported representations fail explicitly instead of emitting an unmanaged substitute.
TEST(managed_schema, rejects_unsupported_backends_and_shapes) {
  const auto parsed = parse_managed(schema);
  EXPECT_THROW(writer::java::generate(parsed, "Example", {}), std::invalid_argument);
  for (const auto language :
       {"js", "typescript", "go", "csharp", "c", "rust", "python", "swift", "kotlin"}) {
    EXPECT_THROW(writer::portable::generate(parsed, language, "example", {}),
                 std::invalid_argument);
  }
  writer::cpp_options options;
  options.format = false;
  options.managed_separate_values = true;
  for (const auto source :
       {"class item managed { private uint32 value; }",
        "class item managed view owning { public uint32 value; }",
        "class base {} class item managed : public base { public uint32 value; }",
        "class item managed {} class managed_item_storage {}",
        "class item managed {} class item_editor {}",
        "class item managed {} enum managed_item_storage { value }",
        "class item managed {} namespace managed_item_data {}",
        "class item managed { public array item children; }"}) {
    auto invalid = parse_managed(source);
    EXPECT_THROW(writer::cpp::generate(invalid, options), std::invalid_argument) << source;
  }
}
