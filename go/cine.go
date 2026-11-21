// Package cine loads JSON configuration values into a set of registered
// properties. Registered values provide defaults and define the types used to
// validate and convert values read from a configuration file.
package cine

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"maps"
	"math"
	"math/big"
	"os"
	"reflect"
	"sort"
	"strconv"
)

// UnknownPolicy controls how ParseFile handles keys that were not registered
// with Config.Prop.
type UnknownPolicy uint8

const (
	// IgnoreUnknown skips keys that have no registered property.
	IgnoreUnknown UnknownPolicy = iota
	// RejectUnknown returns an error when the file contains an unregistered key.
	RejectUnknown
	// PanicUnknown panics when the file contains an unregistered key.
	// Prefer RejectUnknown in libraries and applications that need recoverable errors.
	PanicUnknown
)

// Config stores registered defaults and the options used to parse JSON files.
// Its exported fields and property map are mutable; callers must not mutate a
// Config concurrently with Parse or ParseFile.
type Config struct {
	// Properties maps registered property names to their default values. The
	// defaults also determine the concrete types used during parsing.
	Properties map[string]any
	// FilePath is the path read by Parse.
	FilePath string
	// Policy determines how unknown JSON property names are handled.
	Policy UnknownPolicy
}

func validPolicy(policy UnknownPolicy) bool {
	return policy == IgnoreUnknown || policy == RejectUnknown || policy == PanicUnknown
}

// New creates a Config for filePath and policy. It panics if policy is not a
// defined UnknownPolicy value.
func New(filePath string, policy UnknownPolicy) *Config {
	if !validPolicy(policy) {
		panic("cine: invalid unknown property policy")
	}
	return &Config{
		Properties: make(map[string]any),
		FilePath:   filePath,
		Policy:     policy,
	}
}

// Prop registers name with value as its default and expected type. Register
// slices or arrays with the element types the JSON array should contain.
func (c *Config) Prop(name string, value any) {
	c.Properties[name] = value
}

// Parse reads and validates the file at Config.FilePath.
func (c *Config) Parse() (map[string]any, error) {
	return c.ParseFile(c.FilePath)
}

// ParseFile reads filePath and returns a new map containing registered
// defaults overridden by values from the JSON object. It does not modify
// Config.Properties. A JSON array property is converted recursively using the
// registered slice or array element type; fixed-size arrays require an exact
// length match.
func (c *Config) ParseFile(filePath string) (map[string]any, error) {
	data, err := os.ReadFile(filePath)
	if err != nil {
		return nil, fmt.Errorf("read configuration %q: %w", filePath, err)
	}

	properties, err := decodeProperties(data)
	if err != nil {
		return nil, fmt.Errorf("parse configuration %q: %w", filePath, err)
	}

	result := maps.Clone(c.Properties)
	keys := make([]string, 0, len(properties))
	for key := range properties {
		keys = append(keys, key)
	}
	sort.Strings(keys)

	for _, key := range keys {
		value := properties[key]
		expected, ok := result[key]
		if !ok {
			switch c.Policy {
			case IgnoreUnknown:
				continue
			case PanicUnknown:
				panic(fmt.Sprintf("Unknown property: %s", key))
			default:
				return nil, fmt.Errorf("unknown property %q", key)
			}
		}

		converted, err := convertValue(expected, value)
		if err != nil {
			return nil, fmt.Errorf("property %q: expected %T, got %T: %w", key, expected, value, err)
		}
		result[key] = converted
	}
	return result, nil
}

// decodeProperties requires one top-level JSON object. UseNumber keeps numeric
// literals exact until they can be converted according to the registered type.
func decodeProperties(data []byte) (map[string]any, error) {
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.UseNumber()

	var properties map[string]any
	if err := decoder.Decode(&properties); err != nil {
		return nil, err
	}
	if properties == nil {
		return nil, fmt.Errorf("top-level JSON value must be an object")
	}

	var extra any
	if err := decoder.Decode(&extra); err != io.EOF {
		if err == nil {
			return nil, fmt.Errorf("configuration must contain exactly one JSON value")
		}
		return nil, err
	}
	return properties, nil
}

// convertValue applies the registered property's type recursively to a value
// decoded from JSON.
func convertValue(expected, value any) (any, error) {
	expectedType := reflect.TypeOf(expected)
	if expectedType == nil {
		if value == nil {
			return nil, nil
		}
		return nil, fmt.Errorf("no registered type can be inferred from a nil default")
	}

	switch expectedType.Kind() {
	case reflect.Int:
		converted, err := convertInt(value, expectedType.Bits())
		if err != nil {
			return nil, err
		}
		return reflect.ValueOf(converted).Convert(expectedType).Interface(), nil
	case reflect.Float64:
		converted, err := convertFloat64(value)
		if err != nil {
			return nil, err
		}
		return reflect.ValueOf(converted).Convert(expectedType).Interface(), nil
	case reflect.Slice, reflect.Array:
		return convertCollection(expectedType, value)
	}

	if expectedType == reflect.TypeOf(value) {
		return value, nil
	}
	return nil, fmt.Errorf("incompatible types")
}

// convertInt accepts only whole values within the signed range described by
// bits. json.Number is evaluated as an exact rational to avoid float rounding.
func convertInt(value any, bits int) (int, error) {
	switch number := value.(type) {
	case int:
		return number, nil
	case float64:
		limit := math.Ldexp(1, bits-1)
		if math.IsNaN(number) || math.IsInf(number, 0) || math.Trunc(number) != number || number < -limit || number >= limit {
			return 0, fmt.Errorf("%v is not an integral value in the int range", number)
		}
		return int(number), nil
	case json.Number:
		rational, ok := new(big.Rat).SetString(number.String())
		if !ok || !rational.IsInt() {
			return 0, fmt.Errorf("%q is not an integer", number)
		}
		converted, err := strconv.ParseInt(rational.Num().String(), 10, bits)
		if err != nil {
			return 0, fmt.Errorf("%q is outside the int range", number)
		}
		return int(converted), nil
	default:
		return 0, fmt.Errorf("value is not numeric")
	}
}

// convertFloat64 parses JSON numbers and rejects integer conversions that
// would silently lose precision.
func convertFloat64(value any) (float64, error) {
	switch number := value.(type) {
	case float64:
		return number, nil
	case int:
		converted := float64(number)
		exact := new(big.Rat).SetFloat64(converted)
		if exact.Cmp(big.NewRat(int64(number), 1)) != 0 {
			return 0, fmt.Errorf("%d cannot be represented exactly as float64", number)
		}
		return converted, nil
	case json.Number:
		converted, err := strconv.ParseFloat(number.String(), 64)
		if err != nil {
			return 0, fmt.Errorf("%q cannot be represented as float64: %w", number, err)
		}
		return converted, nil
	default:
		return 0, fmt.Errorf("value is not numeric")
	}
}

// convertCollection builds a typed slice or array, reporting the failing
// element index when recursive conversion cannot satisfy its element type.
func convertCollection(expectedType reflect.Type, value any) (any, error) {
	items, ok := value.([]any)
	if !ok {
		if value == nil && expectedType.Kind() == reflect.Slice {
			return reflect.Zero(expectedType).Interface(), nil
		}
		return nil, fmt.Errorf("value is not a JSON array")
	}
	if expectedType.Kind() == reflect.Array && len(items) != expectedType.Len() {
		return nil, fmt.Errorf("array has length %d, want %d", len(items), expectedType.Len())
	}

	var converted reflect.Value
	if expectedType.Kind() == reflect.Slice {
		converted = reflect.MakeSlice(expectedType, len(items), len(items))
	} else {
		converted = reflect.New(expectedType).Elem()
	}

	for index, item := range items {
		elementType := expectedType.Elem()
		var element any
		var err error
		if elementType.Kind() == reflect.Interface && elementType.NumMethod() == 0 {
			element = item
		} else {
			element, err = convertValue(reflect.Zero(elementType).Interface(), item)
			if err != nil {
				return nil, fmt.Errorf("element %d: %w", index, err)
			}
		}

		if element == nil {
			converted.Index(index).Set(reflect.Zero(elementType))
			continue
		}
		value := reflect.ValueOf(element)
		if !value.Type().AssignableTo(elementType) {
			return nil, fmt.Errorf("element %d has type %s, which cannot be assigned to %s", index, value.Type(), elementType)
		}
		converted.Index(index).Set(value)
	}
	return converted.Interface(), nil
}
