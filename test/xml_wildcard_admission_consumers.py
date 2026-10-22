# Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
# SPDX-License-Identifier: 0BSD
"""Inject shared-operation and retained-state probes into actual JVM/C#/Go wildcard SDK consumers."""
JAVA = r'''
  /** Keep nested private XML validation on one ledger and preserve original admission failures. */
  private static void admissionProbe(byte[] data,boolean skip){
    var extensionLimits=new Schema.XmlExtensionLimits();
    var extensions=skip?new Schema.XmlExtensionOptions(java.util.List.of(),"",new Schema.XmlExtensionLimits(extensionLimits.maxSlots,extensionLimits.maxItems,0)):new Schema.XmlExtensionOptions();
    var value=Schema.Ordered.Root.decodeXml(data);byte[] nativeBefore=value.encode(Schema.Protocol.BINARY_NONE);
    java.util.List<java.util.function.Consumer<Schema.Limits>> operations=java.util.List.of(
      limits->{Schema.Ordered.Root.decodeXml(data,limits,null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),extensions);},
      limits->{value.encodeXml(limits,null,new Schema.XmlMapLimits(),new Schema.XmlMixedLimits(),new Schema.XmlIdentityLimits(),new Schema.XmlDigestLimits(),extensions);});
    for(var operation:operations){
      var counts=new java.util.HashMap<String,Long>();
      operation.accept(admissionLimits((resource,amount,field)->{if(amount<0)throw new AssertionError();counts.merge(resource,amount,Long::sum);}));
      if(counts.getOrDefault("work",0L)==0||counts.getOrDefault("allocation",0L)==0)throw new AssertionError("Missing recursive admission");
      if(skip&&counts.getOrDefault("preserved",0L)!=0)throw new AssertionError("Temporary declaration retained bytes");
      if(!skip&&counts.getOrDefault("preserved",0L)==0)throw new AssertionError("Retained declaration skipped preservation admission");
      for(String resource:java.util.List.of("work","allocation","preserved")){
        long maximum=counts.getOrDefault(resource,0L);if(maximum==0)continue;
        long[] used={0};operation.accept(admissionLimits((kind,amount,field)->{if(kind.equals(resource)){used[0]+=amount;if(used[0]>maximum)throw new AssertionError("Exact admission rejected");}}));
        used[0]=0;var original=new IllegalStateException("Original one-tight declaration failure");Throwable caught=null;
        try{operation.accept(admissionLimits((kind,amount,field)->{if(kind.equals(resource)){used[0]+=amount;if(used[0]>maximum-1)throw original;}}));}catch(Throwable failure){caught=failure;}
        if(caught!=original)throw new AssertionError("Nested declaration reset "+resource,caught);
      }
      var cancellation=new IllegalStateException("Original wildcard cancellation");Throwable caught=null;
      try{operation.accept(admissionLimits((resource,amount,field)->{throw cancellation;}));}catch(Throwable failure){caught=failure;}
      if(caught!=cancellation)throw new AssertionError("Cancellation identity replaced",caught);
      operation.accept(Schema.Limits.defaults());
      if(!java.util.Arrays.equals(nativeBefore,value.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Validation changed native payload");
    }
    if(!skip){
      var state=declarationMember(value,"srlXmlPreserved");
      var children=(java.util.List<?>)declarationMember(state,"children");
      var packet=declarationMember(children.get(0),"node");
      var packetContent=(java.util.List<?>)declarationMember(packet,"content");
      var content=(java.util.List<?>)declarationMember(packetContent.get(0),"content");Object previous=content.get(0);
      try{
        var set=java.util.List.class.getMethod("set",int.class,Object.class);set.invoke(content,0,"0");boolean rejected=false;
        try{value.encodeXml();}catch(IllegalArgumentException expected){rejected=true;}finally{set.invoke(content,0,previous);}
        if(!rejected)throw new AssertionError("Edited invalid declared wildcard published");
      }catch(ReflectiveOperationException failure){throw new AssertionError(failure);}
      if(!java.util.Arrays.equals(nativeBefore,value.encode(Schema.Protocol.BINARY_NONE)))throw new AssertionError("Edited validation changed native payload");
    }
  }
  /** Inspect only the test's deliberately mutable private companion before checking output admission. */
  private static Object declarationMember(Object owner,String name){
    try{var field=owner.getClass().getDeclaredField(name);field.setAccessible(true);return field.get(owner);}
    catch(ReflectiveOperationException failure){throw new AssertionError(failure);}
  }
  /** Borrow the application's operation without replacing the selected document limits. */
  private static Schema.Limits admissionLimits(Schema.Limits.XmlOperation operation){
    var base=Schema.Limits.defaults();return new Schema.Limits(base.maxBytes,base.maxStringBytes,base.maxElements,base.maxDepth,base.readPolicy,operation);
  }
'''
CSHARP = r'''
  // One borrowed ledger observes nested private validators and preserves the first original failure.
  private sealed class Admission : Limits.IOperation {
    internal readonly System.Collections.Generic.Dictionary<Limits.OperationResource,long> Counts=new();
    internal Limits.OperationResource? Selected;internal long Maximum;internal Exception? Failure;
    private bool failed;
    private sealed class Owner:System.IDisposable{public void Dispose(){}}
    private static readonly Owner owner=new();
    public void Consume(Limits.OperationResource resource,long count){Counts.TryGetValue(resource,out long used);used+=count;Counts[resource]=used;if(failed||Selected==resource&&used>Maximum){failed=true;throw Failure!;}}
    public System.IDisposable EnterOwner()=>owner;
    public void SelectField(string field){}
    public Limits.IOperation Capture()=>this;
    public void RethrowFailure(){if(failed)throw Failure!;}
    public void ResourceLimit(string message){RethrowFailure();throw new FormatException(message);}
  }
  // Measure equality/one-tight boundaries in both directions and reject edited declared content.
  private static void AdmissionProbe(byte[] data,OrderedRoot value,bool skip){
    Action<Admission>[] operations={context=>{_=OrderedRoot.DecodeXml(data,new Limits{Operation=context});},context=>{_=value.EncodeXml(new Limits{Operation=context});}};
    foreach(var operation in operations){var baseline=new Admission();operation(baseline);
      foreach(var resource in new[]{Limits.OperationResource.WorkUnits,Limits.OperationResource.AllocationBytes,Limits.OperationResource.PreservedBytes}){
        baseline.Counts.TryGetValue(resource,out long count);if(count==0)continue;
        operation(new Admission{Selected=resource,Maximum=count,Failure=new Exception("Exact admission unexpectedly rejected")});
        var original=new Exception("Original one-tight declaration admission");Exception? caught=null;
        try{operation(new Admission{Selected=resource,Maximum=count-1,Failure=original});}catch(Exception actual){caught=actual;}
        if(!ReferenceEquals(caught,original))throw new Exception("Nested declaration reset "+resource);
      }
      if(skip&&baseline.Counts.TryGetValue(Limits.OperationResource.PreservedBytes,out long retained)&&retained!=0)throw new Exception("Temporary candidate consumed preservation");
    }
    if(skip){_=OrderedRoot.DecodeXml(data,new Limits(),null,new XmlMapLimits(),new XmlMixedLimits(),new XmlIdentityLimits(),new XmlDigestLimits(),new XmlExtensionOptions(limits:new XmlExtensionLimits(maxPreservedBytes:0)));return;}
    var field=typeof(OrderedRoot).GetField("SrlXmlPreservedState",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic)!;
    var state=(SrlXmlPreserved)field.GetValue(value)!;var packet=state.Children[0].Node;var countNode=(SrlXmlForeignNode)packet.Content[0];object previous=countNode.Content[0];countNode.Content[0]="0";
    bool rejected=false;try{_=value.EncodeXml();}catch(FormatException){rejected=true;}finally{countNode.Content[0]=previous;}
    if(!rejected)throw new Exception("Edited invalid declared wildcard published");
  }
'''
GO = r'''
// Keep one original error pointer distinguishable from lexical XML rejection.
type wildcardAdmissionFailure struct{}
func(*wildcardAdmissionFailure)Error()string{return "Original declaration admission failure"}
// Count actual nested work/storage, equality admission and one-tight original failures.
func wildcardAdmissionProbe(data []byte,value *OrderedRoot,skip bool)error{
 operations:=[]func(func(string,int64,string)error)error{
  func(operation func(string,int64,string)error)error{_,err:=DecodeOrderedRootXML(data,srlXmlWithOperation(DefaultXmlOptions(),operation,"",""));return err},
  func(operation func(string,int64,string)error)error{_,err:=value.EncodeXML(srlXmlWithOperation(DefaultXmlOptions(),operation,"",""));return err},
 }
 for _,operation:=range operations{counts:=map[string]int64{};if err:=operation(func(resource string,amount int64,_ string)error{counts[resource]+=amount;return nil});err!=nil{return err}
  for _,resource:=range []string{"work","allocation","preserved"}{maximum:=counts[resource];if maximum==0{continue};used:=int64(0);original:=&wildcardAdmissionFailure{}
   if err:=operation(func(kind string,amount int64,_ string)error{if kind==resource{used+=amount;if used>maximum{return original}};return nil});err!=nil{return fmt.Errorf("Exact declaration admission %s: %w",resource,err)}
   used=0;err:=operation(func(kind string,amount int64,_ string)error{if kind==resource{used+=amount;if used>maximum-1{return original}};return nil});if err!=original{return fmt.Errorf("Nested declaration reset %s: %v",resource,err)}
  };if skip&&counts["preserved"]!=0{return fmt.Errorf("Temporary candidate consumed preservation")}
 }
 if skip{extensions:=DefaultXmlExtensionOptions();extensions.Limits.MaxPreservedBytes=0;_,err:=DecodeOrderedRootXMLWithExtensionOptions(data,extensions);return err}
 packet:=value.srlXmlPreserved.children[0].node;countNode:=packet.content[0].(*srlXmlForeignNode);previous:=countNode.content[0];countNode.content[0]="0";_,err:=value.EncodeXML();countNode.content[0]=previous;if err==nil{return fmt.Errorf("Edited invalid declared wildcard published")};return nil
}
'''


RUST = r'''
/// Original application errors retain their Arc identity through private declaration codecs.
#[derive(Debug)]
struct WildcardAdmissionFailure;
impl std::fmt::Display for WildcardAdmissionFailure{fn fmt(&self,output:&mut std::fmt::Formatter<'_>)->std::fmt::Result{output.write_str("Original declaration admission failure")}}
impl std::error::Error for WildcardAdmissionFailure{}
/// One borrowed ledger observes allocation/work/retention without fresh nested quotas.
struct WildcardAdmission{counts:[usize;3],selected:Option<usize>,maximum:usize,cancelled:bool,failure:serializer_xml_operation::Failure}
impl WildcardAdmission{
 fn new()->Self{Self{counts:[0;3],selected:None,maximum:usize::MAX,cancelled:false,failure:std::sync::Arc::new(WildcardAdmissionFailure)}}
 fn charge(&mut self,resource:usize,count:usize)->serializer_xml_operation::Result<()>{self.counts[resource]=self.counts[resource].checked_add(count).expect("Admission overflow");if self.selected==Some(resource)&&self.counts[resource]>self.maximum{return Err(self.failure.clone())}Ok(())}
}
impl serializer_xml_operation::Operation for WildcardAdmission{
 fn consume(&mut self,resource:serializer_xml_operation::Resource,count:usize,_field:&str)->serializer_xml_operation::Result<()>{use serializer_xml_operation::Resource;match resource{Resource::WorkUnits=>self.charge(0,count),Resource::DecoderAllocationBytes=>self.charge(1,count),_=>Ok(())}}
 fn preserve_bytes(&mut self,count:usize,_field:&str)->serializer_xml_operation::Result<()>{self.charge(2,count)}
 fn poll(&mut self,_field:&str)->serializer_xml_operation::Result<()>{if self.cancelled{Err(self.failure.clone())}else{Ok(())}}
 fn limit(&mut self,_message:&'static str,_field:&str)->serializer_xml_operation::Failure{self.failure.clone()}
}
/// Exercise exact boundaries, skip quota0 and failure identity using only public document/operation APIs.
fn wildcard_admission_probe(input:&str,document:&mut XmlDocument<OrderedRoot>,skip:bool){
 use serializer_xml_operation::{DocumentCodec,Options};
 let native=document.value.encode(Protocol::Json).unwrap();let retained=document.extensions.preserved_bytes();
 for direction in 0..2{
  let run=|ledger:&mut WildcardAdmission|->serializer_xml_operation::Result<()>{if direction==0{<OrderedRoot as DocumentCodec>::decode_document_with_operation(input,Options::default(),None,ledger).map(|_|())}else{document.value.encode_document_with_operation(Some(&document.extensions),Options::default(),ledger).map(|_|())}};
  let mut baseline=WildcardAdmission::new();run(&mut baseline).unwrap();
  for resource in 0..3{let maximum=baseline.counts[resource];if maximum==0{continue}let mut exact=WildcardAdmission::new();exact.selected=Some(resource);exact.maximum=maximum;run(&mut exact).unwrap();let mut tight=WildcardAdmission::new();tight.selected=Some(resource);tight.maximum=maximum-1;let failure=tight.failure.clone();let actual=match run(&mut tight){Err(error)=>error,Ok(())=>panic!("Nested declaration reset admission")};assert!(std::sync::Arc::ptr_eq(&actual,&failure));}
  let mut cancelled=WildcardAdmission::new();cancelled.cancelled=true;let failure=cancelled.failure.clone();let actual=match run(&mut cancelled){Err(error)=>error,Ok(())=>panic!("Nested declaration lost cancellation")};assert!(std::sync::Arc::ptr_eq(&actual,&failure));
  if skip{assert_eq!(baseline.counts[2],0);}
 }
 assert_eq!(document.value.encode(Protocol::Json).unwrap(),native);assert_eq!(document.extensions.preserved_bytes(),retained);
 if skip{let mut options=XmlExtensionOptions::default();options.limits.max_preserved_bytes=0;OrderedRoot::decode_xml_document(input,XmlLimits::default(),None,&options).unwrap();}
 let previous=std::mem::replace(&mut document.value.first,String::from("\0"));let rejected=document.value.encode_document_with_operation(Some(&document.extensions),Options::default(),&mut WildcardAdmission::new()).is_err();document.value.first=previous;assert!(rejected);assert_eq!(document.extensions.preserved_bytes(),retained);
}
'''


def wrap_consumer(consumer, skip):
    """Add probes only to one independently valid nested-declaration fixture per actual SDK."""
    def wrapped(language, grouped=False):
        """Retain existing semantic/native protocol checks and generated naming contracts."""
        source = consumer(language, grouped)
        marker = str(skip).lower()
        if language == 'java':
            source = source.replace('  /** Preserve all native protocols', JAVA + '\n  /** Preserve all native protocols')
            source = source.replace('if(!valid)throw new AssertionError(path);', 'if(!valid)throw new AssertionError(path);if(path.getFileName().toString().startsWith("root-sequence-nested.valid"))admissionProbe(Files.readAllBytes(path),' + marker + ');')
        elif language == 'csharp':
            source = source.replace('  // Verify exact native payload', CSHARP + '\n  // Verify exact native payload')
            source = source.replace('var copy=OrderedRoot.DecodeXml(output);', 'var copy=OrderedRoot.DecodeXml(output);if(Path.GetFileName(path).StartsWith("root-sequence-nested.valid",StringComparison.Ordinal))AdmissionProbe(File.ReadAllBytes(path),value,' + marker + ');')
        elif language == 'go':
            source += GO
            source = source.replace('copy,err:=DecodeOrderedRootXML(output);if err!=nil{return nil,err}', 'copy,err:=DecodeOrderedRootXML(output);if err!=nil{return nil,err};if strings.HasPrefix(filepath.Base(path),"root-sequence-nested.valid"){if err=wildcardAdmissionProbe(input,value,' + marker + ');err!=nil{return nil,err}}')
        elif language == 'rust':
            source += RUST
            source = source.replace('let document=OrderedRoot::decode_xml_document', 'let mut document=OrderedRoot::decode_xml_document')
            source = source.replace('let copy=OrderedRoot::decode_xml_document(&output,XmlLimits::default(),None,&options)?;', 'let copy=OrderedRoot::decode_xml_document(&output,XmlLimits::default(),None,&options)?;if Path::new(path).file_name().unwrap().to_str().unwrap().starts_with("root-sequence-nested.valid"){wildcard_admission_probe(&input,&mut document,' + marker + ');}')
        return source
    return wrapped
