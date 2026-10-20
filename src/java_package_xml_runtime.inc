// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
// Serializer runtime template source; emitted support may also be used under 0BSD.
R"packagejava(
  /** Expanded names retain the original namespace identity, independently of prefix spelling. */
  private record QName(String uri,String local) {}
  /** Bounded owning metadata tree; namespace scopes support Relationships compatibility processing. */
  private static final class Node {
    final QName name;final java.util.Map<QName,String> attributes=new java.util.LinkedHashMap<>();
    final java.util.Map<String,String> namespaces;java.util.List<Node> children=new java.util.ArrayList<>();
    String text="";final StringBuilder parsedText=new StringBuilder();
    /** Snapshot only the current prefix bindings while ordinary metadata remains immutable to callers. */
    Node(QName name,java.util.Map<String,String> namespaces){this.name=name;this.namespaces=java.util.Map.copyOf(namespaces);}
  }
  /** Admit only strict UTF8 or BOM-marked UTF16 and the matching XML1.0 declaration. */
  private static void metadataEncoding(byte[] bytes,String part) throws java.io.IOException {
    boolean little=bytes.length>=2&&(bytes[0]&255)==255&&(bytes[1]&255)==254;
    boolean big=bytes.length>=2&&(bytes[0]&255)==254&&(bytes[1]&255)==255;
    int offset=little||big?2:bytes.length>=3&&(bytes[0]&255)==239&&(bytes[1]&255)==187&&(bytes[2]&255)==191?3:0;
    var charset=little?java.nio.charset.StandardCharsets.UTF_16LE:big?java.nio.charset.StandardCharsets.UTF_16BE:java.nio.charset.StandardCharsets.UTF_8;
    String text;
    try{text=charset.newDecoder().onMalformedInput(java.nio.charset.CodingErrorAction.REPORT).onUnmappableCharacter(java.nio.charset.CodingErrorAction.REPORT).decode(java.nio.ByteBuffer.wrap(bytes,offset,bytes.length-offset)).toString();}
    catch(java.nio.charset.CharacterCodingException failure){throw error(Code.INVALID_METADATA,"Invalid OPC metadata encoding",part);}
    if(text.indexOf(0)>=0)throw error(Code.INVALID_METADATA,"Unmarked UTF16 or invalid XML character",part);
    if(text.startsWith("<?xml")&&text.length()>5&&" \t\r\n".indexOf(text.charAt(5))>=0) {
      int end=text.indexOf("?>");if(end<0)throw error(Code.INVALID_METADATA,"Unterminated XML declaration",part);String declaration=text.substring(0,end);
      var version=java.util.regex.Pattern.compile("(?:^|[ \\t\\r\\n])version[ \\t\\r\\n]*=[ \\t\\r\\n]*(['\"])([^'\"]*)\\1").matcher(declaration);
      if(version.find()&&!version.group(2).equals("1.0"))throw error(Code.INVALID_METADATA,"Unsupported XML version",part);
      var encoding=java.util.regex.Pattern.compile("(?:^|[ \\t\\r\\n])encoding[ \\t\\r\\n]*=[ \\t\\r\\n]*(['\"])([^'\"]*)\\1").matcher(declaration);
      if(encoding.find()&&!encoding.group(2).equalsIgnoreCase(little||big?"UTF-16":"UTF-8"))throw error(Code.INVALID_METADATA,"Metadata encoding declaration does not match admitted bytes",part);
    }
  }
  /** Parse XML1.0 metadata with external entities, DTDs and XInclude unavailable. */
  private static Node metadata(byte[] bytes,String part,State state) throws java.io.IOException {
    state.consume(Resource.METADATA_BYTES,bytes.length,part);
    state.consume(Resource.DECODER_ALLOCATION_BYTES,(long)bytes.length*4,part);
    state.consume(Resource.WORK_UNITS,bytes.length*4L,part);metadataEncoding(bytes,part);
    try {
      var factory=javax.xml.parsers.SAXParserFactory.newInstance();factory.setNamespaceAware(true);factory.setXIncludeAware(false);
      factory.setFeature(javax.xml.XMLConstants.FEATURE_SECURE_PROCESSING,true);
      factory.setFeature("http://apache.org/xml/features/disallow-doctype-decl",true);
      factory.setFeature("http://xml.org/sax/features/external-general-entities",false);
      factory.setFeature("http://xml.org/sax/features/external-parameter-entities",false);
      var reader=factory.newSAXParser().getXMLReader();
      reader.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_DTD,"");reader.setProperty(javax.xml.XMLConstants.ACCESS_EXTERNAL_SCHEMA,"");
      final class Handler extends org.xml.sax.helpers.DefaultHandler {
        final java.util.ArrayDeque<Node> stack=new java.util.ArrayDeque<>();
        final java.util.Map<String,String> pending=new java.util.HashMap<>();Node root;
        /** Convert checked activity failures to SAX without losing their retained original identity. */
        void poll() throws org.xml.sax.SAXException {try{state.poll();}catch(java.io.IOException failure){throw new org.xml.sax.SAXException(failure);}}
        /** Retain namespace declarations for the next element only. */
        @Override public void startPrefixMapping(String prefix,String uri){pending.put(prefix,uri);}
        /** Check depth and every allocation estimate before growing the metadata tree. */
        @Override public void startElement(String uri,String local,String qualified,org.xml.sax.Attributes attributes) throws org.xml.sax.SAXException {
          poll();if(stack.size()>=128)throw new org.xml.sax.SAXException("Metadata depth exceeds supported profile");
          try {state.consume(Resource.DECODER_ALLOCATION_BYTES,128L+attributes.getLength()*128L,part);state.consume(Resource.WORK_UNITS,attributes.getLength()+1L,part);}
          catch(java.io.IOException failure){throw new org.xml.sax.SAXException(failure);}
          try{long bindings=pending.size()+(!stack.isEmpty()?stack.peek().namespaces.size():0L);state.consume(Resource.WORK_UNITS,bindings,part);state.consume(Resource.DECODER_ALLOCATION_BYTES,bindings*64,part);}
          catch(java.io.IOException failure){throw new org.xml.sax.SAXException(failure);}
          var scope=new java.util.HashMap<String,String>();if(!stack.isEmpty())scope.putAll(stack.peek().namespaces);scope.putAll(pending);pending.clear();scope.put("xml",XML);
          var node=new Node(new QName(uri,local),scope);
          for(int index=0;index<attributes.getLength();++index)node.attributes.put(new QName(attributes.getURI(index),attributes.getLocalName(index)),attributes.getValue(index));
          if(stack.isEmpty()){if(root!=null)throw new org.xml.sax.SAXException("Multiple metadata roots");root=node;}else stack.peek().children.add(node);
          stack.push(node);
        }
        /** End exactly the current owning element. */
        @Override public void endElement(String uri,String local,String qualified){Node node=stack.pop();node.text=node.parsedText.toString();}
        /** Preserve non-element text for exact content-model checks after compatibility processing. */
        @Override public void characters(char[] chars,int offset,int count) throws org.xml.sax.SAXException {
          poll();if(stack.isEmpty())return;
          try{state.consume(Resource.WORK_UNITS,count,part);state.consume(Resource.DECODER_ALLOCATION_BYTES,count*2L,part);}
          catch(java.io.IOException failure){throw new org.xml.sax.SAXException(failure);}
          stack.peek().parsedText.append(chars,offset,count);
        }
        /** Reject entities even if an alternate parser ignored a feature setting. */
        @Override public org.xml.sax.InputSource resolveEntity(String publicId,String systemId) throws org.xml.sax.SAXException {throw new org.xml.sax.SAXException("External metadata entity unavailable");}
        /** Report parser errors without writing untrusted excerpts to stderr. */
        @Override public void error(org.xml.sax.SAXParseException failure) throws org.xml.sax.SAXException {throw failure;}
        /** Report fatal parser errors without exposing payload text through our public diagnostic. */
        @Override public void fatalError(org.xml.sax.SAXParseException failure) throws org.xml.sax.SAXException {throw failure;}
      }
      var handler=new Handler();reader.setContentHandler(handler);reader.setErrorHandler(handler);reader.setEntityResolver(handler);
      reader.parse(new org.xml.sax.InputSource(new java.io.ByteArrayInputStream(bytes)));state.poll();
      if(handler.root==null)throw error(Code.INVALID_METADATA,"Missing OPC metadata root",part);
      return handler.root;
    } catch(javax.xml.parsers.ParserConfigurationException|org.xml.sax.SAXException failure) {
      if(state.first!=null)throw state.fail(state.first);
      throw error(Code.INVALID_METADATA,"Invalid or unsupported OPC XML metadata",part);
    }
  }
  /** XML whitespace is narrower than Java's Unicode strip operation. */
  private static boolean whitespace(String value){for(int index=0;index<value.length();++index)if(" \t\r\n".indexOf(value.charAt(index))<0)return false;return true;}
  /** Require exact expanded metadata names. */
  private static boolean named(Node node,String uri,String local){return node.name.equals(new QName(uri,local));}
  /** Require an element-only OPC metadata content model. */
  private static void elementContent(Node node,String part) throws java.io.IOException {if(!whitespace(node.text))throw error(Code.INVALID_METADATA,"Unexpected OPC metadata text",part);}
  /** Check the complete permitted attribute set before interpreting individual values. */
  private static void attributes(Node node,String part,String... allowed) throws java.io.IOException {
    var names=java.util.Set.of(allowed);for(QName name:node.attributes.keySet())if(!name.uri.isEmpty()||!names.contains(name.local))throw error(Code.INVALID_METADATA,"Unexpected OPC metadata attribute",part);
  }
  /** Read one required metadata token using XML whitespace collapse semantics. */
  private static String attribute(Node node,String name,String part) throws java.io.IOException {
    return collapse(rawAttribute(node,name,part));
  }
  /** MIME parameter whitespace remains significant inside quoted values. */
  private static String rawAttribute(Node node,String name,String part) throws java.io.IOException {String value=node.attributes.get(new QName("",name));if(value==null)throw error(Code.INVALID_METADATA,"Missing OPC metadata attribute",part);return value;}
)packagejava"
R"packagejava(
  /** Scoped compatibility declarations refer to URI identities, never later prefix rebinding. */
  private record Scope(Scope parent,java.util.Set<String> ignorable,java.util.Set<QName> process) {}
  /** Apply the selected OPC application configuration: only Relationships is understood. */
  private static final class Compatibility {
    final State state;final String part;
    /** Borrow the metadata operation's one cumulative ledger. */
    Compatibility(State state,String part){this.state=state;this.part=part;}
    /** Split exactly XML whitespace, charging temporary token work before allocation. */
    String[] tokens(String value) throws java.io.IOException {state.consume(Resource.WORK_UNITS,value.length()+1L,part);state.consume(Resource.DECODER_ALLOCATION_BYTES,value.length()*8L,part);String text=collapse(value);return text.isEmpty()?new String[0]:text.split(" ");}
    /** Resolve an original-scope NCName prefix while keeping compatibility's own namespace reserved. */
    String prefix(Node node,String value) throws java.io.IOException {if(!ncname(value))throw error(Code.INVALID_METADATA,"Invalid compatibility prefix",part);String uri=node.namespaces.get(value);if(uri==null||uri.isEmpty()||uri.equals(MC))throw error(Code.INVALID_METADATA,"Unbound compatibility prefix",part);return uri;}
    /** Search ancestors without copying the accumulated capability set. */
    boolean ignorable(Scope scope,String uri) throws java.io.IOException {for(;scope!=null;scope=scope.parent){state.consume(Resource.WORK_UNITS,1,part);if(scope.ignorable.contains(uri))return true;}return false;}
    /** Match original expanded ProcessContent names or namespace wildcards. */
    boolean process(Scope scope,QName name) throws java.io.IOException {for(;scope!=null;scope=scope.parent){state.consume(Resource.WORK_UNITS,1,part);if(scope.process.contains(name)||scope.process.contains(new QName(name.uri,"*")))return true;}return false;}
    /** Validate declarations even on an unselected wrapper without claiming its subtree. */
    Scope declarations(Node node,Scope parent) throws java.io.IOException {
      state.consume(Resource.DECODER_ALLOCATION_BYTES,128,part);var ignore=new java.util.HashSet<String>();var selected=new java.util.HashSet<QName>();var scope=new Scope(parent,ignore,selected);
      String value=node.attributes.get(new QName(MC,"Ignorable"));if(value!=null)for(String token:tokens(value))ignore.add(prefix(node,token));
      for(var entry:node.attributes.entrySet())if(entry.getKey().uri.equals(MC)) {
        String local=entry.getKey().local;
        if(local.equals("ProcessContent"))for(String token:tokens(entry.getValue())) {
          int colon=token.indexOf(':');if(colon<1||colon!=token.lastIndexOf(':')||!(token.substring(colon+1).equals("*")||ncname(token.substring(colon+1))))throw error(Code.INVALID_METADATA,"Invalid ProcessContent name",part);
          String uri=prefix(node,token.substring(0,colon));if(!ignorable(scope,uri))throw error(Code.INVALID_METADATA,"ProcessContent namespace is not ignorable",part);selected.add(new QName(uri,token.substring(colon+1)));
        } else if(local.equals("MustUnderstand")){for(String token:tokens(entry.getValue()))prefix(node,token);}
        else if(!local.equals("Ignorable"))throw error(Code.INVALID_METADATA,"Unknown compatibility attribute",part);
      }
      return scope;
    }
    /** Require only capabilities on retained, selected or unwrapped content. */
    void must(Node node) throws java.io.IOException {String value=node.attributes.get(new QName(MC,"MustUnderstand"));if(value!=null)for(String token:tokens(value))if(!prefix(node,token).equals(RELS))throw error(Code.INVALID_METADATA,"Unsupported required metadata namespace",part);}
    /** Check every Choice/Fallback attribute before selecting the first supported branch. */
    boolean wrapper(Node node,Scope scope,boolean choice) throws java.io.IOException {
      String requires=null;
      for(var entry:node.attributes.entrySet()) {
        QName name=entry.getKey();if(name.uri.isEmpty()){if(!choice||!name.local.equals("Requires"))throw error(Code.INVALID_METADATA,"Unexpected compatibility wrapper attribute",part);requires=entry.getValue();}
        else if(!name.uri.equals(MC)&&(name.uri.equals(XML)||!ignorable(scope,name.uri)))throw error(Code.INVALID_METADATA,"Invalid qualified wrapper attribute",part);
      }
      if(!choice)return false;if(requires==null||whitespace(requires))throw error(Code.INVALID_METADATA,"Choice requires namespace prefixes",part);
      boolean supported=true;for(String token:tokens(requires))if(!prefix(node,token).equals(RELS))supported=false;return supported;
    }
    /** Flatten selected children while retaining all text that can violate OPC's element-only grammar. */
    void children(Node node,Scope scope) throws java.io.IOException {
      var result=new java.util.ArrayList<Node>();var text=new StringBuilder(node.text);
      for(Node child:node.children){int action=visit(child,scope,false);if(action==0)result.add(child);else if(action==1){result.addAll(child.children);text.append(child.text);}}
      node.children=result;node.text=text.toString();
    }
    /** Visit one node; keep=0, unwrap=1, discard=2. */
    int visit(Node node,Scope parent,boolean selectedBranch) throws java.io.IOException {
      state.consume(Resource.WORK_UNITS,1,part);Scope scope=declarations(node,parent);String uri=node.name.uri,local=node.name.local;
      if(!uri.equals(RELS)&&ignorable(scope,uri)) {
        if(!process(scope,node.name))return 2;
        for(String context:new String[]{"base","lang","space"})if(node.attributes.containsKey(new QName(XML,context)))throw error(Code.INVALID_METADATA,"XML context on unwrapped metadata",part);
        must(node);children(node,scope);return 1;
      }
      if(uri.equals(MC)) {
        if(local.equals("AlternateContent")) {
          wrapper(node,scope,false);elementContent(node,part);int choices=0;boolean fallback=false;Node selected=null;
          for(Node child:node.children) {
            Scope childScope=declarations(child,scope);
            if(!child.name.uri.equals(RELS)&&ignorable(childScope,child.name.uri)&&!process(childScope,child.name))continue;
            if(!child.name.uri.equals(MC))throw error(Code.INVALID_METADATA,"Unexpected AlternateContent child",part);
            if(child.name.local.equals("Choice")&&!fallback){++choices;boolean supported=wrapper(child,childScope,true);if(selected==null&&supported)selected=child;}
            else if(child.name.local.equals("Fallback")&&choices>0&&!fallback){fallback=true;wrapper(child,childScope,false);if(selected==null)selected=child;}
            else throw error(Code.INVALID_METADATA,"Invalid Choice/Fallback order",part);
          }
          if(choices==0)throw error(Code.INVALID_METADATA,"AlternateContent has no Choice",part);must(node);
          if(selected==null){node.children=new java.util.ArrayList<>();node.text="";}else{visit(selected,scope,true);node.children=selected.children;node.text=selected.text;}return 1;
        }
        if(!selectedBranch||!(local.equals("Choice")||local.equals("Fallback")))throw error(Code.INVALID_METADATA,"Unexpected compatibility element",part);
        wrapper(node,scope,local.equals("Choice"));must(node);children(node,scope);return 1;
      }
      must(node);children(node,scope);
      var iterator=node.attributes.entrySet().iterator();while(iterator.hasNext()){QName name=iterator.next().getKey();if(name.uri.equals(MC)||!name.uri.equals(RELS)&&ignorable(scope,name.uri))iterator.remove();}return 0;
    }
    /** Require exactly one document root after compatibility selection. */
    Node apply(Node root) throws java.io.IOException {var parent=new Node(new QName("",""),java.util.Map.of());parent.children.add(root);children(parent,null);if(parent.children.size()!=1||!whitespace(parent.text))throw error(Code.INVALID_METADATA,"Compatibility output requires one root",part);return parent.children.get(0);}
  }
)packagejava"
