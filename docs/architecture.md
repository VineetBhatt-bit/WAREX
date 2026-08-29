# WAREX architecture

## Data flow

```text
Browser UI (frontend/index.html)
          ↓ HTTP form requests
Minimal local HTTP server (C++)
          ↓
WarehouseService (C++): validation, workflow, persistence
          ↓ extern "C" interface
C warehouse data structures: product/supplier lists, indexes, queues, heap, stack
          ↓
data/warehouse.wrx (atomic local persistence)
```

The server exposes only local loopback HTTP. The frontend never edits storage directly; every write reaches `WarehouseService`, which repeats validation and persists a successful change.

## Module boundaries

- `src/c/warehouse_ds.c` owns all manually allocated C nodes, the hash index, BST nodes, normal-order nodes, heap buffer, and action nodes. Its public header explicitly exposes matching `ds_free_*` functions for collected arrays.
- `src/cpp/warehouse_service.cpp` owns the `DSWarehouse*` lifetime through constructor/destructor, applies warehouse rules, and performs the file format conversion. It does not replicate C data structures.
- `src/cpp/http_server.cpp` converts HTTP forms into typed records and serializes response JSON. It contains no inventory decisions.
- `frontend/index.html` renders states returned from API calls. It has loading/error feedback, empty states, valid input constraints, and real navigation.

## Persistence

The line-based `WAREX-1` format uses `std::quoted` fields, so spaces and quotes in textual values survive save/load. Saving writes `warehouse.wrx.tmp`, closes it successfully, then renames it into place. Loading rejects malformed individual records while retaining valid records and reports that records were skipped. Missing data starts a clean warehouse.

## Complexity

| Operation | Structure | Complexity |
| --- | --- | --- |
| Find product by ID | chained hash table | O(1) average, O(n) worst case |
| Add linked-list product/supplier | head insertion | O(1), plus product index rebuild currently O(n) |
| Normal order enqueue/dequeue | head/tail queue | O(1) |
| Priority insert/remove | binary max heap | O(log n) |
| Alphabetical display | BST in-order traversal | O(n) |
| BST search/order | unbalanced BST | O(log n) average, O(n) worst case |
| Quantity report | merge sort | O(n log n) |
| Low-stock scan | product list | O(n) |

The product index is deliberately rebuilt after a product add/remove/name update because BST nodes hold product-node pointers and records can change their sort key. This is an honest trade-off for a small academic warehouse; ID lookup and the operational order queues keep their intended complexities.

## Validation and edge cases

The backend rejects duplicate/empty IDs, missing names, negative product quantities/prices/reorder levels, zero or negative order quantities, invalid priorities, absent products/suppliers, insufficient stock, deleting suppliers assigned to products, and deleting products referenced by queued orders. The C layer additionally guards empty dequeues/pops and allocations. A priority order is always processed before a normal order; a processing-time stock failure removes that stale order and explains the outcome rather than allowing negative inventory.
