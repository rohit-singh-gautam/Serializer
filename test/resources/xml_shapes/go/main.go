// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
	srlxml "encoding/xml"
	"fmt"
	"math"
	"reflect"
	"strings"
)

type rejectingSchema struct{ calls int }

// Exercise a caller-prepared provider boundary; the adapter intentionally rejects this contract.
func (schema *rejectingSchema) ValidateXML(_ []byte) error {
	schema.calls++
	return fmt.Errorf("Selected XSD rejects document")
}

// Require token failures to publish neither a candidate value nor partial output bytes.
func checkDirectTokens(encoded []byte, value *ShapesRecord) {
	checks := []struct{ document, message string }{
		{strings.Replace(string(encoded), "</record>", "<unexpected/></record>", 1), "Unexpected XML field"},
		{string(encoded) + "<other/>", "Multiple XML roots"},
		{strings.Replace(string(encoded), ">1.25<", ">broken<", 1), "Invalid XML float"},
		{strings.Replace(string(encoded), "</nested>", "</wrong>", 1), "Unexpected XML end"},
	}
	for _, check := range checks {
		candidate, err := DecodeShapesRecordXML([]byte(check.document))
		if candidate != nil || err == nil || err.Error() != check.message {
			panic(fmt.Sprintf("Token failure published a candidate or changed its diagnostic: %v", err))
		}
	}
	// Adjacent text, entity references and CDATA are one scalar value, including empty text.
	split := strings.Replace(string(encoded), ">1.25<", ">1<![CDATA[.]]>&#50;5<", 1)
	copy, err := DecodeShapesRecordXML([]byte(split))
	if err != nil || copy.Small != 1.25 { panic("Split scalar token decoding") }
	for _, budget := range []string{"bytes", "text", "nodes", "attributes", "depth"} {
		policy := DefaultXmlOptions()
		switch budget {
		case "bytes": policy.Limits.MaxBytes = 1
		case "text": policy.Limits.MaxTextBytes = 1
		case "nodes": policy.Limits.MaxNodes = 1
		case "attributes": policy.Limits.MaxAttributes = 0
		case "depth": policy.Limits.MaxDepth = 1
		}
		if candidate, err := DecodeShapesRecordXML(encoded, policy); candidate != nil || err == nil { panic("Token input budget ignored: " + budget) }
		if bytes, err := value.EncodeXML(policy); bytes != nil || err == nil { panic("Token output budget published partial bytes: " + budget) }
	}
	// The exact output byte boundary admits one complete message and rejects one byte less.
	policy := DefaultXmlOptions(); policy.Limits.MaxBytes = len(encoded)
	if exact, err := value.EncodeXML(policy); err != nil || string(exact) != string(encoded) { panic("Exact output byte limit") }
	policy.Limits.MaxBytes--
	if partial, err := value.EncodeXML(policy); partial != nil || err == nil { panic("Output exceeded exact byte limit") }
}

// Qualify direct typed mapping, namespace safety, limits and the explicit runtime provider scope.
func main() {
	namesXML := "<r:names xmlns:r='urn:root' xmlns:a='urn:attribute' xml:lang='en' a:qualified='A&amp;B'><unqualified>plain</unqualified><f:foreign xmlns:f='urn:foreign'>foreign</f:foreign></r:names>"
	names, nameErr := DecodeShapesNamesXML([]byte(namesXML))
	if nameErr != nil || names.Language != "en" || names.Qualified != "A&B" || names.Unqualified != "plain" || names.Foreign != "foreign" { panic("Expanded namespace input") }
	nameBytes, nameErr := names.EncodeXML(); if nameErr != nil { panic(nameErr) }
	nameCopy, nameErr := DecodeShapesNamesXML(nameBytes)
	if nameErr != nil || !reflect.DeepEqual(names, nameCopy) { panic("Expanded namespace output") }
	integerBox, genericErr := DecodeShapesIntegerBoxXML([]byte("<integer_box><value>4294967295</value></integer_box>"))
	if genericErr != nil || integerBox.Value != math.MaxUint32 { panic("Concrete numeric XML generic") }
	textBox, genericErr := DecodeShapesTextBoxXML([]byte("<text_box><value>A&amp;B</value></text_box>"))
	if genericErr != nil || textBox.Value != "A&B" { panic("Concrete text XML generic") }
	textBytes, genericErr := textBox.EncodeXML()
	if genericErr != nil { panic(genericErr) }
	textCopy, genericErr := DecodeShapesTextBoxXML(textBytes)
	if genericErr != nil || textCopy.Value != textBox.Value { panic("Concrete XML generic roundtrip") }
	catalogXML := "<catalog xmlns='urn:serializer:vocabulary'><prices><price currency='USD'>12.5</price></prices><labels><label>A&amp;B</label><label/></labels></catalog>"
	catalog, catalogErr := DecodeShapesCatalogXML([]byte(catalogXML))
	if catalogErr != nil || len(catalog.Prices) != 1 || catalog.Prices[0].Value != 12.5 || catalog.Prices[0].Currency != "USD" || catalog.Labels[0] != "A&B" { panic("Simple content/wrappers") }
	catalogBytes, catalogErr := catalog.EncodeXML()
	if catalogErr != nil { panic(catalogErr) }
	catalogCopy, catalogErr := DecodeShapesCatalogXML(catalogBytes)
	if catalogErr != nil || !reflect.DeepEqual(catalog, catalogCopy) { panic("Vocabulary roundtrip") }
	emptyBytes, catalogErr := NewShapesCatalog().EncodeXML()
	if catalogErr != nil { panic(catalogErr) }
	emptyCatalog, catalogErr := DecodeShapesCatalogXML(emptyBytes)
	if catalogErr != nil || len(emptyCatalog.Prices) != 0 { panic("Empty wrappers") }
	if _, catalogErr = DecodeShapesCatalogXML([]byte(strings.Replace(catalogXML, "<prices>", "<prices extra='1'>", 1))); catalogErr == nil { panic("Unknown wrapper attribute") }
	depthPolicy := DefaultXmlOptions()
	depthPolicy.Limits.MaxDepth = 1
	if _, catalogErr = NewShapesCatalog().EncodeXML(depthPolicy); catalogErr == nil { panic("Empty wrapper depth budget") }
	depthPolicy.Limits.MaxDepth = 2
	if _, catalogErr = catalog.EncodeXML(depthPolicy); catalogErr == nil { panic("Wrapped item depth budget") }
	namespaceNode := &srlXmlNode{name: srlxml.Name{Space:"urn:qualified",Local:"root"}, children: []*srlXmlNode{{name:srlxml.Name{Local:"child"},text:"value"}}}
	namespaceBytes, namespaceErr := srlXmlWrite(namespaceNode, DefaultXmlLimits())
	if namespaceErr != nil { panic(namespaceErr) }
	namespaceCopy, namespaceErr := srlXmlParse(namespaceBytes, DefaultXmlLimits())
	if namespaceErr != nil || namespaceCopy.children[0].name.Space != "" { panic("Unqualified child inherited its parent's namespace") }
	value := NewShapesRecord()
	value.Big = math.MaxUint64
	value.Label = "\tX\r\n😀"
	flag := true
	value.Flag = &flag
	value.Nested.Value = math.MinInt16
	child := NewShapesChild()
	child.Value = math.MaxInt16
	value.Children = []*ShapesChild{child}
	value.States = []ShapesState{ShapesStateZero, ShapesStateOne}
	value.Amount = math.Inf(1)
	value.Small = 1.25
	encoded, err := value.EncodeXML()
	if err != nil {
		panic(err)
	}
	checkDirectTokens(encoded, value)
	copy, err := DecodeShapesRecordXML(encoded)
	if err != nil {
		panic(err)
	}
	original, err := value.Encode(BINARY_NONE)
	if err != nil {
		panic(err)
	}
	recovered, err := copy.Encode(BINARY_NONE)
	if err != nil || !reflect.DeepEqual(original, recovered) {
		panic("Typed XML round trip differs")
	}
	flag = false
	presentFalse, err := value.EncodeXML()
	if err != nil {
		panic(err)
	}
	copy, err = DecodeShapesRecordXML(presentFalse)
	if err != nil || copy.Flag == nil || *copy.Flag {
		panic("Present false differs from absence")
	}
	value.Flag = nil
	absent, err := value.EncodeXML()
	if err != nil {
		panic(err)
	}
	copy, err = DecodeShapesRecordXML(absent)
	if err != nil || copy.Flag != nil {
		panic("XML optional absence differs")
	}
	zero := strings.Replace(string(encoded), `big="18446744073709551615"`, `big="-000"`, 1)
	copy, err = DecodeShapesRecordXML([]byte(zero))
	if err != nil || copy.Big != 0 {
		panic("Unsigned negative zero rejected")
	}
	invalid := []string{
		strings.Replace(string(encoded), `big="18446744073709551615"`, `big="-"`, 1),
		strings.Replace(string(encoded), `<record`, `<p:record`, 1),
		`<?xml version="1.1"?>` + string(encoded),
		`<?xml version="1.0" encoding="UTF-16"?>` + string(encoded),
		strings.Replace(string(encoded), `<record`, `<record xmlns:p="urn:p" xmlns:p="urn:q"`, 1),
	}
	for _, document := range invalid {
		if _, err = DecodeShapesRecordXML([]byte(document)); err == nil {
			panic("Malformed XML accepted")
		}
	}
	provider := &rejectingSchema{}
	policy := DefaultXmlOptions()
	policy.Validator = provider
	if _, err = DecodeShapesRecordXML(encoded, policy); err == nil || provider.calls != 1 {
		panic("Input validator ignored")
	}
	if _, err = value.EncodeXML(policy); err == nil || provider.calls != 2 {
		panic("Output validator ignored")
	}
	policy.Limits.MaxBytes = 1
	if _, err = DecodeShapesRecordXML(encoded, policy); err == nil || provider.calls != 2 {
		panic("Validator ran before input safety checks")
	}
	fmt.Println("Go generated typed XML and validator boundary checks passed")
}
