# cine roadmap

## Principles

- Keep behavior predictable across language implementations.
- Define parsing, type conversion, and error behavior before porting the API.
- Share conformance cases so each implementation follows the same contract.
- Mark a language as supported only when it has usable code, tests, and documentation.

## Phases

### 1. Consolidate the Go implementation

- [x] Keep the Go module, implementation, tests, and fixtures together in `go/`.
- [x] Document the current Go API and conversion behavior.
- [ ] Specify the language-independent contract and document unavoidable differences.
- [ ] Add reusable fixtures and conformance cases.

### 2. Prepare contributions

- [ ] Document naming, null-value, and error conventions.
- [ ] Define contribution guidelines and compatibility policy.
- [ ] Add CI for each implementation as it becomes available.

### 3. Add language implementations

- [ ] Choose the next language based on demand and maintenance capacity.
- [ ] Implement and test one language at a time.
- [ ] Update the support table in the root README when an implementation is usable.

## Support criteria

A language is implemented when its folder contains usable code, automated tests, installation and usage instructions, and behavior aligned with the documented contract.
