// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"xmlnamespacecs(
  // Preserve exact XML namespace identity per field occurrence without changing native wire fields.
  private sealed class SrlXmlNamespaceValues {
    private readonly System.Collections.Generic.Dictionary<int,System.Collections.Generic.List<System.Collections.Generic.Dictionary<string,string>>> fields=new();
    private System.Collections.Generic.Dictionary<string,string> fallback=new();
    // Reject malformed Unicode names, empty prefixes and reserved namespace spellings.
    private static (string Prefix,string Uri,string Local) Resolve(string token,int mode,System.Collections.Generic.Dictionary<string,string> scope){
      string prefix="",local=token;if(mode==2){prefix=token;local="";}else{int colon=token.IndexOf(':');if(colon>=0){prefix=token[..colon];local=token[(colon+1)..];}}
      if((mode==2||token.IndexOf(':')>=0)&&(!SrlXmlExtensionNcName(prefix)||prefix=="xmlns")||mode!=2&&!SrlXmlExtensionNcName(local))throw new System.FormatException("Invalid XML QName or prefix");string uri=prefix=="xml"?"http://www.w3.org/XML/1998/namespace":scope.TryGetValue(prefix,out string? selected)?selected:"";if(prefix.Length!=0&&uri.Length==0)throw new System.FormatException("Unbound XML QName prefix");return(prefix,uri,local);
    }
    // Copy complete explicit bindings before replacing any existing loaded occurrence metadata.
    internal void Set(System.Collections.Generic.IDictionary<string,string> bindings){var copy=new System.Collections.Generic.Dictionary<string,string>(System.StringComparer.Ordinal);foreach(var entry in bindings){string prefix=entry.Key,uri=entry.Value;if(!(prefix.Length==0||SrlXmlExtensionNcName(prefix))||prefix=="xmlns"||(prefix=="xml")!=(uri=="http://www.w3.org/XML/1998/namespace")||prefix.Length!=0&&uri.Length==0||uri=="http://www.w3.org/2000/xmlns/")throw new System.FormatException("Invalid XML QName namespace binding");SrlXmlExtensionSize(uri);copy.Add(prefix,uri);}fallback=copy;fields.Clear();}
    // Retain used bindings only after shared operation-local scope-byte admission.
    internal string? Capture(SrlXmlInput input,int field,string? text,System.Collections.Generic.Dictionary<string,string> scope,int mode){
      if(text==null)return null;string[] tokens=SrlXmlRule.Tokens(text,input.ListLimit,input.Operation);if(mode==0&&tokens.Length!=1)throw new System.FormatException("XML QName requires one name");var retained=new System.Collections.Generic.Dictionary<string,string>(System.StringComparer.Ordinal);foreach(string token in tokens){var name=Resolve(token,mode,scope);if(!retained.ContainsKey(name.Prefix)){long size=(long)SrlUtf8.GetByteCount(name.Prefix)+SrlUtf8.GetByteCount(name.Uri);if(size>input.NamespaceByteLimit-input.NamespaceBytes)throw SrlXmlBudget.Limit("XML QName scope byte limit exceeded",input.Operation);input.NamespaceRetention(size);input.NamespaceBytes+=(int)size;retained.Add(name.Prefix,name.Uri);}}
      if(!fields.TryGetValue(field,out var values)){values=new();fields.Add(field,values);}values.Add(retained);return text;
    }
    // Capture exact child-local scope before its end token advances into a sibling namespace.
    internal string Scalar(SrlXmlInput input,int field,string name,int mode){if(!input.Has(name))throw new System.FormatException("Missing XML QName element");var scope=input.NamespaceScope();return Capture(input,field,input.Scalar(name),scope,mode)!;}
    // Publish only lexical names whose expanded identities have valid output declarations.
    internal string Output(SrlXmlOutput output,int field,int occurrence,string text,int mode){
      var scope=fields.TryGetValue(field,out var values)&&occurrence<values.Count?values[occurrence]:fallback;string[] tokens=SrlXmlRule.Tokens(text,output.ListLimit,output.Operation);if(mode==0&&tokens.Length!=1)throw new System.FormatException("XML QName requires one name");output.NamespaceRetention(SrlUtf8.GetByteCount(text));var lexical=new System.Text.StringBuilder();foreach(string token in tokens){var name=Resolve(token,mode,scope);string prefix="";if(name.Uri.Length!=0){prefix=name.Uri=="http://www.w3.org/XML/1998/namespace"?"xml":output.MatchingPrefix(name.Uri,true)??output.FreshPrefix("q",name.Uri);output.Declare(prefix,name.Uri);}else if(output.IdentityNamespace("")!="")throw new System.FormatException("Unqualified XML QName conflicts with output default namespace");if(lexical.Length!=0)lexical.Append(' ');if(prefix.Length!=0){lexical.Append(prefix);if(mode!=2)lexical.Append(':');}if(mode!=2)lexical.Append(name.Local);}return lexical.ToString();
    }
  }
)xmlnamespacecs"
