package main

import (
	"bytes"
	"fmt"
	model "serializer.example/references/go/internal/model"
)

// Check each condition while keeping the runnable example independent of testing tools.
func require(value bool) {
	if !value {
		panic("Transparent payload export mismatch")
	}
}

// Export source payload buffers directly, with explicit scopes for borrowed access.
func main() {
	value := model.NewPayload()
	value.Values = []float32{7, 8}
	a := model.NewSharedOwner()
	if err := a.B.Bind(value); err != nil {
		panic(err)
	}
	c := model.NewSharedValueConsumer()
	if err := c.D.Bind(value); err != nil {
		panic(err)
	}
	first, err := a.B.ReadValue()
	if err != nil {
		panic(err)
	}
	second, err := c.D.ReadValue()
	if err != nil {
		panic(err)
	}
	require(first == value && second == value)
	value = nil
	a.B.Unbind()
	independentExpected := model.NewOwningConsumer()
	independentExpected.D = second
	for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
		expected, err := independentExpected.Encode(protocol)
		if err != nil {
			panic(err)
		}
		actual, err := c.Encode(protocol)
		if err != nil {
			panic(err)
		}
		require(bytes.Equal(actual, expected))
	}
	owner := model.NewOwner()
	owner.B.Values = []float32{1, 2, 3}
	owner.B.Codes = []uint32{0, 1, 2}
	borrowed := model.NewBorrowedConsumer()
	scope := model.NewBorrowedConsumerDValueReferenceScope(owner)
	defer scope.Close()
	if err := borrowed.D.Bind(scope); err != nil {
		panic(err)
	}
	shared := model.NewSharedConsumer()
	if err := shared.D.Bind(owner); err != nil {
		panic(err)
	}
	weak := model.NewWeakConsumer()
	weakScope := model.NewWeakConsumerDValueReferenceScope(owner)
	defer weakScope.Close()
	if err := weak.D.Bind(weakScope); err != nil {
		panic(err)
	}
	selected, err := shared.D.ReadValue()
	if err != nil {
		panic(err)
	}
	require(selected == owner.B && &selected.Values[0] == &owner.B.Values[0])
	owner.B.Values[0] = 4
	expected := model.NewOwningConsumer()
	expected.D = owner.B
	for _, protocol := range []model.Protocol{model.JSON, model.BINARY_NONE, model.BINARY_INTEGER, model.BINARY_STRING} {
		encoded, err := expected.Encode(protocol)
		if err != nil {
			panic(err)
		}
		got, err := borrowed.Encode(protocol)
		if err != nil {
			panic(err)
		}
		require(bytes.Equal(got, encoded))
		got, err = shared.Encode(protocol)
		if err != nil {
			panic(err)
		}
		require(bytes.Equal(got, encoded))
		got, err = weak.Encode(protocol)
		if err != nil {
			panic(err)
		}
		require(bytes.Equal(got, encoded))
		owner.WithAsConsumer(func(projected model.OwnerAsConsumerTranslator) {
			got, err := projected.Encode(protocol)
			if err != nil {
				panic(err)
			}
			require(bytes.Equal(got, encoded))
		})
		loaded, err := model.DecodeOwningConsumer(encoded, protocol)
		if err != nil {
			panic(err)
		}
		loaded.D.Values[0] = 99
		require(owner.B.Values[0] == 4)
	}
	output, err := shared.Encode(model.JSON)
	if err != nil {
		panic(err)
	}
	fmt.Println(string(output))
}
