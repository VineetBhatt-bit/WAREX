# WAREX

WAREX is a warehouse inventory and order-management system built for a Data Structures and C++ OOP academic project. It is a local, dependency-free application: a C data-structure engine powers a C++ service/API layer and a responsive browser interface.

## Features

- Products, suppliers and inventory adjustments with server-side validation
- Normal FIFO order queue and urgent-order max heap (priority 2–5)
- Low-stock dashboard and quantity-ranked inventory report
- Product ID hash lookup, alphabetical BST traversal, merge-sort reports, action stack/undo
- Safe local persistence of products, suppliers, inventory and pending orders
- Browser UI that consumes the real C++ backend API—no mock data is used

## Quick start

Requirements: CMake 3.20+ and a C11/C++20 compiler (Apple Clang, Clang, or GCC).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/warex
```

Open `http://127.0.0.1:8080` in a browser. Pass a port as the first argument, for example `./build/warex 8081`.

On a new warehouse, use **Load demo data** on the dashboard to create realistic sample records. Runtime data is written to `data/warehouse.wrx`; this local file is intentionally ignored by Git.

## Project layout

```text
include/                 C interface and C++ service interface
src/c/                   C data structures and algorithms
src/cpp/                 C++ business layer and local HTTP server
frontend/                Single-page browser UI
tests/                   Automated data-structure, service, and persistence tests
docs/                    Architecture, algorithms, testing, and viva notes
data/                    Runtime persistence directory (not committed)
```

## Architecture and academic mapping

| Requirement | Actual WAREX role |
| --- | --- |
| C linked lists | Dynamic product and supplier records |
| Hash table | Average O(1) product-ID lookup |
| Binary search tree | Alphabetical product catalogue traversal |
| Stack | Recent stock actions and one-step stock undo |
| Queue | O(1) normal order FIFO processing |
| Binary heap | O(log n) urgent-order scheduling |
| Merge sort | O(n log n) quantity reports |
| C++ OOP | `WarehouseService` owns orchestration, validation, persistence, and API-facing operations |
| File handling | Atomic temp-file then rename persistence |

See [architecture.md](docs/architecture.md), [testing.md](docs/testing.md), and [viva-guide.md](docs/viva-guide.md) for the implementation details and complexity discussion.

## Developer checks

```sh
# Debug build and automated tests
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# Memory/undefined-behaviour checks (Clang/GCC)
cmake -S . -B build-sanitized -DWAREX_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitized --parallel
ctest --test-dir build-sanitized --output-on-failure
```

## Limitations

WAREX is intentionally a single-user local application. It binds only to loopback (`127.0.0.1`), has no authentication, and is not intended as a network-deployed multi-user service. The BST is intentionally unbalanced for transparent academic demonstration, so its worst case is O(n).

## License

MIT — see [LICENSE](LICENSE).
