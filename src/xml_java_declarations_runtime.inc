// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"javadecl(
/** One permitted named derived type selects its own complete private codecs. */
private static final class SrlXmlDeclarationType {
  final String name;final int index,nilIndex;final boolean abstractType;
  /** Store only immutable compiler-resolved identities and derivation permissions. */
  SrlXmlDeclarationType(String name,int index,int nilIndex,boolean abstractType){this.name=name;this.index=index;this.nilIndex=nilIndex;this.abstractType=abstractType;}
}
/** A pinned declaration is separate from the decision to preserve its instance bytes. */
private static final class SrlXmlDeclaration {
  final String name,declaredType,root;final boolean attribute,nillable,abstractElement,fixed,abstractType;final int index,nilIndex;final SrlXmlDeclarationType[] types;
  /** Bind exact expanded identities before any untrusted instance is inspected. */
  SrlXmlDeclaration(String name,boolean attribute,boolean nillable,boolean abstractElement,boolean fixed,int index,int nilIndex,String declaredType,String root,boolean abstractType,SrlXmlDeclarationType[] types){this.name=name;this.attribute=attribute;this.nillable=nillable;this.abstractElement=abstractElement;this.fixed=fixed;this.index=index;this.nilIndex=nilIndex;this.declaredType=declaredType;this.root=root;this.abstractType=abstractType;this.types=types;}
}
/** Find absence independently from declared invalid content so lax never swallows an error. */
private static final class SrlXmlDeclarations {
  final java.util.Map<String,SrlXmlDeclaration> elements=new java.util.HashMap<>(),attributes=new java.util.HashMap<>();
  /** Populate only compiler-owned declarations; duplicate declarations are compiler errors. */
  SrlXmlDeclarations(SrlXmlDeclaration[] declarations){for(var declaration:declarations)(declaration.attribute?attributes:elements).put(declaration.name,declaration);}
  /** Attribute and element declaration spaces remain distinct. */
  SrlXmlDeclaration find(String name,boolean attribute){return(attribute?attributes:elements).get(name);}
}
/** Borrow all local policies and the original cumulative operation without package-root permissions. */
private static final class SrlXmlDeclarationPolicy {
  final SrlXmlBudget budget;final XmlMapLimits maps;final XmlMixedLimits mixed;final XmlIdentityLimits identity;final XmlDigestLimits digests;final XmlExtensionOptions extensions;
  /** Capture only policy references; instance/provider storage remains borrowed. */
  SrlXmlDeclarationPolicy(SrlXmlInput input){budget=input.budget;maps=input.mapLimits;mixed=input.mixedLimits;identity=input.identityLimits;digests=input.digestLimits;extensions=input.extensionOptions;}
  /** Revalidate edited preserved nodes under the same output operation. */
  SrlXmlDeclarationPolicy(SrlXmlOutput output){budget=output.budget;maps=output.mapLimits;mixed=output.mixedLimits;identity=output.identityLimits;digests=output.digestLimits;extensions=output.extensionOptions;}
  /** Admit temporary allocation before building its corresponding owning value. */
  void charge(long count){budget.charge("work",count);budget.charge("allocation",count);}
  /** Framing may expand namespace presentation while source and operation limits remain authoritative. */
  Limits transport(){final int framingExpansion=16;charge(64);var source=budget.limits;int bytes=(int)Math.min(Integer.MAX_VALUE,(long)source.maxBytes*framingExpansion);return new Limits(bytes,Math.max(bytes,source.maxStringBytes),source.maxElements,source.maxDepth,source.readPolicy,source.xmlOperation);}
  /** Suppress opaque retention only inside discarded private DTOs, retaining all original scalar and slot checks. */
  XmlExtensionOptions privateExtensions(){charge(64L+extensions.understoodNamespaces.size()*32L);var result=new XmlExtensionOptions(extensions.understoodNamespaces,extensions.requiredPrefixes,extensions.limits);result.declarationValidation=true;return result;}
  /** Select only permitted named types and reject abstract/default or unrelated instance types. */
  int[] type(SrlXmlDeclaration declaration,SrlXmlForeignNode node){
    charge(32);String actual=declaration.declaredType,lexical=node.attributes.get("{http://www.w3.org/2001/XMLSchema-instance}type");
    if(lexical!=null){charge(128L+lexical.length()*4L);var name=SrlXmlNamespaceValues.resolve(srlXmlTrim(lexical),0,node.scope);actual=srlXmlName(name[1],name[2]);
      if(!actual.equals(declaration.declaredType)){for(var candidate:declaration.types){budget.charge("work",1);if(candidate.name.equals(actual)){if(candidate.abstractType)throw new IllegalArgumentException("Abstract wildcard xsi:type");return new int[]{candidate.index,candidate.nilIndex};}}throw new IllegalArgumentException("Declared wildcard xsi:type mismatch");}}
    if(declaration.abstractType)throw new IllegalArgumentException("Abstract wildcard declared type");return new int[]{declaration.index,declaration.nilIndex};
  }
  /** Emit only a temporary bounded foreign subtree, stripping authorized root transport markers. */
  void emit(SrlXmlOutput output,SrlXmlForeignNode node,boolean strip){
    output.open(node.name,node.scope);for(var entry:node.attributes.entrySet()){if(strip&&(entry.getKey().equals("{http://www.w3.org/2001/XMLSchema-instance}nil")||entry.getKey().equals("{http://www.w3.org/2001/XMLSchema-instance}type")))continue;output.attribute(entry.getKey(),entry.getValue());}
    for(Object content:node.content){budget.charge("work",1);if(content instanceof String text)output.text(text);else if(content instanceof SrlXmlForeignNode child)emit(output,child,false);else throw new IllegalArgumentException("Invalid declared wildcard subtree");}output.close();
  }
  /** Complete private decoding preserves the exact same operation and validates all required nil attributes. */
  void validate(int inventory,SrlXmlDeclaration declaration,SrlXmlForeignNode node){
    if(declaration.abstractElement)throw new IllegalArgumentException("Abstract wildcard declaration");var selected=type(declaration,node);boolean nil=false;String marker=node.attributes.get("{http://www.w3.org/2001/XMLSchema-instance}nil");
    if(marker!=null){if(!declaration.nillable)throw new IllegalArgumentException("Wildcard declaration is not nillable");marker=srlXmlTrim(marker);if(!marker.equals("true")&&!marker.equals("false")&&!marker.equals("1")&&!marker.equals("0"))throw new IllegalArgumentException("Invalid wildcard xsi:nil");nil=marker.equals("true")||marker.equals("1");
      if(nil){if(declaration.fixed||selected[1]<0)throw new IllegalArgumentException("Fixed wildcard cannot be nil");for(Object content:node.content)if(!(content instanceof String text)||!text.isEmpty())throw new IllegalArgumentException("Nil wildcard has content");}}
    var limits=transport();var output=new SrlXmlOutput(limits,maps,mixed,identity,digests,extensions);emit(output,node,true);byte[] bytes=output.finish();
    try(var input=new SrlXmlInput(bytes,limits,maps,mixed,identity,digests,privateExtensions())){srlXmlDeclarationRead(inventory,nil?selected[1]:selected[0],input,node.name);input.finish();}
  }
  /** A true attribute wrapper preserves present-empty values and the original complete namespace scope. */
  void validateAttribute(int inventory,SrlXmlDeclaration declaration,String value,java.util.Map<String,String> scope){
    var limits=transport();var output=new SrlXmlOutput(limits,maps,mixed,identity,digests,extensions);output.open(declaration.root,scope);output.attribute(declaration.name,value);output.close();byte[] bytes=output.finish();
    try(var input=new SrlXmlInput(bytes,limits,maps,mixed,identity,digests,privateExtensions())){srlXmlDeclarationRead(inventory,declaration.index,input,declaration.root);input.finish();}
  }
}
)javadecl"
