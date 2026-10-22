// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagecs(
    // Retain an exact discovered part/link without retaining the decoded model.
    public readonly record struct PayloadLocation(string Part,string RelationshipId);
    private const long ReferenceMetadataAdmissionBytes=64;
    private const long XmlDispatchAdmissionBytes=256;
    // Resolve only declared ZIP storage choices without replacing model or vocabulary identities.
    public static Role[] ResolveProfiles(Role[] roles,global::System.Collections.Generic.IReadOnlyDictionary<string,string>? overrides=null) {
      return ResolveProfiles(roles,overrides,null);
    }
    // Resolve all codec/storage choices before native work while preserving older storage-map calls.
    public static Role[] ResolveProfiles(Role[] roles,global::System.Collections.Generic.IReadOnlyDictionary<string,string>? overrides,global::System.Collections.Generic.IReadOnlyDictionary<string,PipelineProfile>? pipelines) {
      var selected=(Role[])roles.Clone();
      if(overrides!=null)foreach(var item in overrides) {
        int index=global::System.Array.FindIndex(selected,role=>role.Field==item.Key);
        if(index<0||item.Value!="stored"&&item.Value!="deflate")throw new PackageError(Code.InvalidState,"Unknown package profile override");
        if(selected[index].Policy=="fixed"&&selected[index].Compression!=item.Value)throw new PackageError(Code.InvalidState,"Fixed package profile cannot change",fieldPath:item.Key);
        selected[index]=selected[index] with {Compression=item.Value};
      }
      if(pipelines!=null)foreach(var item in pipelines){int index=global::System.Array.FindIndex(selected,role=>role.Field==item.Key);if(index<0||item.Value==null)throw new PackageError(Code.InvalidState,"Unknown package pipeline role");var original=roles[index];string codec=item.Value.Codec??original.Codec,compression=item.Value.Compression??selected[index].Compression;if(overrides!=null&&overrides.TryGetValue(item.Key,out string? storage)&&item.Value.Compression!=null&&storage!=compression)throw new PackageError(Code.InvalidState,"Conflicting package storage overrides",fieldPath:item.Key);if((original.Codec=="bytes"?codec!="bytes":codec!="xml"&&codec!="json"&&codec!="binary_none")||compression!="stored"&&compression!="deflate"||original.Policy=="fixed"&&(codec!=original.Codec||compression!=original.Compression))throw new PackageError(Code.InvalidState,"Conflicting package pipeline override",fieldPath:item.Key);selected[index]=selected[index] with{Codec=codec,Compression=compression};}
      return selected;
    }
    // Inspect one root event before the generated native decoder constructs model fields.
    private static void Dispatch(byte[] bytes,Role role,State state,string part,string link) {
      if(role.Codec!="xml"||role.ExpectedRoot==null)return;
      state.Budget.ConsumeAt(Resource.WorkUnits,bytes.Length,part,()=>role.Field,link);
      state.Budget.ConsumeAt(Resource.DecoderAllocationBytes,XmlDispatchAdmissionBytes,part,()=>role.Field,link);
      var settings=new global::System.Xml.XmlReaderSettings {DtdProcessing=global::System.Xml.DtdProcessing.Prohibit,XmlResolver=null,MaxCharactersInDocument=state.Budget.Limits.MaxPartBytes,CloseInput=false};
      using var source=new global::System.IO.MemoryStream(bytes,false);
      using var reader=global::System.Xml.XmlReader.Create(source,settings);
      state.Poll();reader.MoveToContent();
      string root=reader.NamespaceURI.Length==0?reader.LocalName:"{"+reader.NamespaceURI+"}"+reader.LocalName;
      if(root!=role.ExpectedRoot)throw new PackageError(Code.Codec,"Package expected XML root mismatch",part,role.Field,relationshipId:link);
      string? type=reader.GetAttribute("type","http://www.w3.org/2001/XMLSchema-instance");
      if(role.RequiresType&&type==null)throw new PackageError(Code.Codec,"Required package XML type missing",part,role.Field,relationshipId:link);
      string? required=reader.GetAttribute("MustUnderstand","http://schemas.openxmlformats.org/markup-compatibility/2006");if(required!=null){var prefixes=required.Split(new[]{' ','\t','\r','\n'},global::System.StringSplitOptions.RemoveEmptyEntries);if(prefixes.Length==0)throw new PackageError(Code.Codec,"Empty package MustUnderstand vocabulary",part,role.Field,relationshipId:link);foreach(string prefix in prefixes){global::System.Xml.XmlConvert.VerifyNCName(prefix);string? uri=reader.LookupNamespace(prefix);bool understood=false;foreach(string candidate in role.Vocabularies)if(candidate==uri){understood=true;break;}if(uri==null||uri.Length==0||!understood)throw new PackageError(Code.Codec,"Unsupported package MustUnderstand vocabulary",part,role.Field,relationshipId:link);}}
      if(type!=null) {
        var pieces=type.Trim(' ','\t','\r','\n').Split(':');
        if(pieces.Length>2||pieces[0].Length==0||pieces[^1].Length==0)throw new PackageError(Code.Codec,"Invalid package XML type",part,role.Field,relationshipId:link);
        foreach(string piece in pieces)global::System.Xml.XmlConvert.VerifyNCName(piece);
        string? uri=reader.LookupNamespace(pieces.Length==2?pieces[0]:"");
        string expanded=string.IsNullOrEmpty(uri)?pieces[^1]:"{"+uri+"}"+pieces[^1];
        if(pieces.Length==2&&uri==null||expanded!=role.ExpectedType)throw new PackageError(Code.Codec,"Package expected XML type mismatch",part,role.Field,relationshipId:link);
      }
      state.Poll();
    }
    // Index compiler-selected scalar keys and forward requests in one operation ledger.
    public sealed class ReferenceIndex {
      private readonly Budget budget;
      private readonly global::System.Collections.Generic.Dictionary<(string,string,string),string> targets=new();
      private readonly global::System.Collections.Generic.List<((string,string,string) Key,string Type,string Part,string Field,string Link)> requests=new();
      // Borrow the aggregate budget; indexes never retain model pointers.
      public ReferenceIndex(Budget shared){budget=shared;}
      // Render only schema-admitted integers or strings and admit retained metadata before indexing.
      private (string,string,string) Key(string scope,object value,string part,string field,string link) {
        string text=global::System.Convert.ToString(value,global::System.Globalization.CultureInfo.InvariantCulture)??"";
        budget.ConsumeAt(Resource.References,1,part,()=>field,link);
        budget.ConsumeAt(Resource.MetadataBytes,ReferenceMetadataAdmissionBytes+scope.Length*2L+text.Length*2L,part,()=>field,link);
        budget.ConsumeAt(Resource.WorkUnits,1,part,()=>field,link);
        return(scope,value is string?"string":"integer",text);
      }
      // Reject duplicate keys in a scope independently of their target model types.
      public void Add(string scope,object key,string type,string part,string field,string link="") {
        var identity=Key(scope,key,part,field,link);
        if(!targets.TryAdd(identity,type))throw new PackageError(Code.InvalidReference,"Duplicate package payload key",part,field,reference:identity.Item3,relationshipId:link);
      }
      // Retain forward references until every candidate role has been decoded.
      public void Require(string scope,object key,string type,string part,string field,string link=""){requests.Add((Key(scope,key,part,field,link),type,part,field,link));}
      // Validate the complete inventory before publishing the aggregate or final file.
      public void Validate(){foreach(var item in requests){budget.ConsumeAt(Resource.WorkUnits,1,item.Part,()=>item.Field,item.Link);if(!targets.TryGetValue(item.Key,out string? type)||type!=item.Type)throw new PackageError(Code.InvalidReference,"Missing or wrong-type package payload target",item.Part,item.Field,reference:item.Key.Item3,relationshipId:item.Link);}}
    }
)packagecs"
