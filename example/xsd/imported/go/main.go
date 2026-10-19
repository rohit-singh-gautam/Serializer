// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
	"fmt"
	"os"
	"reflect"
	model "serializer-xml-example/internal/model"
	"strings"
)

// Exercise actual generated XML methods and reject malformed messages before publication.
func main() {
	if len(os.Args) != 3 {
		panic("usage: xml-example INPUT.xml OUTPUT.xml")
	}
	data, err := os.ReadFile(os.Args[1])
	if err != nil {
		panic(err)
	}
	value, err := model.DecodeXsdExampleXsdOrder196281168XML(data)
	if err != nil {
		panic(err)
	}
	if value.XsdId != 7 || value.XsdCustomer != "R&D" || value.XsdCount != 3 || !reflect.DeepEqual(value.XsdTag, []string{"A", "B"}) {
		panic("XML fixture values differ")
	}
	encoded, err := value.EncodeXML()
	if err != nil {
		panic(err)
	}
	copy, err := model.DecodeXsdExampleXsdOrder196281168XML(encoded)
	if err != nil || !reflect.DeepEqual(value, copy) {
		panic("XML round trip failed")
	}
	original, err := value.Encode(model.BINARY_NONE)
	if err != nil {
		panic(err)
	}
	recovered, err := copy.Encode(model.BINARY_NONE)
	if err != nil || !reflect.DeepEqual(original, recovered) {
		panic("Binary projection differs")
	}
	invalid := []string{strings.Replace(string(data), "id=\"7\"", "id=\"4294967296\"", 1), strings.Replace(string(data), "urn:serializer:xsd-example", "urn:wrong", 1), "<!DOCTYPE order [<!ENTITY x 'bad'>]>" + string(encoded)}
	for _, document := range invalid {
		if _, err = model.DecodeXsdExampleXsdOrder196281168XML([]byte(document)); err == nil {
			panic("Malformed XML accepted")
		}
	}
	limits := model.DefaultXmlOptions()
	limits.Limits.MaxBytes = 3
	if _, err = model.DecodeXsdExampleXsdOrder196281168XML(data, limits); err == nil {
		panic("XML input bound ignored")
	}
	if _, err = value.EncodeXML(limits); err == nil {
		panic("XML output bound ignored")
	}
	if err = os.WriteFile(os.Args[2], encoded, 0600); err != nil {
		panic(err)
	}
	fmt.Println("Generated Go XML checks passed")
}
