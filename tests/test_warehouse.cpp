#include "warehouse_ds.h"
#include "warehouse_service.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

int failures = 0;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": "      \
                      << #condition << "\n";                                 \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

DSProduct product(
    const char* id,
    const char* name,
    int quantity = 10
) {
    DSProduct p{};

    std::snprintf(p.id, sizeof p.id, "%s", id);
    std::snprintf(p.sku, sizeof p.sku, "SKU-%s", id);
    std::snprintf(p.name, sizeof p.name, "%s", name);
    std::snprintf(p.category, sizeof p.category, "General");

    p.quantity = quantity;
    p.reorder_level = 5;
    p.price = 3.5;

    return p;
}

DSSupplier supplier(const char* id) {
    DSSupplier s{};

    std::snprintf(s.id, sizeof s.id, "%s", id);
    std::snprintf(s.name, sizeof s.name, "Supplier %s", id);
    std::snprintf(s.contact, sizeof s.contact, "Contact");
    std::snprintf(s.email, sizeof s.email, "a@example.test");

    return s;
}

DSOrder order(
    const char* id,
    const char* product_id,
    int qty,
    int priority
) {
    DSOrder o{};

    std::snprintf(o.id, sizeof o.id, "%s", id);
    std::snprintf(o.product_id, sizeof o.product_id, "%s", product_id);
    std::snprintf(o.customer, sizeof o.customer, "Customer");

    o.quantity = qty;
    o.priority = priority;

    return o;
}

void data_structure_tests() {
    DSWarehouse* w = ds_warehouse_create();

    CHECK(w != nullptr);
    CHECK(ds_product_count(w) == 0);
    CHECK(!ds_remove_product(w, "none"));

    auto a = product("A", "Alpha", 8);
    auto b = product("B", "Beta", 3);
    auto c = product("C", "Gamma", 12);

    CHECK(ds_add_product(w, &a));
    CHECK(ds_add_product(w, &b));
    CHECK(ds_add_product(w, &c));

    // Hash-table duplicate protection.
    CHECK(!ds_add_product(w, &a));

    DSProduct found{};

    // Hash-table lookup.
    CHECK(
        ds_find_product(w, "B", &found) &&
        found.quantity == 3
    );

    // BST name lookup.
    CHECK(
        ds_find_product_by_name(w, "Beta", &found) &&
        std::string(found.id) == "B"
    );

    CHECK(
        !ds_find_product_by_name(
            w,
            "Missing",
            &found
        )
    );

    CHECK(
        !ds_find_product(
            w,
            "missing",
            &found
        )
    );

    // Stock cannot become negative.
    CHECK(!ds_adjust_stock(w, "B", -4));

    CHECK(ds_adjust_stock(w, "B", 2));

    CHECK(
        ds_find_product(w, "B", &found) &&
        found.quantity == 5
    );

    // BST alphabetical traversal.
    std::size_t count = 0;

    auto ordered =
        ds_collect_products_name_order(
            w,
            &count
        );

    CHECK(count == 3);
    CHECK(
        ordered != nullptr &&
        std::string(ordered[0].name) == "Alpha"
    );

    ds_free_products(ordered);

    // Merge sort ascending.
    auto sortable =
        ds_collect_products(
            w,
            &count
        );

    CHECK(sortable != nullptr);

    ds_sort_products_by_quantity(
        sortable,
        count,
        1
    );

    CHECK(sortable[0].quantity == 5);

    // Merge sort descending.
    ds_sort_products_by_quantity(
        sortable,
        count,
        0
    );

    CHECK(sortable[0].quantity == 12);

    ds_free_products(sortable);

    // Supplier linked-list behavior.
    auto s = supplier("S1");

    CHECK(ds_add_supplier(w, &s));
    CHECK(!ds_add_supplier(w, &s));
    CHECK(ds_remove_supplier(w, "S1"));
    CHECK(!ds_remove_supplier(w, "S1"));

    // Normal queue FIFO behavior.
    auto n1 = order("N1", "A", 1, 1);
    auto n2 = order("N2", "A", 1, 1);

    CHECK(ds_enqueue_order(w, &n1));
    CHECK(ds_enqueue_order(w, &n2));

    DSOrder out{};

    CHECK(
        ds_dequeue_order(w, &out) &&
        std::string(out.id) == "N1"
    );

    CHECK(
        ds_dequeue_order(w, &out) &&
        std::string(out.id) == "N2"
    );

    CHECK(!ds_dequeue_order(w, &out));

    // Priority queue / heap behavior.
    auto p2 = order("P2", "A", 1, 2);
    auto p5 = order("P5", "A", 1, 5);
    auto p3 = order("P3", "A", 1, 3);

    CHECK(ds_enqueue_priority_order(w, &p2));
    CHECK(ds_enqueue_priority_order(w, &p5));
    CHECK(ds_enqueue_priority_order(w, &p3));

    CHECK(
        ds_pop_priority_order(w, &out) &&
        std::string(out.id) == "P5"
    );

    CHECK(
        ds_pop_priority_order(w, &out) &&
        std::string(out.id) == "P3"
    );

    CHECK(
        ds_pop_priority_order(w, &out) &&
        std::string(out.id) == "P2"
    );

    CHECK(!ds_pop_priority_order(w, &out));

    // Stack behavior.
    DSAction act{};

    std::snprintf(
        act.description,
        sizeof act.description,
        "received"
    );

    CHECK(ds_push_action(w, &act));
    CHECK(ds_peek_action(w, &act));
    CHECK(ds_pop_action(w, &act));
    CHECK(!ds_pop_action(w, &act));

    // Product removal.
    CHECK(ds_remove_product(w, "B"));
    CHECK(!ds_find_product(w, "B", nullptr));

    ds_warehouse_destroy(w);
}

void service_tests() {
    const auto path =
        (
            std::filesystem::temp_directory_path() /
            "warex_test_data.wrx"
        ).string();

    std::filesystem::remove(path);

    {
        warex::WarehouseService service(path);

        // Supplier creation.
        auto s = supplier("S1");

        auto add_supplier_result =
            service.add_supplier(s);

        CHECK(add_supplier_result.ok);
        CHECK(
            add_supplier_result.message ==
            "Supplier added successfully."
        );

        // Duplicate supplier should fail with a meaningful message.
        auto duplicate_supplier =
            service.add_supplier(s);

        CHECK(!duplicate_supplier.ok);
        CHECK(
            duplicate_supplier.message.find(
                "Supplier could not be added"
            ) != std::string::npos
        );

        // Supplier deletion.
        auto removable =
            supplier("S2");

        CHECK(service.add_supplier(removable).ok);
        CHECK(service.delete_supplier("S2").ok);

        // Supplier update.
        std::snprintf(
            s.name,
            sizeof s.name,
            "Updated Supplier"
        );

        auto updated_supplier =
            service.update_supplier(s);

        CHECK(updated_supplier.ok);
        CHECK(
            updated_supplier.message ==
            "Supplier updated successfully."
        );

        // Product creation.
        auto p =
            product("P1", "Paper", 12);

        std::snprintf(
            p.supplier_id,
            sizeof p.supplier_id,
            "S1"
        );

        auto add_product_result =
            service.add_product(p);

        CHECK(add_product_result.ok);
        CHECK(
            add_product_result.message ==
            "Product added successfully."
        );

        // Product duplicate should fail.
        auto duplicate_product =
            service.add_product(p);

        CHECK(!duplicate_product.ok);
        CHECK(
            duplicate_product.message.find(
                "Product could not be added"
            ) != std::string::npos
        );

        // Supplier assigned to a product cannot be removed.
        CHECK(!service.delete_supplier("S1").ok);

        // Product update.
        p.quantity = 13;

        auto update_product_result =
            service.update_product(p);

        CHECK(update_product_result.ok);
        CHECK(
            update_product_result.message ==
            "Product updated successfully."
        );

        // BST-backed name search through service.
        auto found =
            service.find_product_by_name("Paper");

        CHECK(found.has_value());
        CHECK(
            found.has_value() &&
            std::string(found->id) == "P1"
        );

        // Stock cannot become negative.
        CHECK(
            !service.adjust_stock("P1", -14).ok
        );

        // Stock adjustment.
        CHECK(
            service.adjust_stock("P1", 4).ok
        );

        auto after_adjustment =
            service.find_product_by_name("Paper");

        CHECK(after_adjustment.has_value());
        CHECK(
            after_adjustment.has_value() &&
            after_adjustment->quantity == 17
        );

        // Undo stock adjustment.
        CHECK(
            service.undo_last_action().ok
        );

        auto after_undo =
            service.find_product_by_name("Paper");

        CHECK(after_undo.has_value());
        CHECK(
            after_undo.has_value() &&
            after_undo->quantity == 13
        );

        // There should now be no remaining stock action to undo.
        CHECK(
            !service.undo_last_action().ok
        );

        // Normal order.
        CHECK(
            service.add_order(
                order("O1", "P1", 3, 1)
            ).ok
        );

        // Duplicate order ID must fail.
        auto duplicate_order =
            service.add_order(
                order("O1", "P1", 3, 1)
            );

        CHECK(!duplicate_order.ok);

        // Priority order.
        CHECK(
            service.add_order(
                order("O2", "P1", 2, 5)
            ).ok
        );

        /*
         * Priority order must be processed before
         * the normal FIFO queue.
         *
         * Starting stock = 13.
         * Priority order O2 consumes 2.
         * Remaining stock = 11.
         */
        CHECK(service.process_next_order().ok);

        auto after_priority =
            service.find_product_by_name("Paper");

        CHECK(after_priority.has_value());
        CHECK(
            after_priority.has_value() &&
            after_priority->quantity == 11
        );

        // Normal order remains pending.
        CHECK(
            service.normal_orders().size() == 1
        );

        // Process normal order.
        CHECK(service.process_next_order().ok);

        auto after_normal =
            service.find_product_by_name("Paper");

        CHECK(after_normal.has_value());
        CHECK(
            after_normal.has_value() &&
            after_normal->quantity == 8
        );

        // No orders should remain.
        CHECK(
            service.normal_orders().empty()
        );

        CHECK(
            service.priority_orders().empty()
        );

        // No more orders to process.
        CHECK(
            !service.process_next_order().ok
        );
    }

    // Verify persistence.
    {
        warex::WarehouseService restored(path);

        CHECK(restored.load().ok);

        CHECK(
            restored.products().size() == 1
        );

        CHECK(
            restored.snapshot().suppliers == 1
        );

        CHECK(
            restored.normal_orders().empty()
        );

        CHECK(
            restored.priority_orders().empty()
        );

        auto restored_product =
            restored.find_product_by_name("Paper");

        CHECK(restored_product.has_value());

        CHECK(
            restored_product.has_value() &&
            restored_product->quantity == 8
        );
    }

    std::filesystem::remove(path);
}

} // namespace

int main() {
    data_structure_tests();
    service_tests();

    if (failures) {
        std::cerr
            << failures
            << " test assertion(s) failed.\n";

        return EXIT_FAILURE;
    }

    std::cout
        << "All WAREX tests passed.\n";

    return EXIT_SUCCESS;
}
