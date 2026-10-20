// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejs(
  const mcNamespace = 'http://schemas.openxmlformats.org/markup-compatibility/2006', xmlNamespace = 'http://www.w3.org/XML/1998/namespace';
  // Apply OPC's exact Relationships compatibility configuration without interpreting application payloads.
  function relationshipsCompatibility(document, budget) {
    // Keep compatibility failures distinct from budget and cancellation failures.
    const bad = () => { throw new PackageError(Code.InvalidMetadata, 'Invalid Relationships markup compatibility contract'); };
    // Resolve original lexical prefixes into inherited namespace identities.
    const prefix = (node, value) => { budget.consume('workUnits', value.length + 1); if (!SrlXmlNcName.test(value)) bad(); const selected = value === 'xml' ? xmlNamespace : node.ns[value]; if (!selected || selected === mcNamespace) bad(); return selected; };
    // Tokenize only XML whitespace under a precharged temporary allocation allowance.
    const tokens = value => { budget.consume('workUnits', value.length + 1); budget.consume('decoderAllocationBytes', value.length * 4); const text = collapse(value); return text ? text.split(' ') : []; };
    // Search borrowed ancestor declarations without copying growing scope sets.
    const ignorable = (scope, uri) => { for (let current = scope; current; current = current.parent) for (const candidate of current.ignored) { budget.consume('workUnits', 1); if (candidate === uri) return true; } return false; };
    // Match expanded ProcessContent names and namespace wildcards across prefix rebindings.
    const content = (scope, node) => { for (let current = scope; current; current = current.parent) for (const [uri, local] of current.content) { budget.consume('workUnits', 1); if (uri === node.uri && (local === '*' || local === node.local)) return true; } return false; };
    // Validate declaration syntax even on discarded elements, retaining only local rule values.
    const declarations = (node, parent) => {
      budget.check(); budget.consume('decoderAllocationBytes', 64); const scope = {parent, ignored: [], content: []};
      for (const attr of node.attrs) if (attr.uri === mcNamespace && attr.local === 'Ignorable') for (const token of tokens(attr.value)) { budget.consume('decoderAllocationBytes', 8); scope.ignored.push(prefix(node, token)); }
      for (const attr of node.attrs) {
        budget.consume('workUnits', 1); if (attr.uri !== mcNamespace) continue;
        if (attr.local === 'ProcessContent') for (const token of tokens(attr.value)) {
          const at = token.indexOf(':'), local = token.slice(at + 1); if (at <= 0 || local !== '*' && !SrlXmlNcName.test(local)) bad();
          const uri = prefix(node, token.slice(0, at)); if (!ignorable(scope, uri)) bad(); budget.consume('decoderAllocationBytes', 64); scope.content.push([uri, local]);
        } else if (attr.local === 'MustUnderstand') { for (const token of tokens(attr.value)) prefix(node, token); }
        else if (attr.local !== 'Ignorable') bad();
      }
      return scope;
    };
    // Capabilities are checked only on retained, unwrapped or selected content.
    const mustUnderstand = node => { for (const attr of node.attrs) if (attr.uri === mcNamespace && attr.local === 'MustUnderstand') for (const token of tokens(attr.value)) if (prefix(node, token) !== relNamespace) bad(); };
    // Validate complete Choice/Fallback wrapper attributes before considering branch selection.
    const wrapper = (node, scope, choice) => {
      let requires = null;
      for (const attr of node.attrs) {
        budget.consume('workUnits', 1);
        if (!attr.uri) { if (!choice || attr.local !== 'Requires') bad(); requires = attr.value; }
        else if (attr.uri !== mcNamespace && (attr.uri === xmlNamespace || !ignorable(scope, attr.uri))) bad();
      }
      if (!choice) return false; if (requires === null || !collapse(requires)) bad(); let supported = true;
      for (const token of tokens(requires)) if (prefix(node, token) !== relNamespace) supported = false;
      return supported;
    };
    // Flatten selected fragments; text is retained for the final root and child-content rules.
    const children = (node, scope) => {
      const result = []; budget.consume('decoderAllocationBytes', node.children.length * 8);
      for (const child of node.children) { const action = process(child, scope, false); for (const selected of action.nodes) result.push(selected); if (action.unwrap) node.text += child.text; }
      node.children = result;
    };
    // Select the first understood Choice, otherwise one correctly ordered Fallback.
    const alternate = (node, scope) => {
      wrapper(node, scope, false); if (collapse(node.text)) bad(); let choices = 0, selected = null, fallback = false;
      for (const child of node.children) {
        const childScope = declarations(child, scope);
        if (child.uri !== relNamespace && ignorable(childScope, child.uri) && !content(childScope, child)) continue;
        if (child.uri !== mcNamespace) bad();
        if (child.local === 'Choice' && !fallback) { ++choices; const supported = wrapper(child, childScope, true); if (selected === null && supported) selected = child; }
        else if (child.local === 'Fallback' && choices && !fallback) { fallback = true; wrapper(child, childScope, false); if (selected === null) selected = child; }
        else bad();
      }
      if (!choices) bad(); mustUnderstand(node);
      if (selected === null) { node.children = []; node.text = ''; }
      else { process(selected, scope, true); node.children = selected.children; node.text = selected.text; }
    };
    // Process one element with scoped namespace identities before discarded content can affect OPC.
    const process = (node, parent, selected) => {
      budget.check(); budget.consume('workUnits', 1); const scope = declarations(node, parent);
      if (node.uri !== relNamespace && ignorable(scope, node.uri)) {
        if (!content(scope, node)) return {nodes: [], unwrap: false};
        if (node.attrs.some(attr => attr.uri === xmlNamespace && ['base', 'lang', 'space'].includes(attr.local))) bad();
        mustUnderstand(node); children(node, scope); return {nodes: node.children, unwrap: true};
      }
      if (node.uri === mcNamespace) {
        if (node.local === 'AlternateContent') alternate(node, scope);
        else { if (!selected || !['Choice', 'Fallback'].includes(node.local)) bad(); wrapper(node, scope, node.local === 'Choice'); mustUnderstand(node); children(node, scope); }
        return {nodes: node.children, unwrap: true};
      }
      mustUnderstand(node); children(node, scope);
      budget.consume('decoderAllocationBytes', node.attrs.length * 64); node.attrs = node.attrs.filter(attr => attr.uri !== mcNamespace && (attr.uri === relNamespace || !ignorable(scope, attr.uri)));
      return {nodes: [node], unwrap: false};
    };
    const output = process(document, null, false);
    if (output.nodes.length !== 1 || output.unwrap && collapse(document.text)) bad(); return output.nodes[0];
  }
)packagejs"
