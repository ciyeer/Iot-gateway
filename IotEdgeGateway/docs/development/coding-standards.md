# Coding Standards (IotEdgeGateway)

## C++ Standard
- C++17
- Use `std::filesystem` for paths, `std::optional` for optional values, and `std::string_view` for non-owning string parameters.

## Naming
- Namespace: `iotgw::...`
- Types: `PascalCase` (`ConfigManager`)
- Functions: `PascalCase` or `camelCase` — follow existing file style
- Variables: `snake_case` for locals and members with trailing underscore (`level_`, `mu_`)

## Includes
- Prefer project headers first, then standard headers.
- Keep includes minimal; include what you use.

## Error Handling
- Prefer returning `bool` / `std::optional<T>` for recoverable failures.
- Avoid throwing across module boundaries unless a clear policy exists.

## Threading
- Guard shared state with `std::mutex` and RAII locks (`std::lock_guard`).
- Keep critical sections small.

## Logging
- Use the existing logger interface (`core/common/logger/logger.hpp`).
- Avoid logging secrets (passwords/tokens).

## Formatting
- Keep functions small and cohesive.
- Prefer early-return to reduce nesting.
