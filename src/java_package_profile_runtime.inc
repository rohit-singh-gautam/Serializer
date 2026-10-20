// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejava(
  /** Parsed standard infrastructure retains payload metadata without reading external targets. */
  private static final class Opc {
    final java.util.Map<String,PartMetadata> parts=new java.util.LinkedHashMap<>();
    final java.util.List<Relationship> links=new java.util.ArrayList<>();
  }
  /** Check scoped relationship IDs, inert type/target URIs, source membership and internal closure. */
  private static java.util.List<Relationship> relationships(java.util.List<Relationship> links,java.util.Map<String,?> parts,State state) throws java.io.IOException {
    var seen=new java.util.HashSet<String>();var result=new java.util.ArrayList<Relationship>();
    for(Relationship link:links) {
      try{state.consume(Resource.RELATIONSHIPS,1,link.source);state.consume(Resource.REFERENCES,1,link.source);}catch(PackageError failure){if(state.first==failure||state.budget.policyFailure==failure)throw failure;throw new PackageError(failure.code,failure.getMessage(),link.source,0,"relationships",link.target,link.id);}
      if(!ncname(link.id))throw error(Code.INVALID_RELATIONSHIP,"Invalid relationship ID",link.source);
      String source="/".equals(link.source)?"/":partName(link.source,false);
      if(!source.equals("/")&&!parts.containsKey(fold(source)))throw error(Code.MISSING_PART,"Relationship source is missing",source);
      if(!seen.add(fold(source)+"\u0000"+link.id))throw error(Code.INVALID_RELATIONSHIP,"Duplicate source-local relationship ID",source);
      uri(link.type,true);
      if("internal".equals(link.mode)){String destination=target(source,link.target);if(!parts.containsKey(fold(destination)))throw new PackageError(Code.MISSING_PART,"Relationship target is missing",source,0,"",destination,link.id);}
      else if("external".equals(link.mode))uri(link.target,false);
      else throw error(Code.INVALID_RELATIONSHIP,"Invalid relationship target mode",source);
      result.add(new Relationship(source,link.id,link.type,link.target,link.mode));
    }
    return result;
  }
  /** Require one exact infrastructure root and leaf metadata declarations after optional MCE. */
  private static java.util.List<Node> declarations(byte[] bytes,String namespace,String local,String part,State state,boolean compatibility) throws java.io.IOException {
    Node root=metadata(bytes,part,state);if(compatibility)root=new Compatibility(state,part).apply(root);
    if(!named(root,namespace,local)||!root.attributes.isEmpty())throw error(Code.INVALID_METADATA,"Invalid OPC infrastructure root",part);
    elementContent(root,part);for(Node child:root.children){if(!child.children.isEmpty())throw error(Code.INVALID_METADATA,"Nested OPC metadata declaration",part);elementContent(child,part);}return root.children;
  }
  /** Read content types and all relationship parts before profile-specific role dispatch. */
  private static Opc opcRead(ZipReader archive,State state) throws java.io.IOException {
    Entry manifest=null;var defaults=new java.util.HashMap<String,String>();var overrides=new java.util.HashMap<String,String>();
    for(Entry entry:archive.entries){state.consume(Resource.WORK_UNITS,1,"");if(fold(entry.name).equals("[content_types].xml")){if(manifest!=null)throw new PackageError(Code.DUPLICATE_NAME,"Duplicate content type manifest");if(!entry.regularFile())throw new PackageError(Code.INVALID_METADATA,"Content type manifest is not a physical file");manifest=entry;}}
    if(manifest==null)throw new PackageError(Code.INVALID_METADATA,"Content type manifest is missing");
    for(Node child:declarations(archive.read(manifest.name),CT,"Types","/[Content_Types].xml",state,false)) {
      if(named(child,CT,"Default")) {
        attributes(child,manifest.name,"Extension","ContentType");String extension=fold(attribute(child,"Extension",manifest.name));
        if(!extension.matches("(?:[A-Za-z0-9_~!$&'()*+,:=@-]|%[0-9A-Fa-f]{2})+")||defaults.containsKey(extension))throw new PackageError(Code.INVALID_METADATA,"Invalid or duplicate default extension");
        String media=rawAttribute(child,"ContentType",manifest.name);mime(media);defaults.put(extension,media);
      } else if(named(child,CT,"Override")) {
        attributes(child,manifest.name,"PartName","ContentType");String name=fold(partName(attribute(child,"PartName",manifest.name),true));
        if(overrides.containsKey(name))throw new PackageError(Code.INVALID_METADATA,"Duplicate content type override");String media=rawAttribute(child,"ContentType",manifest.name);mime(media);overrides.put(name,media);
      } else throw new PackageError(Code.INVALID_METADATA,"Unknown content type declaration");
    }
    if(defaults.isEmpty()&&overrides.isEmpty())throw new PackageError(Code.INVALID_METADATA,"Empty content type manifest");
    var result=new Opc();var mapped=new java.util.HashSet<String>();var identities=new java.util.HashSet<String>();var relationshipParts=new java.util.LinkedHashMap<String,String>();
    for(Entry entry:archive.entries) {
      state.consume(Resource.WORK_UNITS,entry.name.length()+1L,entry.name);if(entry==manifest)continue;
      String name="/"+entry.name,key=fold(name);boolean accepted=entry.regularFile()&&mapsToPart(name,state);
      if(!accepted){archive.read(entry.name);continue;}
      unique(identities,name);mapped.add(key);String media=overrides.get(key);
      if(media==null){int dot=name.lastIndexOf('.'),slash=name.lastIndexOf('/');media=dot>slash?defaults.get(fold(name.substring(dot+1))):null;}
      if(media==null)throw error(Code.INVALID_METADATA,"Part has no content type",name);
      String source=relationshipSource(name);Mime parsed=mime(media);
      if(source!=null){if(!parsed.media.equals(RELS_MIME))throw error(Code.INVALID_METADATA,"Relationship part has wrong content type",name);relationshipParts.put(name,source);}
      else {partName(name,false);if(parsed.media.equals(RELS_MIME))throw error(Code.INVALID_METADATA,"Relationship media type requires infrastructure",name);result.parts.put(key,new PartMetadata(name,media,entry));}
    }
    for(String name:overrides.keySet())if(!mapped.contains(name))throw error(Code.INVALID_METADATA,"Content type override names a missing or reserved part",name);
    var links=new java.util.ArrayList<Relationship>();
    for(var item:relationshipParts.entrySet()) {
      String name=item.getKey(),source=item.getValue();if(!source.equals("/")&&!result.parts.containsKey(fold(source)))throw error(Code.MISSING_PART,"Relationship-part source is missing",name);
      for(Node child:declarations(archive.read(name.substring(1)),RELS,"Relationships",name,state,true)) {
        if(!named(child,RELS,"Relationship"))throw error(Code.INVALID_METADATA,"Invalid relationship declaration",name);
        attributes(child,name,"Id","Type","Target","TargetMode");String mode=child.attributes.get(new QName("","TargetMode"));mode=mode==null?"Internal":collapse(mode);
        if(!(mode.equals("Internal")||mode.equals("External")))throw error(Code.INVALID_RELATIONSHIP,"Invalid TargetMode",name);
        links.add(new Relationship(source,attribute(child,"Id",name),attribute(child,"Type",name),attribute(child,"Target",name),fold(mode)));
      }
    }
    result.links.addAll(relationships(links,result.parts,state));state.poll();return result;
  }
  /** Measure exact escaped UTF8 before retaining any generated metadata text. */
  private static long escapedSize(String value) throws java.io.IOException {
    long bytes=0;for(int offset=0;offset<value.length();) {
      int code=value.codePointAt(offset);offset+=Character.charCount(code);
      if(!(code==9||code==10||code==13||code>=32&&code<=0xd7ff||code>=0xe000&&code<=0xfffd||code>=0x10000&&code<=0x10ffff))throw new PackageError(Code.INVALID_METADATA,"Invalid XML metadata character");
      bytes+=switch(code){case '&'->5;case '<'->4;case '"'->6;case 9->4;case 10,13->5;default->code<128?1:code<2048?2:code<65536?3:4;};
    }return bytes;
  }
  /** Append already admitted attribute text directly, avoiding an unbounded escaped temporary. */
  private static void escape(StringBuilder text,String value) {
    for(int offset=0;offset<value.length();){int code=value.codePointAt(offset);offset+=Character.charCount(code);switch(code){case '&'->text.append("&amp;");case '<'->text.append("&lt;");case '"'->text.append("&quot;");case 9->text.append("&#9;");case 10->text.append("&#10;");case 13->text.append("&#13;");default->text.appendCodePoint(code);}}
  }
  /** Build bounded standard infrastructure; callers never supply raw XML declarations. */
  private static byte[] metadataWrite(String namespace,String root,String child,java.util.List<java.util.Map<String,String>> declarations,State state) throws java.io.IOException {
    String prefix="<?xml version=\"1.0\" encoding=\"UTF-8\"?><"+root+" xmlns=\""+namespace+"\">";long size=prefix.length()+root.length()+3L;
    for(var attributes:declarations){state.consume(Resource.WORK_UNITS,1,"");size+=child.length()+3L;for(var attribute:attributes.entrySet()){state.consume(Resource.WORK_UNITS,attribute.getValue().length()+1L,"");size+=attribute.getKey().length()+4L+escapedSize(attribute.getValue());}}
    int length=arraySize(size,state.budget.limits.maxPartBytes,"");state.consume(Resource.METADATA_BYTES,size,"");state.consume(Resource.DECODER_ALLOCATION_BYTES,size*3,"");state.budget.hold(size*3,"");
    try {
      var text=new StringBuilder(length);text.append(prefix);
      for(var attributes:declarations){state.poll();text.append('<').append(child);for(var attribute:new java.util.TreeMap<>(attributes).entrySet()){text.append(' ').append(attribute.getKey()).append("=\"");escape(text,attribute.getValue());text.append('"');}text.append("/>");}
      text.append("</").append(root).append('>');byte[] bytes=text.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8);state.poll();return bytes;
    }finally{state.budget.release(size*3);}
  }
  /** Count a generated metadata result during the writer's simultaneous private input snapshot. */
  private static void addMetadata(ZipWriter writer,String name,String namespace,String root,String child,java.util.List<java.util.Map<String,String>> declarations,State state) throws java.io.IOException {byte[] bytes=metadataWrite(namespace,root,child,declarations,state);state.budget.hold(bytes.length,name);try{writer.add(name,bytes,"deflate");}finally{state.budget.release(bytes.length);}}
)packagejava"
R"packagejava(
  /** Assemble standard metadata and all payloads before a caller sink or final path is touched. */
  private static byte[] opcWrite(java.util.List<BinaryPart> parts,java.util.List<Relationship> links,State state) throws java.io.IOException {
    var identities=new java.util.HashSet<String>();var lookup=new java.util.LinkedHashMap<String,BinaryPart>();
    for(BinaryPart part:parts){partName(part.name,false);Mime type=mime(part.contentType);if(type.media.equals(RELS_MIME))throw error(Code.INVALID_METADATA,"Relationship media type requires infrastructure",part.name);lookup.put(unique(identities,part.name),part);}
    var checked=relationships(links,lookup,state);var groups=new java.util.TreeMap<String,java.util.List<Relationship>>();for(Relationship link:checked)groups.computeIfAbsent(link.source,key->new java.util.ArrayList<>()).add(link);
    var declarations=new java.util.ArrayList<java.util.Map<String,String>>();
    try(var writer=new ZipWriter(state.budget,state)) {
      var ordered=new java.util.ArrayList<>(parts);ordered.sort(java.util.Comparator.comparing(part->part.name));
      for(BinaryPart part:ordered){writer.add(part.name.substring(1),part.data,part.compression);declarations.add(java.util.Map.of("PartName",part.name,"ContentType",part.contentType));}
      for(var group:groups.entrySet()) {
        String source=group.getKey(),name=source.equals("/")?"/_rels/.rels":source.substring(0,source.lastIndexOf('/')+1)+"_rels/"+source.substring(source.lastIndexOf('/')+1)+".rels";
        unique(identities,name);var children=new java.util.ArrayList<java.util.Map<String,String>>();group.getValue().sort(java.util.Comparator.comparing(link->link.id));
        for(Relationship link:group.getValue()){var attrs=new java.util.LinkedHashMap<String,String>();attrs.put("Id",link.id);attrs.put("Type",link.type);attrs.put("Target",link.target);if(link.mode.equals("external"))attrs.put("TargetMode","External");children.add(attrs);}
        addMetadata(writer,name.substring(1),RELS,"Relationships","Relationship",children,state);declarations.add(java.util.Map.of("PartName",name,"ContentType",RELS_MIME));
      }
      if(declarations.isEmpty()) {
        addMetadata(writer,"[Content_Types].xml",CT,"Types","Default",java.util.List.of(java.util.Map.of("Extension","rels","ContentType",RELS_MIME)),state);
      } else addMetadata(writer,"[Content_Types].xml",CT,"Types","Override",declarations,state);
      byte[] bytes=writer.finish();state.poll();return bytes;
    }
  }
  /** Declarative schema roles keep generated typed access independent of reflection. */
  public static final class Role {
    public final String policy,expectedRoot,expectedType,role;
    public final String field,name,contentType,source,relationship,id,compression;public final long minimum,maximum;
    /** Retain immutable schema constants; a negative maximum means unbounded. */
    public Role(String field,String name,String contentType,String source,String relationship,String id,String compression,long minimum,long maximum){this(field,name,contentType,source,relationship,id,compression,minimum,maximum,"defaults",null,null,field);}
    /** Bind fixed envelope and immutable expected XML identities. */
    public Role(String field,String name,String contentType,String source,String relationship,String id,String compression,long minimum,long maximum,String policy,String root,String type,String role){this.field=field;this.name=name;this.contentType=contentType;this.source=source;this.relationship=relationship;this.id=id;this.compression=compression;this.minimum=minimum;this.maximum=maximum;this.role=role;this.policy=policy;this.expectedRoot=root;this.expectedType=type;}
  }
  /** Resolve one closed compression override without replacing the payload identity. */
  public static Role selectProfile(Role role,java.util.Map<String,String> profiles) throws java.io.IOException {String selected=profiles.getOrDefault(role.role,role.compression);if(!(selected.equals("stored")||selected.equals("deflate"))||role.policy.equals("fixed")&&!selected.equals(role.compression))throw new PackageError(Code.INVALID_OPTIONS,"Conflicting package profile override","",0,role.field,"","");return new Role(role.field,role.name,role.contentType,role.source,role.relationship,role.id,selected,role.minimum,role.maximum,role.policy,role.expectedRoot,role.expectedType,role.role);}
  /** Complete typed application inventory; no caller callback implements the declared checks. */
  public static final class References {
    private final Budget budget;private final java.util.Map<String,String> keys=new java.util.HashMap<>();private final java.util.List<String[]> pending=new java.util.ArrayList<>();private String part="",id="";
    /** Borrow the operation ledger through complete candidate validation. */
    public References(Budget budget){this.budget=budget;}
    /** Select an immutable actual part/link before admitting this model's selectors. */
    public void location(String part,String id){this.part=part;this.id=id;}
    /** Charge only runtime-owned admission errors; arbitrary caller failures retain exact identity. */
    private void consume(Resource resource,long amount,String field)throws java.io.IOException{try{budget.consume(resource,amount,part);}catch(PackageError failure){if(budget.policyFailure==failure)throw failure;throw new PackageError(failure.code,failure.getMessage(),part,0,field,"",id);}}
    /** Reject duplicate scoped value identities before retaining an index entry. */
    public void key(String scope,String value,String role,String field,String modelType)throws java.io.IOException{String path=role+'.'+field,slot=scope+'\0'+value;consume(Resource.REFERENCES,1,path);consume(Resource.WORK_UNITS,slot.length()+1,path);consume(Resource.DECODER_ALLOCATION_BYTES,128L+2L*slot.length(),path);if(keys.putIfAbsent(slot,modelType)!=null)throw new PackageError(Code.SEMANTIC,"Duplicate application key",part,0,path,scope,id);}
    /** Retain every forward reference and its exact logical source context. */
    public void reference(String scope,String value,String target,String role,String field)throws java.io.IOException{String path=role+'.'+field;consume(Resource.REFERENCES,1,path);consume(Resource.DECODER_ALLOCATION_BYTES,160L+2L*value.length(),path);pending.add(new String[]{scope+'\0'+value,target,path,part,id});}
    /** Resolve complete forward requests before returning any owning aggregate. */
    public void finish()throws java.io.IOException{for(String[] item:pending){location(item[3],item[4]);consume(Resource.WORK_UNITS,item[0].length()+1,item[2]);if(!item[1].equals(keys.get(item[0])))throw new PackageError(Code.SEMANTIC,"Missing or wrong-type application key",part,0,item[2],item[0].substring(0,item[0].indexOf('\0')),id);}}
  }
  /** A native XML encoder receives a narrowed policy; nested native allocations remain a documented scope limit. */
  @FunctionalInterface public interface Encoder<T>{byte[] encode(T value,@outer_name@.Limits limits) throws java.io.IOException;}
  /** A native XML decoder returns a fresh owning value before aggregate publication. */
  @FunctionalInterface public interface Decoder<T>{T decode(byte[] bytes,@outer_name@.Limits limits) throws java.io.IOException;}
  /** Domain validation runs only after the complete declared aggregate exists. */
  @FunctionalInterface public interface Validator<T>{void validate(T value,Budget budget) throws java.io.IOException;}
  /** Share logical scalar/token/identity admission while native heap peaks remain separately measured. */
  private static @outer_name@.Limits nativeLimits(State state,Role role,String part,String id) throws java.io.IOException {
    int bytes=arraySize(state.budget.limits.maxPartBytes,Integer.MAX_VALUE-8,"");
    int elements=(int)Math.min(1000000,state.budget.remaining(Resource.WORK_UNITS));var limits=new @outer_name@.Limits(bytes,bytes,elements,64,@outer_name@.ReadPolicy.STRICT,(resource,amount,field)->{
      try{Resource selected=switch(resource){case "allocation"->Resource.DECODER_ALLOCATION_BYTES;case "reference"->Resource.REFERENCES;default->Resource.WORK_UNITS;};state.consume(selected,amount,part);}
      catch(java.io.IOException failure){if(state.first==null){PackageError located=failure instanceof PackageError value?new PackageError(value.code,value.getMessage(),part,0,role.field+(field.isEmpty()?"":"."+field),"",id):null;state.first=located==null?failure:located;}throw new IllegalArgumentException("Package native admission failed",failure);}
    });limits.xmlExpectedRoot=role.expectedRoot;limits.xmlExpectedType=role.expectedType;return limits;
  }
  /** Attach only role context to model-codec failures without copying payload excerpts. */
  private static PackageError codec(Role role,String name,String id,Exception cause) {var failure=new PackageError(cause instanceof SrlXmlRule.LimitFailure?Code.RESOURCE_LIMIT:Code.CODEC,"Package typed XML codec rejected part",name,0,role.field,"",id);failure.initCause(cause);return failure;}
  /** Tag only runtime quotas raised during this synchronous package encoder invocation. */
  private static <T> byte[] encodeNative(Encoder<T> encoder,T value,State state,Role role,String part,String id)throws java.io.IOException{boolean previous=SrlXmlRule.enterPackageQuota();try{return encoder.encode(value,nativeLimits(state,role,part,id));}finally{SrlXmlRule.leavePackageQuota(previous);}}
  /** Restore the prior thread-local marker even when a decoder or caller callback fails. */
  private static <T> T decodeNative(Decoder<T> decoder,byte[] bytes,State state,Role role,String part,String id)throws java.io.IOException{boolean previous=SrlXmlRule.enterPackageQuota();try{return decoder.decode(bytes,nativeLimits(state,role,part,id));}finally{SrlXmlRule.leavePackageQuota(previous);}}
  /** Check aggregate cardinality after all source-scoped role occurrences are counted. */
  private static void occurs(Role role,long count) throws java.io.IOException {if(count<role.minimum||role.maximum>=0&&count>role.maximum)throw new PackageError(Code.SEMANTIC,"Package role cardinality failed","",0,role.field,"","");}
  /** Statically generated facade calls compose one complete role at a time in source order. */
  public static final class Composer {
    private final State state;private final java.util.Map<String,java.util.List<String>> sources=new java.util.HashMap<>();
    private final java.util.List<BinaryPart> parts=new java.util.ArrayList<>();private final java.util.List<Relationship> links=new java.util.ArrayList<>();
    /** Share one policy ledger across every model, binary part and infrastructure stage. */
    public Composer(Budget budget){state=new State(budget);}
    /** Resolve one role's declared parent occurrence before model serialization. */
    private String parent(Role role,int index,String name) throws java.io.IOException {
      var parents=role.source.equals("/")?java.util.List.of("/"):sources.get(role.source);if(parents==null||index<0||index>=parents.size())throw error(Code.INVALID_RELATIONSHIP,"Role source occurrence is missing",name);return parents.get(index);
    }
    /** Validate role metadata and retain a checked payload/link occurrence. */
    private void add(Role role,byte[] data,String dynamicName,String dynamicType,int parent,int index,java.util.List<String> discovered) throws java.io.IOException {
      String name=role.name==null?dynamicName:role.name,media=role.contentType==null?dynamicType:role.contentType;partName(name,false);mime(media);
      String source=parent(role,parent,name);parts.add(new BinaryPart(data,name,media,parent,role.compression));links.add(new Relationship(source,role.id==null?"srl_"+role.field+"_"+index:role.id,role.relationship,name));discovered.add(name);
    }
    /** Encode one fixed typed role without changing existing native model APIs. */
    public <T> void single(Role role,T value,Encoder<T> encoder) throws java.io.IOException {var items=new java.util.ArrayList<ModelPart<T>>();if(value!=null)items.add(new ModelPart<>(value,role.name,role.contentType,0));models(role,items,encoder);}
    /** Encode repeated/dynamic typed values with source-occurrence routing. */
    public <T> void models(Role role,java.util.List<ModelPart<T>> values,Encoder<T> encoder) throws java.io.IOException {
      state.begin();try{java.util.Objects.requireNonNull(values);occurs(role,values.size());var discovered=new java.util.ArrayList<String>();int index=0;
        for(ModelPart<T> item:values){state.poll();if(item==null||item.value==null)throw error(Code.CODEC,"Missing typed role value",role.name==null?"":role.name);String name=role.name==null?item.name:role.name;parent(role,item.sourceIndex,name);partName(name,false);mime(role.contentType==null?item.contentType:role.contentType);byte[] data;
          try{data=encodeNative(encoder,item.value,state,role,name,role.id==null?"srl_"+role.field+"_"+index:role.id);}catch(IllegalArgumentException failure){if(state.first!=null)throw state.fail(state.first);throw codec(role,name,role.id==null?"srl_"+role.field+"_"+index:role.id,failure);}state.poll();if(data==null)throw error(Code.CODEC,"Typed encoder returned no bytes",name);state.consume(Resource.DECODER_ALLOCATION_BYTES,data.length,name);add(role,data,item.name,item.contentType,item.sourceIndex,index++,discovered);}
        sources.put(role.field,discovered);
      }catch(java.io.IOException|RuntimeException|Error failure){state.closed=true;throw state.fail(failure);}finally{state.busy=false;}
    }
    /** Retain opaque occurrences as whole bytes without XML/model interpretation. */
    public void binaries(Role role,java.util.List<BinaryPart> values) throws java.io.IOException {
      state.begin();try{java.util.Objects.requireNonNull(values);occurs(role,values.size());var discovered=new java.util.ArrayList<String>();int index=0;for(BinaryPart item:values){state.poll();if(item==null||item.data==null)throw error(Code.CODEC,"Missing binary role value","");add(role,item.data,item.name,item.contentType,item.sourceIndex,index++,discovered);}sources.put(role.field,discovered);}
      catch(java.io.IOException|RuntimeException|Error failure){state.closed=true;throw state.fail(failure);}finally{state.busy=false;}
    }
    /** Complete standard metadata after independently preserved extension parts/links are supplied. */
    public byte[] finish(Extensions extensions) throws java.io.IOException {
      state.begin();try{if(extensions!=null){parts.addAll(extensions.parts);links.addAll(extensions.relationships);}byte[] bytes=opcWrite(parts,links,state);state.poll();state.closed=true;return bytes;}
      catch(java.io.IOException|RuntimeException|Error failure){state.closed=true;throw state.fail(failure);}finally{state.busy=false;parts.clear();links.clear();sources.clear();}
    }
  }
)packagejava"
R"packagejava(
  /** An unpublished complete-profile load owns only its private archive adapter. */
  public static final class Loader implements AutoCloseable {
    private final State state;private final ZipReader archive;private final Opc opc;
    private final java.util.Map<String,java.util.List<String>> sources=new java.util.HashMap<>();
    private final java.util.Set<String> decoded=new java.util.HashSet<>();private final java.util.Set<Integer> selected=new java.util.HashSet<>();
    private boolean finished;
    /** Parse all standard metadata before any typed decoder or required-capability callback. */
    public Loader(Object source,Budget budget) throws java.io.IOException {
      state=new State(budget);state.begin();ZipReader opened=null;
      try{opened=ZipReader.open(source,budget,state);archive=opened;opc=opcRead(archive,state);state.poll();}
      catch(java.io.IOException|RuntimeException|Error failure){state.closed=true;throw state.fail(failure);}
      finally{state.busy=false;if(state.closed&&opened!=null){try{opened.close();}catch(java.io.IOException cleanup){state.first.addSuppressed(cleanup);}}}
    }
    /** Retain metadata/routing context for one selected declared role occurrence. */
    private record Occurrence(PartMetadata metadata,int parent,String id) {}
    private final java.util.Map<String,java.util.List<Occurrence>> referenceLocations=new java.util.HashMap<>();
    /** Reuse already retained discovery records so loaded selector diagnostics keep authored IDs. */
    public void locateReferences(References references,String role,int occurrence){var selected=referenceLocations.get(role).get(occurrence);references.location(selected.metadata.name,selected.id);}

    /** Discover role occurrences by source/type/content type; fixed names remain writer defaults. */
    private java.util.List<Occurrence> discover(Role role) throws java.io.IOException {
      var parents=role.source.equals("/")?java.util.List.of("/"):sources.get(role.source);if(parents==null)throw error(Code.INVALID_RELATIONSHIP,"Role source is not available","");
      var values=new java.util.ArrayList<Occurrence>();var discovered=new java.util.ArrayList<String>();
      for(int parent=0;parent<parents.size();++parent)for(int index=0;index<opc.links.size();++index) {
        state.consume(Resource.WORK_UNITS,1,"");Relationship link=opc.links.get(index);if(!fold(link.source).equals(fold(parents.get(parent)))||!link.type.equals(role.relationship))continue;
        if(!link.mode.equals("internal"))throw new PackageError(Code.INVALID_RELATIONSHIP,"Declared role requires an internal target",link.source,0,role.field,"",link.id);
        String key=fold(target(link.source,link.target));PartMetadata metadata=opc.parts.get(key);if(role.policy.equals("fixed")&&metadata.entry.method!=(role.compression.equals("stored")?0:8))throw new PackageError(Code.INVALID_OPTIONS,"Fixed package compression mismatch",metadata.name,0,role.field,"",link.id);
        if(role.contentType!=null&&!mime(metadata.contentType).equals(mime(role.contentType)))throw new PackageError(Code.CODEC,"Role content type mismatch",metadata.name,0,role.field,"",link.id);
        if(!decoded.add(key))throw new PackageError(Code.INVALID_RELATIONSHIP,"Part is claimed by multiple role occurrences",metadata.name,0,role.field,"",link.id);
        selected.add(index);values.add(new Occurrence(metadata,parent,link.id));discovered.add(metadata.name);
      }
      occurs(role,values.size());state.consume(Resource.DECODER_ALLOCATION_BYTES,128,"");referenceLocations.put(role.field,values);sources.put(role.field,discovered);return values;
    }
    /** Decode one fixed typed role to a nullable owning native value. */
    public <T> T single(Role role,Decoder<T> decoder) throws java.io.IOException {var values=models(role,decoder);return values.isEmpty()?null:values.get(0).value;}
    /** Return complete typed occurrences only after every selected part passes native XML validation. */
    public <T> java.util.List<ModelPart<T>> models(Role role,Decoder<T> decoder) throws java.io.IOException {
      state.begin();try{var result=new java.util.ArrayList<ModelPart<T>>();
        for(Occurrence item:discover(role)){state.poll();byte[] bytes=archive.read(item.metadata.entry.name);T value;
          try{value=decodeNative(decoder,bytes,state,role,item.metadata.name,item.id);}catch(IllegalArgumentException failure){if(state.first!=null)throw state.fail(state.first);throw codec(role,item.metadata.name,item.id,failure);}state.poll();if(value==null)throw error(Code.CODEC,"Typed decoder returned no value",item.metadata.name);result.add(new ModelPart<>(value,item.metadata.name,item.metadata.contentType,item.parent));}
        state.poll();return java.util.List.copyOf(result);
      }catch(java.io.IOException|RuntimeException|Error failure){state.closed=true;throw state.fail(failure);}finally{leave();}
    }
    /** Return owning opaque bytes with their actual metadata and original ZIP method. */
    public java.util.List<BinaryPart> binaries(Role role) throws java.io.IOException {
      state.begin();try{var result=new java.util.ArrayList<BinaryPart>();for(Occurrence item:discover(role)){byte[] bytes=archive.read(item.metadata.entry.name);state.poll();result.add(new BinaryPart(bytes,item.metadata.name,item.metadata.contentType,item.parent,item.metadata.entry.method==8?"deflate":"stored"));}state.poll();return java.util.List.copyOf(result);}
      catch(java.io.IOException|RuntimeException|Error failure){state.closed=true;throw state.fail(failure);}finally{leave();}
    }
    /** Validate all unclaimed parts/links before a complete aggregate can be published. */
    public <T> Result<T> finish(T value,ExtensionPolicy policy,Validator<T> validate) throws java.io.IOException {
      return finish(value,policy,validate,null);
    }
    /** Bind retained extension state before complete-aggregate validation, using generated typed access. */
    public <T> Result<T> finish(T value,ExtensionPolicy policy,Validator<T> validate,java.util.function.BiConsumer<T,Extensions> bind) throws java.io.IOException {
      state.begin();try {
        if(finished)throw new java.io.IOException("Finished package loader");if(policy==null)policy=new ExtensionPolicy();
        var opaque=new java.util.ArrayList<BinaryPart>();var links=new java.util.ArrayList<Relationship>();
        for(var entry:opc.parts.entrySet()) {
          if(decoded.contains(entry.getKey()))continue;PartMetadata metadata=entry.getValue();state.poll();boolean required=policy.requiredPart!=null&&policy.requiredPart.test(metadata);state.poll();
          if(required||policy.parts==Unknown.REJECT)throw error(Code.UNSUPPORTED_FEATURE,"Unknown required or rejected package part",metadata.name);
          byte[] bytes=archive.read(metadata.entry.name);state.poll();
          if(policy.parts==Unknown.PRESERVE){state.consume(Resource.PRESERVED_BYTES,bytes.length,metadata.name);opaque.add(new BinaryPart(bytes,metadata.name,metadata.contentType,0,metadata.entry.method==8?"deflate":"stored"));}
        }
        for(int index=0;index<opc.links.size();++index) {
          if(selected.contains(index))continue;Relationship link=opc.links.get(index);state.poll();boolean required=policy.requiredRelationship!=null&&policy.requiredRelationship.test(link);state.poll();
          if(required||policy.relationships==Unknown.REJECT)throw error(Code.UNSUPPORTED_FEATURE,"Unknown required or rejected package relationship",link.source);
          if(policy.relationships==Unknown.PRESERVE){long bytes=0;for(String text:new String[]{link.source,link.id,link.type,link.target,link.mode})bytes+=text.getBytes(java.nio.charset.StandardCharsets.UTF_8).length;state.consume(Resource.PRESERVED_BYTES,bytes,link.source);links.add(link);}
        }
        var extensions=new Extensions(opaque,links);if(bind!=null){state.poll();bind.accept(value,extensions);state.poll();}
        if(validate!=null){state.poll();validate.validate(value,state.budget);state.poll();}
        state.poll();finished=true;return new Result<>(value,extensions);
      }catch(java.io.IOException|RuntimeException|Error failure){state.closed=true;throw state.fail(failure);}finally{leave();}
    }
    /** Finish the borrow before closing an archive abandoned by an application callback. */
    private void leave() throws java.io.IOException {state.busy=false;if(state.closed)archive.close();}
    /** Close only owned channels/private spools; the application's source is always borrowed. */
    @Override public void close() throws java.io.IOException {state.close();archive.close();}
  }
  /** Borrow a sink only after complete package preparation; errors may leave a prefix. */
  public static void write(java.io.OutputStream destination,byte[] bytes,Budget budget) throws java.io.IOException {
    java.util.Objects.requireNonNull(destination);var state=new State(budget);state.begin();
    try{arraySize(bytes.length,budget.limits.maxArchiveBytes,"");for(int offset=0;offset<bytes.length;){state.poll();int count=Math.min(CHUNK_BYTES,bytes.length-offset);state.consume(Resource.IO_BYTES,count,"");destination.write(bytes,offset,count);state.poll();offset+=count;}state.poll();}
    catch(java.io.IOException|RuntimeException|Error failure){throw state.fail(failure);}finally{state.busy=false;}
  }
  /** Save fault hooks name publication boundaries and never receive model payloads. */
  @FunctionalInterface public interface SaveFault {void at(String point) throws java.io.IOException;}
  /** Poll before and after an application fault hook without losing the first exception. */
  private static void savePoint(String point,SaveFault fault,State state) throws java.io.IOException {state.poll();if(fault!=null)fault.at(point);state.poll();}
  /** Publish validated bytes through an owned synchronized sibling; replacement must be atomic. */
  public static void save(java.nio.file.Path path,byte[] bytes,Budget budget,SaveFault fault) throws java.io.IOException {
    java.util.Objects.requireNonNull(path);var state=new State(budget);state.begin();java.nio.file.Path temporary=null;boolean published=false;
    try {
      arraySize(bytes.length,budget.limits.maxArchiveBytes,"");
      savePoint("before_temporary",fault,state);java.nio.file.Path finalPath=path.toAbsolutePath(),directory=finalPath.getParent();temporary=privateFile(directory,".serializer-save-");
      savePoint("after_temporary",fault,state);
      try(var output=java.nio.channels.FileChannel.open(temporary,java.nio.file.StandardOpenOption.WRITE)) {
        for(int offset=0;offset<bytes.length;){state.poll();int count=Math.min(CHUNK_BYTES,bytes.length-offset);state.consume(Resource.IO_BYTES,count,"");var batch=java.nio.ByteBuffer.wrap(bytes,offset,count);while(batch.hasRemaining()){state.poll();if(output.write(batch)<=0)throw new java.io.IOException("Private Save made no progress");}offset+=count;}
        savePoint("before_file_sync",fault,state);output.force(true);savePoint("after_file_sync",fault,state);
      }
      savePoint("before_replace",fault,state);java.nio.file.Files.move(temporary,finalPath,java.nio.file.StandardCopyOption.ATOMIC_MOVE,java.nio.file.StandardCopyOption.REPLACE_EXISTING);published=true;temporary=null;
      savePoint("after_replace",fault,state);
      if(java.nio.file.Files.getFileStore(directory).supportsFileAttributeView("posix")){try(var parent=java.nio.channels.FileChannel.open(directory,java.nio.file.StandardOpenOption.READ)){parent.force(true);}}
      savePoint("after_directory_sync",fault,state);state.poll();
    } catch(java.io.IOException|RuntimeException|Error failure) {
      if(published){var uncertain=new PackageError(Code.PUBLICATION_UNCERTAIN,"Final package replaced; durability/publication acknowledgement failed");uncertain.initCause(state.first==null?failure:state.first);state.first=uncertain;throw state.fail(uncertain);}throw state.fail(failure);
    } finally {
      state.busy=false;if(temporary!=null){try{java.nio.file.Files.deleteIfExists(temporary);}catch(java.io.IOException cleanup){if(state.first!=null)state.first.addSuppressed(cleanup);else throw cleanup;}}
    }
  }
)packagejava"
