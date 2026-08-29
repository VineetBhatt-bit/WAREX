#pragma once

#include "warehouse_ds.h"

#include <string>
#include <optional>
#include <vector>

namespace warex {

struct Result { bool ok; std::string message; };
struct Snapshot { std::size_t products, suppliers, normal_orders, priority_orders, low_stock; double inventory_value; };

class WarehouseService {
public:
    explicit WarehouseService(std::string data_file);
    ~WarehouseService();
    WarehouseService(const WarehouseService&) = delete;
    WarehouseService& operator=(const WarehouseService&) = delete;

    Result load();
    Result save() const;
    Result seed_demo_data();
    Snapshot snapshot() const;

    Result add_product(const DSProduct& product);
    Result update_product(const DSProduct& product);
    Result delete_product(const std::string& id);
    Result adjust_stock(const std::string& id, int delta);
    Result add_supplier(const DSSupplier& supplier);
    Result update_supplier(const DSSupplier& supplier);
    Result delete_supplier(const std::string& id);
    Result add_order(const DSOrder& order);
    Result process_next_order();
    Result undo_last_action();

    std::vector<DSProduct> products(bool alphabetical = true) const;
    std::optional<DSProduct> find_product_by_name(const std::string& name) const;
    std::vector<DSSupplier> suppliers() const;
    std::vector<DSOrder> normal_orders() const;
    std::vector<DSOrder> priority_orders() const;
    std::vector<DSAction> actions() const;
    std::vector<DSProduct> low_stock() const;
    std::vector<DSProduct> stock_report(bool ascending) const;

private:
    DSWarehouse* data_{};
    std::string data_file_;
    Result persist_after(const Result& result) const;
    void record_stock_action(const std::string& description, const std::string& product_id, int delta);
};

} // namespace warex
