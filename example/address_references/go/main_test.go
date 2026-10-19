package main

import (
	"encoding/json"
	"testing"
)

// TestProjectedJSON checks the wire result independently of the generated target decoder.
func TestProjectedJSON(t *testing.T) {
	var target struct {
		Label string `json:"label"`
		Count uint32 `json:"count"`
	}
	if err := json.Unmarshal(projectedJSON(), &target); err != nil {
		t.Fatal(err)
	}
	if target.Label != "updated" || target.Count != 2 {
		t.Fatalf("unexpected projected target: %+v", target)
	}
}
