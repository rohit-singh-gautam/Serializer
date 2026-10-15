package main

// Generate the schema-owned model package before building or testing this module.
//go:generate serializer --input model.serializer --language go --go.package model --output internal/model/model_gen.go --depfile internal/model/model_gen.go.d
