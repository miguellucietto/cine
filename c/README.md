# cine for C

`cine` loads a JSON object into registered C configuration properties. Each
registered property has a type schema and a default value. Values found in the
JSON file replace those defaults in the parse result; the registered defaults
remain unchanged.

The library targets C11 and has no external JSON dependency.

## Repository layout

- `cine.c` and `cine.h`: the library implementation and public API.
- `tests/`: executable tests and the JSON files consumed by those tests.
- `examples/`: a complete application example and its JSON configuration.

## Build, test, and run the example

From this directory:

```sh
make             # build libcine.a
make test        # build and run the test suite
make example     # build and run the example application
make clean       # remove generated build outputs
```

The test suite uses the JSON fixtures under `tests/fixtures/`. The example
loads `examples/config.json` and prints selected settings.

## Quick start

```c
#include "cine.h"

#include <stdio.h>

int main(void)
{
    CineConfig *config = NULL;
    CineProperties properties = {0};
    CineError error = {0};
    CineValue port_default = cine_value_int(3000);

    if (!cine_config_new("config.json", CINE_REJECT_UNKNOWN,
                         &config, &error)) {
        return 1;
    }
    if (!cine_config_prop(config, "port", &CINE_TYPE_INT,
                          &port_default, &error) ||
        !cine_config_parse(config, &properties, &error)) {
        fprintf(stderr, "Configuration error: %s\n", error.message);
        cine_properties_free(&properties);
        cine_config_free(config);
        return 1;
    }

    const CineValue *port = cine_properties_get(&properties, "port");
    printf("Listening on port %lld\n", (long long)port->as.integer);

    cine_properties_free(&properties);
    cine_config_free(config);
    return 0;
}
```

Compile an application against the static library with:

```sh
cc -std=c11 -I. app.c libcine.a -o app
```

## Types and arrays

C has no runtime reflection, so each property receives a `CineType` schema.
Primitive schemas are exposed as constants, such as `CINE_TYPE_STRING`,
`CINE_TYPE_BOOL`, `CINE_TYPE_INT`, and `CINE_TYPE_FLOAT64`.

Use `cine_type_array(&ELEMENT_TYPE)` for a dynamic array and
`cine_type_fixed_array(&ELEMENT_TYPE, LENGTH)` for an array that must have an
exact length. Schemas can be nested by keeping each `CineType` alive through
property registration. For example:

```c
CineType integer_list = cine_type_array(&CINE_TYPE_INT);
CineType matrix = cine_type_array(&integer_list);
```

Dynamic arrays accept JSON `null` as an empty array. Fixed arrays require an
array of the declared length. Array elements are converted recursively.
`CINE_TYPE_ANY` accepts any JSON value and retains its JSON kind. JSON numbers
inside an `ANY` value retain their original decimal text as `CINE_NUMBER`.

JSON numbers converted to `CINE_TYPE_INT` must be exact integers in the signed
64-bit range. Integral decimal and exponent forms such as `12.0` and `1.2e3`
are accepted. `CINE_TYPE_FLOAT64` parses JSON numbers as `double`.

## Unknown properties

Pass one of these policies to `cine_config_new`:

- `CINE_IGNORE_UNKNOWN`: skip JSON keys that were not registered.
- `CINE_REJECT_UNKNOWN`: fail with `CINE_ERROR_UNKNOWN_PROPERTY`.
- `CINE_PANIC_UNKNOWN`: print the unknown key and call `abort()`.

Use `CINE_REJECT_UNKNOWN` for applications that need normal error handling.

## Ownership and errors

- Initialize `CineProperties` with `{0}` before its first parse.
- `cine_config_prop` clones the default value. Constructor values and their
  backing arrays/objects are borrowed during registration, so they can be
  stack allocated and do not need to outlive the call.
- A successful parse allocates its result. Read values with
  `cine_properties_get` and release the result with `cine_properties_free`.
- Release each configuration with `cine_config_free`.
- Parse results are atomic: on failure, the output is not modified and the
  registered defaults are not modified.
- Functions that return `bool` report details through `CineError`. Its `code`
  identifies the error category and `message` provides a readable summary.
  JSON syntax errors also report a line and column.

The configuration object owns its registered property names, type schemas,
and cloned defaults. Keep nested type schemas alive until `cine_config_prop`
has returned; the implementation copies the schema during registration.
