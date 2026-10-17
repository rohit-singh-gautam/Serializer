// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
#include <model.hpp>
#include <rohit/compression.hpp>
#include <rohit/compression_stream.hpp>
#include <rohit/package.hpp>
#include <rohit/serializer.hpp>
#include <rohit/xml.hpp>

#include <iostream>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace codec = rohit::serializer;
namespace package = codec::package;
using order = xml_example::order;

// Compare the complete logical projection after each independent transport round trip.
void require_equal(const order& expected, const order& actual) {
  if (expected.id != actual.id || expected.customer != actual.customer ||
      expected.counts != actual.counts) {
    throw std::runtime_error{"Order round trip failed"};
  }
}

// Present complete serialized text to the codec-independent archive API.
std::span<const std::uint8_t> as_bytes(std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

// Decode one selected XML part after ZIP integrity has been checked.
order decode_xml(const std::vector<std::uint8_t>& bytes) {
  return codec::xml::deserialize<order>(
      {reinterpret_cast<const char*>(bytes.data()), bytes.size()});
}
} // namespace

// Exercise whole-object XML, JSON+gzip, mixed-codec ZIP and standard OPC composition.
int main(int argc, char** argv) {
  try {
    if (!codec::xml::available() || !codec::compression::available(codec::compression::format::gzip)) {
      throw std::runtime_error{"This example requires XML and ZLIB-enabled Serializer"};
    }
    order original{};
    original.id = 7;
    original.customer = "R&D";
    original.counts = {2, 4};
    const auto xml = codec::xml::serialize(original);
    require_equal(original, codec::xml::deserialize<order>(xml));

    // One complete object goes through a compression stream, followed by explicit finalization.
    std::ostringstream gzip_bytes;
    codec::compression::output_stream compressed{gzip_bytes, codec::compression::gzip_options{}};
    codec::serialize_to<codec::json>(compressed, original);
    compressed.finish();
    std::istringstream gzip_source{gzip_bytes.str()};
    codec::compression::input_stream expanded{gzip_source,
        codec::compression::decode_options{.format = codec::compression::format::gzip}};
    require_equal(original, codec::deserialize_exact<order, codec::json>(expanded));

    // ZIP is independent of its parts' codecs and requires no OPC metadata.
    std::ostringstream zip_bytes;
    package::zip_writer archive{zip_bytes};
    archive.add("document.xml", as_bytes(xml), {.method = package::zip_method::deflate});
    archive.write("document.bin", [&original](std::ostream& output) {
      codec::serialize_to<codec::binary_none>(output, original);
    });
    archive.finish();
    std::istringstream zip_source{zip_bytes.str()};
    package::zip_reader zip{zip_source};
    require_equal(original, decode_xml(zip.read("document.xml")));
    const auto binary = zip.read("document.bin");
    const auto binary_input = rohit::make_constant_full_stream(binary.data(), binary.size());
    require_equal(original, codec::deserialize_exact<order, codec::binary_none>(binary_input));

    // OPC emits [Content_Types].xml and .rels; consumers discover the document by its role.
    std::ostringstream opc_bytes;
    package::opc_writer document{opc_bytes};
    document.add_part("/data/order.xml", "application/xml", as_bytes(xml));
    document.add_relationship({"/", "rId1", "urn:serializer:main-document", "data/order.xml"});
    document.finish();
    std::istringstream opc_source{opc_bytes.str()};
    package::opc_reader saved{opc_source};
    const auto links = saved.find_relationships("/", "urn:serializer:main-document");
    if (links.size() != 1) { throw std::runtime_error{"Expected one main document relationship"}; }
    require_equal(original, decode_xml(saved.read(package::resolve_target("/", links.front()->target))));
    saved.validate_all();
    if (argc == 2) {
      const std::filesystem::path directory{argv[1]};
      std::filesystem::create_directories(directory);
      std::ofstream xml_file{directory / "order.xml", std::ios::binary};
      xml_file << xml;
      xml_file.close();
      std::ofstream gzip_file{directory / "order.json.gz", std::ios::binary};
      gzip_file << gzip_bytes.str();
      gzip_file.close();
      if (!xml_file || !gzip_file) { throw std::runtime_error{"Cannot write example artifacts"}; }
      package::save_zip_file(directory / "order.zip", [&xml, &original](package::zip_writer& writer) {
        writer.add("document.xml", as_bytes(xml), {.method = package::zip_method::deflate});
        writer.write("document.bin", [&original](std::ostream& output) {
          codec::serialize_to<codec::binary_none>(output, original);
        });
      });
      package::save_opc_file(directory / "order.opc", [&xml](package::opc_writer& writer) {
        writer.add_part("/data/order.xml", "application/xml", as_bytes(xml));
        writer.add_relationship({"/", "rId1", "urn:serializer:main-document", "data/order.xml"});
      });
    } else if (argc != 1) {
      throw std::invalid_argument{"Usage: serializer_cpp_xml_example [OUTPUT_DIRECTORY]"};
    }
    std::cout << xml << "\nXML, JSON+gzip, mixed ZIP and OPC passed\n";
    return 0;
  } catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
