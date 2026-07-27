# lhcui

> **L**inux **H**andheld **C**onsole **UI** Toolkit

A modern C++ UI toolkit for handheld and embedded devices, featuring a lightweight view system, stack‑based navigation, and a clean, extensible design.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Building the Project](#building-the-project)
- [Running the Example](#running-the-example)
- [Tests](#tests)
- [Documentation](#documentation)
- [License](#license)

---

## Overview

`lhcui` provides a minimal yet powerful set of UI primitives to build applications for constrained platforms. The core concepts include:

- **View** – abstract base class for UI components.
- **ViewStack** – a stack‑based navigation system that manages active views.
- **Asset handling** – easy integration of textures, fonts, and shaders.
- **Cross‑platform rendering** – supports SDL1, SDL2, and can be extended to other back‑ends.

The implementation follows the design outlined in [`DESIGN.md`](DESIGN.md) and the development roadmap in [`TODO.md`](TODO.md).

---

## Features

- **Lightweight** – pure C++14 with no heavy external dependencies.
- **Modular** – header‑only public API, internal implementation in `src/`.
- **Stack navigation** – push/pop view semantics for easy screen management.
- **Extensible rendering** – plug‑in renderers (SDL1, SDL2) under `src/render/`.
- **Comprehensive tests** – unit tests in `tests/` using GoogleTest.

---

## Prerequisites

- **CMake** ≥ 3.15
- **C++ compiler** with C++14 support (e.g., clang++, g++)
- **SDL2** (optional, needed for the example applications)
- **Git** (for cloning the repository)

---

## Building the Project

```bash
# Clone the repository (if you haven't already)
git clone https://github.com/psiroki/lhcui.git
cd lhcui

# Create a build directory
mkdir -p build && cd build

# Configure with CMake
cmake ..

# Build the library and the example
make -j$(nproc)
```

The static library `liblhcui.a` will be generated in `build/`. The example executable can be found at `build/example/main`.

---

## Running the Example

```bash
# From the build directory
./hui_example
```

The example demonstrates a simple view stack with two screens and navigation controls.

---

## Tests

The test suite uses GoogleTest. To run the tests:

```bash
# Assuming you are still in the build directory
ctest --output-on-failure
```

---

## Documentation

- **Design Document** – detailed architecture in [`DESIGN.md`](DESIGN.md).
- **TODO List** – open tasks and roadmap in [`TODO.md`](TODO.md).
- **Header Documentation** – each public header contains Doxygen‑compatible comments.

---

## License

`lhcui` is released under the MIT License – see the [LICENSE](LICENSE) file for details.
