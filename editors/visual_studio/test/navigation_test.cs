using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using Rohit.Serializer.VisualStudio;

internal static class NavigationTest {
  /// <summary>Check the installed interpreter and path adapter against real schemas and generated output.</summary>
  private static int Main(string[] args) {
    try {
      var repository = Path.GetFullPath(args[0]);
      var generated = Path.GetFullPath(args[1]);
      var model = Path.GetFullPath(Path.Combine(repository, "example/schemas/complex/model.serializer"));
      var text = File.ReadAllText(model).Replace("serializer version 1;", "serializer version 1.0.0;");
      var live = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { [model] = text };
      var checks = 0;
      var optionalText = "serializer version 1.7.0; include sales/order; " +
        "class box<T = optional<demo::order>> { public optional<T> value; } class model { " +
        "public optional<optional<demo::order>> nested; public optional<array<demo::order>> rows; " +
        "public optional<map<string, demo::order>> lookup; public array optional<demo::order> values; " +
        "public map(string) optional<demo::order> entries; public array<optional<demo::order>> generic_rows; " +
        "public function optional<demo::order> find(optional<demo::order> input) cpp { return input; }; }";
      live[model] = optionalText;
      for (var occurrence = optionalText.IndexOf("demo::order", StringComparison.Ordinal); occurrence >= 0;
           occurrence = optionalText.IndexOf("demo::order", occurrence + "demo::order".Length, StringComparison.Ordinal)) {
        for (var cursor = occurrence; cursor <= occurrence + "demo::order".Length; ++cursor) {
          var destination = NavigationRunner.Resolve(model, cursor, false, generated, live, CancellationToken.None);
          Require(destination.Length == 1 && File.ReadAllText(destination[0].file)
            .Substring(destination[0].start, destination[0].end - destination[0].start) == "order",
            "optional nested operand cursor boundaries and transitive includes");
          ++checks;
        }
        Require(NavigationRunner.Resolve(model, occurrence, true, generated, live, CancellationToken.None).Length == 11,
          "optional operand definitions in every output language");
        ++checks;
      }
      var genericOptional = optionalText.IndexOf("T> value", StringComparison.Ordinal);
      Require(NavigationRunner.Resolve(model, genericOptional, false, generated, live, CancellationToken.None)
        .Single().start == optionalText.IndexOf("T = optional", StringComparison.Ordinal), "optional generic parameter");
      foreach (var spelling in new[] { "optional<", "array<", "map<", "nested;", "find(", "input)" }) {
        Require(NavigationRunner.Resolve(model, optionalText.IndexOf(spelling, StringComparison.Ordinal), false,
          generated, live, CancellationToken.None).Length == 0, "optional constructors and fields have no type destination");
        ++checks;
      }
      var constructionText = "serializer version 1.8.0; include sales/order; " +
        "class base<T> {} class derived<T> : public base<T> { " +
        "public transient layout(empty) demo::order passive; " +
        "public constructor(demo::order input) { value = input; } " +
        "public factory make(demo::order input) { value = input; } }";
      live[model] = constructionText;
      for (var occurrence = constructionText.IndexOf("demo::order", StringComparison.Ordinal); occurrence >= 0;
           occurrence = constructionText.IndexOf("demo::order", occurrence + "demo::order".Length, StringComparison.Ordinal)) {
        for (var cursor = occurrence; cursor <= occurrence + "demo::order".Length; ++cursor) {
          var destination = NavigationRunner.Resolve(model, cursor, false, generated, live, CancellationToken.None);
          Require(destination.Length == 1 && File.ReadAllText(destination[0].file)
            .Substring(destination[0].start, destination[0].end - destination[0].start) == "order",
            "construction/layout parameter boundaries and live includes");
          ++checks;
        }
      }
      var inheritedParameter = constructionText.IndexOf("public base<T>", StringComparison.Ordinal) + "public base<".Length;
      var ownParameter = constructionText.IndexOf("T> :", StringComparison.Ordinal);
      Require(NavigationRunner.Resolve(model, inheritedParameter, false, generated, live, CancellationToken.None)
        .Single().start == ownParameter, "generic base parameter uses derived declaration scope");
      ++checks;
      var predicateText = "serializer version 1.8.0; include sales/order; class sha256 {} class reading { " +
        "public function digest(sha256) fingerprint(demo::order input) readonly cpp { return {}; }; " +
        "public function bool usable() readonly predicate(all(samples, 8, item => " +
        "item.state == demo::order_state::draft && math.is_finite(item.value))); }";
      live[model] = predicateText;
      foreach (var name in new[] { "demo::order", "demo::order_state" }) {
        var start = predicateText.IndexOf(name, StringComparison.Ordinal);
        for (var cursor = start; cursor <= start + name.Length; ++cursor) {
          var destination = NavigationRunner.Resolve(model, cursor, false, generated, live, CancellationToken.None);
          Require(destination.Length == 1 && Path.GetFileName(destination[0].file) == "order.serializer",
            "digest signature and typed predicate enum cursor boundaries");
          ++checks;
        }
      }
      foreach (var spelling in new[] { "sha256)", "fingerprint(", "samples,", "item =>", "draft &&", "is_finite" }) {
        Require(NavigationRunner.Resolve(model, predicateText.IndexOf(spelling, StringComparison.Ordinal), false,
          generated, live, CancellationToken.None).Length == 0, "predicate values and digest algorithms are not type operands");
        ++checks;
      }
      var functionText = "serializer version 1.10.0; include sales/order; namespace helpers { " +
        "function demo::order identity(demo::order value) readonly expression(value); " +
        "class choice<T: demo::order || demo::order_state> { public T value; } }";
      live[model] = functionText;
      foreach (var name in new[] { "demo::order identity", "demo::order value", "demo::order ||", "demo::order_state>" }) {
        var start = functionText.IndexOf(name, StringComparison.Ordinal);
        var destination = NavigationRunner.Resolve(model, start, false, generated, live, CancellationToken.None);
        Require(destination.Length == 1 && Path.GetFileName(destination[0].file) == "order.serializer",
          "namespace signatures and exact constraint types retain included navigation");
        ++checks;
      }
      var extendedConstructionText = "serializer version 1.11.0; include sales/order; class holder<T> { " +
        "public map(demo::order) demo::order entries; " +
        "public constructor<Input: demo::order>(Input input, array[3] demo::order items, demo::order fallback = default(origin)) explicit { value = checked<T>(input.value); } " +
        "public function T converted() readonly expression(checked<T>(value)); }";
      live[model] = extendedConstructionText;
      foreach (var name in new[] { "demo::order)", "demo::order entries", "demo::order>", "demo::order items", "demo::order fallback" }) {
        var start = extendedConstructionText.IndexOf(name, StringComparison.Ordinal);
        var destination = NavigationRunner.Resolve(model, start, false, generated, live, CancellationToken.None);
        Require(destination.Length == 1 && Path.GetFileName(destination[0].file) == "order.serializer",
          "extended construction signature and map key types retain included navigation");
        ++checks;
      }
      foreach (var name in new[] { "Input input", "T>(input.value)", "T>(value)" }) {
        var start = extendedConstructionText.IndexOf(name, StringComparison.Ordinal);
        var destination = NavigationRunner.Resolve(model, start, false, generated, live, CancellationToken.None);
        Require(destination.Length == 1 && Path.GetFileName(destination[0].file) == "model.serializer",
          "construction input and checked result types retain parameter navigation");
        ++checks;
      }
      var qualifiedConstructionText = "serializer version 1.12.0; include sales/order; " +
        "class holder<T, Frame: factory(identity) = demo::order> { " +
        "public cpp storage(none) { class NativeHidden {}; } " +
        "public constructor(T input, Frame placement = default(identity)) { value = input; " +
        "validate expression(input > 0); initialize cpp { class HookHidden {}; }; } " +
        "public constructor<Input: integral || floating_point>(optional<Input> input) explicit { value = checked<T>(input.value); } " +
        "public function common_type<T, Input> scaled<Input: integral || floating_point>(Input factor) readonly " +
        "expression(checked<common_type<T, Input>>(value) * factor); " +
        "public function T native_value() readonly constexpr(cpp) cpp { return value; }; } " +
        "class factory {} class identity {} class storage {} class none {} class common_type {} " +
        "class validate {} class initialize {} class constexpr {}";
      live[model] = qualifiedConstructionText;
      var constructorInput = qualifiedConstructionText.IndexOf("Input: integral", StringComparison.Ordinal);
      var methodInput = qualifiedConstructionText.IndexOf("Input: integral", constructorInput + 1, StringComparison.Ordinal);
      foreach (Match input in Regex.Matches(qualifiedConstructionText, @"\bInput\b")) {
        var target = input.Index < qualifiedConstructionText.IndexOf("public function common_type", StringComparison.Ordinal)
          ? constructorInput : methodInput;
        var destination = NavigationRunner.Resolve(model, input.Index, false, generated, live, CancellationToken.None);
        Require(destination.Length == 1 && destination[0].start == target,
          "generic readonly return/input signatures retain their independent declaration scopes");
        ++checks;
      }
      foreach (var spelling in new[] { "factory(identity)", "identity)", "storage(none)", "none)",
          "default(identity)", "validate expression", "initialize cpp", "constexpr(cpp)",
          "common_type<T", "scaled<Input", "NativeHidden", "HookHidden" }) {
        Require(NavigationRunner.Resolve(model, qualifiedConstructionText.IndexOf(spelling, StringComparison.Ordinal), false,
          generated, live, CancellationToken.None).Length == 0,
          "construction/native contract metadata cannot claim unrelated schema symbols");
        ++checks;
      }
      var includedOrder = qualifiedConstructionText.IndexOf("demo::order", StringComparison.Ordinal);
      for (var cursor = includedOrder; cursor <= includedOrder + "demo::order".Length; ++cursor) {
        var destination = NavigationRunner.Resolve(model, cursor, false, generated, live, CancellationToken.None);
        Require(destination.Length == 1 && Path.GetFileName(destination[0].file) == "order.serializer",
          "factory-constrained class defaults retain qualified selection boundaries");
        ++checks;
      }
      var nonthrowingText = "serializer version 1.13.0; include sales/order; class holder<T> { " +
        "public function demo::order complete(demo::order input) readonly noexcept(cpp); " +
        "public function virtual demo::order dispatched(demo::order input) readonly noexcept(cpp); " +
        "public function T native_value() readonly constexpr(cpp) noexcept(cpp) cpp { class NativeHidden {}; return {}; }; " +
        "public function T reversed<Input: demo::order>(Input input) readonly noexcept(cpp) constexpr(cpp) cpp { return {}; }; " +
        "public demo::order after; public noexcept ordinary; } class noexcept {} class cpp {}";
      live[model] = nonthrowingText;
      foreach (Match reference in Regex.Matches(nonthrowingText, "demo::order")) {
        for (var cursor = reference.Index; cursor <= reference.Index + reference.Length; ++cursor) {
          var destination = NavigationRunner.Resolve(model, cursor, false, generated, live, CancellationToken.None);
          Require(destination.Length == 1 && Path.GetFileName(destination[0].file) == "order.serializer",
            "nonthrowing native and external signatures retain qualified type boundaries");
          ++checks;
        }
      }
      foreach (Match metadata in Regex.Matches(nonthrowingText, @"noexcept\(cpp\)|constexpr\(cpp\)|NativeHidden")) {
        for (var cursor = metadata.Index; cursor < metadata.Index + metadata.Length; ++cursor) {
          Require(NavigationRunner.Resolve(model, cursor, false, generated, live, CancellationToken.None).Length == 0,
            "C++ exception contract and opaque native body cannot claim unrelated schema symbols");
          ++checks;
        }
      }
      Require(NavigationRunner.Resolve(model, nonthrowingText.IndexOf("Input input", StringComparison.Ordinal), false,
        generated, live, CancellationToken.None).Single().start == nonthrowingText.IndexOf("Input:", StringComparison.Ordinal),
        "nonthrowing generic method preserves its independent input scope");
      Require(NavigationRunner.Resolve(model, nonthrowingText.IndexOf("noexcept ordinary", StringComparison.Ordinal), false,
        generated, live, CancellationToken.None).Single().start == nonthrowingText.IndexOf("noexcept {}", StringComparison.Ordinal),
        "ordinary same-spelling schema types remain navigable beside C++ exception metadata");
      checks += 2;
      var legacyCommonTypeText = "serializer version 1.11.0; class common_type<T> {} " +
        "class holder { public common_type<uint32> value; }";
      live[model] = legacyCommonTypeText;
      var legacyCommonType = legacyCommonTypeText.IndexOf("common_type<uint32>", StringComparison.Ordinal);
      var legacyCommonTypeDeclaration = legacyCommonTypeText.IndexOf("common_type<T>", StringComparison.Ordinal);
      for (var cursor = legacyCommonType; cursor <= legacyCommonType + "common_type".Length; ++cursor) {
        var destination = NavigationRunner.Resolve(model, cursor, false, generated, live, CancellationToken.None);
        Require(destination.Length == 1 && destination[0].start == legacyCommonTypeDeclaration,
          "older user-defined common_type generic applications retain their language-selected meaning");
        ++checks;
      }
      var conversionText = "serializer version 1.9.0; class empty {} class passive<T: stateless = empty> { " +
        "public transient layout(empty) T reference; } class contact { public string street; } " +
        "class other { public string street; } class base { public uint32 rank; } class view { public string address; public uint32 rank; } " +
        "class ticket : public base { public contact contact; public wire_only uint32 encoded_size(1) auto_size(binary_integer) " +
        "size_scope(payload) omit(all_except(binary_integer)); public conversion view as to_view { address = contact.street; rank = rank; }; }";
      live[model] = conversionText;
      foreach (var pair in new[] {
        new[] { conversionText.IndexOf("address =", StringComparison.Ordinal), conversionText.IndexOf("address;", StringComparison.Ordinal) },
        new[] { conversionText.IndexOf("contact.street", StringComparison.Ordinal), conversionText.IndexOf("contact;", StringComparison.Ordinal) },
        new[] { conversionText.LastIndexOf("street;", StringComparison.Ordinal), conversionText.IndexOf("street;", StringComparison.Ordinal) },
        new[] { conversionText.IndexOf("rank =", StringComparison.Ordinal), conversionText.IndexOf("rank;", conversionText.IndexOf("class view", StringComparison.Ordinal), StringComparison.Ordinal) },
        new[] { conversionText.IndexOf("= rank", StringComparison.Ordinal) + 2, conversionText.IndexOf("rank;", StringComparison.Ordinal) }
      }) {
        var destination = NavigationRunner.Resolve(model, pair[0], false, generated, live, CancellationToken.None);
        Require(destination.Length == 1 && destination[0].start == pair[1], "conversion source/target inherited field projection");
        Require(NavigationRunner.Reference(model, conversionText, pair[0], CancellationToken.None) != null,
          "conversion projection Ctrl+click range");
        checks += 2;
      }
      var conversionChanged = conversionText.Replace("public contact contact", "public other contact");
      live[model] = conversionChanged;
      Require(NavigationRunner.Resolve(model, conversionChanged.LastIndexOf("street;", StringComparison.Ordinal), false,
        generated, live, CancellationToken.None).Single().start == conversionChanged.IndexOf("street;",
        conversionChanged.IndexOf("class other", StringComparison.Ordinal), StringComparison.Ordinal), "unsaved conversion field type");
      ++checks;
      live[model] = text;
      foreach (var name in new[] { "demo::order", "demo::snapshot", "demo::customer" }) {
        var start = text.IndexOf(name, StringComparison.Ordinal);
        foreach (var offset in new[] { start, start + name.Length - 1, start + name.Length }) {
          var reference = NavigationRunner.Reference(model, text, offset, CancellationToken.None);
          Require(reference != null && reference.start == start && reference.end == start + name.Length, "schema token range");
          var declarations = NavigationRunner.Resolve(model, offset, false, generated, live, CancellationToken.None);
          Require(declarations.Length == 1, "one source declaration for " + name);
          Require(File.ReadAllText(declarations[0].file).Substring(declarations[0].start, declarations[0].end - declarations[0].start) == name.Split(':').Last(), "exact source identifier");
          ++checks;
        }
        var definitions = NavigationRunner.Resolve(model, start + name.Length, true, generated, live, CancellationToken.None);
        Require(definitions.Length == 11, "all 11 output languages for " + name + ": " + string.Join(", ", definitions.Select(item => item.file)));
        foreach (var definition in definitions) {
          var source = NavigationRunner.Resolve(definition.file, definition.end, false, generated, live, CancellationToken.None);
          Require(source.Length == 1 && Path.GetFileName(source[0].file) == name.Split(':').Last() + ".serializer", "reverse output mapping " + definition.file);
          ++checks;
        }
      }
      // Exercise the new metadata type operands against the same transitive and generated enum graph.
      var typedText = "serializer version 1.1.0; include sales/order; class typed_model { " +
        "public array[] float numbers (2) {1'000.25, 2'000.5}; " +
        "private magic demo::order_state (99) {demo::order_state::draft}; " +
        "public array[] demo::order_state states (1) {demo::order_state::draft}; }";
      live[model] = typedText;
      var enumName = "demo::order_state";
      for (var occurrence = typedText.IndexOf(enumName, StringComparison.Ordinal); occurrence >= 0;
           occurrence = typedText.IndexOf(enumName, occurrence + enumName.Length, StringComparison.Ordinal)) {
        for (var offset = occurrence; offset <= occurrence + enumName.Length; ++offset) {
          var destination = NavigationRunner.Resolve(model, offset, false, generated, live, CancellationToken.None);
          Require(destination.Length == 1 && Path.GetFileName(destination[0].file) == "order.serializer",
            "typed magic/inferred array enum cursor boundary");
          ++checks;
        }
        var definitions = NavigationRunner.Resolve(model, occurrence, true, generated, live, CancellationToken.None);
        Require(definitions.Select(item => item.file).Distinct().Count() == 11,
          "typed magic/inferred array enum definitions in every output language");
        ++checks;
      }
      live[model] = text;
      // Warning rules are field metadata; unnamed revisions preserve adjacent type navigation.
      var warningText = "serializer version 1.5.0; include sales/order; class model { " +
        "public version uint32 (1) { 1 }; public uint32 revision (2) { 1 } ignore(warning version); " +
        "public array demo::order orders (3) ignore(warning magic, version); " +
        "public demo::order ignore (4); public demo::order warning (5); }";
      live[model] = warningText;
      for (var occurrence = warningText.IndexOf("demo::order", StringComparison.Ordinal); occurrence >= 0;
           occurrence = warningText.IndexOf("demo::order", occurrence + "demo::order".Length, StringComparison.Ordinal)) {
        for (var cursor = occurrence; cursor <= occurrence + "demo::order".Length; ++cursor) {
          var declarations = NavigationRunner.Resolve(model, cursor, false, generated, live, CancellationToken.None);
          Require(declarations.Length == 1 && Path.GetFileName(declarations[0].file) == "order.serializer",
            "qualified type navigation beside warning suffix and unnamed revision");
          ++checks;
        }
        Require(NavigationRunner.Resolve(model, occurrence, true, generated, live, CancellationToken.None).Length == 11,
          "warning adjacent output navigation in every language");
        ++checks;
      }
      foreach (var metadata in new[] { "ignore(warning", "warning version", "magic, version", "orders", "ignore (4)", "warning (5)" }) {
        Require(NavigationRunner.Resolve(model, warningText.IndexOf(metadata, StringComparison.Ordinal),
          false, generated, live, CancellationToken.None).Length == 0, "warning metadata is not a type operand");
        ++checks;
      }
      live[model] = text;
      // Generic compact operands remain navigable and modifiers never become destinations.
      var compactText = "serializer version 1.2.0; include sales/order; class compact_box<T> { " +
        "public compact_prefix strict T first; public compact_varint lenient T second; " +
        "public compact_varint T third; public demo::order owner; }";
      live[model] = compactText;
      foreach (var member in new[] { "T first", "T second", "T third" }) {
        var start = compactText.IndexOf(member, StringComparison.Ordinal);
        for (var offset = start; offset <= start + 1; ++offset) {
          var destination = NavigationRunner.Resolve(model, offset, false, generated, live, CancellationToken.None);
          Require(destination.Length == 1 && destination[0].file == model &&
            compactText.Substring(destination[0].start, destination[0].end - destination[0].start) == "T",
            "compact generic type cursor boundary");
          ++checks;
        }
      }
      foreach (var modifier in new[] { "compact_prefix", "compact_varint", "strict", "lenient" }) {
        var offset = compactText.IndexOf(modifier, StringComparison.Ordinal);
        Require(NavigationRunner.Resolve(model, offset, false, generated, live, CancellationToken.None).Length == 0,
          "compact modifier is not a type reference");
        ++checks;
      }
      var compactOwner = compactText.IndexOf("demo::order", StringComparison.Ordinal);
      Require(NavigationRunner.Resolve(model, compactOwner, true, generated, live, CancellationToken.None).Length == 11,
        "adjacent compact output navigation in every language");
      ++checks;
      var variantText = "serializer version 1.4.0; include sales/order; class model { " +
        "public variant(demo::order = order, uint32 = empty) payload; }";
      live[model] = variantText;
      var variantStart = variantText.IndexOf("demo::order", StringComparison.Ordinal);
      for (var cursor = variantStart; cursor <= variantStart + "demo::order".Length; ++cursor) {
        var variantResult = NavigationRunner.Resolve(model, cursor, false, generated, live, CancellationToken.None);
        Require(variantResult.Length == 1 && File.ReadAllText(variantResult[0].file)
          .Substring(variantResult[0].start, variantResult[0].end - variantResult[0].start) == "order",
          "owning variant alternative navigation");
        ++checks;
      }
      foreach (var alias in new[] { "order,", "empty)", "payload" }) {
        Require(NavigationRunner.Resolve(model, variantText.IndexOf(alias, StringComparison.Ordinal),
          false, generated, live, CancellationToken.None).Length == 0, "variant alias is not a type operand");
        ++checks;
      }
      live[model] = text;
      // Digest selectors are metadata, while adjacent schema types and legacy names remain navigable.
      var digestText = "serializer version 1.3.0; include sales/order; class sha256 {} " +
        "class digest_box<T = digest(sha256), U = digest[32]> {} class digest_model { " +
        "public digest opaque; public digest[32] fixed; public digest(sha256) named; " +
        "public array digest(sha3_256) values; public digest_box<digest(sha512), demo::order> pair; " +
        "public demo::order owner; public union(digest(sha256)=hash, demo::order=order) choice; }";
      live[model] = digestText;
      foreach (var metadata in new[] { "sha256", "sha3_256", "sha512", "32]", "opaque", "fixed", "named", "values" }) {
        for (var start = digestText.IndexOf(metadata, digestText.IndexOf("class digest_box", StringComparison.Ordinal), StringComparison.Ordinal);
             start >= 0; start = digestText.IndexOf(metadata, start + metadata.Length, StringComparison.Ordinal)) {
          for (var offset = start; offset < start + metadata.Length; ++offset) {
            Require(NavigationRunner.Resolve(model, offset, false, generated, live, CancellationToken.None).Length == 0,
              "digest metadata is not a type reference");
            ++checks;
          }
        }
      }
      for (var occurrence = digestText.IndexOf("demo::order", StringComparison.Ordinal); occurrence >= 0;
           occurrence = digestText.IndexOf("demo::order", occurrence + "demo::order".Length, StringComparison.Ordinal)) {
        for (var offset = occurrence; offset <= occurrence + "demo::order".Length; ++offset) {
          Require(NavigationRunner.Resolve(model, offset, false, generated, live, CancellationToken.None).Length == 1,
            "type navigation beside digest selectors");
          ++checks;
        }
        Require(NavigationRunner.Resolve(model, occurrence, true, generated, live, CancellationToken.None).Length == 11,
          "digest adjacent output navigation in every language");
        ++checks;
      }
      var legacyDigest = "serializer version 1.2.0; class digest {} class old { public digest value; }";
      live[model] = legacyDigest;
      var legacyStart = legacyDigest.IndexOf("digest value", StringComparison.Ordinal);
      Require(NavigationRunner.Resolve(model, legacyStart, false, generated, live, CancellationToken.None).Single().start ==
        legacyDigest.IndexOf("digest {}", StringComparison.Ordinal), "legacy declared digest type");
      ++checks;
      live[model] = text;
      var order = Path.GetFullPath(Path.Combine(repository, "example/schemas/complex/sales/order.serializer"));
      live[order] = File.ReadAllText(order).Replace("serializer version 1;", "serializer version 1.0.0;")
        .Replace("class order ", "class unsaved_order ");
      live[model] = text.Replace("demo::order", "demo::unsaved_order");
      var unsaved = NavigationRunner.Resolve(model, live[model].IndexOf("demo::unsaved_order", StringComparison.Ordinal), false, generated, live, CancellationToken.None);
      Require(unsaved.Length == 1 && live[order].Substring(unsaved[0].start, unsaved[0].end - unsaved[0].start) == "unsaved_order", "unsaved transitive schema edits");
      using (var cancelled = new CancellationTokenSource()) {
        cancelled.Cancel();
        try {
          NavigationRunner.Resolve(model, 0, true, generated, live, cancelled.Token);
          throw new Exception("Cancelled navigation did not stop");
        } catch (OperationCanceledException) { }
      }
      if (args.Length > 2) {
        var genericOutput = Path.GetFullPath(args[2]);
        var genericModel = Path.GetFullPath(Path.Combine(repository, "example/generics/result.serializer"));
        var genericText = File.ReadAllText(genericModel);
        live[genericModel] = genericText;
        var genericStart = genericText.IndexOf("result<T>", StringComparison.Ordinal);
        var definitions = NavigationRunner.Resolve(genericModel, genericStart, true, genericOutput, live, CancellationToken.None);
        Require(definitions.Select(item => Path.GetExtension(item.file)).Distinct().Count() == 11,
          "generic definitions in all eleven output languages");
        foreach (var definition in definitions) {
          var source = NavigationRunner.Resolve(definition.file, definition.end, false, genericOutput, live, CancellationToken.None);
          Require(source.Length == 1 && source[0].file == genericModel && source[0].start == genericStart,
            "generic reverse mapping " + definition.file);
          ++checks;
        }
        var parameter = genericText.IndexOf("T value", StringComparison.Ordinal);
        var local = NavigationRunner.Resolve(genericModel, parameter, false, genericOutput, live, CancellationToken.None);
        Require(local.Length == 1 && local[0].start == genericStart + "result<".Length, "generic parameter declaration");
        ++checks;
      }
      if (args.Length > 3) {
        var inheritedOutput = Path.GetFullPath(args[3]);
        var inheritedModel = Path.Combine(Path.GetDirectoryName(inheritedOutput), "generic-inheritance.serializer");
        var inheritedText = File.ReadAllText(inheritedModel);
        // A live edit moves the field while the generated output and dependency file remain unchanged.
        live[inheritedModel] = "// Unsaved field declaration offset.\n" + inheritedText;
        var declaration = live[inheritedModel].IndexOf("inherited_count", StringComparison.Ordinal);
        var fieldDefinitions = NavigationRunner.Resolve(inheritedModel, declaration, true, inheritedOutput, live, CancellationToken.None);
        Require(fieldDefinitions.Select(item => Path.GetExtension(item.file)).Distinct().Count() == 11,
          "schema inherited field definitions in every output language");
        ++checks;
        var fieldLanguages = new HashSet<string>();
        foreach (var outputFile in Directory.GetFiles(inheritedOutput).Where(file => Path.GetExtension(file) != ".d")) {
          var outputText = File.ReadAllText(outputFile);
          foreach (Match occurrence in Regex.Matches(outputText, @"\b(?:inherited_count|inheritedCount|InheritedCount)\b")) {
            var source = NavigationRunner.Resolve(outputFile, occurrence.Index, false, inheritedOutput, live, CancellationToken.None);
            if (source.Length != 1 || source[0].file != inheritedModel || source[0].start != declaration) { continue; }
            var endpoint = NavigationRunner.Resolve(outputFile, occurrence.Index + occurrence.Length, false,
              inheritedOutput, live, CancellationToken.None);
            Require(endpoint.Length == 1 && endpoint[0].start == declaration, "inherited generic field selection endpoint");
            fieldLanguages.Add(Path.GetExtension(outputFile));
            ++checks;
            break;
          }
        }
        Require(fieldLanguages.Count == 11, "inherited generic field declarations in every output language with unsaved schema edits");
      }
      var dimensionsFile = Path.GetFullPath(Path.Combine(repository, "test/resources/dimensions.serializer"));
      var dimensionsText = File.ReadAllText(dimensionsFile);
      live[dimensionsFile] = dimensionsText;
      foreach (var parameterName in new[] { "Rows", "Cols" }) {
        var declaration = dimensionsText.IndexOf(parameterName, StringComparison.Ordinal);
        for (var occurrence = declaration; occurrence >= 0;
             occurrence = dimensionsText.IndexOf(parameterName, occurrence + parameterName.Length, StringComparison.Ordinal)) {
          for (var offset = occurrence; offset <= occurrence + parameterName.Length; ++offset) {
            var destination = NavigationRunner.Resolve(dimensionsFile, offset, false, generated, live, CancellationToken.None);
            Require(destination.Length == 1 && destination[0].file == dimensionsFile && destination[0].start == declaration,
              "dimension parameter/default/extent boundary " + parameterName);
            ++checks;
          }
        }
      }
      Console.WriteLine("Visual Studio shared resolver passed: " + checks + " source/output checks, real .NET interpreter, all 11 languages, construction/layout signatures, inherited generic fields, digest metadata/legacy names, optional operands/signatures, dimension defaults/extents, typed magic/inferred arrays, selection endpoints, unsaved includes and cancellation.");
      return 0;
    } catch (Exception error) { Console.Error.WriteLine(error); return 1; }
  }

  /// <summary>Fail the executable on any observable navigation mismatch.</summary>
  private static void Require(bool condition, string message) {
    if (!condition) { throw new Exception(message); }
  }
}
