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
    return resolve(token,mode,scope,false);
  }
  /** Preserve a failed QName arm's vocabulary without requiring an ordinary string to bind prefixes. */
  private static String[] resolve(String token,int mode,java.util.Map<String,String> scope,boolean allowUnbound){
    String prefix="",local=token;if(mode==2){prefix=token;local="";}else{int colon=token.indexOf(':');if(colon>=0){prefix=token.substring(0,colon);local=token.substring(colon+1);}}
    if((mode==2||token.indexOf(':')>=0)&&(!srlXmlExtensionNcName(prefix)||prefix.equals("xmlns"))||mode!=2&&!srlXmlExtensionNcName(local))throw new IllegalArgumentException("Invalid XML QName or prefix");
    String uri=prefix.equals("xml")?"http://www.w3.org/XML/1998/namespace":scope.getOrDefault(prefix,"");if(!allowUnbound&&!prefix.isEmpty()&&uri.isEmpty())throw new IllegalArgumentException("Unbound XML QName prefix");return new String[]{prefix,uri,local};
  }
  /** Copy valid explicit native-authoring bindings before replacing existing occurrence metadata. */
  void set(java.util.Map<String,String> bindings){
    var copy=new java.util.LinkedHashMap<String,String>();for(var entry:bindings.entrySet()){String prefix=entry.getKey(),uri=entry.getValue();if(!(prefix.isEmpty()||srlXmlExtensionNcName(prefix))||prefix.equals("xmlns")||prefix.equals("xml")!=uri.equals("http://www.w3.org/XML/1998/namespace")||!prefix.isEmpty()&&uri.isEmpty()||uri.equals("http://www.w3.org/2000/xmlns/"))throw new IllegalArgumentException("Invalid XML QName namespace binding");srlXmlExtensionSize(uri);copy.put(prefix,uri);}fallback=copy;fields.clear();
  }
  /** Retain only used bindings after cumulative scope-byte admission, preserving absent optionals. */
  String capture(SrlXmlInput input,int field,String text,java.util.Map<String,String> scope,int mode,SrlXmlRule rule,boolean attribute){
    if(rule!=null&&rule.defaultText!=null&&(text==null||!attribute&&text.isEmpty())){text=rule.defaultText;scope=rule.defaultScope;}
    if(text==null)return null;if(rule!=null){var checked=rule.checkNamespace(text,scope,input.listLimit(),input.budget);text=checked.checked;if(mode==3)mode=checked.selected.namespaceMode();}
    String[] tokens=SrlXmlRule.tokens(text,mode<0?text.length():input.listLimit());if(mode==0&&tokens.length!=1)throw new IllegalArgumentException("XML QName requires one name");var retained=new java.util.LinkedHashMap<String,String>();
    for(String token:tokens){String[] name;if(mode<0){try{name=resolve(token,0,scope,true);}catch(IllegalArgumentException invalidName){continue;}}else{name=resolve(token,mode,scope);}if(!retained.containsKey(name[0])){int size=utf8(name[0]).length+utf8(name[1]).length;if(size>input.budget.limits.maxStringBytes-input.namespaceBytes)throw SrlXmlRule.quota("XML QName scope byte limit exceeded");input.budget.charge("allocation",128L+size);input.budget.charge("work",size);input.namespaceBytes+=size;retained.put(name[0],name[1]);}}
    fields.computeIfAbsent(field,ignored->new java.util.ArrayList<>()).add(retained);return text;
  }
  /** Capture child-local overrides before consuming its scalar and restoring its parent scope. */
  String scalar(SrlXmlInput input,int field,String name,int mode,SrlXmlRule rule){if(!input.has(name))throw new IllegalArgumentException("Missing XML QName element");var scope=input.namespaceScope();return capture(input,field,input.scalar(name),scope,mode,rule,false);}
  /** Preserve the owner default only when it affects the first successful union member. */
  String preferredDefault(SrlXmlOutput output,int field,String text,SrlXmlRule rule){var values=fields.get(field);var scope=values!=null&&!values.isEmpty()?values.get(0):fallback;var selected=rule.checkNamespace(text,scope,output.listLimit(),output.budget).selected;if(selected.namespaceMode()>=0){if(rule.keepNamespaceLexical)for(String token:SrlXmlRule.tokens(text,output.listLimit()))if(token.indexOf(':')<0)return scope.getOrDefault("","");return null;}output.budget.charge("allocation",128L+scope.size()*64L);var cleared=new java.util.LinkedHashMap<>(scope);cleared.put("","");return rule.checkNamespace(text,cleared,output.listLimit(),output.budget).selected!=selected?scope.getOrDefault("",""):null;}
  /** Regenerate collision-free declarations while the exact output head remains pending. */
  String output(SrlXmlOutput output,int field,int occurrence,String text,int mode,SrlXmlRule rule){
    var scopes=fields.get(field);var scope=scopes!=null&&occurrence<scopes.size()?scopes.get(occurrence):fallback;SrlXmlRule selected=null;
    if(rule!=null){var checked=rule.checkNamespace(text,scope,output.listLimit(),output.budget);text=checked.checked;selected=checked.selected;if(mode==3)mode=selected.namespaceMode();}if(mode<0){if(rule.checkNamespace(text,outputScope(output,text),output.listLimit(),output.budget).selected!=selected){output.namespaceStringDefault(scope.getOrDefault("",""));if(rule.checkNamespace(text,outputScope(output,text),output.listLimit(),output.budget).selected!=selected)throw new IllegalArgumentException("XML QName output would change selected string member");}return text;}
    if(mode!=2&&output.omitNamespaceDefault(text,scope,rule))return text;
    String[] tokens=SrlXmlRule.tokens(text,output.listLimit());if(mode==0&&tokens.length!=1)throw new IllegalArgumentException("XML QName requires one name");var lexical=new StringBuilder();var rendered=new java.util.LinkedHashMap<String,String>();rendered.put("",output.identityNamespace(""));
    // A QName pattern and a prefix-list string restriction observe aliases, including default absence.
    if(rule!=null&&rule.keepNamespaceLexical){for(String token:tokens){var name=resolve(token,mode,scope);if(name[0].isEmpty()){output.namespaceStringDefault(name[1]);output.markUnqualifiedQName();}else output.declare(name[0],name[1]);}if(rule.checkNamespace(text,outputScope(output,text),output.listLimit(),output.budget).selected!=selected)throw new IllegalArgumentException("XML QName output changed selected union member");return text;}
    for(String token:tokens){var name=resolve(token,mode,scope);String prefix="";if(!name[1].isEmpty()){prefix=name[1].equals("http://www.w3.org/XML/1998/namespace")?"xml":output.matchingPrefix(name[1],true);if(prefix==null)prefix=output.freshPrefix("q",name[1]);output.declare(prefix,name[1]);}else if(!"".equals(output.identityNamespace("")))throw new IllegalArgumentException("Unqualified XML QName conflicts with output default namespace");if(name[1].isEmpty())output.markUnqualifiedQName();rendered.put(prefix,name[1]);if(lexical.length()!=0)lexical.append(' ');if(!prefix.isEmpty()){lexical.append(prefix);if(mode!=2)lexical.append(':');}if(mode!=2)lexical.append(name[2]);}
    String result=lexical.toString();if(rule!=null&&rule.checkNamespace(result,rendered,output.listLimit(),output.budget).selected!=selected)throw new IllegalArgumentException("XML QName output changed selected union member");return result;
  }
  /** Check actual inherited output bindings before returning an uninterpreted union string. */
  private static java.util.Map<String,String> outputScope(SrlXmlOutput output,String text){output.budget.charge("allocation",128L+text.length()*4L);var result=new java.util.LinkedHashMap<String,String>();result.put("",java.util.Objects.requireNonNullElse(output.identityNamespace(""),""));for(String token:SrlXmlRule.tokens(text,text.length())){int colon=token.indexOf(':');if(colon>=0){String prefix=token.substring(0,colon);result.put(prefix,java.util.Objects.requireNonNullElse(output.identityNamespace(prefix),""));}}return result;}
}
)xmlnamespacejava"
