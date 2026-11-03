# Contributing

Thank you for your interest in contributing to this project! Here are some guidelines.

## Development Setup

1. Clone the repository
2. Ensure you have a C++17 compiler (Clang or GCC)
3. Run `make run-tests` to verify everything builds and passes

## Code Standards

- **C++17** standard
- All code must compile cleanly with `-Wall -Wextra -Werror`
- Tests must pass under `-fsanitize=address,undefined`
- Use `__int128_t` for distance arithmetic (GCC/Clang extension)
- No floating-point tolerance in distance comparisons

## Adding a New Algorithm

1. Create a header in `include/` with the implementation
2. Add a demo in `src/` that reads stdin and prints results
3. Add a test in `tests/` that verifies against the Floyd-Warshall oracle
4. Add a benchmark entry in `benchmarks/` with seeded graph families
5. Update the `Makefile` with the new targets
6. Document the algorithm, its correctness argument, and complexity in `docs/`

## Testing

Every solver must be tested against an independent oracle (Floyd-Warshall for small graphs). Negative-cycle witnesses must be checked for:

- **Connectivity**: consecutive edges share endpoints
- **Reachability**: cycle vertices are reachable from the source
- **Negativity**: the sum of original edge weights is strictly negative

## Commit Messages

Use clear, descriptive commit messages. Reference relevant algorithm names or issue numbers where applicable.
