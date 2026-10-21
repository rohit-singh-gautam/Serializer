// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
  "bytes"
  "fmt"
  "os"
  "strings"
  "sync"
)

const providerDocument="<order xmlns='urn:serializer:xsd' id='7'><customer>R&amp;D</customer><value>3</value></order>"

type cancelAfterProvider struct { delegate XmlValidator; completed *bool }

// Signal cancellation only after the real prepared provider has accepted the complete document.
func (provider cancelAfterProvider) ValidateXML(data []byte)error{
  if err:=provider.delegate.ValidateXML(data);err!=nil{return err}
  *provider.completed=true
  return nil
}

// A provider that returns normally cannot bypass cancellation before publishing bytes or a candidate.
func checkLateProviderCancellation(value *XmlProviderFixtureOrder,data []byte,provider XmlValidator){
  completed:=false
  sentinel:=fmt.Errorf("Operation canceled while the provider was validating")
  operation:=XmlOperationOptions{Options:XmlOptions{DefaultXmlLimits(),cancelAfterProvider{provider,&completed}},Operation:func(string,int64,string)error{if completed{return sentinel};return nil}}
  output,err:=value.EncodeXMLWithOperation(operation)
  if !completed||output!=nil||err!=sentinel{panic("XML output escaped late provider cancellation")}
  completed=false
  candidate,err:=DecodeXmlProviderFixtureOrderXMLWithOperation(data,operation)
  if !completed||candidate!=nil||err!=sentinel{panic("XML candidate escaped late provider cancellation")}
}

// Require an invalid complete-message operation to fail before publishing candidate XML bytes.
func rejectedBytes(data []byte,err error){if data!=nil||err==nil{panic("Provider published invalid XML bytes")}}

// Require a real provider failure with a retained bounded diagnostic.
func rejected(err error){if err==nil||err.Error()==""{panic("Prepared provider accepted invalid input")}}

// Qualify one real prepared provider through direct and generated complete-message callbacks.
func main(){
  if len(os.Args)!=3{panic("Usage: provider bridge.dll schema.xsd")}
  bridge,err:=loadProviderBridge(os.Args[1]);if err!=nil{panic(err)};defer func(){if err:=bridge.Close();err!=nil{panic(err)}}()
  schema,err:=os.ReadFile(os.Args[2]);if err!=nil{panic(err)};provider,err:=newPreparedProvider(bridge,schema);if err!=nil{panic(err)};defer provider.Close()
  options:=XmlOptions{DefaultXmlLimits(),provider};document:=[]byte(providerDocument)
  value,err:=DecodeXmlProviderFixtureOrderXML(document,options);if err!=nil{panic(err)}
  if value.Id!=7||value.Customer!="R&D"||len(value.Values)!=1||value.Values[0]!=3{panic("Generated provider values differ")}
  output,err:=value.EncodeXML(options);if err!=nil{panic(err)};if err=provider.ValidateXML(output);err!=nil{panic(err)}
  copy,err:=DecodeXmlProviderFixtureOrderXML(output,options);if err!=nil{panic(err)};first,err:=value.Encode(BINARY_NONE);if err!=nil{panic(err)};second,err:=copy.Encode(BINARY_NONE);if err!=nil||!bytes.Equal(first,second){panic("Provider changed native wire bytes")}
  checkLateProviderCancellation(value,document,provider)
  if provider.state.calls.Load()<4{panic("Generated input/output skipped the real provider")}
  for _,bad:=range []string{"<wrong xmlns='urn:serializer:xsd'/>",strings.ReplaceAll(providerDocument,"urn:serializer:xsd","urn:wrong"),providerDocument+"<extra/>",strings.ReplaceAll(providerDocument,"<value>3</value>",""),"<!DOCTYPE order SYSTEM 'file:///must-not-open'>"+providerDocument,"<?xml version='1.1'?>"+providerDocument}{rejected(provider.ValidateXML([]byte(bad)))}
  rejected(provider.ValidateXML([]byte{0xc3,0x28}))
  mismatchSource:=[]byte("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:serializer:xsd'><xs:element name='order' type='xs:string'/></xs:schema>")
  mismatch,err:=newPreparedProvider(bridge,mismatchSource);if err!=nil{panic(err)};defer mismatch.Close();rejectedBytes(value.EncodeXML(XmlOptions{DefaultXmlLimits(),mismatch}))
  candidate,err:=DecodeXmlProviderFixtureOrderXML(document,XmlOptions{DefaultXmlLimits(),mismatch});if err==nil||candidate!=nil{panic("Provider published invalid native candidate")}
  for _,directive:=range []string{"include","import","redefine","override"}{for _,location:=range []string{"file:///must-not-open.xsd","https://invalid.example/must-not-open.xsd"}{source:=[]byte("<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'><xs:"+directive+" schemaLocation='"+location+"'/></xs:schema>");unexpected,failure:=newPreparedProvider(bridge,source);if unexpected!=nil||failure==nil||!strings.Contains(failure.Error(),"external"){panic(fmt.Sprintf("Schema no-fetch preflight differs: %s %v",directive,failure))}}}
  hinted:=strings.Replace(providerDocument," id="," xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance' xsi:schemaLocation='urn:serializer:xsd https://invalid.example/must-not-open.xsd' id=",1);if err=provider.ValidateXML([]byte(hinted));err!=nil{panic(err)}
  for index:=0;index<5;index++{bounded:=*provider;switch index{case 0:bounded.limits.MaxBytes=len(document)-1;case 1:bounded.limits.MaxDepth=1;case 2:bounded.limits.MaxNodes=1;case 3:bounded.limits.MaxAttributes=0;case 4:bounded.limits.MaxTextBytes=1};rejected(bounded.ValidateXML(document))}
  var workers sync.WaitGroup;failures:=make(chan error,8);for worker:=0;worker<8;worker++{workers.Add(1);go func(){defer workers.Done();for index:=0;index<16;index++{if err:=provider.ValidateXML(document);err!=nil{failures<-err;return};if err:=provider.ValidateXML([]byte("<wrong/>"));err==nil{failures<-fmt.Errorf("Concurrent provider admitted wrong root");return};if err:=provider.ValidateXML(document);err!=nil{failures<-err;return}}}()};workers.Wait();close(failures);for failure:=range failures{panic(failure)}
  version:=bridge.message(bridge.version);if version==""{panic("Real provider version unavailable")};provider.Close();rejected(provider.ValidateXML(document))
  fmt.Printf("Go prepared libxml2 provider %s: owning input/output, bounds, no-fetch, concurrent reuse and closed-state checks pass (%d callbacks)\n",version,provider.state.calls.Load())
}
