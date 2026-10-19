// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_rust_writer.hpp"
#include <functional>
#include <string>

namespace rohit::serializer::writer::xml_binding {
// Produce a borrowed scalar spelling only during its synchronous native owner callback.
inline std::string rust_xml_projection_scalar(const type_name& type, const std::string& expression,
    const std::function<std::string(const type_name&)>& identifier) {
  if (type.is_digest()) {
    return "&srl_xml_digest_text(" + expression + "," + std::to_string(type.digest_extent) + ",output.digest_limits)?";
  }
  if (type.name == "string" || type.name.starts_with("version")) { return expression + ".as_str()"; }
  if (type.type == object_type::enum_type) { return identifier(type) + "::NAMES[*" + expression + " as usize]"; }
  if (type.name == "float" || type.name == "double") {
    return "&{let value=*" + expression + "; if value.is_nan() {\"NaN\".to_owned()} else if value==" +
        identifier(type) + "::INFINITY {\"INF\".to_owned()} else if value==" + identifier(type) +
        "::NEG_INFINITY {\"-INF\".to_owned()} else {value.to_string()}}";
  }
  return "&" + expression + ".to_string()";
}
// Hold a borrowed, shared or weak native owner through the entire direct XML export traversal.
inline std::string rust_reference_xml_methods(const class_node& owner, const record& contract,
    const std::function<std::string(const type_name&)>& identifier, const xml_options& options) {
  const auto& payload = owner.reference->value_type;
  const auto body = [&] {
    std::string result = "/// Borrow live payload storage without constructing an owning XML target.\n"
        "fn srl_xml_write(&self,output:&mut SrlXmlWriter,local:&str,uri:&str)->Result<(),Error> {\n"
        "self.with_value(|selected| {\n";
    if (payload.type == object_type::class_type) {
      result += "selected.srl_xml_write(output,local,uri)\n";
    } else {
      result += "output.open(local,uri)?; output.put(\">\")?; output.escaped(" +
          rust_xml_projection_scalar(payload, "selected", identifier) + ",false)?; output.close(local)\n";
    }
    return result + "})??; Ok(()) }\n";
  };
  return rust_methods(owner, contract, [](const member&) { return std::string{}; }, identifier,
      options, true, {}, body);
}
} // namespace rohit::serializer::writer::xml_binding
