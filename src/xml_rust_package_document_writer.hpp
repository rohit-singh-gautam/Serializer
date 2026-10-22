// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "xml_schema.hpp"

#include <string>
#include <string_view>

namespace rohit::serializer::writer::xml_binding {
// Generate companion-aware operations without changing native models or legacy XML traits.
inline std::string rust_document_operation_impl(const class_node& owner, const record& contract,
    std::string_view name, std::string_view module, const xml_options& options,
    std::string_view lifetime = {}) {
  const auto quote = [](std::string_view value) { return native::quote(value); };
  const auto graph = xml_codegen::native_identity_graph(owner, contract, xml_binding::schema{options});
  const std::string api{module};
  std::string result;
  const auto line = [&](const std::string& value) { result += value + '\n'; };
  // Every method borrows exactly one synchronous operation; retained companions own no latch.
  const auto prepare = [&] {
    line("use " + api + "::{SrlXmlIdentityDiagnosticNode,SrlXmlIdentityDiagnosticConstraint,SrlXmlIdentityDiagnosticScalar};");
    line("const GRAPH:&[SrlXmlIdentityNode]=" + xml_codegen::rust_identity_graph(graph, quote) + ";");
    line("const DIAGNOSTICS:&[SrlXmlIdentityDiagnosticNode]=" + xml_codegen::rust_identity_diagnostics(graph, quote) + ";");
    line("let package_contract=operation.package_contract();let state=" + api + "::State::new(operation,GRAPH,DIAGNOSTICS)?;state.configure(&options);");
  };
  // Identity checks always see the complete original/final XML rather than filtered transport.
  const auto validation = [&](bool input) {
    line("srl_xml_identity_validate_with_operation(text,options.limits,options.identity_limits,GRAPH,Some(state.secondary()),DIAGNOSTICS)?;");
    if (!options.runtime_xsd) {
      line("if options.validator.is_some(){return Err(Error(\"Runtime XSD not enabled during generation\"));}");
    } else if (options.validation_scope != (input ? "output" : "input")) {
      line("if let Some(validator)=options.validator{state.poll()?;validator.validate(text)?;state.poll()?;}");
    }
  };
  line("impl" + std::string{lifetime} + " " + api + "::DocumentCodec for " + std::string{name} + std::string{lifetime} + " {");
  line("/// Borrow native payload and immutable presentation until final XML admission succeeds.");
  line("fn encode_document_with_operation(&self,extensions:Option<&XmlExtensionState>,options:" + api + "::Options<'_>,operation:&mut dyn " + api + "::Operation)->" + api + "::Result<String>{");
  prepare();
  line("let result=(||{state.poll()?;let empty=XmlExtensionState::default();let extensions=extensions.unwrap_or(&empty);state.preserve_bytes(extensions.preserved_bytes())?;");
  line("let transport=srl_xml_extension_output_limits(options.limits)?;let mut output=SrlXmlWriter::new_with_operation(transport,Some(state.secondary()));output.map_limits=options.map_limits;output.mixed_limits=options.mixed_limits;output.digest_limits=options.digest_limits;");
  line("if let Some(contract)=package_contract{if contract.requires_type{output.pending_type=contract.expected_type;}}");
  line("self.srl_xml_write(&mut output," + quote(contract.root.local) + "," + quote(contract.root.namespace_uri) + ")?;");
  line("let final_text=srl_xml_extension_replay_with_operation(&output.text,options.limits,&XmlExtensionOptions::default(),Self::srl_xml_extension_model(),extensions,Some(state.clone()))?;let text=final_text.as_str();");
  validation(false);
  line("state.poll()?;Ok(final_text)})();state.finish(result) }");
  line("/// Validate presentation before constructing an unpublished owning native candidate.");
  line("fn decode_document_with_operation(text:&str,options:" + api + "::Options<'_>,expected_type:Option<(&str,&str)>,operation:&mut dyn " + api + "::Operation)->" + api + "::Result<XmlDocument<Self>>{");
  prepare();
  line("let result=(||{state.poll()?;");
  line("let (known,extensions)=srl_xml_extension_filter_with_operation(text,options.limits,&XmlExtensionOptions::default(),Self::srl_xml_extension_model(),Some(state.clone()),package_contract)?;");
  validation(true);
  line("let transport=srl_xml_extension_transport_limits(&known,options.limits)?;let mut input=SrlXmlInput::new_with_operation(&known,transport,Some(state.secondary()))?;input.permitted_root_type=expected_type;input.map_limits=options.map_limits;input.mixed_limits=options.mixed_limits;input.digest_limits=options.digest_limits;");
  line("let value=Self::srl_xml_read_tokens(&mut input," + quote(contract.root.namespace_uri) + "," + quote(contract.root.local) + ")?;input.finish_document()?;state.poll()?;Ok(XmlDocument{value,extensions})})();state.finish(result) }");
  line("/// Capture only opaque presentation for a caller-selected owning decoder.");
  line("fn prepare_document_with_operation(text:&str,options:" + api + "::Options<'_>,operation:&mut dyn " + api + "::Operation)->" + api + "::Result<XmlExtensionState>{");
  prepare();
  line("let result=(||{state.poll()?;let (_,extensions)=srl_xml_extension_filter_with_operation(text,options.limits,&XmlExtensionOptions::default(),Self::srl_xml_extension_model(),Some(state.clone()),package_contract)?;");
  validation(true);
  line("state.poll()?;Ok(extensions)})();state.finish(result) }\n}");
  return result;
}
} // namespace rohit::serializer::writer::xml_binding
