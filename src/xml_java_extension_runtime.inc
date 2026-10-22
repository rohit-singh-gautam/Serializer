// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlextension(
/** Bound only explicit foreign roots and retained semantic storage. */
public static final class XmlExtensionLimits {
  public final int maxSlots,maxItems;
  public final long maxPreservedBytes;
  /** Select shared defaults independently from complete-document policies. */
  public XmlExtensionLimits(){this(srlXmlExtensionMaxSlots,srlXmlExtensionMaxItems,srlXmlExtensionMaxPreservedBytes);}
  /** Keep zero exact and reject negative policies before counters or storage are allocated. */
  public XmlExtensionLimits(int slots,int items,long bytes){if(slots<0||items<0||bytes<0)throw new IllegalArgumentException("Negative XML extension limit");maxSlots=slots;maxItems=items;maxPreservedBytes=bytes;}
}
/** Vocabulary understanding is independent from permission to preserve opaque payload. */
public static final class XmlExtensionOptions {
  public final java.util.List<String> understoodNamespaces;
  public final String requiredPrefixes;
  public final XmlExtensionLimits limits;
  private boolean declarationValidation;
  /** Require explicit understanding only when the selected contract requests it. */
  public XmlExtensionOptions(){this(java.util.List.of(),"",new XmlExtensionLimits());}
  /** Own a validated bounded vocabulary list without retaining the caller's mutable collection. */
  public XmlExtensionOptions(java.util.List<String> understood,String prefixes,XmlExtensionLimits limits){
    this.limits=java.util.Objects.requireNonNull(limits);if(understood.size()>limits.maxItems)throw SrlXmlRule.quota("XML capability item limit");
    long bytes=srlXmlExtensionSize(prefixes);if(bytes>srlXmlExtensionMaxPreservedBytes)throw SrlXmlRule.quota("XML capability metadata byte limit");
    for(String uri:understood){long size=srlXmlExtensionSize(uri);if(size>srlXmlExtensionMaxPreservedBytes-bytes)throw SrlXmlRule.quota("XML capability metadata byte limit");bytes+=size;}
    understoodNamespaces=java.util.List.copyOf(understood);requiredPrefixes=prefixes;
  }
}
/** Validate exact XML scalar text while measuring UTF8 bytes without allocating an encoded copy. */
private static long srlXmlExtensionSize(String text){long size=0;for(int offset=0;offset<text.length();){int value=text.codePointAt(offset);offset+=Character.charCount(value);if(!(value==9||value==10||value==13||value>=32&&value<=0xd7ff||value>=0xe000&&value<=0xfffd||value>=0x10000&&value<=0x10ffff))throw new IllegalArgumentException("Invalid XML extension character");size+=value<0x80?1:value<0x800?2:value<0x10000?3:4;}return size;}
/** Use the shared XML1.0 name ranges for namespace prefixes and opaque local names. */
private static boolean srlXmlExtensionNcName(String text){if(text.isEmpty())return false;for(int offset=0;offset<text.length();){int value=text.codePointAt(offset);if(!SrlXmlRule.nameCharacter(value,offset==0,false))return false;offset+=Character.charCount(value);}return true;}
/** Split and validate one opaque expanded element or attribute identity. */
private static String[] srlXmlExtensionName(String name){int end=name.startsWith("{")?name.indexOf('}'):-1;String uri=end<0?"":name.substring(1,end),local=end<0?name:name.substring(end+1);if(!srlXmlExtensionNcName(local)||end>=0&&uri.isEmpty()||uri.equals("http://www.w3.org/2000/xmlns/"))throw new IllegalArgumentException("Invalid XML extension expanded name");srlXmlExtensionSize(uri);return new String[]{uri,local};}
/** Retain immutable compiler-owned slot permissions without interpreting opaque lexical values. */
private static final class SrlXmlExtensionSlot {
  final String id;final boolean attribute;final int position,policy,minimum,maximum,match,process;final String[] namespaces;
  /** Bind one complete foreign-vocabulary slot; negative maximum denotes unbounded. */
  SrlXmlExtensionSlot(String id,boolean attribute,int position,String[] namespaces,int policy,int minimum,int maximum,int match,int process){this.id=id;this.attribute=attribute;this.position=position;this.namespaces=namespaces;this.policy=policy;this.minimum=minimum;this.maximum=maximum;this.match=match;this.process=process;}
}
/** Share immutable expanded permissions independently from per-owner admission counters. */
private static final class SrlXmlExtensionContract {
  final String core;final int inventory;final String[] known,knownAttributes,required;final int[] knownPositions;final SrlXmlExtensionSlot[] slots;
  final java.util.Set<String> knownNames,knownAttributeNames;
  final java.util.Map<String,Integer> attributeSlots;
  final java.util.Map<String,java.util.Map<Integer,Integer>> childSlots;
  /** Bind only compiler-validated metadata; callers cannot edit its representation. */
  SrlXmlExtensionContract(String core,String[] known,String[] knownAttributes,String[] required,SrlXmlExtensionSlot[] slots,int[] knownPositions){this(core,known,knownAttributes,required,slots,knownPositions,-1);}
  /** Carry the additive declaration inventory selector without changing existing metadata calls. */
  SrlXmlExtensionContract(String core,String[] known,String[] knownAttributes,String[] required,SrlXmlExtensionSlot[] slots,int[] knownPositions,int inventory){this.inventory=inventory;this.core=core;this.known=known;this.knownAttributes=knownAttributes;this.required=required;this.slots=slots;this.knownPositions=knownPositions;knownNames=java.util.Set.copyOf(java.util.Arrays.asList(known));knownAttributeNames=java.util.Set.copyOf(java.util.Arrays.asList(knownAttributes));var attributes=new java.util.HashMap<String,Integer>();var children=new java.util.HashMap<String,java.util.Map<Integer,Integer>>();for(int index=0;index<slots.length;++index){var slot=slots[index];if(slot.match==1||slot.match==3||slot.match==4)continue;for(String uri:slot.namespaces){if(slot.attribute)attributes.put(uri,index);else children.computeIfAbsent(uri,key->new java.util.HashMap<>()).put(slot.position,index);}}attributeSlots=java.util.Map.copyOf(attributes);var prepared=new java.util.HashMap<String,java.util.Map<Integer,Integer>>();for(var entry:children.entrySet())prepared.put(entry.getKey(),java.util.Map.copyOf(entry.getValue()));childSlots=java.util.Map.copyOf(prepared);}
}
/** Own only an explicitly preserved semantic foreign subtree, including unused QName bindings. */
private static final class SrlXmlForeignNode {
  final String name;final java.util.Map<String,String> attributes,scope;final java.util.ArrayList<Object> content=new java.util.ArrayList<>();
  /** Take already-admitted private maps; source/provider strings remain immutable values. */
  SrlXmlForeignNode(String name,java.util.Map<String,String> attributes,java.util.Map<String,String> scope){this.name=name;this.attributes=attributes;this.scope=scope;}
}
/** Place one preserved foreign root at its exact known field and repeated-item boundary. */
private static final class SrlXmlForeignChild {
  final int position,item;final SrlXmlForeignNode node;
  /** Take the completely captured node only after its retaining content cell was admitted. */
  SrlXmlForeignChild(int position,int item,SrlXmlForeignNode node){this.position=position;this.item=item;this.node=node;}
}
/** Keep XML-only preservation out of every native field and ordinary wire codec. */
private static final class SrlXmlPreserved {
  final java.util.Map<String,String> scope;final java.util.Map<String,String> attributes=new java.util.LinkedHashMap<>();final java.util.ArrayList<SrlXmlForeignChild> children=new java.util.ArrayList<>();
  /** Retain the original owner namespace scope after its complete logical charge. */
  SrlXmlPreserved(java.util.Map<String,String> scope){this.scope=scope;}
}
/** Validate one owner while retaining only explicitly preserved foreign content. */
private static final class SrlXmlExtensions {
  private static final long OWNER_BYTES=64,NODE_BYTES=64,SCOPE_ENTRY_BYTES=64,ATTRIBUTE_BYTES=48,CONTENT_ENTRY_BYTES=24;
  final SrlXmlExtensionContract meta;final java.util.Map<String,String> scope;final XmlExtensionOptions options;final int[] counts;
  int items,cursor;long bytes;SrlXmlPreserved state;final SrlXmlDeclarationPolicy validation;
  /** Validate capabilities before constructing a known candidate or beginning output traversal. */
  SrlXmlExtensions(SrlXmlExtensionContract meta,java.util.Map<String,String> scope,XmlExtensionOptions options,SrlXmlPreserved state){
    this(meta,scope,options,state,null);
  }
  /** Carry the original local policies and ledger through private declaration validation. */
  SrlXmlExtensions(SrlXmlExtensionContract meta,java.util.Map<String,String> scope,XmlExtensionOptions options,SrlXmlPreserved state,SrlXmlDeclarationPolicy validation){
    this.validation=validation;this.meta=meta;this.scope=scope;this.options=java.util.Objects.requireNonNull(options);this.state=state;
    var limits=options.limits;if(meta.slots.length>limits.maxSlots||meta.known.length>limits.maxItems||meta.knownAttributes.length>limits.maxItems||meta.required.length>limits.maxItems)throw SrlXmlRule.quota("XML extension slot limit");counts=new int[meta.slots.length];
    var understood=new java.util.HashSet<>(options.understoodNamespaces);for(String uri:meta.required)if(!uri.equals(meta.core)&&!understood.contains(uri))throw new IllegalArgumentException("Required XML capability is not understood");
    for(String prefix:SrlXmlRule.tokens(options.requiredPrefixes,limits.maxItems)){String uri=scope.get(prefix);if(!srlXmlExtensionNcName(prefix)||prefix.equals("xmlns")||uri==null||uri.isEmpty())throw new IllegalArgumentException("Unbound required XML capability prefix");if(!uri.equals(meta.core)&&!understood.contains(uri))throw new IllegalArgumentException("Required XML capability is not understood");}
    if(state!=null)charge(add(OWNER_BYTES,scopeBytes(scope)));
  }
  /** Bound partial measurement without integer overflow or allocation of corresponding retained storage. */
  long add(long first,long second){if(first<0||second<0||second>options.limits.maxPreservedBytes-first)throw SrlXmlRule.quota("XML extension preserved-byte limit");return first+second;}
  /** Charge logical bytes before retaining another owning map, node, string or content cell. */
  void charge(long count){long total=add(bytes,count);if(validation!=null){validation.charge(count);validation.budget.charge("preserved",count);}bytes=total;}
  /** Temporary declared content has no retained-byte obligation and borrows the same XML budgets. */
  void captureCharge(long count,boolean retained){if(retained)charge(count);else if(validation!=null)validation.charge(count);}
  /** Resolve strict/lax declarations independently of retention and never swallow a known declaration error. */
  SrlXmlDeclaration declaration(String name,boolean attribute,int position){var slot=meta.slots[slot(name,attribute,position)];if(slot.process==0)return null;if(validation!=null)validation.budget.charge("work",1);var declared=meta.inventory<0?null:srlXmlDeclarationInventories[meta.inventory].find(name,attribute);if(slot.process==2&&declared==null)throw new IllegalArgumentException("Strict XML wildcard declaration missing");return declared;}
  /** Attribute replay uses a real attribute field, preserving present-empty and namespace identities. */
  void validateAttribute(String name,String value){var declared=declaration(name,true,0);if(declared!=null){if(validation==null)throw new IllegalArgumentException("Missing XML declaration policies");validation.validateAttribute(meta.inventory,declared,value,scope);}}
  /** Validate complete in-scope bindings, including prefixes used only by unknown lexical QName values. */
  long scopeBytes(java.util.Map<String,String> bindings){long size=0;for(var entry:bindings.entrySet()){String prefix=entry.getKey(),uri=entry.getValue();if(!(prefix.isEmpty()||srlXmlExtensionNcName(prefix))||prefix.equals("xmlns")||prefix.equals("xml")!=uri.equals("http://www.w3.org/XML/1998/namespace")||!prefix.isEmpty()&&uri.isEmpty()||uri.equals("http://www.w3.org/2000/xmlns/"))throw new IllegalArgumentException("Invalid preserved XML namespace binding");size=add(size,add(SCOPE_ENTRY_BYTES,add(srlXmlExtensionSize(prefix),srlXmlExtensionSize(uri))));}return size;}
  /** Preserve exact attribute names and logical text without interpreting vocabulary-specific values. */
  long attributeBytes(String name,String value){srlXmlExtensionName(name);return add(ATTRIBUTE_BYTES,add(srlXmlExtensionSize(name),srlXmlExtensionSize(value)));}
  /** Keep all unqualified, core, known and reserved model names closed before consulting foreign slots. */
  int slot(String name,boolean attribute,int position){String uri=srlXmlExtensionName(name)[0];if(uri.equals("urn:serializer:xml:model")||attribute&&meta.knownAttributeNames.contains(name))return -1;Integer index=attribute?meta.attributeSlots.get(uri):meta.childSlots.getOrDefault(uri,java.util.Map.of()).get(position);if(index!=null&&knownAllowed(index,name,attribute,position))return index;for(int ordinal=0;ordinal<meta.slots.length;++ordinal){var slot=meta.slots[ordinal];if(slot.attribute==attribute&&(attribute||slot.position<0||slot.position==position)&&(slot.match==1||(slot.match==3||slot.match==4)&&!java.util.Arrays.asList(slot.namespaces).contains(uri))&&knownAllowed(ordinal,name,attribute,position))return ordinal;}return -1;}
  /** Dispatch an active known field first, retaining completed names only in generated wildcard positions. */
  boolean knownAllowed(int index,String name,boolean attribute,int position){if(attribute||!meta.knownNames.contains(name))return true;var slot=meta.slots[index];int known=java.util.Arrays.asList(meta.known).indexOf(name);return slot.match!=0&&slot.match!=4&&(known<meta.knownPositions.length&&meta.knownPositions[known]>=0&&meta.knownPositions[known]<position||counts[index]<slot.minimum);}
  /** Enforce item, per-slot occurrence and policy bounds before capturing or skipping one root. */
  boolean admit(String name,boolean attribute,int position){int index=slot(name,attribute,position);if(index<0)throw new IllegalArgumentException("XML extension has no permitted slot");var selected=meta.slots[index];if(selected.policy==srlXmlExtensionReject)throw new IllegalArgumentException("XML extension slot rejects foreign vocabulary");if(items>=options.limits.maxItems)throw SrlXmlRule.quota("XML extension item/occurrence limit");if(selected.maximum>=0&&counts[index]>=selected.maximum)throw new IllegalArgumentException("XML extension item/occurrence limit");++items;++counts[index];return selected.policy==srlXmlExtensionPreserve&&!options.declarationValidation;}
  /** Allocate no preserved sidecar for skip-only traversals. */
  SrlXmlPreserved storage(){if(state==null){charge(add(OWNER_BYTES,scopeBytes(scope)));state=new SrlXmlPreserved(new java.util.LinkedHashMap<>(scope));}return state;}
  /** Retain an expanded foreign attribute only after its preserve slot and bytes are admitted. */
  void attribute(String name,String value){boolean preserve=admit(name,true,0);validateAttribute(name,value);if(preserve){charge(attributeBytes(name,value));storage().attributes.put(name,value);}}
  /** Measure one edited or captured semantic node before allocating its owning copies. */
  void measure(String name,java.util.Map<String,String> attributes,java.util.Map<String,String> scope){srlXmlExtensionName(name);long size=add(NODE_BYTES,add(srlXmlExtensionSize(name),scopeBytes(scope)));for(var attribute:attributes.entrySet())size=add(size,attributeBytes(attribute.getKey(),attribute.getValue()));charge(size);}
)xmlextension"
R"xmlextension(
  /** Capture one foreign root from typed events; skipped subtrees retain no payload or namespace copies. */
  SrlXmlForeignNode foreign(SrlXmlInput input,boolean preserve){return foreign(input,preserve,preserve);}
  /** Capture validation-only nodes under XML/work/allocation policies without retaining opaque state. */
  SrlXmlForeignNode foreign(SrlXmlInput input,boolean preserve,boolean retained){
    SrlXmlForeignNode node=null;StringBuilder pending=preserve?new StringBuilder():null;
    if(preserve){String name=input.name();var bindings=input.namespaceScope();long size=NODE_BYTES+srlXmlExtensionSize(name);if(retained)size=add(size,scopeBytes(bindings));else for(var entry:bindings.entrySet())size+=SCOPE_ENTRY_BYTES+srlXmlExtensionSize(entry.getKey())+srlXmlExtensionSize(entry.getValue());for(int index=0;index<input.reader.getAttributeCount();++index){long amount=ATTRIBUTE_BYTES+srlXmlExtensionSize(srlXmlName(input.reader.getAttributeNamespace(index),input.reader.getAttributeLocalName(index)))+srlXmlExtensionSize(input.reader.getAttributeValue(index));size=retained?add(size,amount):size+amount;}captureCharge(size,retained);
      var attributes=new java.util.LinkedHashMap<String,String>();for(int index=0;index<input.reader.getAttributeCount();++index)attributes.put(srlXmlName(input.reader.getAttributeNamespace(index),input.reader.getAttributeLocalName(index)),input.reader.getAttributeValue(index));node=new SrlXmlForeignNode(name,attributes,bindings);}
    input.advance();while(input.kind!='e'){
      if(input.kind=='t'){if(preserve){String text=input.reader.getText();captureCharge(srlXmlExtensionSize(text),retained);pending.append(text);}input.advance();}
      else if(input.kind=='s'){if(preserve&&pending.length()!=0){captureCharge(CONTENT_ENTRY_BYTES,retained);node.content.add(pending.toString());pending.setLength(0);}var child=foreign(input,preserve,retained);if(preserve){captureCharge(CONTENT_ENTRY_BYTES,retained);node.content.add(child);}}
      else throw new IllegalArgumentException("Incomplete foreign XML subtree");
    }
    if(preserve&&pending.length()!=0){captureCharge(CONTENT_ENTRY_BYTES,retained);node.content.add(pending.toString());}input.advance();return node;
  }
  /** Consume only foreign roots permitted at this exact field and repeated-item boundary. */
  void readAt(SrlXmlInput input,int position,int item){readAt(input,position,item,true);}
  // Preserve text events at mixed end slots so retained suffixes cannot move across native text.
  void readAt(SrlXmlInput input,int position,int item,boolean skipWhitespace){if(skipWhitespace)input.skipSpace();while(input.kind=='s'&&slot(input.name(),false,position)>=0){if(item>0&&meta.slots[slot(input.name(),false,position)].match!=0&&meta.slots[slot(input.name(),false,position)].match!=4)break;boolean preserve=admit(input.name(),false,position);var declared=declaration(input.name(),false,position);var node=foreign(input,preserve||declared!=null,preserve);if(declared!=null){if(validation==null)throw new IllegalArgumentException("Missing XML declaration policies");validation.validate(meta.inventory,declared,node);}if(preserve&&node!=null){charge(CONTENT_ENTRY_BYTES);storage().children.add(new SrlXmlForeignChild(position,item,node));}if(skipWhitespace)input.skipSpace();}}
  /** Require every explicit occurrence floor before publishing a complete native owner. */
  void finish(){for(int index=0;index<counts.length;++index)if(counts[index]<meta.slots[index].minimum)throw new IllegalArgumentException("Required XML extension occurrence missing");}
  /** Emit retained attributes only while the known owner's start tag remains pending. */
  void emitAttributes(SrlXmlOutput output){if(state!=null)for(var entry:state.attributes.entrySet()){if(!admit(entry.getKey(),true,0))throw new IllegalArgumentException("Retained XML attribute no longer has a preserve slot");validateAttribute(entry.getKey(),entry.getValue());charge(attributeBytes(entry.getKey(),entry.getValue()));output.attribute(entry.getKey(),entry.getValue());}}
  /** Replay only opaque foreign semantic nodes through the same bounded token writer. */
  void emitNode(SrlXmlOutput output,SrlXmlForeignNode node){measure(node.name,node.attributes,node.scope);output.open(node.name,node.scope);for(var attribute:node.attributes.entrySet())output.attribute(attribute.getKey(),attribute.getValue());for(Object content:node.content){charge(CONTENT_ENTRY_BYTES);if(content instanceof String text){charge(srlXmlExtensionSize(text));output.text(text);}else if(content instanceof SrlXmlForeignNode child)emitNode(output,child);else throw new IllegalArgumentException("Invalid retained XML extension content");}output.close();}
  /** Preserve original unknown-root order around exact native repeated items after known fields are edited. */
  void emitAt(SrlXmlOutput output,int position,int item){if(state==null)return;while(cursor<state.children.size()){var child=state.children.get(cursor);if(child.position<position||child.position==position&&child.item<item)throw new IllegalArgumentException("Preserved XML extension no longer fits edited field position");if(child.position!=position||child.item!=item)return;if(!admit(child.node.name,false,position))throw new IllegalArgumentException("Retained XML child no longer has a preserve slot");var declared=declaration(child.node.name,false,position);if(declared!=null){if(validation==null)throw new IllegalArgumentException("Missing XML declaration policies");validation.validate(meta.inventory,declared,child.node);}charge(CONTENT_ENTRY_BYTES);emitNode(output,child.node);++cursor;}}
  /** Reject extension positions lost when native collections were shortened or removed. */
  void endPosition(int position){if(state!=null&&cursor<state.children.size()&&state.children.get(cursor).position==position)throw new IllegalArgumentException("Preserved XML extension no longer fits edited field position");}
  /** Require complete replay and all minima before returning complete owning XML bytes. */
  void finishOutput(){if(state!=null&&cursor!=state.children.size())throw new IllegalArgumentException("Unreplayed XML extension position");finish();}
}
)xmlextension"
