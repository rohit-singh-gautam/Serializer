// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"csdecl(
  // A pinned legal derived type selects another private codec rather than weakening the base checks.
  internal sealed class SrlXmlDeclarationType {
    internal readonly string Name;internal readonly int Index,NilIndex;internal readonly bool Abstract;
    // Store exact expanded identities and compiler-checked derivation permissions once.
    internal SrlXmlDeclarationType(string name,int index,int nilIndex,bool abstractType){Name=name;Index=index;NilIndex=nilIndex;Abstract=abstractType;}
  }
  // Static declaration identities select only compiler-owned validation DTOs.
  internal sealed class SrlXmlDeclaration {
    internal readonly string Name,DeclaredType,Root;internal readonly bool Attribute,Nillable,Abstract,Fixed,AbstractType;internal readonly int Index,NilIndex;internal readonly SrlXmlDeclarationType[] Types;
    // Bind declaration flags once, independently of instance attributes and namespace aliases.
    internal SrlXmlDeclaration(string name,bool attribute,bool nillable,bool abstractElement,bool fixedValue,int index,int nilIndex,string declaredType,string root,bool abstractType,SrlXmlDeclarationType[] types){Name=name;Attribute=attribute;Nillable=nillable;Abstract=abstractElement;Fixed=fixedValue;Index=index;NilIndex=nilIndex;DeclaredType=declaredType;Root=root;AbstractType=abstractType;Types=types;}
  }
  // A complete immutable namespace/name inventory never fetches undeclared schemas.
  internal sealed class SrlXmlDeclarations {
    private readonly System.Collections.Generic.Dictionary<(string,bool),SrlXmlDeclaration> values=new();
    // Prepare lookup before any document operation; generated duplicates are compiler errors.
    internal SrlXmlDeclarations(SrlXmlDeclaration[] declarations){foreach(var declaration in declarations)values.Add((declaration.Name,declaration.Attribute),declaration);}
    // Return absence separately from declared invalid content so lax never swallows failures.
    internal SrlXmlDeclaration? Find(string name,bool attribute)=>values.TryGetValue((name,attribute),out var value)?value:null;
  }
  // Borrow every existing local policy and the exact operation while masking package root authorizations.
  internal readonly struct SrlXmlDeclarationPolicy {
    internal readonly Limits Limits;internal readonly XmlMapLimits Maps;internal readonly XmlMixedLimits Mixed;internal readonly XmlIdentityLimits Identity;internal readonly XmlDigestLimits Digests;internal readonly XmlExtensionOptions Extensions;
    // Read only existing context; all owned temporary storage is separately admitted below.
    internal SrlXmlDeclarationPolicy(SrlXmlInput input){Limits=input.DeclarationBudget.Limits;Maps=input.MapLimits;Mixed=input.MixedLimits;Identity=input.IdentityLimits;Digests=input.DigestLimits;Extensions=input.ExtensionOptions;}
    // Share output policy without carrying package root type/capability flags into a foreign declaration.
    internal SrlXmlDeclarationPolicy(SrlXmlOutput output){Limits=output.DeclarationBudget.Limits;Maps=output.MapLimits;Mixed=output.MixedLimits;Identity=output.IdentityLimits;Digests=output.DigestLimits;Extensions=output.ExtensionOptions;}
    // Charge actual temporary metadata before allocation; the caller's exception object propagates unchanged.
    internal void Charge(long count){Limits.Operation?.Consume(Limits.OperationResource.WorkUnits,count);Limits.Operation?.Consume(Limits.OperationResource.AllocationBytes,count);}
    // Admit canonical alias expansion independently while clearing package-specific root flags.
    private Limits Transport(){const int FramingExpansion=16;Charge(64);int bytes=(int)System.Math.Min(int.MaxValue,(long)Limits.MaxBytes*FramingExpansion);int text=System.Math.Max(bytes,Limits.MaxStringBytes);return new Limits(bytes,text,Limits.MaxElements,Limits.MaxDepth,Limits.ReadPolicy){Operation=Limits.Operation,XmlDeclarationValidation=true};}
    // Rebuild only a bounded foreign subtree, leaving retained instance state immutable.
    private static void Emit(SrlXmlOutput output,SrlXmlForeignNode node,bool stripTransport){
      output.Open(node.Name,node.Scope);foreach(var entry in node.Attributes){if(stripTransport&&(entry.Key=="{http://www.w3.org/2001/XMLSchema-instance}nil"||entry.Key=="{http://www.w3.org/2001/XMLSchema-instance}type"))continue;output.Attribute(entry.Key,entry.Value);}
      foreach(object content in node.Content){output.Operation?.Consume(Limits.OperationResource.WorkUnits,1);if(content is string text)output.Text(text);else if(content is SrlXmlForeignNode child)Emit(output,child,false);else throw new System.FormatException("Invalid declared wildcard subtree");}output.Close();
    }
    // Reject malformed or conflicting xsi:type before removing only the authorized root marker.
    private (int Index,int NilIndex) Type(SrlXmlDeclaration declaration,SrlXmlForeignNode node){
      string actual=declaration.DeclaredType;if(node.Attributes.TryGetValue("{http://www.w3.org/2001/XMLSchema-instance}type",out string? lexical)){
        Charge(lexical.Length*4L);lexical=SrlXmlTrim(lexical);var name=SrlXmlNamespaceValues.Resolve(lexical,0,node.Scope);actual=SrlXmlName(name.Uri,name.Local);
        if(actual!=declaration.DeclaredType){foreach(var type in declaration.Types){Limits.Operation?.Consume(Limits.OperationResource.WorkUnits,1);if(type.Name==actual){if(type.Abstract)throw new System.FormatException("Abstract wildcard xsi:type");return(type.Index,type.NilIndex);}}throw new System.FormatException("Declared wildcard xsi:type mismatch");}
      }
      if(declaration.AbstractType)throw new System.FormatException("Abstract wildcard declared type");return(declaration.Index,declaration.NilIndex);
    }
    // Validate complete declared content through its generated private DTO, including edited replay.
    internal void Validate(int inventory,SrlXmlDeclaration declaration,SrlXmlForeignNode node){
      if(declaration.Abstract)throw new System.FormatException("Abstract wildcard declaration");var selected=Type(declaration,node);bool nil=false;
      if(node.Attributes.TryGetValue("{http://www.w3.org/2001/XMLSchema-instance}nil",out string? marker)){
        if(!declaration.Nillable)throw new System.FormatException("Wildcard declaration is not nillable");marker=SrlXmlTrim(marker);if(marker!="true"&&marker!="1"&&marker!="false"&&marker!="0")throw new System.FormatException("Invalid wildcard xsi:nil");nil=marker=="true"||marker=="1";
        if(nil){if(declaration.Fixed||selected.NilIndex<0)throw new System.FormatException("Fixed wildcard cannot be nil");foreach(object content in node.Content)if(content is not string text||text.Length!=0)throw new System.FormatException("Nil wildcard has content");}
      }
      var limits=Transport();var output=new SrlXmlOutput(limits,Maps,Mixed,Identity,Digests,Extensions);Emit(output,node,true);byte[] bytes=output.Finish();
      using var input=new SrlXmlInput(bytes,limits,Maps,Mixed,Identity,Digests,Extensions);SrlXmlDeclarationRead(inventory,nil?selected.NilIndex:selected.Index,input,node.Name);input.Finish();
    }
    // Attribute values use an independent attribute wrapper with the original complete namespace scope.
    internal void ValidateAttribute(int inventory,SrlXmlDeclaration declaration,string value,System.Collections.Generic.Dictionary<string,string> scope){
      var limits=Transport();var output=new SrlXmlOutput(limits,Maps,Mixed,Identity,Digests,Extensions);output.Open(declaration.Root,scope);output.Attribute(declaration.Name,value);output.Close();byte[] bytes=output.Finish();
      using var input=new SrlXmlInput(bytes,limits,Maps,Mixed,Identity,Digests,Extensions);SrlXmlDeclarationRead(inventory,declaration.Index,input,declaration.Root);input.Finish();
    }
  }
)csdecl"
