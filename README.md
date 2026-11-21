# cine

`cine` is a collection of language-specific libraries for loading JSON configuration into application properties. Each implementation is maintained in its own folder and repository history.

## Support status

| Language | Status | Folder |
| --- | --- | --- |
| Go | Implemented | [`go/`](go/) |
| C | Planned | [`c/`](c/) |
| C++ | Planned | [`cpp/`](cpp/) |
| C# | Planned | [`csharp/`](csharp/) |
| Java | Planned | [`java/`](java/) |
| JavaScript | Planned | [`js/`](js/) |
| Python | Planned | [`python/`](python/) |
| Rust | Planned | [`rust/`](rust/) |

Go is the only implementation currently available. See [`go/README.md`](go/README.md) for installation, usage, supported types, and parsing behavior. Planned language folders contain only a status README.

## Quick start (Go)

```sh
cd go
go test ./...
```

```go
package main

import cine "github.com/miguellucietto/cine-go"

func main() {
	config := cine.New("config.json", cine.RejectUnknown)
	config.Prop("theme", "light")
	config.Prop("ports", []int{8080})
	properties, err := config.Parse()
	if err != nil {
		panic(err)
	}
	_ = properties
}
```

## Repository layout

```text
.
├── README.md
├── LICENSE
├── roadmap/       # Shared project direction and task list
├── go/            # Go module, tests, and JSON fixtures
├── c/             # Planned C implementation
├── cpp/           # Planned C++ implementation
├── csharp/        # Planned C# implementation
├── java/          # Planned Java implementation
├── js/            # Planned JavaScript implementation
├── python/        # Planned Python implementation
└── rust/          # Planned Rust implementation
```

Each language folder has an independent Git history and release lifecycle. The `roadmap/` folder contains shared planning documents.

## License

MIT. See [`LICENSE`](LICENSE).
