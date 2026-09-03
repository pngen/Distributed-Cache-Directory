# Contributing to Distributed Cache Directory

Thank you for your interest in contributing to Distributed Cache Directory.

## Scope

Distributed Cache Directory is a vendor-neutral runtime that answers one systems
question: *who has what reusable state, where is it, at what generation, with what
integrity and freshness, and what will it cost to access?*

It is a **distributed location/replica directory** for reusable AI state. It is,
by design, not:

- a State Index (it does not answer what content matches a query), and
- a runtime registry, storage fabric, KV fabric, or tensor cache (it does not
  own state objects).

Please keep changes aligned with that single boundary.

## Build

A CMake + Ninja + MSVC toolchain is the reference environment. CUDA is optional.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDCD_BUILD_TESTS=ON
ninja -C build
ctest --test-dir build
```

## Behavioral expectations

- C++20, strong types, deterministic behavior.
- Zero project-source warnings under `/W4 /WX`.
- All mutations are generation-fenced and authority-scoped.
- No telemetry transmission; all observations remain local.

## Submitting changes

1. Make focused, self-contained commits with neutral, public-facing subjects.
2. Add or update tests for behavior you change.
3. Run the applicable tests in both Release and Debug.
4. Keep the README accurate.

By contributing, you agree that your contributions are licensed under the
Apache License 2.0.
