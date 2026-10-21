// Copyright (C) 2026 Rohit Jairaj Singh (rohit@singh.org.in)
// SPDX-License-Identifier: 0BSD
package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"time"
)

var checks int
var entriesPerRegistry int
var resources = []SerializerPackageResource{SerializerPackageReferences, SerializerPackageWorkUnits, SerializerPackageDecoderAllocationBytes, SerializerPackagePreservedBytes}
var preserve = SerializerPackageExtensionPolicy{Parts: SerializerPackagePreserve, Relationships: SerializerPackagePreserve}

// require rejects a false decision independently of generated error handling.
func require(value bool, message string) {
	if !value {
		panic(message)
	}
}

// limits bounds the complete repeated pipeline without promising a physical process-memory ceiling.
func limits() SerializerPackageLimits {
	value := SerializerPackageDefaultLimits()
	value.MaxReferences = 4000000
	value.MaxDecoderAllocationBytes = 2 << 30
	value.MaxWorkUnits = 4 << 30
	return value
}

// maximum maps public resource identifiers to their corresponding retained policy ceilings.
func maximum(value SerializerPackageLimits, resource SerializerPackageResource) int64 {
	switch resource {
	case SerializerPackageReferences:
		return value.MaxReferences
	case SerializerPackageWorkUnits:
		return value.MaxWorkUnits
	case SerializerPackageDecoderAllocationBytes:
		return value.MaxDecoderAllocationBytes
	case SerializerPackagePreservedBytes:
		return value.MaxPreservedBytes
	}
	panic("Unexpected resource")
}

// ceiling changes one exact/one-tight capacity without relaxing any other bound.
func ceiling(value *SerializerPackageLimits, resource SerializerPackageResource, count int64) {
	switch resource {
	case SerializerPackageReferences:
		value.MaxReferences = count
	case SerializerPackageWorkUnits:
		value.MaxWorkUnits = count
	case SerializerPackageDecoderAllocationBytes:
		value.MaxDecoderAllocationBytes = count
	case SerializerPackagePreservedBytes:
		value.MaxPreservedBytes = count
	default:
		panic("Unexpected resource")
	}
}

// used derives consumption exclusively through the public snapshot and remaining allowance.
func used(budget *SerializerPackageBudget, resource SerializerPackageResource) int64 {
	return maximum(budget.Configured(), resource) - budget.Remaining(resource)
}

// pass records a completely executed decision.
func pass(name string) { checks++; fmt.Println("PASS " + name) }

// shape requires all large owning values and inert extension/link metadata.
func shape(value *IdentityPackageArchive, count int) {
	require(value != nil && len(value.Registries) == 3 && len(value.Extensions.Parts) == 1 && len(value.Extensions.Relationships) == 3, "Aggregate/preservation shape")
	for _, item := range value.Registries {
		require(len(item.Value.Requests) == count && len(item.Value.Entries) == count, "Incomplete large model")
	}
}

// load invokes only the generated public owning facade and its native XML bindings.
func load(data []byte, budget *SerializerPackageBudget) (*IdentityPackageArchive, error) {
	return SerializerPackageCodecsIdentityPackageArchiveLoad(data, IdentityPackageArchiveLoadOptions{Budget: budget, ExtensionPolicy: preserve}, decodeModel)
}

// budget creates a finite caller-owned ledger or fails the test immediately.
func budget(policy SerializerPackageLimits) *SerializerPackageBudget {
	value, err := SerializerPackageNewBudget(policy)
	if err != nil {
		panic(err)
	}
	return value
}

// quota requires a resource-limit rejection rather than an unrelated runtime/provider error.
func quota(err error) bool {
	var value *SerializerPackageError
	return errors.As(err, &value) && value.Code == SerializerPackageResourceLimit
}

// encodeModel admits encoding/xml's physical child declarations through the public selected codec.
// The original XmlOptions.Validator carries the package operation; retain it unchanged.
func encodeModel(value *IdentityXmlRegistry, options XmlOptions, part, id string) ([]byte, error) {
	options.Limits.MaxAttributes = 2*len(value.Requests) + 3*len(value.Entries) + 8
	return value.EncodeXML(options)
}

// decodeModel admits the same finite physical namespace inventory on round-trip input.
func decodeModel(data []byte, options XmlOptions, part, id string) (*IdentityXmlRegistry, error) {
	options.Limits.MaxAttributes = 5*entriesPerRegistry + 8
	return DecodeIdentityXmlRegistryXML(data, options)
}

// pipeline spans DEFLATE and repeated native XML identity admission without creating a replacement ledger.
func pipeline(data []byte, path string, selected *SerializerPackageBudget, count int) error {
	previous := used(selected, resources[0])
	value, err := load(data, selected)
	if err != nil {
		return err
	}
	shape(value, count)
	require(used(selected, resources[0]) > previous, "Load bypassed references")
	previous = used(selected, resources[0])
	encoded, err := SerializerPackageCodecsIdentityPackageArchiveEncode(value, map[string]string{"registries": "deflate"}, selected, encodeModel)
	if err != nil {
		return err
	}
	require(used(selected, resources[0]) > previous, "Encode reset references")
	previous = used(selected, resources[0])
	reopened, err := load(encoded, selected)
	if err != nil {
		return err
	}
	shape(reopened, count)
	require(used(selected, resources[0]) > previous, "Reload reset references")
	return SerializerPackageCodecsIdentityPackageArchiveSave(path, value, IdentityPackageArchiveSaveOptions{Budget: selected}, encodeModel, decodeModel)
}

// metrics exposes measured logical admissions for the independent report.
func metrics(value *SerializerPackageBudget) map[string]int64 {
	return map[string]int64{"references": used(value, resources[0]), "work": used(value, resources[1]), "allocation": used(value, resources[2]), "preserved": used(value, resources[3])}
}

// main executes large malformed, cumulative, interruption and atomic-publication controls.
func main() {
	directory := os.Args[1]
	count, err := strconv.Atoi(os.Args[2])
	if err != nil {
		panic(err)
	}
	entriesPerRegistry = count
	valid, err := os.ReadFile(filepath.Join(directory, "valid.opc"))
	if err != nil {
		panic(err)
	}
	invalid, err := os.ReadFile(filepath.Join(directory, "invalid.opc"))
	if err != nil {
		panic(err)
	}
	baseline := budget(limits())
	value, err := load(valid, baseline)
	if err != nil {
		panic(err)
	}
	shape(value, count)
	require(used(baseline, resources[0]) >= 9*int64(count), "Identity tuples bypassed ledger")
	pass("large generated identity load and preserved opaque/link metadata")
	validated := false
	candidate, err := IdentityPackageArchiveLoad(invalid, IdentityPackageArchiveLoadOptions{Budget: budget(limits()), ExtensionPolicy: preserve, Validate: func(*IdentityPackageArchive, *SerializerPackageBudget) error { validated = true; return nil }})
	require(candidate == nil && err != nil && !validated, "Invalid final-owner keyref published")
	pass("invalid final owner has no aggregate publication")
	for _, resource := range resources {
		for _, tight := range []bool{false, true} {
			policy := limits()
			maximum := used(baseline, resource)
			if tight {
				maximum--
			}
			ceiling(&policy, resource, maximum)
			candidate, err = load(valid, budget(policy))
			if tight {
				require(candidate == nil && quota(err), "Wrong tight quota failure")
			} else {
				if err != nil {
					panic(err)
				}
				shape(candidate, count)
			}
			pass(fmt.Sprintf("resource %d tight %v", resource, tight))
		}
	}
	for _, mode := range []string{"middle", "late", "expiry", "original"} {
		marker := fmt.Errorf("original %s", mode)
		secondary := fmt.Errorf("secondary cancellation")
		armed := false
		var expiry time.Time
		var selected *SerializerPackageBudget
		policy := limits()
		policy.CheckCancelled = func() error {
			if selected == nil {
				return nil
			}
			if (mode == "middle" || mode == "expiry") && used(selected, resources[0]) >= 9*int64(count)/2 {
				if mode == "middle" {
					return marker
				}
				if expiry.IsZero() {
					expiry = time.Now()
				}
				if !time.Now().Before(expiry) {
					return marker
				}
			}
			if armed {
				if mode == "original" {
					return secondary
				}
				return marker
			}
			return nil
		}
		selected = budget(policy)
		candidate, err = IdentityPackageArchiveLoad(valid, IdentityPackageArchiveLoadOptions{Budget: selected, ExtensionPolicy: preserve, Validate: func(value *IdentityPackageArchive, active *SerializerPackageBudget) error {
			require(active == selected, "Aggregate validator received a fresh ledger")
			shape(value, count)
			armed = true
			if mode == "original" {
				return marker
			}
			return nil
		}})
		require(candidate == nil && err == marker, "Cancellation identity/publication changed: "+mode)
		if mode == "expiry" {
			require(!expiry.IsZero(), "Host monotonic expiry not reached")
		}
		pass("large load " + mode)
	}
	marker := fmt.Errorf("original encode cancellation")
	var selected *SerializerPackageBudget
	policy := limits()
	policy.CheckCancelled = func() error {
		if selected != nil && used(selected, resources[0]) >= 9*int64(count)/2 {
			return marker
		}
		return nil
	}
	selected = budget(policy)
	sink := new(bytes.Buffer)
	_, err = SerializerPackageCodecsIdentityPackageArchiveWrite(sink, value, selected, encodeModel)
	require(err == marker && sink.Len() == 0, fmt.Sprintf("Incomplete XML reached sink: error=%v exact=%v bytes=%d refs=%d", err, err == marker, sink.Len(), used(selected, resources[0])))
	pass("middle encode cancellation publishes no sink bytes")
	old := []byte("existing complete sentinel")
	guarded := filepath.Join(directory, "guarded.opc")
	if err = os.WriteFile(guarded, old, 0600); err != nil {
		panic(err)
	}
	armed := false
	policy = limits()
	policy.CheckCancelled = func() error {
		if armed {
			return marker
		}
		return nil
	}
	err = SerializerPackageCodecsIdentityPackageArchiveSave(guarded, value, IdentityPackageArchiveSaveOptions{Budget: budget(policy), Fault: func(stage string) error {
		if stage == "before_replace" {
			armed = true
		}
		return nil
	}}, encodeModel, decodeModel)
	require(err == marker && armed, fmt.Sprintf("Late Save error changed: %v armed=%v", err, armed))
	actual, err := os.ReadFile(guarded)
	if err != nil {
		panic(err)
	}
	require(bytes.Equal(actual, old), "Late Save replaced sentinel")
	pass("late atomic Save preserves previous file")
	combined := budget(limits())
	if err = pipeline(valid, filepath.Join(directory, "output.opc"), combined, count); err != nil {
		panic(err)
	}
	pass("shared load DEFLATE reload Save pipeline")
	for _, tight := range []bool{false, true} {
		policy = limits()
		policy.MaxReferences = used(combined, resources[0])
		name := "exact.opc"
		if tight {
			policy.MaxReferences--
			name = "tight.opc"
		}
		path := filepath.Join(directory, name)
		if err = os.WriteFile(path, old, 0600); err != nil {
			panic(err)
		}
		err = pipeline(valid, path, budget(policy), count)
		if tight {
			actual, readErr := os.ReadFile(path)
			require(readErr == nil && quota(err) && bytes.Equal(actual, old), "Combined quota published Save")
		} else if err != nil {
			panic(err)
		}
		pass(fmt.Sprintf("combined reference tight %v", tight))
	}
	recovery, err := os.ReadFile(filepath.Join(directory, "output.opc"))
	if err != nil {
		panic(err)
	}
	reopened, err := load(recovery, budget(limits()))
	if err != nil {
		panic(err)
	}
	shape(reopened, count)
	pass("fresh load recovery")
	report, err := json.Marshal(map[string]any{"load": metrics(baseline), "combined": metrics(combined), "checks": checks})
	if err != nil {
		panic(err)
	}
	fmt.Println("LEDGER\t" + string(report))
}
