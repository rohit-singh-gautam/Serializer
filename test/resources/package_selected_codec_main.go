// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
  "bytes"
  "errors"
  "fmt"
  "os"
  m "serializer-selected/model"
)

var passed int

// Keep acceptance assertions enabled independently of SDK optimization settings.
func check(condition bool, message string) {
  if !condition { panic(message) }
  passed++
}

// Construct a public caller ledger without reading generated private state.
func budget(limits m.SerializerPackageLimits) *m.SerializerPackageBudget {
  value, err := m.SerializerPackageNewBudget(limits)
  if err != nil { panic(err) }
  return value
}

// Admit two selected native decoders through the exact same borrowed public XML options.
func composed(data []byte, options m.XmlOptions, part, id string) (*m.IdentityXmlRegistry, error) {
  check(part != "" && id != "", "public codec physical origin")
  if _, err := m.DecodeIdentityXmlRegistryXML(data, options); err != nil { return nil, err }
  return m.DecodeIdentityXmlRegistryXML(data, options)
}

// Exercise generated native APIs from a separate Go package with no private helper access.
func main() {
  data, err := os.ReadFile("fixtures/valid.opc"); if err != nil { panic(err) }
  limits := m.SerializerPackageDefaultLimits()
  baseline := budget(limits)
  // The old positional load-options literal still has exactly the historical fields.
  loaded, err := m.IdentityPackageArchiveLoad(data, m.IdentityPackageArchiveLoadOptions{baseline, m.SerializerPackageExtensionPolicy{}, nil})
  check(err == nil && loaded != nil && len(loaded.Registries) == 3, "old default profile")
  check(limits.MaxReferences-baseline.Remaining(m.SerializerPackageReferences) == 15, "default fifteen reference ledger")
  empty,err:=os.ReadFile("fixtures/empty.opc");if err!=nil{panic(err)}
  emptyCalls:=0
  nativeDecode:=func(data []byte,options m.XmlOptions,part,id string)(*m.IdentityXmlRegistry,error){emptyCalls++;return m.DecodeIdentityXmlRegistryXML(data,options)}
  emptyValue,err:=m.@selected@Load(empty,m.IdentityPackageArchiveLoadOptions{},nativeDecode)
  check(err==nil&&emptyValue!=nil&&len(emptyValue.Registries)==3&&emptyCalls==3,"small valid empty XML passes public typed dispatch")
  wrongRoot,err:=os.ReadFile("fixtures/wrong-root.opc");if err!=nil{panic(err)};emptyCalls=0
  wrongValue,wrongError:=m.@selected@Load(wrongRoot,m.IdentityPackageArchiveLoadOptions{},nativeDecode)
  check(wrongValue==nil&&wrongError!=nil&&emptyCalls==0,"wrong XML root rejects before selected decoder")
  shared := budget(limits)
  selected, err := m.@selected@Load(data, m.IdentityPackageArchiveLoadOptions{Budget:shared}, composed)
  check(err == nil && selected != nil, "external selected decoder composition")
  check(limits.MaxReferences-shared.Remaining(m.SerializerPackageReferences) == 27, "double native decode shares twenty-seven references")
  resources := []m.SerializerPackageResource{m.SerializerPackageReferences,m.SerializerPackageWorkUnits,m.SerializerPackageDecoderAllocationBytes}
  maxima := []int64{limits.MaxReferences,limits.MaxWorkUnits,limits.MaxDecoderAllocationBytes}
  for index, resource := range resources {
    used := maxima[index]-shared.Remaining(resource)
    for _, tight := range []bool{false,true} {
      bounded := limits; maximum := used; if tight { maximum-- }
      switch resource {case m.SerializerPackageReferences:bounded.MaxReferences=maximum;case m.SerializerPackageWorkUnits:bounded.MaxWorkUnits=maximum;default:bounded.MaxDecoderAllocationBytes=maximum}
      value, failure := m.@selected@Load(data,m.IdentityPackageArchiveLoadOptions{Budget:budget(bounded)},composed)
      if !tight { check(failure==nil&&value!=nil,"exact selected native quota") } else {
        var located *m.SerializerPackageError
        check(value==nil&&errors.As(failure,&located)&&located.Code==m.SerializerPackageResourceLimit,"one tight selected native quota")
        check(located.Part!=""&&located.FieldPath!=""&&located.RelationshipID!="","selected native quota location")
      }
    }
  }
  marker := errors.New("original caller error")
  calls := 0
  rejected := func(data []byte, options m.XmlOptions, part,id string)(*m.IdentityXmlRegistry,error){calls++;if calls==2{check(part=="/Registries/registry1.xml"&&id=="record1","middle selected caller origin");return nil,marker};return m.DecodeIdentityXmlRegistryXML(data,options)}
  value, failure := m.@selected@Load(data,m.IdentityPackageArchiveLoadOptions{},rejected)
  check(value==nil&&failure==marker&&calls==2,"original caller error identity")
  fenced:=limits;fenced.MaxReferences=3
  var swallowed error
  ignored:=func(data []byte,options m.XmlOptions,part,id string)(*m.IdentityXmlRegistry,error){value,err:=m.DecodeIdentityXmlRegistryXML(data,options);if err!=nil{swallowed=err;return &m.IdentityXmlRegistry{},nil};return value,nil}
  value,failure=m.@selected@Load(data,m.IdentityPackageArchiveLoadOptions{Budget:budget(fenced)},ignored)
  check(value==nil&&swallowed!=nil&&failure==swallowed,"selected callback cannot swallow native shared-budget failure")
  cancelled := false; calls=0; cancelling:=limits; cancelling.CheckCancelled=func()error{if cancelled{return marker};return nil}
  cancelDecode:=func(data []byte,options m.XmlOptions,part,id string)(*m.IdentityXmlRegistry,error){calls++;if calls==2{cancelled=true};return m.DecodeIdentityXmlRegistryXML(data,options)}
  value,failure=m.@selected@Load(data,m.IdentityPackageArchiveLoadOptions{Budget:budget(cancelling)},cancelDecode)
  check(value==nil&&failure==marker&&calls==2,"middle native cancellation identity")
  destination:="fixtures/atomic.opc";check(os.WriteFile(destination,[]byte("previous destination"),0600)==nil,"seed destination")
  calls=0
  rejectedEncode:=func(model *m.IdentityXmlRegistry,options m.XmlOptions,part,id string)([]byte,error){calls++;if calls==2{return nil,marker};return model.EncodeXML(options)}
  failure=m.@selected@Save(destination,loaded,m.IdentityPackageArchiveSaveOptions{},rejectedEncode,nil)
  retained,err:=os.ReadFile(destination);check(err==nil&&failure==marker&&bytes.Equal(retained,[]byte("previous destination")),"selected middle Save failure is atomic")
  output,err:=m.@selected@Encode(loaded,nil,budget(limits),func(model *m.IdentityXmlRegistry,options m.XmlOptions,part,id string)([]byte,error){return model.EncodeXML(options)})
  check(err==nil&&os.WriteFile("fixtures/produced.opc",output,0600)==nil,"selected native output")
  fmt.Printf("{\"passed\":%d,\"default_references\":15,\"composed_references\":27}\n",passed)
}
