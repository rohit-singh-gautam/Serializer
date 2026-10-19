package main

// Regenerate the native Go API before building this module.
//go:generate serializer --input model.serializer --language go --go.output internal/model/model_gen.go --go.package model
