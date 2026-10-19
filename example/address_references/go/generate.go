package main

// Generate the shared schema into this native module before building or testing it.
//go:generate serializer --input model.serializer --language go --go.package model --output internal/model/model_gen.go --depfile internal/model/model_gen.go.d
