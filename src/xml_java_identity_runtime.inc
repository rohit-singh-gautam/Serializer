// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"identityjava(
/** Immutable bounds for one owner-local key/unique/keyref index, independent of ordinary XML limits. */
public static final class XmlIdentityLimits {
  public final int maxConstraints,maxFields,maxDepth,maxSelectedNodes,maxTuples,maxTupleBytes,maxWorkUnits;
  /** Derive default admission from the shared compiled XML identity contract. */
  public XmlIdentityLimits(){this(SRL_XML_IDENTITY_MAX_CONSTRAINTS,SRL_XML_IDENTITY_MAX_FIELDS,SRL_XML_IDENTITY_MAX_DEPTH,SRL_XML_IDENTITY_MAX_SELECTED_NODES,SRL_XML_IDENTITY_MAX_TUPLES,SRL_XML_IDENTITY_MAX_TUPLE_BYTES,SRL_XML_IDENTITY_MAX_WORK_UNITS);}
  /** Reject negative policies before any instance input or native state is touched. */
  public XmlIdentityLimits(int constraints,int fields,int depth,int selected,int tuples,int bytes,int work){
    if(constraints<0||fields<0||depth<0||selected<0||tuples<0||bytes<0||work<0)throw new IllegalArgumentException("Negative XML identity limit");
    maxConstraints=constraints;maxFields=fields;maxDepth=depth;maxSelectedNodes=selected;maxTuples=tuples;maxTupleBytes=bytes;maxWorkUnits=work;
  }
}
/** Compiler-owned expanded paths contain no runtime XPath syntax or mutable document state. */
private static final class SrlXmlIdentityPath {
  final String[] steps;final boolean descendants;final String attribute;
  /** Bind the private immutable projection emitted by the compiler. */
  SrlXmlIdentityPath(String[] steps,boolean descendants,String attribute){this.steps=steps;this.descendants=descendants;this.attribute=attribute;}
}
private static final class SrlXmlIdentityField {
  final SrlXmlIdentityPath path;final String kind;final SrlXmlRule rule;final boolean nillable;
  /** Retain the scalar value space independently of its native storage type. */
  SrlXmlIdentityField(SrlXmlIdentityPath path,String kind,boolean nillable){this.path=path;this.kind=kind;this.nillable=nillable;this.rule=new SrlXmlRule(kind,kind.equals("string")?"preserve":kind.equals("normalizedString")?"replace":"collapse",0,new String[]{"","","",""},-1,-1,-1,new String[0],false,new SrlXmlRule.Pattern[0][],"",new SrlXmlRule[0],null);}
}
private static final class SrlXmlIdentityConstraint {
  final String kind;final SrlXmlIdentityPath selector;final SrlXmlIdentityField[] fields;final int refer;
  /** Bind the checked owner-local reference index and ordered tuple fields. */
  SrlXmlIdentityConstraint(String kind,SrlXmlIdentityPath selector,SrlXmlIdentityField[] fields,int refer){this.kind=kind;this.selector=selector;this.fields=fields;this.refer=refer;}
}
private static final class SrlXmlIdentityScope {
  final SrlXmlIdentityConstraint[] constraints;
  /** Own only prepared metadata; an operation supplies the actual overridden owner element name. */
  SrlXmlIdentityScope(SrlXmlIdentityConstraint[] constraints){this.constraints=constraints;}
}
/** Incremental bounded tuples and open scalar captures; no known-element tree is constructed. */
private static final class SrlXmlIdentity {
  final SrlXmlIdentityScope scope;final XmlIdentityLimits limits;final Limits.XmlOperation operation;final String packageField;
  final java.util.ArrayList<String> path=new java.util.ArrayList<>();
  final java.util.ArrayList<Candidate> active=new java.util.ArrayList<>();
  final java.util.ArrayList<java.util.HashSet<String>> tables=new java.util.ArrayList<>();
  int selected,tuples,tupleBytes;long workUnits;boolean complete,failed;
  private static final class Capture {
    final int depth;final boolean nil;final StringBuilder text=new StringBuilder();int bytes;
    /** Capture only one selected scalar at its exact element depth. */
    Capture(int depth,boolean nil){this.depth=depth;this.nil=nil;}
  }
  private static final class Candidate {
    final int constraint,depth;final String[] values;final boolean[] seen;final Capture[] captures;int bytes;
    /** Admit selection before allocating a bounded candidate tuple. */
    Candidate(int constraint,int depth,int fields){this.constraint=constraint;this.depth=depth;values=new String[fields];seen=new boolean[fields];captures=new Capture[fields];}
  }
  /** Allocate only checked static constraint tables for this independent owner scope. */
  SrlXmlIdentity(SrlXmlIdentityScope scope,XmlIdentityLimits limits){this(scope,limits,null,"");}
  /** Capture original owner context for deferred tuple and forward-reference work. */
  SrlXmlIdentity(SrlXmlIdentityScope scope,XmlIdentityLimits limits,Limits.XmlOperation operation,String field){
    this.scope=scope;this.limits=limits;this.operation=operation;this.packageField=field;
    charge("allocation",128L*scope.constraints.length);
    if(scope.constraints.length>limits.maxConstraints)throw SrlXmlRule.quota("XML identity constraint limit");
    for(var item:scope.constraints){if(item.fields.length==0||item.fields.length>limits.maxFields)throw SrlXmlRule.quota("XML identity field limit");tables.add(new java.util.HashSet<>());}
  }
  /** Count logical UTF8 bytes without allocating a temporary encoded string. */
  static int bytes(String value){int count=0;for(int offset=0;offset<value.length();){int scalar=value.codePointAt(offset);offset+=Character.charCount(scalar);int width=scalar<128?1:scalar<2048?2:scalar<65536?3:4;if(width>Integer.MAX_VALUE-count)throw new IllegalArgumentException("XML identity byte overflow");count+=width;}return count;}
  /** Charge before selector comparison or tuple work; zero remains exact admission. */
  void charge(String resource,long count){if(operation!=null)operation.consume(resource,count,packageField);}
  /** Charge selector work to the enclosing operation before local admission. */
  void work(long count){charge("work",count);if(count>limits.maxWorkUnits-workUnits)throw SrlXmlRule.quota("XML identity work limit");workUnits+=count;}
  /** Compare expanded paths without allocating suffix arrays or interpreting XPath. */
  boolean matches(int first,SrlXmlIdentityPath expected){
    int count=path.size()-first;work((long)count+expected.steps.length+1);
    if(expected.descendants){if(count<expected.steps.length)return false;first=path.size()-expected.steps.length;}else if(count!=expected.steps.length)return false;
    for(int index=0;index<expected.steps.length;++index){String actual=path.get(first+index),wanted=expected.steps[index];work((long)Math.min(bytes(actual),bytes(wanted))+1);if(!actual.equals(wanted))return false;}
    return true;
  }
  /** Normalize one exact builtin scalar to a collision-free typed tuple value. */
  String key(String lexical,SrlXmlIdentityField field,java.util.function.Function<String,String> resolve){
    String kind=field.kind;
    int size=bytes(lexical);if(size>limits.maxTupleBytes)throw SrlXmlRule.quota("XML identity field-byte limit");work((long)size+1);
    if(kind.equals("QName")){
      String value=srlXmlTrim(lexical);String[] parts=value.split(":",-1);String prefix="",local=value;
      if(parts.length==2&&!parts[0].isEmpty()&&!parts[1].isEmpty()){prefix=parts[0];local=parts[1];}else if(parts.length!=1||value.isEmpty())throw new IllegalArgumentException("Invalid XML identity QName");
      var rule=new SrlXmlRule("NCName","collapse",0,new String[]{"","","",""},-1,-1,-1,new String[0],false,new SrlXmlRule.Pattern[0][],"",new SrlXmlRule[0],null);
      rule.check(local);if(!prefix.isEmpty())rule.check(prefix);String uri=resolve.apply(prefix);if(!prefix.isEmpty()&&uri==null)throw new IllegalArgumentException("Undeclared XML identity QName prefix");
      return "q"+(uri==null?"":uri)+'\0'+local;
    }
    String value=field.rule.check(lexical);
    if(kind.equals("boolean"))return srlXmlBoolean(value)?"btrue":"bfalse";
    if(kind.equals("decimal")||SrlXmlRule.integerKind(kind)||SrlXmlRule.integerWidth(kind)!=0){var decimal=new java.math.BigDecimal(value).stripTrailingZeros();return "d"+(decimal.signum()==0?"0":decimal.toPlainString());}
    if(kind.equals("float")||kind.equals("double"))throw new IllegalArgumentException("Floating XML identity fields are unsupported");
    return (kind.equals("anyURI")?"u":"s")+value;
  }
  /** Bound every partial canonical tuple before retaining the next field. */
  void store(Candidate candidate,int index,String value){int count=bytes(value)-(value.startsWith("q")?2:1);if(count>limits.maxTupleBytes-candidate.bytes)throw SrlXmlRule.quota("XML identity candidate tuple-byte limit");candidate.bytes+=count;charge("allocation",2L*value.length());candidate.values[index]=value;}
  /** Observe a complete current start tag, including namespaces needed by QName attributes. */
  void start(String name,java.util.Map<String,String> attributes,java.util.function.Function<String,String> resolve){
    try{
      if(complete||failed)throw new IllegalArgumentException("XML identity observer inactive");work((long)bytes(name)+1);if(path.size()>=limits.maxDepth)throw SrlXmlRule.quota("XML identity depth limit");
      for(var candidate:active)for(var capture:candidate.captures)if(capture!=null)throw new IllegalArgumentException("XML identity scalar has complex child content");
      path.add(name);
      for(int index=0;index<scope.constraints.length;++index){var constraint=scope.constraints[index];if(matches(1,constraint.selector)){if(selected>=limits.maxSelectedNodes)throw SrlXmlRule.quota("XML identity selected-node limit");++selected;active.add(new Candidate(index,path.size(),constraint.fields.length));}}
      for(var candidate:active){var constraint=scope.constraints[candidate.constraint];for(int index=0;index<constraint.fields.length;++index){var field=constraint.fields[index];if(!matches(candidate.depth,field.path)||field.path.attribute!=null&&!attributes.containsKey(field.path.attribute))continue;
        if(constraint.kind.equals("key")&&field.nillable)throw new IllegalArgumentException("XSD key field selects an element declared nillable");
        if(candidate.seen[index])throw new IllegalArgumentException("XML identity field selects multiple values");candidate.seen[index]=true;
        if(field.path.attribute!=null)store(candidate,index,key(attributes.get(field.path.attribute),field,resolve));
        else{String nil=srlXmlTrim(attributes.getOrDefault("{http://www.w3.org/2001/XMLSchema-instance}nil","false"));if(!nil.equals("true")&&!nil.equals("1")&&!nil.equals("false")&&!nil.equals("0"))throw new IllegalArgumentException("Malformed XML identity nil");candidate.captures[index]=new Capture(path.size(),nil.equals("true")||nil.equals("1"));}
      }}
    }catch(RuntimeException failure){failed=true;throw failure;}
  }
  /** Charge logical UTF8 text independent of provider chunk/entity/CDATA boundaries. */
  void text(String value){try{if(complete||failed||path.isEmpty())throw new IllegalArgumentException("XML identity observer inactive");int count=bytes(value);work(count);for(var candidate:active)for(var capture:candidate.captures)if(capture!=null){work(count);if(capture.nil&&!value.isEmpty())throw new IllegalArgumentException("Nil XML identity scalar has text");if(count>limits.maxTupleBytes-capture.bytes)throw SrlXmlRule.quota("XML identity field-byte limit");capture.bytes+=count;capture.text.append(value);}}catch(RuntimeException failure){failed=true;throw failure;}}
  /** Close captures while their original namespace scope still resolves QName values. */
  void end(java.util.function.Function<String,String> resolve){
    try{
      if(complete||failed||path.isEmpty())throw new IllegalArgumentException("Unbalanced XML identity end");work(1);
      for(var candidate:active){var constraint=scope.constraints[candidate.constraint];for(int index=0;index<candidate.captures.length;++index){work(1);var capture=candidate.captures[index];if(capture!=null&&capture.depth==path.size()){if(!capture.nil)store(candidate,index,key(capture.text.toString(),constraint.fields[index],resolve));candidate.captures[index]=null;}}}
      for(var iterator=active.iterator();iterator.hasNext();){var candidate=iterator.next();if(candidate.depth!=path.size())continue;iterator.remove();var constraint=scope.constraints[candidate.constraint];boolean missing=false;for(var value:candidate.values)if(value==null)missing=true;
        if(missing){if(constraint.kind.equals("key"))throw new IllegalArgumentException("XSD key tuple is incomplete or nil");continue;}
        if(tuples>=limits.maxTuples)throw SrlXmlRule.quota("XML identity tuple-count limit");if(candidate.bytes>limits.maxTupleBytes-tupleBytes)throw SrlXmlRule.quota("XML identity tuple-byte limit");charge("reference",1);charge("allocation",128L+candidate.bytes);++tuples;tupleBytes+=candidate.bytes;work((long)candidate.bytes+1);
        var tuple=new StringBuilder();for(var value:candidate.values)tuple.append(value.length()).append(':').append(value);boolean added=tables.get(candidate.constraint).add(tuple.toString());if(!added&&!constraint.kind.equals("keyref"))throw new IllegalArgumentException("Duplicate XSD key/unique tuple");
      }
      path.remove(path.size()-1);if(path.isEmpty())complete=true;
    }catch(RuntimeException failure){failed=true;throw failure;}
  }
  /** Resolve forward references only after every checked tuple in this owner scope exists. */
  void finish(){
    if(failed||!complete||!path.isEmpty()||!active.isEmpty())throw new IllegalArgumentException("Incomplete or failed XML identity scope");
    try{work(1);for(int index=0;index<scope.constraints.length;++index){var constraint=scope.constraints[index];if(!constraint.kind.equals("keyref"))continue;var target=tables.get(constraint.refer);for(String tuple:tables.get(index)){work((long)bytes(tuple)+1);if(!target.contains(tuple))throw new IllegalArgumentException("Unresolved XSD keyref tuple");}}}catch(RuntimeException failure){failed=true;throw failure;}
  }
}
)identityjava"
