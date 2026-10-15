package main

import (
	"testing"

	model "serializer.example/custom_ids/go/internal/model"
)

// TestScopedRegistryExample checks binding, retirement, rebind identity, and all four protocol restores.
func TestScopedRegistryExample(t *testing.T) {
	main()
}

// testBinding constructs application-owned import data independently of the registry's bind path.
func testBinding(id *model.CustomId) *model.EntityBinding {
	binding := model.NewEntityBinding()
	binding.Id = id
	binding.Payload = 17
	return binding
}

// TestInvalidRegistryImports rejects invalid identities, duplicate keys, and live/retired overlap.
func TestInvalidRegistryImports(t *testing.T) {
	for _, invalid := range []struct {
		name    string
		prepare func(*model.RegistryState)
	}{
		{"nil_binding", func(state *model.RegistryState) {
			state.Bindings = []*model.EntityBinding{nil}
		}},
		{"nil_id", func(state *model.RegistryState) {
			state.Bindings = []*model.EntityBinding{testBinding(nil)}
		}},
		{"reserved_scope", func(state *model.RegistryState) {
			state.Bindings = []*model.EntityBinding{testBinding(makeID(0, widgetDomain, 42))}
		}},
		{"reserved_local_id", func(state *model.RegistryState) {
			state.Bindings = []*model.EntityBinding{testBinding(makeID(7, widgetDomain, 0))}
		}},
		{"unknown_domain", func(state *model.RegistryState) {
			state.Bindings = []*model.EntityBinding{testBinding(makeID(7, gadgetDomain+1, 42))}
		}},
		{"duplicate_live_id", func(state *model.RegistryState) {
			state.Bindings = []*model.EntityBinding{
				testBinding(makeID(7, widgetDomain, 42)),
				testBinding(makeID(7, widgetDomain, 42)),
			}
		}},
		{"duplicate_retired_id", func(state *model.RegistryState) {
			state.Retired = []*model.CustomId{makeID(7, widgetDomain, 42), makeID(7, widgetDomain, 42)}
		}},
		{"live_retired_overlap", func(state *model.RegistryState) {
			state.Bindings = []*model.EntityBinding{testBinding(makeID(7, widgetDomain, 42))}
			state.Retired = []*model.CustomId{makeID(7, widgetDomain, 42)}
		}},
	} {
		t.Run(invalid.name, func(t *testing.T) {
			state := model.NewRegistryState()
			invalid.prepare(state)
			registry := &applicationRegistry{state}
			if registry.validate() {
				t.Fatal("registry accepted an invalid imported state")
			}
		})
	}
}
