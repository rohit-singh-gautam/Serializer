// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Member definitions included inside the portable emitter; no independent namespace or storage.
#pragma once

  // Bind each reference field to its exact logical target and selected object-ID wire profile.
  std::string managed_arguments(const rohit::serializer::type_name& value) const {
    const auto* target_record = static_cast<const class_node*>(value.arguments.front().resolved_node);
    return std::to_string(managed_model->class_ids.at(target_record)) + "n, " +
        (options.managed_id_type == "uint64" ? "8" : "4") + ", " +
        (options.managed_id_encoding == persistent_id_encoding::compact_varint ? "true" : "false");
  }

  // Select only owning occurrences and typed envelopes that require metadata traversal.
  static bool managed_shape(const rohit::serializer::type_name& value) {
    if (value.is_managed_reference()) { return true; }
    if (value.is_optional() || value.is_sequence() || value.is_associative()) {
      return managed_shape(value.arguments.back());
    }
    return value.resolved_node && value.resolved_node->type == object_type::class_type;
  }

  // Follow actual generated containers without following nonowning token targets.
  void managed_visit_shape(const rohit::serializer::type_name& value,
                           const std::string& expression, const std::string& participating,
                           std::size_t& next_variable) {
    if (!managed_shape(value)) { return; }
    if (value.is_managed_reference()) {
      const auto variable = "reference" + std::to_string(next_variable++);
      line("const " + variable + " = " + expression + ";");
      const auto* target_record = static_cast<const class_node*>(value.arguments.front().resolved_node);
      const auto tag = std::to_string(managed_model->class_ids.at(target_record)) + "n";
      line("srlManagedValidate(" + variable + ", " + tag + ", " +
          (options.managed_id_type == "uint64" ? "8" : "4") + ");");
      if (options.managed_engine) {
        line("if (visitor) visitor.reference(new ManagedDocument(" + variable + ".documentHigh, " +
            variable + ".documentLow), " + variable + ".classId, " + variable + ".objectId, " + tag + ");");
      }
    } else if (value.is_optional() || value.is_sequence() || value.is_associative()) {
      const auto variable = "occurrence" + std::to_string(next_variable++);
      open("for (const " + variable + " of " + expression + (value.is_associative() ? ".values()" : "") + ")");
      managed_visit_shape(value.arguments.back(), variable, participating, next_variable);
      close();
    } else {
      line("srlManagedVisit" + names.at(value.resolved_node) + "(" + expression + ", visitor, " + participating + ");");
    }
  }

  // Reject unsupported graph shapes explicitly before a generated artifact can be published.
  void validate_managed_profile() const {
    std::set<const class_node*> visiting;
    std::set<const class_node*> visited;
    const auto validate_record = [&](auto&& recurse, const class_node* record) -> void {
      if (visited.contains(record)) { return; }
      if (!visiting.insert(record).second) {
        throw std::invalid_argument{"JavaScript managed outputs require finite acyclic owning records"};
      }
      const auto& identifier = names.at(record);
      if (identifier.starts_with("Managed") || identifier.starts_with("_mg") ||
          identifier.starts_with("srlManaged") || !record->parents.empty() || record->reference ||
          !record->generic_name.empty() || record->has_magic() ||
          !record->functions.empty() || !record->constructions.empty() || !record->conversions.empty() ||
          !record->arithmetic.empty() || !record->native_blocks.empty()) {
        throw std::invalid_argument{"JavaScript managed outputs require concrete public owning records without inheritance, magic or behavior"};
      }
      for (const auto& item : record->member_list) {
        if (item.access != access_type::public_access || item.transient || item.cached || item.wire_only || item.version ||
            item.modifier == member::modifier_type::variant || item.array_exposure ||
            item.indexing != member::index_method::none || !item.readonly_getter.empty() ||
            !item.omitted_formats.empty() || !item.created_version.empty() || !item.obsolete_version.empty() ||
            !item.auto_size_formats.empty()) {
          throw std::invalid_argument{"JavaScript managed outputs require public unversioned durable fields without variants, indexes or computed exposure"};
        }
        const auto validate_type = [&](auto&& check_type, const rohit::serializer::type_name& value) -> void {
          if (value.is_managed_reference()) { return; }
          if (value.resolved_node && value.resolved_node->type == object_type::class_type) {
            recurse(recurse, static_cast<const class_node*>(value.resolved_node));
          }
          for (const auto& argument : value.arguments) { check_type(check_type, argument); }
        };
        for (const auto& value : item.type_name_list) { validate_type(validate_type, value); }
      }
      visiting.erase(record); visited.insert(record);
    };
    for (const auto* node : nodes) {
      if (node->type == object_type::class_type) { validate_record(validate_record, static_cast<const class_node*>(node)); }
    }
  }

  // Emit deterministic ordinary validation and engine metadata independently of pointer ordering.
  void managed_metadata() {
    validate_managed_profile();
    std::map<std::string, const class_node*> records;
    std::map<std::string, std::uint64_t> tags;
    for (const auto* node : nodes) {
      if (node->type == object_type::class_type) { records.emplace(names.at(node), static_cast<const class_node*>(node)); }
    }
    for (const auto& [record, tag] : managed_model->class_ids) { tags.emplace(record->get_full_name(), tag); }
    const auto native_id = options.managed_id_type == "uint64" ? "bigint" : "number";
    const std::string protocol_name = options.protocols == output_protocols::binary_none ? "BINARY_NONE" : "BINARY_INTEGER";
    std::string manifest = std::string{":"} + protocol_name;
    for (const auto& [identifier, tag] : tags) { manifest += ":" + identifier + "=" + std::to_string(tag); }
    if (options.managed_engine) {
      constexpr std::size_t maximum_binding_bytes = 4096;
      for (const auto& [record, binding] : managed_model->bindings) {
        static_cast<void>(record);
        const auto contract = "javascript.native.v1:" + binding +
            (options.managed_id_encoding == persistent_id_encoding::compact_varint ? ":compact_varint" : ":fixed") + manifest;
        if (contract.size() > maximum_binding_bytes) {
          throw std::invalid_argument{"JavaScript managed schema manifest exceeds metadata bound"};
        }
      }
    }
    if (language == target::typescript) {
      if (options.managed_engine) {
        for (const auto& [identifier, record] : records) {
          if (managed_model->bindings.contains(record)) {
            line("export declare function managedSchema(root: typeof " + identifier +
                ", limits?: ManagedSchemaLimits): ManagedSchema<" + identifier + ", " + native_id + ">;");
          }
        }
      }
      return;
    }
    for (const auto& [identifier, record] : records) {
      line("/** Validate complete ordinary token requirements and visit exact owning occurrences. */");
      open("function srlManagedVisit" + identifier + "(value, visitor, participating)");
      line("if (!value || Object.getPrototypeOf(value) !== " + identifier + ".prototype) throw new TypeError('Managed record type differs');");
      const auto tag = managed_model->class_ids.find(record);
      if (options.managed_engine) {
        if (tag == managed_model->class_ids.end()) {
          line(std::string{"if (visitor) visitor.entity(value, 0n, () => "} +
              (options.managed_id_type == "uint64" ? "0n" : "0") + ", () => {}, false);");
        } else {
          const auto identity = "value." + field("persistent_id");
          line("if (visitor) visitor.entity(value, " + std::to_string(tag->second) + "n, () => " + identity +
              ", id => { " + identity + " = id; }, participating);");
        }
      }
      std::size_t next_variable{};
      for (const auto& item : record->member_list) {
        if (tag != managed_model->class_ids.end() && item.display_name == "persistent_id") { continue; }
        const auto& value = item.type_name_list.front();
        if (!managed_shape(value)) { continue; }
        const auto fields = managed_model->managed_fields.find(record);
        const bool managed = fields != managed_model->managed_fields.end() && fields->second.contains(item.name);
        const auto participating = std::string{managed ? "participating" : "false"};
        const auto expression = "value." + field(item.name);
        if (item.modifier == member::modifier_type::array || item.modifier == member::modifier_type::map) {
          const auto variable = "occurrence" + std::to_string(next_variable++);
          open("for (const " + variable + " of " + expression + (item.modifier == member::modifier_type::map ? ".values()" : "") + ")");
          managed_visit_shape(value, variable, participating, next_variable); close();
        } else { managed_visit_shape(value, expression, participating, next_variable); }
      }
      close();
    }
    if (!options.managed_engine) { return; }
    line("/** Convert a validated engine token to the corresponding ordinary wire envelope. */");
    line("export function managedReference(token) { if (!_mgTokens.has(token)) throw new ManagedError('Invalid engine token'); return new ManagedReference(token.document.high, token.document.low, token.classId, token.objectId); }");
    line("/** Restore exact nonowning coordinates; resolve still checks the selected document and liveness. */");
    line("export function managedToken(reference) { if (!(reference instanceof ManagedReference)) throw new ManagedError('Invalid wire reference'); return new ManagedToken(new ManagedDocument(reference.documentHigh, reference.documentLow), reference.classId, reference.objectId); }");
    line("/** Bind generated codecs and participation metadata with finite snapshot/work admission. */");
    open("export function managedSchema(root, limits = {})");
    for (const auto& [identifier, record] : records) {
      const auto binding = managed_model->bindings.find(record);
      if (binding == managed_model->bindings.end()) { continue; }
      const auto contract = "javascript.native.v1:" + binding->second +
          (options.managed_id_encoding == persistent_id_encoding::compact_varint ? ":compact_varint" : ":fixed") + manifest;
      constexpr std::size_t maximum_binding_bytes = 4096;
      if (contract.size() > maximum_binding_bytes) { throw std::invalid_argument{"JavaScript managed schema manifest exceeds metadata bound"}; }
      open("if (root === " + identifier + ")");
      line("return new ManagedSchema(root, " + quote(contract) + ", " + (options.managed_id_type == "uint64" ? "8" : "4") + ", {");
      line("encode: value => value.encode(Protocol." + protocol_name + "),");
      line("decode: bytes => " + identifier + ".decode(bytes, Protocol." + protocol_name + "),");
      line("visit: (value, visitor) => srlManagedVisit" + identifier + "(value, visitor, true)}, limits);");
      close();
    }
    line("throw new ManagedError('Root is not an explicitly managed generated class');"); close();
  }
