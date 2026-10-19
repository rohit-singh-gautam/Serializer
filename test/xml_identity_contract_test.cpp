// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/xml_identity_contract.hpp"
#include "../src/xml_schema.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>

namespace binding = rohit::serializer::writer::xml_binding;
namespace xml = rohit::serializer::xml;
namespace {
// Own parsed XSD trees directly so focused binding fixtures require no temporary files.
binding::schema_bundle bundle(std::string_view schema, std::string target = {}) {
  binding::schema_bundle result{};
  binding::schema_bundle::document document{};
  document.root = xml::parse(schema);
  document.target = std::move(target);
  result.documents.push_back(std::move(document));
  return result;
}
// Borrow the named global declaration from one immutable test bundle.
const xml::element& root_declaration(const binding::schema_bundle& source) {
  for (const auto& child : source.documents.front().root.children) {
    if (child.qualified_name.local == "element") { return child; }
  }
  throw std::logic_error{"Missing identity test root"};
}
} // namespace

// Named and anonymous nested record fields keep the actual XSD scalar types in compiled tuple IR.
TEST(xml_identity_contract, named_anonymous_fields_and_forward_targets_bind) {
  const auto source = bundle(R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:t="urn:test" targetNamespace="urn:test" elementFormDefault="qualified">
    <xs:complexType name="row_type"><xs:sequence><xs:element name="label" type="xs:token"/></xs:sequence><xs:attribute name="id" type="xs:integer"/></xs:complexType>
    <xs:element name="root"><xs:complexType><xs:sequence><xs:element name="row" type="t:row_type" maxOccurs="unbounded"/>
      <xs:element name="ref" maxOccurs="unbounded"><xs:complexType><xs:attribute name="id" type="xs:integer"/></xs:complexType></xs:element>
    </xs:sequence></xs:complexType>
    <xs:keyref name="references" refer="t:ids"><xs:selector xpath="t:ref"/><xs:field xpath="@id"/></xs:keyref>
    <xs:key name="ids"><xs:selector xpath="t:row"/><xs:field xpath="@id"/></xs:key>
    <xs:unique name="labels"><xs:selector xpath="t:row"/><xs:field xpath="t:label"/></xs:unique></xs:element></xs:schema>)", "urn:test");
  const auto contract = binding::bind_identity_constraints(source, root_declaration(source), "urn:test");
  ASSERT_EQ(contract.constraints.size(), 3U);
  EXPECT_EQ(contract.constraints[0].fields.front().lexical_kind, "integer");
  EXPECT_EQ(contract.constraints[2].fields.front().lexical_kind, "token");
  EXPECT_EQ(contract.owner, (xml::name{"root", "urn:test"}));
  EXPECT_NO_THROW(xml::validate_identity_scope(xml::parse("<root xmlns='urn:test'><row id='1'><label>hello</label></row><ref id='01'/></root>"), contract));
  EXPECT_THROW(xml::validate_identity_scope(xml::parse("<root xmlns='urn:test'><row id='1'><label>hello</label></row><ref id='2'/></root>"), contract), std::invalid_argument);
}

// Prefix resolution and compiler field type checks reject wrong-scope and mismatched references.
TEST(xml_identity_contract, wrong_scope_and_value_space_mismatches_reject) {
  const std::string prefix = R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:other="urn:other"><xs:element name="root"><xs:complexType><xs:sequence>
    <xs:element name="row"><xs:complexType><xs:attribute name="id" type="xs:integer"/></xs:complexType></xs:element>
    <xs:element name="ref"><xs:complexType><xs:attribute name="id" type="xs:boolean"/></xs:complexType></xs:element>
    </xs:sequence></xs:complexType><xs:key name="ids"><xs:selector xpath="row"/><xs:field xpath="@id"/></xs:key>)";
  for (const auto refer : {"ids", "other:ids", "missing"}) {
    const auto source = bundle(prefix + "<xs:keyref name='references' refer='" + refer +
        "'><xs:selector xpath='ref'/><xs:field xpath='@id'/></xs:keyref></xs:element></xs:schema>");
    EXPECT_THROW((void)binding::bind_identity_constraints(source, root_declaration(source), {}), std::invalid_argument);
  }
}

// Simple-content fields retain the base scalar while attribute projections keep their own types.
TEST(xml_identity_contract, simple_content_and_descendant_projection) {
  const auto source = bundle(R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="root"><xs:complexType><xs:sequence>
    <xs:element name="group"><xs:complexType><xs:sequence><xs:element name="value" maxOccurs="unbounded"><xs:complexType><xs:simpleContent>
    <xs:extension base="xs:decimal"><xs:attribute name="label" type="xs:token"/></xs:extension></xs:simpleContent></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element>
    </xs:sequence></xs:complexType><xs:unique name="values"><xs:selector xpath=".//value"/><xs:field xpath="."/></xs:unique></xs:element></xs:schema>)");
  const auto contract = binding::bind_identity_constraints(source, root_declaration(source), {});
  ASSERT_EQ(contract.constraints.size(), 1U);
  EXPECT_EQ(contract.constraints.front().fields.front().lexical_kind, "decimal");
  EXPECT_THROW(xml::validate_identity_scope(xml::parse("<root><group><value label='a'>1</value><value label='b'>1.00</value></group></root>"), contract), std::invalid_argument);
}

// Unknown scalar targets and unsupported selector grammars never compile guessed identity semantics.
TEST(xml_identity_contract, unresolved_ambiguous_and_unsupported_fields_reject) {
  for (const auto field : {"@missing", "child", "@id[1]", "text()", "@*"}) {
    const auto source = bundle(std::string{R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="root"><xs:complexType><xs:sequence><xs:element name="row"><xs:complexType><xs:attribute name="id" type="xs:integer"/></xs:complexType></xs:element></xs:sequence></xs:complexType><xs:key name="ids"><xs:selector xpath="row"/><xs:field xpath=")"} + field + R"("/></xs:key></xs:element></xs:schema>)");
    EXPECT_THROW((void)binding::bind_identity_constraints(source, root_declaration(source), {}), std::invalid_argument) << field;
  }
}

// Wildcard-covered candidates cannot inherit a guessed scalar identity from a known sibling.
TEST(xml_identity_contract, wildcard_projection_does_not_guess_field_types) {
  const auto source = bundle(R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="root"><xs:complexType><xs:sequence>
    <xs:element name="row"><xs:complexType><xs:attribute name="id" type="xs:integer"/><xs:anyAttribute namespace="##other" processContents="lax"/></xs:complexType></xs:element>
    <xs:any namespace="##other" processContents="lax" minOccurs="0" maxOccurs="unbounded"/>
    </xs:sequence></xs:complexType><xs:key name="ids"><xs:selector xpath=".//row"/><xs:field xpath="@id"/></xs:key></xs:element></xs:schema>)");
  EXPECT_THROW((void)binding::bind_identity_constraints(source, root_declaration(source), {}), std::invalid_argument);
}

// Default/fixed projections cannot silently inspect unaugmented input, including empty defaults.
TEST(xml_identity_contract, identity_fields_requiring_schema_value_augmentation_reject) {
  for (const auto metadata : {"default='x'", "fixed='x'", "default=''", "fixed=''"}) {
    for (const auto attribute_field : {true, false}) {
      const auto field = std::string{attribute_field ? "<xs:attribute name='id' type='xs:string' " :
          "<xs:sequence><xs:element name='id' type='xs:string' "} + metadata +
          (attribute_field ? "/>" : "/></xs:sequence>");
      const auto prefix = std::string{"<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:element name='root'><xs:complexType><xs:sequence><xs:element name='row'><xs:complexType>"} +
          field + "</xs:complexType></xs:element></xs:sequence></xs:complexType>";
      const auto source = bundle(prefix + "<xs:key name='ids'><xs:selector xpath='row'/><xs:field xpath='" +
          (attribute_field ? "@id" : "id") + "'/></xs:key></xs:element></xs:schema>");
      EXPECT_THROW((void)binding::bind_identity_constraints(source, root_declaration(source), {}),
          std::invalid_argument) << metadata;
      const auto ordinary = bundle(prefix + "</xs:element></xs:schema>");
      EXPECT_NO_THROW((void)binding::bind_identity_constraints(ordinary, root_declaration(ordinary), {}));
    }
  }
}

// Resolved element references retain declaration nillability; attributes and legacy kinds stay distinct.
TEST(xml_identity_contract, resolved_scalar_declaration_nillability_is_preserved) {
  for (const auto marker : {"true", "1"}) {
    const auto source = bundle(std::string{R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
      <xs:element name="root"><xs:complexType><xs:sequence><xs:element ref="value" minOccurs="0" maxOccurs="unbounded"/>
      </xs:sequence></xs:complexType><xs:key name="values"><xs:selector xpath="value"/><xs:field xpath="."/></xs:key>
      <xs:unique name="ids"><xs:selector xpath="value"/><xs:field xpath="@id"/></xs:unique></xs:element>
      <xs:element name="value" nillable=")"} + marker + R"("><xs:complexType><xs:simpleContent><xs:extension base="xs:string">
      <xs:attribute name="id" type="xs:string"/></xs:extension></xs:simpleContent></xs:complexType></xs:element></xs:schema>)");
    const auto scope = binding::bind_identity_constraints(source, root_declaration(source), {});
    ASSERT_EQ(scope.constraints.size(), 2U);
    ASSERT_EQ(scope.constraints.front().fields.size(), 1U);
    EXPECT_EQ(scope.constraints.front().fields.front().lexical_kind, "string");
    EXPECT_TRUE(scope.constraints.front().fields.front().nillable);
    EXPECT_FALSE(scope.constraints.back().fields.front().nillable);
  }
}

// A descendant projection cannot collapse different declared nillability into one guessed contract.
TEST(xml_identity_contract, ambiguous_scalar_declaration_nillability_rejects) {
  const auto source = bundle(R"(<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema"><xs:element name="root"><xs:complexType><xs:sequence>
    <xs:element name="row" type="xs:string" nillable="true"/><xs:element name="container"><xs:complexType><xs:sequence>
    <xs:element name="row" type="xs:string"/></xs:sequence></xs:complexType></xs:element></xs:sequence></xs:complexType>
    <xs:key name="ids"><xs:selector xpath=".//row"/><xs:field xpath="."/></xs:key></xs:element></xs:schema>)");
  EXPECT_THROW((void)binding::bind_identity_constraints(source, root_declaration(source), {}), std::invalid_argument);
}
