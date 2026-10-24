## Cleanup

- **Todo**: Replace pointer-difference lengths (`long` / `ptrdiff_t`) assigned to `int` in parsers (`advanced_tables.c`, `callouts.c`, `citations.c`, `critic.c`, `emoji.c`, `ial.c`, etc.) with a two-step, range-checked pattern (e.g. compute to `ptrdiff_t`, validate against `INT_MAX`, then cast to `int`) or refactor to use `size_t` consistently.
- **Todo**: Gradually standardize length/index types across parsing code to avoid implicit narrowing conversions and make warnings-free builds the default in Xcode and Swift package targets.

