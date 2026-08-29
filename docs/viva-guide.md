# Viva guide

**Why C and C++ together?** C implements the required low-level data structures with explicit memory ownership. C++ adds encapsulated service responsibilities, RAII destruction, string/filesystem handling, and an application boundary. `warehouse_ds.h` wraps C declarations in `extern "C"` when consumed by C++ to prevent name-mangling mismatch.

**Why a hash table for products?** Inventory and orders commonly identify a product by ID. The hash table provides expected O(1) lookup; collisions use separate chaining with linked hash nodes.

**Why both a linked list and BST?** The linked list is the dynamic source of product records. The BST indexes those records for alphabetical in-order catalogue traversal without duplicating them. It is unbalanced, so worst-case search is O(n).

**How are orders processed?** Normal orders enter a head/tail FIFO queue. Priority 2–5 orders enter a binary max heap. The service always pops a priority order first; it decrements inventory only after validating the stock remains non-negative.

**How does persistence work?** The C++ service serializes valid records to a temporary file and then renames it. On launch it validates and loads records; absent data is a valid empty start and malformed records are skipped safely.

**How is the frontend integrated?** Browser forms send URL-encoded requests to the local C++ HTTP server. The server calls `WarehouseService`; the UI refreshes tables/cards using API responses. No browser-side data is authoritative.
