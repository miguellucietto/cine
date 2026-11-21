# cine

`cine` is a small Go library for loading JSON configuration into registered
application properties. Registered values provide defaults and define the
expected types of values read from the file.

## Install

```sh
go get github.com/miguellucietto/cine-go
```

## Usage

```go
package main

import (
	"fmt"
	"log"

	cine "github.com/miguellucietto/cine-go"
)

func main() {
	config := cine.New("config.json", cine.RejectUnknown)
	config.Prop("theme", "light")
	config.Prop("fontSize", float64(14))
	config.Prop("ports", []int{8080})

	properties, err := config.Parse()
	if err != nil {
		log.Fatal(err)
	}

	fmt.Println(properties["theme"])
	fmt.Println(properties["ports"])
}
```

The corresponding JSON can contain scalar values and arrays:

```json
{
  "theme": "dark",
  "fontSize": 16.5,
  "ports": [8080, 8443]
}
```

## Type conversion

The type of each registered default controls conversion and validation:

- Strings, booleans, and other non-numeric values must match the registered
  type.
- JSON numbers registered as `int` must be mathematically integral and fit in
  the platform's `int` range. Integer parsing preserves the exact digits in the
  JSON file.
- JSON numbers registered as `float64` are parsed as `float64`; use this type
  for fractional values.
- A registered slice such as `[]int{}` or `[]string{}` accepts a JSON array and
  converts each element to the slice's element type. Nested slices are
  converted recursively.
- A registered fixed-size array such as `[2]int{}` requires the JSON array to
  have exactly that length.
- A registered `[]any{}` accepts mixed array elements. Numeric elements remain
  `json.Number` values so callers can decide how to convert them without losing
  precision.

`Parse()` reads the path passed to `New`. `ParseFile(path)` reads the supplied
path without changing `Config.FilePath`. Both methods return a new properties
map and leave registered defaults unchanged.

## Unknown properties

- `cine.IgnoreUnknown` skips unregistered JSON keys.
- `cine.RejectUnknown` returns an error for an unregistered key.
- `cine.PanicUnknown` panics for an unregistered key. Prefer `RejectUnknown`
  when the caller should handle errors normally.

`Config` exposes mutable fields and is not safe to mutate concurrently with
`Parse` or `ParseFile`.

## Tests

Run the test suite with:

```sh
go test ./...
```
