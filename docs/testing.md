# Testing and verification

Automated tests are in `tests/test_warehouse.cpp` and run with CTest. They cover:

- Product add, duplicate rejection, hash lookup, missing lookup, stock increase/decrease, and deletion
- BST alphabetical traversal and ascending/descending merge-sort output
- Supplier duplicate and deletion behavior
- FIFO queue order and empty dequeue
- Heap priority ordering and empty pop
- Stack push, peek, pop, and empty behavior
- Service validation, stock undo, priority-first processing, and restart persistence

Run the full suite:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For C/C++ memory and undefined-behaviour checks, enable `WAREX_ENABLE_SANITIZERS=ON` as documented in the README. The build uses `-Wall -Wextra -Wpedantic`; warnings are fixed rather than suppressed.

## Manual acceptance walkthrough

1. Start the app, open the browser UI, and load demo data.
2. Confirm dashboard product count, total inventory value, and two low-stock alerts.
3. Add a normal order and a priority-5 order. Process next order and confirm priority-5 is processed first and stock falls by its quantity.
4. Make a stock adjustment, use Undo stock action, and confirm the previous quantity returns.
5. Restart the server and verify suppliers, products, quantities, and pending orders persist.
6. Try duplicate IDs, negative stock, empty fields, insufficient stock, invalid priorities, and deletion of an assigned supplier; each must return a readable error without changing data.
