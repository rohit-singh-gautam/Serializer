// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlnamespacejava(
/** Private XML-only per-field occurrence scopes; native values and codecs keep their existing layout. */
private static final class SrlXmlNamespaceValues {
  private final java.util.Map<Integer,java.util.List<java.util.Map<String,String>>> fields=new java.util.HashMap<>();
  private java.util.Map<String,String> fallback=java.util.Map.of();
  /** Resolve one complete lexical name without accepting malformed Unicode or reserved prefixes. */
  private static String[] resolve(String token,int mode,java.util.Map<String,String> scope){
    String prefix="",local=token;if(mode==2){prefix=token;local="";}else{int colon=token.indexOf(':');if(colon>=0){prefix=token.substring(0,colon);local=token.substring(colon+1);}}
    if((mode==2||token.indexOf(':')>=0)&&(!srlXmlExtensionNcName(prefix)||prefix.equals("xmlns"))||mode!=2&&!srlXmlExtensionNcName(local))throw new IllegalArgumentException("Invalid XML QName or prefix");
    String uri=prefix.equals("xml")?"http://www.w3.org/XML/1998/namespace":scope.getOrDefault(prefix,"");if(!prefix.isEmpty()&&uri.isEmpty())throw new IllegalArgumentException("Unbound XML QName prefix");return new String[]{prefix,uri,local};
  }
  /** Copy valid explicit native-authoring bindings before replacing existing occurrence metadata. */
  void set(java.util.Map<String,String> bindings){
    var copy=new java.util.LinkedHashMap<String,String>();for(var entry:bindings.entrySet()){String prefix=entry.getKey(),uri=entry.getValue();if(!(prefix.isEmpty()||srlXmlExtensionNcName(prefix))||prefix.equals("xmlns")||prefix.equals("xml")!=uri.equals("http://www.w3.org/XML/1998/namespace")||!prefix.isEmpty()&&uri.isEmpty()||uri.equals("http://www.w3.org/2000/xmlns/"))throw new IllegalArgumentException("Invalid XML QName namespace binding");srlXmlExtensionSize(uri);copy.put(prefix,uri);}fallback=copy;fields.clear();
  }
  /** Retain only used bindings after cumulative scope-byte admission, preserving absent optionals. */
  String capture(SrlXmlInput input,int field,String text,java.util.Map<String,String> scope,int mode){
    if(text==null)return null;String[] tokens=SrlXmlRule.tokens(text,input.listLimit());if(mode==0&&tokens.length!=1)throw new IllegalArgumentException("XML QName requires one name");var retained=new java.util.LinkedHashMap<String,String>();
    for(String token:tokens){var name=resolve(token,mode,scope);if(!retained.containsKey(name[0])){int size=utf8(name[0]).length+utf8(name[1]).length;if(size>input.budget.limits.maxStringBytes-input.namespaceBytes)throw new IllegalArgumentException("XML QName scope byte limit exceeded");input.namespaceBytes+=size;retained.put(name[0],name[1]);}}
    fields.computeIfAbsent(field,ignored->new java.util.ArrayList<>()).add(retained);return text;
  }
  /** Capture child-local overrides before consuming its scalar and restoring its parent scope. */
  String scalar(SrlXmlInput input,int field,String name,int mode){if(!input.has(name))throw new IllegalArgumentException("Missing XML QName element");var scope=input.namespaceScope();return capture(input,field,input.scalar(name),scope,mode);}
  /** Regenerate collision-free declarations while the exact output head remains pending. */
  String output(SrlXmlOutput output,int field,int occurrence,String text,int mode){
    var scopes=fields.get(field);var scope=scopes!=null&&occurrence<scopes.size()?scopes.get(occurrence):fallback;String[] tokens=SrlXmlRule.tokens(text,output.listLimit());if(mode==0&&tokens.length!=1)throw new IllegalArgumentException("XML QName requires one name");var lexical=new StringBuilder();
    for(String token:tokens){var name=resolve(token,mode,scope);String prefix="";if(!name[1].isEmpty()){prefix=name[1].equals("http://www.w3.org/XML/1998/namespace")?"xml":output.matchingPrefix(name[1],true);if(prefix==null)prefix=output.freshPrefix("q",name[1]);output.declare(prefix,name[1]);}else if(!"".equals(output.identityNamespace("")))throw new IllegalArgumentException("Unqualified XML QName conflicts with output default namespace");if(lexical.length()!=0)lexical.append(' ');if(!prefix.isEmpty()){lexical.append(prefix);if(mode!=2)lexical.append(':');}if(mode!=2)lexical.append(name[2]);}return lexical.toString();
  }
}
)xmlnamespacejava"
