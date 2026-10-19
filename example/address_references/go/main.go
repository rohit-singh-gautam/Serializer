// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
	"fmt"

	"serializer.example/address_references/go/internal/model"
)

// require keeps the ownership and decoded-address checks active in every build.
func require(condition bool) {
	if !condition {
		panic("Reference example contract failed")
	}
}

// must stops on a failed generated binding or codec operation.
func must(err error) {
	if err != nil {
		panic(err)
	}
}

// projectedJSON borrows a live field and writes the target shape without constructing that target.
func projectedJSON() []byte {
	storage := model.NewOwner()
	storage.B.Text, storage.B.Count = "original", 1
	links := model.NewConsumer()
	must(links.D.Bind(storage, "document-1", 7, 1))
	borrowedScope := model.NewBorrowedBScope(storage)
	weakScope := model.NewWeakBScope(storage)
	borrowed, weak := model.NewBorrowedB(), model.NewWeakB()
	must(borrowed.Bind(borrowedScope, "document-1", 7, 1))
	must(weak.Bind(weakScope, "document-1", 7, 1))

	storage.B.Text, storage.B.Count = "updated", 2
	var payload []byte
	var encodeError error
	must(links.D.WithValue(func(selected *model.LiveValue) {
		selected.WithToWire(func(projected model.LiveValueToWireTranslator) {
			payload, encodeError = projected.Encode(model.JSON)
		})
	}))
	must(encodeError)
	received, err := model.DecodeProjectedValue(payload, model.JSON)
	must(err)
	require(received.Label == "updated" && received.Count == 2)

	// Construct owners first and resolve the separate address using application-owned identity.
	ownerBytes, err := storage.Encode(model.JSON)
	must(err)
	loadedOwner, err := model.DecodeOwner(ownerBytes, model.JSON)
	must(err)
	addressBytes, err := links.Encode(model.JSON)
	must(err)
	loaded, err := model.DecodeConsumer(addressBytes, model.JSON)
	must(err)
	require(!loaded.D.IsBound())
	require(loaded.D.Resolve(loadedOwner, "document-1", 7, 1))

	// Go scopes express logical non-owning lifetime explicitly; close them before owner disposal.
	storage = nil
	borrowedScope.Close()
	weakScope.Close()
	require(!borrowed.IsBound() && !weak.IsBound())
	require(links.D.IsBound())
	links.D.Unbind()
	return payload
}

// main prints the projected target JSON, whose label field comes from the live text field.
func main() {
	fmt.Println(string(projectedJSON()))
}
