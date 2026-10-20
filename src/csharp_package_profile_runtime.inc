// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagecs(

    // Describe compiler-prepared cardinality and relationship dispatch.
    public sealed record Role(string Field,string? Name,string? ContentType,string Source,string Relationship,string? Id,string Compression,long Minimum,long Maximum,string Policy="defaults",string? ExpectedRoot=null,string? ExpectedType=null) {
      // Retain the original constructor signature for existing compiled role consumers.
      public Role(string Field,string? Name,string? ContentType,string Source,string Relationship,string? Id,string Compression,long Minimum,long Maximum):this(Field,Name,ContentType,Source,Relationship,Id,Compression,Minimum,Maximum,"defaults",null,null){}
      // Preserve nine-value deconstruction alongside the new profile metadata overload.
      public void Deconstruct(out string Field,out string? Name,out string? ContentType,out string Source,out string Relationship,out string? Id,out string Compression,out long Minimum,out long Maximum){Field=this.Field;Name=this.Name;ContentType=this.ContentType;Source=this.Source;Relationship=this.Relationship;Id=this.Id;Compression=this.Compression;Minimum=this.Minimum;Maximum=this.Maximum;}
    }

    // Retain immutable native field ancestry for deferred diagnostics.
    private sealed record FieldNode(string Member,FieldNode? Parent);
    private sealed class Context:Limits.IOperation {
      private readonly State state;private readonly string part,link;private FieldNode current;private Owner? owner;

    // Bind native XML accounting to an immutable part and relationship identity.
    internal Context(State state,string part,string link,string field){this.state=state;this.part=part;this.link=link;current=new(field,null);}

    // Bind native XML accounting to an immutable part and relationship identity.
    private Context(Context other){state=other.state;part=other.part;link=other.link;current=other.current;}

    // Format native ancestry only when a structured failure needs it.
    private string Path(){var names=new global::System.Collections.Generic.List<string>();for(FieldNode? value=current;value!=null;value=value.Parent)if(value.Member.Length!=0)names.Add(value.Member);names.Reverse();return string.Join(".",names);}

    // Admit cumulative work before mutation while preserving callback exception identity.
    public void Consume(Limits.OperationResource resource,long amount){state.Active();var selected=resource switch{Limits.OperationResource.AllocationBytes=>Resource.DecoderAllocationBytes,Limits.OperationResource.WorkUnits=>Resource.WorkUnits,Limits.OperationResource.References=>Resource.References,Limits.OperationResource.PreservedBytes=>Resource.PreservedBytes,_=>throw new global::System.ArgumentOutOfRangeException(nameof(resource))};try{state.Budget.ConsumeAt(selected,amount,part,Path,link);}catch(global::System.Exception failure){state.Fail(failure);}state.Active();}
      private sealed class Owner:global::System.IDisposable {internal readonly Context Context;internal readonly FieldNode Parent;internal readonly Owner? Previous;private bool closed;
    // Borrow a native parent scope and restore it on every exit.
    internal Owner(Context context){Context=context;Parent=context.current;Previous=context.owner;context.owner=this;}
    // Release runtime-owned state and restore borrowed scope without transferring ownership.
    public void Dispose(){if(!closed){Context.current=Parent;Context.owner=Previous;closed=true;}}}

    // Admit a native owner scope before nested traversal.
    public global::System.IDisposable EnterOwner(){Consume(Limits.OperationResource.AllocationBytes,64);return new Owner(this);}

    // Admit the requested sibling field before retaining its ancestry.
    public void SelectField(string member){try{FieldNode parent=owner?.Parent??current;if(member.Length==0){current=parent;state.Budget.CheckAt(part,Path,link);state.Active();return;}state.Active();string RequestedPath(){var names=new global::System.Collections.Generic.List<string>{member};for(FieldNode? value=parent;value!=null;value=value.Parent)if(value.Member.Length!=0)names.Add(value.Member);names.Reverse();return string.Join(".",names);}state.Budget.ConsumeAt(Resource.DecoderAllocationBytes,64,part,RequestedPath,link);state.Active();current=new(member,parent);state.Budget.CheckAt(part,Path,link);state.Active();}catch(global::System.Exception failure){state.Fail(failure);}}

    // Preserve original caller exceptions across provider error translation.
    public void RethrowFailure(){state.First?.Throw();}
    // Classify a genuine native-local bound before its owning field scope unwinds.
    public void ResourceLimit(string message){state.Poll();state.Fail(new PackageError(Code.ResourceLimit,message,part,Path(),relationshipId:link));}

    // Snapshot immutable owner ancestry for deferred identity observation.
    public Limits.IOperation Capture(){Consume(Limits.OperationResource.AllocationBytes,64);return new Context(this);}
    }

    // Bridge ordinary native XML limits to the same cumulative package ledger.
    private static Limits NativeLimits(State state,Role role,string part,string link){try{int size=state.Budget.ArraySize(state.Budget.Limits.MaxPartBytes,global::System.Array.MaxLength,part);state.Budget.ConsumeAt(Resource.DecoderAllocationBytes,128L+2L*(part.Length+link.Length+role.Field.Length),part,()=>role.Field,link);return new Limits(size,size,1000000,64){Operation=new Context(state,part,link,role.Field),XmlExpectedType=role.ExpectedType};}catch(global::System.Exception failure){state.Fail(failure);throw;}}

    // Check complete role cardinality before publishing any occurrence.
    private static void Occurs(Role role,long count){if(count<role.Minimum||role.Maximum>=0&&count>role.Maximum)throw new PackageError(Code.Semantic,"Package role occurrence constraint failed",fieldPath:role.Field);}
    // Append independent preserved records using original-context simultaneous fixed-role relocation.
    private static void AppendPreservedRelationships(Extensions extensions,global::System.Collections.Generic.IReadOnlyDictionary<string,global::System.Collections.Generic.List<string>> sources,global::System.Collections.Generic.List<Relationship> output,State state){
      if(extensions.Relocations.Count==0){output.AddRange(extensions.Relationships);return;}
      var destinations=new global::System.Collections.Generic.HashSet<string>(global::System.StringComparer.Ordinal);
      foreach(var move in extensions.Relocations){
        state.Consume(Resource.WorkUnits,1,move.Key);state.Consume(Resource.WorkUnits,move.Key.Length,move.Key);state.Consume(Resource.WorkUnits,move.Value.Length,move.Key);PartName(move.Key,false);PartName(move.Value,false);
        state.Consume(Resource.DecoderAllocationBytes,ReferenceMetadataAdmissionBytes,move.Key);
        state.Consume(Resource.DecoderAllocationBytes,2L*move.Value.Length,move.Key);
        if(!destinations.Add(Fold(move.Value)))throw new PackageError(Code.DuplicateName,"Conflicting canonical role destinations",move.Value);
      }
      // A removed moved origin remains a tombstone even when another swapped role takes its old URI.
      bool Present(string origin)=>!extensions.RelocationRoles.TryGetValue(origin,out string? role)||sources.TryGetValue(role,out var names)&&names.Exists(name=>Fold(name)==Fold(extensions.Relocations[origin]));
      foreach(var original in extensions.Relationships){
        using var located=state.Budget.Locate(original.Source,"relationships",original.Id);
        state.Consume(Resource.WorkUnits,1);state.Consume(Resource.WorkUnits,original.Source.Length);state.Consume(Resource.WorkUnits,original.Target.Length);
        string sourceKey=Fold(original.Source),source=original.Source,target=original.Target;
        if(extensions.Relocations.TryGetValue(sourceKey,out string? replacement)){
          if(!Present(sourceKey))throw new PackageError(Code.MissingPart,"Preserved source role was removed",original.Source,relationshipId:original.Id);
          source=replacement;
        }
        if(original.Mode=="internal"){
          state.Consume(Resource.DecoderAllocationBytes,original.Source.Length*2L);state.Consume(Resource.DecoderAllocationBytes,original.Target.Length*2L);
          string expanded=Target(original.Source,original.Target),targetKey=Fold(expanded);
          bool moved=extensions.Relocations.TryGetValue(targetKey,out string? destination);
          if(moved&&!Present(targetKey))throw new PackageError(Code.MissingPart,"Preserved target role was removed",expanded,relationshipId:original.Id);
          if(moved||source!=original.Source){
            int marker=original.Target.IndexOfAny(new[]{'?','#'});string path=moved?destination!:expanded;
            state.Consume(Resource.DecoderAllocationBytes,path.Length*2L);state.Consume(Resource.DecoderAllocationBytes,marker<0?0:(original.Target.Length-marker)*2L);
            target=marker<0?path:path+original.Target[marker..];
          }
        }
        state.Consume(Resource.DecoderAllocationBytes,ReferenceMetadataAdmissionBytes);
        output.Add(original with{Source=source,Target=target});
      }
    }
    /// <summary>Statically generated adapters compose complete typed roles without reflection.</summary>
    public sealed class Composer {
      private readonly State state;private readonly global::System.Collections.Generic.Dictionary<string,global::System.Collections.Generic.List<string>> sources=new(global::System.StringComparer.Ordinal);private readonly global::System.Collections.Generic.List<BinaryPart> parts=new();private readonly global::System.Collections.Generic.List<Relationship> links=new();

    // Create private typed-role composition without reflection.
    public Composer(Budget budget){state=new(budget);}

    // Resolve an explicit source occurrence before adding a relationship.
    private string Parent(Role role,int index,string name){var parents=role.Source=="/"?new global::System.Collections.Generic.List<string>{"/"}:sources.TryGetValue(role.Source,out var selected)?selected:throw new PackageError(Code.InvalidRelationship,"Source role is missing",name,role.Field);if(index<0||index>=parents.Count)throw new PackageError(Code.InvalidRelationship,"Source occurrence is missing",name,role.Field);return parents[index];}

    // Validate and snapshot an input before any external sink callback.
    private void Add(Role role,byte[] data,string name,string media,int parent,int index,global::System.Collections.Generic.List<string> discovered){name=role.Name??name;media=role.ContentType??media;PartName(name,false);MediaType(media);string source=Parent(role,parent,name),id=role.Id??"srl_"+role.Field+"_"+index.ToString(global::System.Globalization.CultureInfo.InvariantCulture);state.Consume(Resource.MetadataBytes,128L+2L*(name.Length+media.Length+id.Length),name);parts.Add(new(data,name,media,parent,role.Compression));links.Add(new(source,id,role.Relationship,name));discovered.Add(name);}

    // Adapt an optional singleton through the complete typed role contract.
    public void Single<T>(Role role,T? value,global::System.Func<T,Limits,byte[]> encoder) where T:class{var items=new global::System.Collections.Generic.List<ModelPart<T>>();if(value!=null)items.Add(new(value,role.Name??"",role.ContentType??""));Models(role,items,encoder);}

    // Traverse complete typed occurrences using shared native codec limits.
    public void Models<T>(Role role,global::System.Collections.Generic.IReadOnlyList<ModelPart<T>> values,global::System.Func<T,Limits,byte[]> encoder) where T:class{state.Begin();try{Occurs(role,values.Count);var discovered=new global::System.Collections.Generic.List<string>();for(int index=0;index<values.Count;++index){state.Poll();var item=values[index];if(item==null||item.Value==null)throw new PackageError(Code.Codec,"Typed role value is missing",fieldPath:role.Field);string name=role.Name??item.Name,id=role.Id??"srl_"+role.Field+"_"+index.ToString(global::System.Globalization.CultureInfo.InvariantCulture);using var located=state.Budget.Locate(name,role.Field,id);Parent(role,item.SourceIndex,name);PartName(name,false);MediaType(role.ContentType??item.ContentType);byte[] data;try{data=encoder(item.Value,NativeLimits(state,role,name,id));}catch(global::System.FormatException failure){state.First?.Throw();throw new PackageError(Code.Codec,"Generated XML encoder rejected part",name,role.Field,relationshipId:id,inner:failure);}state.Poll();if(data==null)throw new PackageError(Code.Codec,"Typed encoder returned no bytes",name,role.Field,relationshipId:id);state.Budget.ConsumeAt(Resource.DecoderAllocationBytes,data.Length,name,()=>role.Field,id);Add(role,data,item.Name,item.ContentType,item.SourceIndex,index,discovered);}sources.Add(role.Field,discovered);}catch(global::System.Exception failure){state.Closed=true;state.Fail(failure);}finally{state.Busy=false;}}

    // Traverse owning opaque occurrences with checked role metadata.
    public void Binaries(Role role,global::System.Collections.Generic.IReadOnlyList<BinaryPart> values){state.Begin();try{Occurs(role,values.Count);var discovered=new global::System.Collections.Generic.List<string>();for(int index=0;index<values.Count;++index){state.Poll();var item=values[index];if(item?.Data==null)throw new PackageError(Code.Codec,"Binary role value is missing",fieldPath:role.Field);Add(role,item.Data,item.Name,item.ContentType,item.SourceIndex,index,discovered);}sources.Add(role.Field,discovered);}catch(global::System.Exception failure){state.Closed=true;state.Fail(failure);}finally{state.Busy=false;}}

    // Publish a complete private result only after final liveness checks.
    public byte[] Finish(Extensions? extensions=null){state.Begin();try{if(extensions!=null){parts.AddRange(extensions.Parts);AppendPreservedRelationships(extensions,sources,links,state);}byte[] bytes=OpcWrite(parts,links,state);state.Poll();state.Closed=true;return bytes;}catch(global::System.Exception failure){state.Closed=true;state.Fail(failure);throw;}finally{state.Busy=false;parts.Clear();links.Clear();sources.Clear();}}
    }
    /// <summary>Load owns only private staging; all native values remain unpublished until Finish.</summary>
    public sealed class Loader:global::System.IDisposable {
      private readonly State state;private readonly ZipReader archive;private readonly Opc opc;private readonly global::System.Collections.Generic.Dictionary<string,global::System.Collections.Generic.List<string>> sources=new(global::System.StringComparer.Ordinal);private readonly global::System.Collections.Generic.HashSet<string> decoded=new(global::System.StringComparer.Ordinal);private readonly global::System.Collections.Generic.HashSet<int> selected=new();private bool finished;private readonly global::System.Collections.Generic.Dictionary<string,string> relocations=new(global::System.StringComparer.Ordinal);private global::System.Collections.Generic.Dictionary<string,string>? relocationRoles;

    // Validate ZIP and OPC envelopes before any native aggregate can be published.
    public Loader(object source,Budget? budget=null){state=new(budget??new());state.Begin();ZipReader? opened=null;try{opened=ZipReader.OpenCore(source,state.Budget,state);archive=opened;opc=OpcRead(archive,state);state.Poll();}catch(global::System.Exception failure){state.Closed=true;try{opened?.Dispose();}catch(global::System.Exception cleanup){failure.Data["SerializerCleanupFailure"]=cleanup;}state.Fail(failure);throw;}finally{state.Busy=false;}}

    // Retain checked discovery metadata independently of native candidates.
    private sealed record Occurrence(PartMetadata Metadata,int Parent,string Id);
    private readonly global::System.Collections.Generic.Dictionary<(string,int),PayloadLocation> locations=new();
    // Retrieve the exact discovered part/link for generated reference diagnostics.
    public PayloadLocation Location(string role,int index)=>locations.TryGetValue((role,index),out var value)?value:throw new PackageError(Code.InvalidState,"Package occurrence location unavailable",fieldPath:role);

    // Resolve complete role occurrences and claim each target exactly once.
)packagecs"
R"packagecs(    private global::System.Collections.Generic.List<Occurrence> Discover(Role role){var parents=role.Source=="/"?new global::System.Collections.Generic.List<string>{"/"}:sources.TryGetValue(role.Source,out var source)?source:throw new PackageError(Code.InvalidRelationship,"Source role unavailable",fieldPath:role.Field);var values=new global::System.Collections.Generic.List<Occurrence>();var discovered=new global::System.Collections.Generic.List<string>();for(int parent=0;parent<parents.Count;++parent)for(int index=0;index<opc.Links.Count;++index){state.Consume(Resource.WorkUnits,1);var link=opc.Links[index];if(Fold(link.Source)!=Fold(parents[parent])||link.Type!=role.Relationship)continue;if(link.Mode!="internal")throw new PackageError(Code.InvalidRelationship,"Typed role requires internal target",link.Source,role.Field,relationshipId:link.Id);string key=Fold(Target(link.Source,link.Target));var metadata=opc.Parts[key];if(role.ContentType!=null&&!MediaEqual(metadata.ContentType,role.ContentType))throw new PackageError(Code.Codec,"Role content type mismatch",metadata.Name,role.Field,relationshipId:link.Id);if(role.Policy=="fixed"&&metadata.Entry.Method!=(role.Compression=="deflate"?8:0))throw new PackageError(Code.InvalidState,"Fixed package storage mismatch",metadata.Name,role.Field,relationshipId:link.Id);if(!decoded.Add(key))throw new PackageError(Code.InvalidRelationship,"Part claimed by multiple role occurrences",metadata.Name,role.Field,relationshipId:link.Id);selected.Add(index);if(role.Name!=null&&metadata.Name!=role.Name){state.Budget.ConsumeAt(Resource.WorkUnits,1,metadata.Name,()=>role.Field,link.Id);state.Budget.ConsumeAt(Resource.DecoderAllocationBytes,ReferenceMetadataAdmissionBytes,metadata.Name,()=>role.Field,link.Id);state.Budget.ConsumeAt(Resource.DecoderAllocationBytes,ReferenceMetadataAdmissionBytes,metadata.Name,()=>role.Field,link.Id);relocations[key]=role.Name;(relocationRoles??=new(global::System.StringComparer.Ordinal))[key]=role.Field;}state.Budget.ConsumeAt(Resource.DecoderAllocationBytes,ReferenceMetadataAdmissionBytes,metadata.Name,()=>role.Field,link.Id);locations.Add((role.Field,values.Count),new(metadata.Name,link.Id));values.Add(new(metadata,parent,link.Id));discovered.Add(metadata.Name);}Occurs(role,values.Count);sources.Add(role.Field,discovered);return values;}

    // Adapt an optional singleton through the complete typed role contract.
    public T? Single<T>(Role role,global::System.Func<byte[],Limits,T> decoder) where T:class{var values=Models(role,decoder);return values.Count==0?null:values[0].Value;}

    // Traverse complete typed occurrences using shared native codec limits.
    public global::System.Collections.Generic.IReadOnlyList<ModelPart<T>> Models<T>(Role role,global::System.Func<byte[],Limits,T> decoder) where T:class{state.Begin();try{var result=new global::System.Collections.Generic.List<ModelPart<T>>();foreach(var item in Discover(role)){using var located=state.Budget.Locate(item.Metadata.Name,role.Field,item.Id);state.Poll();byte[] bytes=archive.Read(item.Metadata.Entry.Name);T value;try{Dispatch(bytes,role,state,item.Metadata.Name,item.Id);value=decoder(bytes,NativeLimits(state,role,item.Metadata.Name,item.Id));}catch(global::System.Xml.XmlException failure){state.First?.Throw();throw new PackageError(Code.Codec,"Package XML dispatch rejected part",item.Metadata.Name,role.Field,relationshipId:item.Id,inner:failure);}catch(global::System.FormatException failure){state.First?.Throw();throw new PackageError(Code.Codec,"Generated XML decoder rejected part",item.Metadata.Name,role.Field,relationshipId:item.Id,inner:failure);}state.Poll();if(value==null)throw new PackageError(Code.Codec,"Typed decoder returned no candidate",item.Metadata.Name,role.Field,relationshipId:item.Id);result.Add(new(value,item.Metadata.Name,item.Metadata.ContentType,item.Parent));}state.Poll();return result.AsReadOnly();}catch(global::System.Exception failure){state.Closed=true;state.Fail(failure);throw;}finally{Leave();}}

    // Traverse owning opaque occurrences with checked role metadata.
    public global::System.Collections.Generic.IReadOnlyList<BinaryPart> Binaries(Role role){state.Begin();try{var result=new global::System.Collections.Generic.List<BinaryPart>();foreach(var item in Discover(role)){using var located=state.Budget.Locate(item.Metadata.Name,role.Field,item.Id);byte[] bytes=archive.Read(item.Metadata.Entry.Name);state.Poll();result.Add(new(bytes,item.Metadata.Name,item.Metadata.ContentType,item.Parent,item.Metadata.Entry.Method==8?"deflate":"stored"));}return result.AsReadOnly();}catch(global::System.Exception failure){state.Closed=true;state.Fail(failure);throw;}finally{Leave();}}
)packagecs"
R"packagecs(
    // Publish a complete private result only after final liveness checks.
    public Result<T> Finish<T>(T value,ExtensionPolicy? policy=null,global::System.Action<T,Budget>? validate=null,global::System.Action<T,Extensions>? bind=null){state.Begin();try{if(finished)throw new PackageError(Code.InvalidState,"Package loader finished");policy??=new();if((int)policy.Parts<0||(int)policy.Parts>2||(int)policy.Relationships<0||(int)policy.Relationships>2)throw new PackageError(Code.InvalidState,"Invalid extension policy");var parts=new global::System.Collections.Generic.List<BinaryPart>();var links=new global::System.Collections.Generic.List<Relationship>();foreach(var item in opc.Parts){if(decoded.Contains(item.Key))continue;var metadata=item.Value;state.Poll();bool required=policy.IsRequiredPart?.Invoke(metadata)==true;state.Poll();if(required||policy.Parts==Unknown.Reject)throw new PackageError(Code.UnsupportedFeature,"Unknown required or rejected part",metadata.Name);byte[] bytes=archive.Read(metadata.Entry.Name);if(policy.Parts==Unknown.Preserve){state.Consume(Resource.PreservedBytes,bytes.Length,metadata.Name);parts.Add(new(bytes,metadata.Name,metadata.ContentType,0,metadata.Entry.Method==8?"deflate":"stored"));}}for(int index=0;index<opc.Links.Count;++index){if(selected.Contains(index))continue;var link=opc.Links[index];state.Poll();bool required=policy.IsRequiredRelationship?.Invoke(link)==true;state.Poll();if(required||policy.Relationships==Unknown.Reject)throw new PackageError(Code.UnsupportedFeature,"Unknown required or rejected relationship",link.Source,relationshipId:link.Id);if(policy.Relationships==Unknown.Preserve){long size=0;foreach(string text in new[]{link.Source,link.Id,link.Type,link.Target,link.Mode})size+=Utf8.GetByteCount(text);state.Consume(Resource.PreservedBytes,size,link.Source);links.Add(link);}}if(relocationRoles!=null){foreach(var move in relocationRoles){state.Consume(Resource.DecoderAllocationBytes,ReferenceMetadataAdmissionBytes,move.Key);}}var extensions=new Extensions(parts,links,relocations,relocationRoles);state.Poll();bind?.Invoke(value,extensions);state.Poll();validate?.Invoke(value,state.Budget);state.Poll();finished=true;return new(value,extensions);}catch(global::System.Exception failure){state.Closed=true;state.Fail(failure);throw;}finally{Leave();}}

    // Restore wrapper activity and release private resources on failure.
    private void Leave(){state.Busy=false;if(state.Closed)archive.Dispose();}

    // Release runtime-owned state and restore borrowed scope without transferring ownership.
    public void Dispose(){state.Close();archive.Dispose();}
    }
  }
)packagecs"
