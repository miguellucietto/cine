package cine

import (
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
	"testing"
)

func testConfigPath(name string) string {
	return filepath.Join("testdata", name)
}

func writeConfigFile(t *testing.T, contents string) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "config.json")
	if err := os.WriteFile(path, []byte(contents), 0600); err != nil {
		t.Fatal(err)
	}
	return path
}

func TestNew(t *testing.T) {
	config := New("settings.json", RejectUnknown)

	if config.FilePath != "settings.json" {
		t.Errorf("FilePath = %q, want %q", config.FilePath, "settings.json")
	}
	if config.Policy != RejectUnknown {
		t.Errorf("Policy = %v, want %v", config.Policy, RejectUnknown)
	}
	if config.Properties == nil {
		t.Fatal("Properties should be initialized")
	}
	if len(config.Properties) != 0 {
		t.Errorf("Properties has %d entries, want 0", len(config.Properties))
	}
}

func TestNewPanicsOnInvalidUnknownPolicy(t *testing.T) {
	defer func() {
		if recovered := recover(); recovered == nil {
			t.Fatal("New() did not panic for an invalid policy")
		}
	}()
	New("settings.json", UnknownPolicy(255))
}

func TestPropRegistersAndReplacesProperty(t *testing.T) {
	config := New("", IgnoreUnknown)
	config.Prop("theme", "light")
	config.Prop("theme", "dark")

	if got := config.Properties["theme"]; got != "dark" {
		t.Errorf("theme = %v, want %q", got, "dark")
	}
}

func TestParseLoadsRegisteredPropertiesAndKeepsDefaults(t *testing.T) {
	config := New(testConfigPath("valid.json"), RejectUnknown)
	config.Prop("theme", "light")
	config.Prop("fontSize", float64(12))
	config.Prop("showHints", true)

	got, err := config.Parse()
	if err != nil {
		t.Fatalf("Parse() error = %v", err)
	}

	want := map[string]any{
		"theme":     "dark",
		"fontSize":  float64(16),
		"showHints": true,
	}
	if !reflect.DeepEqual(got, want) {
		t.Errorf("Parse() = %#v, want %#v", got, want)
	}
	defaults := map[string]any{"theme": "light", "fontSize": float64(12), "showHints": true}
	if !reflect.DeepEqual(config.Properties, defaults) {
		t.Errorf("Parse() mutated registered defaults: got %#v, want %#v", config.Properties, defaults)
	}
}

func TestParseConvertsJSONNumbersToRegisteredNumericTypes(t *testing.T) {
	tests := []struct {
		name         string
		json         string
		defaultValue any
		want         any
	}{
		{name: "int default", json: `{"value":16}`, defaultValue: int(0), want: int(16)},
		{name: "integral decimal to int", json: `{"value":16.0}`, defaultValue: int(0), want: int(16)},
		{name: "float64 default", json: `{"value":16}`, defaultValue: float64(0), want: float64(16)},
		{name: "fractional float64", json: `{"value":16.25}`, defaultValue: float64(0), want: float64(16.25)},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			path := writeConfigFile(t, tt.json)
			config := New(path, RejectUnknown)
			config.Prop("value", tt.defaultValue)

			got, err := config.Parse()
			if err != nil {
				t.Fatalf("Parse() error = %v", err)
			}
			if reflect.TypeOf(got["value"]) != reflect.TypeOf(tt.want) || !reflect.DeepEqual(got["value"], tt.want) {
				t.Errorf("value = %#v (%T), want %#v (%T)", got["value"], got["value"], tt.want, tt.want)
			}
		})
	}
}

func TestParsePreservesLargeIntegerPrecision(t *testing.T) {
	if strconv.IntSize != 64 {
		t.Skip("test value requires a 64-bit int")
	}

	config := New(writeConfigFile(t, `{"value":9007199254740993}`), RejectUnknown)
	config.Prop("value", int(0))

	got, err := config.Parse()
	if err != nil {
		t.Fatalf("Parse() error = %v", err)
	}
	if got["value"] != int(9007199254740993) {
		t.Errorf("value = %#v (%T), want exact int 9007199254740993", got["value"], got["value"])
	}
}

func TestParseRejectsNonIntegralOrOutOfRangeNumberForInt(t *testing.T) {
	for _, number := range []string{"16.5", "1e100"} {
		t.Run(number, func(t *testing.T) {
			config := New(writeConfigFile(t, `{"value":`+number+`}`), RejectUnknown)
			config.Prop("value", int(0))
			if _, err := config.Parse(); err == nil {
				t.Errorf("Parse() accepted %s as int; want an error", number)
			}
		})
	}
}

func TestParseConvertsJSONArrays(t *testing.T) {
	config := New(writeConfigFile(t, `{"ports":[8080,8443],"labels":["web","secure"],"matrix":[[1.5,2],[3,4.5]],"tuple":[7,8]}`), RejectUnknown)
	config.Prop("ports", []int{})
	config.Prop("labels", []string{})
	config.Prop("matrix", [][]float64{})
	config.Prop("tuple", [2]int{})

	got, err := config.Parse()
	if err != nil {
		t.Fatalf("Parse() error = %v", err)
	}
	want := map[string]any{
		"ports":  []int{8080, 8443},
		"labels": []string{"web", "secure"},
		"matrix": [][]float64{{1.5, 2}, {3, 4.5}},
		"tuple":  [2]int{7, 8},
	}
	if !reflect.DeepEqual(got, want) {
		t.Errorf("Parse() = %#v, want %#v", got, want)
	}
}

func TestParseAcceptsMixedArrayWithExactJSONNumbers(t *testing.T) {
	config := New(writeConfigFile(t, `{"values":["id",9007199254740993,true]}`), RejectUnknown)
	config.Prop("values", []any{})

	got, err := config.Parse()
	if err != nil {
		t.Fatalf("Parse() error = %v", err)
	}
	want := []any{"id", json.Number("9007199254740993"), true}
	if !reflect.DeepEqual(got["values"], want) {
		t.Errorf("values = %#v, want %#v", got["values"], want)
	}
}

func TestParseRejectsArrayElementTypeAndLengthMismatches(t *testing.T) {
	tests := []struct {
		name         string
		json         string
		defaultValue any
	}{
		{name: "element type", json: `{"values":[1,"two"]}`, defaultValue: []int{}},
		{name: "fixed array length", json: `{"values":[1]}`, defaultValue: [2]int{}},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			config := New(writeConfigFile(t, tt.json), RejectUnknown)
			config.Prop("values", tt.defaultValue)
			if _, err := config.Parse(); err == nil {
				t.Fatal("Parse() error = nil, want array validation error")
			}
		})
	}
}

func TestParseRejectsNonObjectAndMultipleTopLevelJSONValues(t *testing.T) {
	for _, contents := range []string{"null", `[]`, `{} {}`} {
		t.Run(contents, func(t *testing.T) {
			config := New(writeConfigFile(t, contents), IgnoreUnknown)
			if _, err := config.Parse(); err == nil {
				t.Fatalf("Parse() accepted top-level JSON %q", contents)
			}
		})
	}
}

func TestParseIgnoresUnknownProperties(t *testing.T) {
	config := New(testConfigPath("unknown_property.json"), IgnoreUnknown)
	config.Prop("theme", "light")

	got, err := config.Parse()
	if err != nil {
		t.Fatalf("Parse() error = %v", err)
	}
	if want := map[string]any{"theme": "dark"}; !reflect.DeepEqual(got, want) {
		t.Errorf("Parse() = %#v, want %#v", got, want)
	}
}

func TestParseRejectsUnknownProperties(t *testing.T) {
	config := New(testConfigPath("unknown_property.json"), RejectUnknown)
	config.Prop("theme", "light")

	got, err := config.Parse()
	if err == nil {
		t.Fatal("Parse() error = nil, want an error for an unknown property")
	}
	if !strings.Contains(err.Error(), "unknown") {
		t.Errorf("Parse() error = %q, want it to mention the unknown property", err)
	}
	if got != nil {
		t.Errorf("Parse() = %#v, want nil when parsing fails", got)
	}
}

func TestParseReturnsFileReadError(t *testing.T) {
	config := New(filepath.Join("testdata", "missing.json"), IgnoreUnknown)

	if got, err := config.Parse(); err == nil {
		t.Fatalf("Parse() = %#v, error = nil; want a file read error", got)
	}
}

func TestParseReturnsInvalidJSONError(t *testing.T) {
	config := New(testConfigPath("invalid.json"), IgnoreUnknown)
	config.Prop("theme", "light")

	if got, err := config.Parse(); err == nil {
		t.Fatalf("Parse() = %#v, error = nil; want an invalid JSON error", got)
	}
}

func TestParseRejectsTypeMismatch(t *testing.T) {
	config := New(writeConfigFile(t, `{"fontSize":"large"}`), RejectUnknown)
	config.Prop("fontSize", 12)

	got, err := config.Parse()
	if err == nil {
		t.Fatal("Parse() error = nil, want a type mismatch error")
	}
	if !strings.Contains(err.Error(), "fontSize") {
		t.Errorf("Parse() error = %q, want it to mention fontSize", err)
	}
	if got != nil {
		t.Errorf("Parse() = %#v, want nil when parsing fails", got)
	}
}

func TestParseFileUsesProvidedPathWithoutChangingConfiguredPath(t *testing.T) {
	configuredPath := testConfigPath("invalid.json")
	config := New(configuredPath, RejectUnknown)
	config.Prop("theme", "light")
	config.Prop("fontSize", float64(12))

	got, err := config.ParseFile(testConfigPath("valid.json"))
	if err != nil {
		t.Fatalf("ParseFile() error = %v", err)
	}
	if got["theme"] != "dark" {
		t.Errorf("theme = %v, want %q", got["theme"], "dark")
	}
	if config.FilePath != configuredPath {
		t.Errorf("FilePath after ParseFile() = %q, want %q", config.FilePath, configuredPath)
	}
}

func TestParsePanicsOnUnknownPropertyWhenConfigured(t *testing.T) {
	config := New(testConfigPath("unknown_property.json"), PanicUnknown)
	config.Prop("theme", "light")

	defer func() {
		if recovered := recover(); recovered == nil {
			t.Fatal("Parse() did not panic for an unknown property")
		}
	}()
	_, _ = config.Parse()
}
