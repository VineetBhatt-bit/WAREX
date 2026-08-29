#include "warehouse_service.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace warex {

namespace {

constexpr const char* kHeader = "WAREX-1";

template <typename T, typename Free>
std::vector<T> copy_array(T* values, std::size_t count, Free free_fn) {
    std::vector<T> result;
    if (values) {
        result.assign(values, values + count);
        free_fn(values);
    }
    return result;
}

void write_product(std::ostream& out, const DSProduct& p) {
    out << "P "
        << std::quoted(p.id) << ' '
        << std::quoted(p.sku) << ' '
        << std::quoted(p.name) << ' '
        << std::quoted(p.supplier_id) << ' '
        << std::quoted(p.category) << ' '
        << p.quantity << ' '
        << p.reorder_level << ' '
        << std::setprecision(17) << p.price << '\n';
}

void write_supplier(std::ostream& out, const DSSupplier& s) {
    out << "S "
        << std::quoted(s.id) << ' '
        << std::quoted(s.name) << ' '
        << std::quoted(s.contact) << ' '
        << std::quoted(s.email) << '\n';
}

void write_order(std::ostream& out, char type, const DSOrder& o) {
    out << type << ' '
        << std::quoted(o.id) << ' '
        << std::quoted(o.product_id) << ' '
        << std::quoted(o.customer) << ' '
        << o.quantity << ' '
        << o.priority << '\n';
}

} // namespace

WarehouseService::WarehouseService(std::string data_file)
    : data_(ds_warehouse_create()),
      data_file_(std::move(data_file)) {}

WarehouseService::~WarehouseService() {
    ds_warehouse_destroy(data_);
}

Result WarehouseService::load() {
    std::ifstream input(data_file_);

    if (!input) {
        return {
            true,
            "No saved data found; starting with an empty warehouse."
        };
    }

    std::string header;

    if (!std::getline(input, header) || header != kHeader) {
        return {
            false,
            "Saved data has an unsupported format; no data was loaded."
        };
    }

    ds_warehouse_clear(data_);

    std::string line;
    std::size_t rejected = 0;

    while (std::getline(input, line)) {
        std::istringstream in(line);
        char type = 0;
        in >> type;

        if (type == 'P') {
            DSProduct p{};
            std::string id;
            std::string sku;
            std::string name;
            std::string supplier;
            std::string category;

            if (in >> std::quoted(id)
                    >> std::quoted(sku)
                    >> std::quoted(name)
                    >> std::quoted(supplier)
                    >> std::quoted(category)
                    >> p.quantity
                    >> p.reorder_level
                    >> p.price
                &&
                id.size() < sizeof p.id &&
                sku.size() < sizeof p.sku &&
                name.size() < sizeof p.name &&
                supplier.size() < sizeof p.supplier_id &&
                category.size() < sizeof p.category) {

                std::snprintf(p.id, sizeof p.id, "%s", id.c_str());
                std::snprintf(p.sku, sizeof p.sku, "%s", sku.c_str());
                std::snprintf(p.name, sizeof p.name, "%s", name.c_str());
                std::snprintf(
                    p.supplier_id,
                    sizeof p.supplier_id,
                    "%s",
                    supplier.c_str()
                );
                std::snprintf(
                    p.category,
                    sizeof p.category,
                    "%s",
                    category.c_str()
                );

                if (!ds_add_product(data_, &p)) {
                    ++rejected;
                }
            } else {
                ++rejected;
            }

        } else if (type == 'S') {
            DSSupplier s{};
            std::string id;
            std::string name;
            std::string contact;
            std::string email;

            if (in >> std::quoted(id)
                    >> std::quoted(name)
                    >> std::quoted(contact)
                    >> std::quoted(email)
                &&
                id.size() < sizeof s.id &&
                name.size() < sizeof s.name &&
                contact.size() < sizeof s.contact &&
                email.size() < sizeof s.email) {

                std::snprintf(s.id, sizeof s.id, "%s", id.c_str());
                std::snprintf(s.name, sizeof s.name, "%s", name.c_str());
                std::snprintf(
                    s.contact,
                    sizeof s.contact,
                    "%s",
                    contact.c_str()
                );
                std::snprintf(
                    s.email,
                    sizeof s.email,
                    "%s",
                    email.c_str()
                );

                if (!ds_add_supplier(data_, &s)) {
                    ++rejected;
                }
            } else {
                ++rejected;
            }

        } else if (type == 'N' || type == 'Q') {
            DSOrder o{};
            std::string id;
            std::string product;
            std::string customer;

            if (in >> std::quoted(id)
                    >> std::quoted(product)
                    >> std::quoted(customer)
                    >> o.quantity
                    >> o.priority
                &&
                id.size() < sizeof o.id &&
                product.size() < sizeof o.product_id &&
                customer.size() < sizeof o.customer) {

                std::snprintf(o.id, sizeof o.id, "%s", id.c_str());
                std::snprintf(
                    o.product_id,
                    sizeof o.product_id,
                    "%s",
                    product.c_str()
                );
                std::snprintf(
                    o.customer,
                    sizeof o.customer,
                    "%s",
                    customer.c_str()
                );

                const bool accepted =
                    type == 'N'
                        ? ds_enqueue_order(data_, &o)
                        : ds_enqueue_priority_order(data_, &o);

                if (!accepted) {
                    ++rejected;
                }
            } else {
                ++rejected;
            }

        } else if (!line.empty()) {
            ++rejected;
        }
    }

    return {
        true,
        rejected
            ? "Data loaded with " + std::to_string(rejected) +
                  " invalid record(s) skipped."
            : "Saved warehouse data loaded."
    };
}

Result WarehouseService::save() const {
    const auto path = std::filesystem::path(data_file_);
    std::error_code ec;

    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    if (ec) {
        return {
            false,
            "Could not create the data directory."
        };
    }

    const auto temporary = path.string() + ".tmp";

    std::ofstream out(temporary, std::ios::trunc);

    if (!out) {
        return {
            false,
            "Could not open the data file for saving."
        };
    }

    out << kHeader << '\n';

    for (const auto& s : suppliers()) {
        write_supplier(out, s);
    }

    for (const auto& p : products(false)) {
        write_product(out, p);
    }

    for (const auto& o : normal_orders()) {
        write_order(out, 'N', o);
    }

    for (const auto& o : priority_orders()) {
        write_order(out, 'Q', o);
    }

    out.close();

    if (!out) {
        return {
            false,
            "Could not finish writing the data file."
        };
    }

    std::filesystem::rename(temporary, path, ec);

    if (ec) {
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(temporary, path, ec);
    }

    if (ec) {
        return {
            false,
            "Could not replace the saved data file."
        };
    }

    return {
        true,
        "Saved."
    };
}

Result WarehouseService::persist_after(const Result& result) const {
    if (!result.ok) {
        return result;
    }

    const auto saved = save();

    if (!saved.ok) {
        return saved;
    }

    return result;
}

void WarehouseService::record_stock_action(
    const std::string& description,
    const std::string& product_id,
    int delta
) {
    DSAction action{};

    std::snprintf(
        action.description,
        sizeof action.description,
        "%s",
        description.c_str()
    );

    std::snprintf(
        action.product_id,
        sizeof action.product_id,
        "%s",
        product_id.c_str()
    );

    action.stock_delta = delta;
    action.undoable = 1;

    ds_push_action(data_, &action);
}

Result WarehouseService::seed_demo_data() {
    if (ds_product_count(data_) || ds_supplier_count(data_)) {
        return {
            false,
            "Demo data can only be loaded into an empty warehouse."
        };
    }

    DSSupplier a{};
    std::snprintf(a.id, sizeof a.id, "SUP-001");
    std::snprintf(a.name, sizeof a.name, "Northstar Supply Co.");
    std::snprintf(a.contact, sizeof a.contact, "Aarav Mehta");
    std::snprintf(
        a.email,
        sizeof a.email,
        "orders@northstar.example"
    );

    DSSupplier b{};
    std::snprintf(b.id, sizeof b.id, "SUP-002");
    std::snprintf(b.name, sizeof b.name, "Metro Industrial");
    std::snprintf(b.contact, sizeof b.contact, "Priya Shah");
    std::snprintf(
        b.email,
        sizeof b.email,
        "sales@metro.example"
    );

    if (!ds_add_supplier(data_, &a) ||
        !ds_add_supplier(data_, &b)) {
        return {
            false,
            "Demo supplier data could not be created."
        };
    }

    const struct {
        const char* id;
        const char* sku;
        const char* name;
        const char* supplier;
        const char* category;
        int qty;
        int reorder;
        double price;
    } demo[] = {
        {
            "PRD-001",
            "BX-100",
            "Corrugated Boxes",
            "SUP-001",
            "Packaging",
            42,
            20,
            18.50
        },
        {
            "PRD-002",
            "TP-220",
            "Thermal Labels",
            "SUP-001",
            "Packaging",
            12,
            20,
            5.75
        },
        {
            "PRD-003",
            "GL-310",
            "Safety Gloves",
            "SUP-002",
            "Safety",
            86,
            25,
            3.40
        },
        {
            "PRD-004",
            "TK-410",
            "Packing Tape",
            "SUP-001",
            "Packaging",
            9,
            15,
            2.95
        }
    };

    for (const auto& d : demo) {
        DSProduct p{};

        std::snprintf(p.id, sizeof p.id, "%s", d.id);
        std::snprintf(p.sku, sizeof p.sku, "%s", d.sku);
        std::snprintf(p.name, sizeof p.name, "%s", d.name);
        std::snprintf(
            p.supplier_id,
            sizeof p.supplier_id,
            "%s",
            d.supplier
        );
        std::snprintf(
            p.category,
            sizeof p.category,
            "%s",
            d.category
        );

        p.quantity = d.qty;
        p.reorder_level = d.reorder;
        p.price = d.price;

        if (!ds_add_product(data_, &p)) {
            return {
                false,
                "Demo product data could not be created."
            };
        }
    }

    return persist_after({
        true,
        "Demo warehouse data loaded."
    });
}

Snapshot WarehouseService::snapshot() const {
    Snapshot s{
        ds_product_count(data_),
        ds_supplier_count(data_),
        ds_normal_order_count(data_),
        ds_priority_order_count(data_),
        0,
        0
    };

    for (const auto& p : products(false)) {
        s.inventory_value += p.quantity * p.price;

        if (p.quantity <= p.reorder_level) {
            ++s.low_stock;
        }
    }

    return s;
}

Result WarehouseService::add_product(const DSProduct& p) {
    if (p.supplier_id[0] &&
        !ds_find_supplier(data_, p.supplier_id, nullptr)) {
        return {
            false,
            "Choose an existing supplier."
        };
    }

    if (!ds_add_product(data_, &p)) {
        return {
            false,
            "Product could not be added. Check that the ID is unique and all values are valid."
        };
    }

    return persist_after({
        true,
        "Product added successfully."
    });
}

Result WarehouseService::update_product(const DSProduct& p) {
    if (p.supplier_id[0] &&
        !ds_find_supplier(data_, p.supplier_id, nullptr)) {
        return {
            false,
            "Choose an existing supplier."
        };
    }

    if (!ds_update_product(data_, &p)) {
        return {
            false,
            "Product could not be updated. Check that the product exists and all values are valid."
        };
    }

    return persist_after({
        true,
        "Product updated successfully."
    });
}

Result WarehouseService::delete_product(const std::string& id) {
    for (const auto& o : normal_orders()) {
        if (id == o.product_id) {
            return {
                false,
                "A queued order still references this product."
            };
        }
    }

    for (const auto& o : priority_orders()) {
        if (id == o.product_id) {
            return {
                false,
                "A queued order still references this product."
            };
        }
    }

    if (!ds_remove_product(data_, id.c_str())) {
        return {
            false,
            "Product could not be deleted because it was not found."
        };
    }

    return persist_after({
        true,
        "Product deleted successfully."
    });
}

Result WarehouseService::adjust_stock(
    const std::string& id,
    int delta
) {
    if (delta == 0) {
        return {
            false,
            "Stock adjustment cannot be zero."
        };
    }

    if (!ds_adjust_stock(data_, id.c_str(), delta)) {
        return {
            false,
            "Stock would become negative or the product was not found."
        };
    }

    record_stock_action(
        "Stock adjusted by " + std::to_string(delta),
        id,
        delta
    );

    return persist_after({
        true,
        "Inventory updated successfully."
    });
}

Result WarehouseService::add_supplier(const DSSupplier& s) {
    if (!ds_add_supplier(data_, &s)) {
        return {
            false,
            "Supplier could not be added. Check that the ID is unique and the details are valid."
        };
    }

    return persist_after({
        true,
        "Supplier added successfully."
    });
}

Result WarehouseService::update_supplier(const DSSupplier& s) {
    if (!ds_update_supplier(data_, &s)) {
        return {
            false,
            "Supplier could not be updated. Check that the supplier exists and the details are valid."
        };
    }

    return persist_after({
        true,
        "Supplier updated successfully."
    });
}

Result WarehouseService::delete_supplier(const std::string& id) {
    if (!ds_find_supplier(data_, id.c_str(), nullptr)) {
        return {
            false,
            "Supplier not found."
        };
    }

    if (!ds_remove_supplier(data_, id.c_str())) {
        return {
            false,
            "Supplier is still assigned to a product."
        };
    }

    return persist_after({
        true,
        "Supplier deleted successfully."
    });
}

Result WarehouseService::add_order(const DSOrder& o) {
    for (const auto& pending : normal_orders()) {
        if (std::strcmp(o.id, pending.id) == 0) {
            return {
                false,
                "Order ID already exists in the normal queue."
            };
        }
    }

    for (const auto& pending : priority_orders()) {
        if (std::strcmp(o.id, pending.id) == 0) {
            return {
                false,
                "Order ID already exists in the priority queue."
            };
        }
    }

    DSProduct product{};

    if (!ds_find_product(data_, o.product_id, &product)) {
        return {
            false,
            "The selected product does not exist."
        };
    }

    if (o.quantity > product.quantity) {
        return {
            false,
            "Insufficient current stock for this order."
        };
    }

    const bool added =
        o.priority == 1
            ? ds_enqueue_order(data_, &o)
            : ds_enqueue_priority_order(data_, &o);

    if (!added) {
        return {
            false,
            "Order could not be queued. Check the order details and priority."
        };
    }

    return persist_after({
        true,
        "Order queued successfully."
    });
}

Result WarehouseService::process_next_order() {
    /*
     * Inspect the next order before removing it from its
     * queue/heap. This prevents an order from disappearing
     * when there is insufficient stock.
     */
    const bool priority =
        ds_priority_order_count(data_) != 0;

    DSOrder next_order{};

    if (priority) {
        const auto pending = priority_orders();

        if (pending.empty()) {
            return {
                false,
                "There are no pending orders."
            };
        }

        /*
         * The priority queue is represented as a heap.
         * The first element is the highest-priority order.
         */
        next_order = pending.front();

    } else {
        const auto pending = normal_orders();

        if (pending.empty()) {
            return {
                false,
                "There are no pending orders."
            };
        }

        /*
         * The normal queue is FIFO, so the first
         * collected order is the next order to process.
         */
        next_order = pending.front();
    }

    DSProduct product{};

    if (!ds_find_product(
            data_,
            next_order.product_id,
            &product)) {
        return {
            false,
            "The order cannot be processed because its product no longer exists."
        };
    }

    if (next_order.quantity > product.quantity) {
        return {
            false,
            "The next order cannot be processed because there is insufficient stock."
        };
    }

    DSOrder order{};

    const bool removed =
        priority
            ? ds_pop_priority_order(data_, &order)
            : ds_dequeue_order(data_, &order);

    if (!removed) {
        return {
            false,
            "The next order could not be removed from its queue."
        };
    }

    if (!ds_adjust_stock(
            data_,
            order.product_id,
            -order.quantity)) {
        /*
         * This should be unreachable after the stock check
         * above, but keep the failure explicit rather than
         * silently reporting success.
         */
        return {
            false,
            "The order was removed but inventory could not be updated."
        };
    }

    record_stock_action(
        "Processed order " +
            std::string(order.id),
        order.product_id,
        -order.quantity
    );

    return persist_after({
        true,
        std::string(
            priority ? "Priority" : "Normal"
        ) +
        " order processed and inventory updated successfully."
    });
}

Result WarehouseService::undo_last_action() {
    /*
     * Peek first so a failed undo does not destroy the
     * most recent action from the stack.
     */
    DSAction action{};

    if (!ds_peek_action(data_, &action)) {
        return {
            false,
            "There is no recent action to undo."
        };
    }

    if (!action.undoable) {
        return {
            false,
            "The most recent action cannot be undone."
        };
    }

    if (!ds_adjust_stock(
            data_,
            action.product_id,
            -action.stock_delta)) {
        return {
            false,
            "The recent stock action can no longer be undone safely."
        };
    }

    DSAction removed{};

    if (!ds_pop_action(data_, &removed)) {
        /*
         * The stock has already been reversed, but this
         * situation should only occur if the stack changed
         * unexpectedly.
         */
        ds_adjust_stock(
            data_,
            action.product_id,
            action.stock_delta
        );

        return {
            false,
            "The undo action could not be completed safely."
        };
    }

    return persist_after({
        true,
        "Last stock action undone successfully."
    });
}

std::vector<DSProduct>
WarehouseService::products(bool alphabetical) const {
    std::size_t n = 0;

    if (alphabetical) {
        return copy_array(
            ds_collect_products_name_order(data_, &n),
            n,
            ds_free_products
        );
    }

    return copy_array(
        ds_collect_products(data_, &n),
        n,
        ds_free_products
    );
}

std::optional<DSProduct>
WarehouseService::find_product_by_name(
    const std::string& name
) const {
    DSProduct product{};

    if (ds_find_product_by_name(
            data_,
            name.c_str(),
            &product)) {
        return std::optional<DSProduct>(product);
    }

    return std::nullopt;
}

std::vector<DSSupplier>
WarehouseService::suppliers() const {
    std::size_t n = 0;

    return copy_array(
        ds_collect_suppliers(data_, &n),
        n,
        ds_free_suppliers
    );
}

std::vector<DSOrder>
WarehouseService::normal_orders() const {
    std::size_t n = 0;

    return copy_array(
        ds_collect_normal_orders(data_, &n),
        n,
        ds_free_orders
    );
}

std::vector<DSOrder>
WarehouseService::priority_orders() const {
    std::size_t n = 0;

    return copy_array(
        ds_collect_priority_orders(data_, &n),
        n,
        ds_free_orders
    );
}

std::vector<DSAction>
WarehouseService::actions() const {
    std::size_t n = 0;

    return copy_array(
        ds_collect_actions(data_, &n),
        n,
        ds_free_actions
    );
}

std::vector<DSProduct>
WarehouseService::low_stock() const {
    auto v = products(true);

    v.erase(
        std::remove_if(
            v.begin(),
            v.end(),
            [](const auto& p) {
                return p.quantity > p.reorder_level;
            }
        ),
        v.end()
    );

    return v;
}

std::vector<DSProduct>
WarehouseService::stock_report(bool ascending) const {
    auto v = products(false);

    if (!v.empty()) {
        ds_sort_products_by_quantity(
            v.data(),
            v.size(),
            ascending
        );
    }

    return v;
}

} // namespace warex
