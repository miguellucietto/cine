# cine

`cine` is a collection of language-specific libraries for loading JSON configuration into application properties. Each implementation is maintained in its own folder and repository history.

## Support status

| Language | Status | Folder |
| --- | --- | --- |
| Go | Implemented | [`go/`](go/) |
| C | Implemented | [`c/`](c/) |
| C++ | Planned | [`cpp/`](cpp/) |
| C# | Planned | [`csharp/`](csharp/) |
| Java | Planned | [`java/`](java/) |
| JavaScript | Planned | [`js/`](js/) |
| Python | Planned | [`python/`](python/) |
| Rust | Planned | [`rust/`](rust/) |

Go is the only implementation currently available. See [`go/README.md`](go/README.md) for installation, usage, supported types, and parsing behavior. Planned language folders contain only a status README.

*Every* implementation is maintained in its own folder and repository history. Each implementation has its own *README.md* file, containing installation, usage, examples, and parsing behavior details.

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

The `roadmap/` folder contains shared planning documents.

## License

MIT. See [`LICENSE`](LICENSE).
