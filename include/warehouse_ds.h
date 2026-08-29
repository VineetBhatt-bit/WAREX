#ifndef WAREX_WAREHOUSE_DS_H
#define WAREX_WAREHOUSE_DS_H

/* C-owned storage and algorithms. Callers own arrays returned by collect APIs. */
#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define WAREX_ID_LEN 32
#define WAREX_TEXT_LEN 96
#define WAREX_ACTION_LEN 160

typedef struct {
    char id[WAREX_ID_LEN];
    char sku[WAREX_ID_LEN];
    char name[WAREX_TEXT_LEN];
    char supplier_id[WAREX_ID_LEN];
    char category[WAREX_TEXT_LEN];
    int quantity;
    int reorder_level;
    double price;
} DSProduct;

typedef struct {
    char id[WAREX_ID_LEN];
    char name[WAREX_TEXT_LEN];
    char contact[WAREX_TEXT_LEN];
    char email[WAREX_TEXT_LEN];
} DSSupplier;

typedef struct {
    char id[WAREX_ID_LEN];
    char product_id[WAREX_ID_LEN];
    char customer[WAREX_TEXT_LEN];
    int quantity;
    int priority; /* 1 (normal) through 5 (urgent). */
} DSOrder;

typedef struct {
    char description[WAREX_ACTION_LEN];
    char product_id[WAREX_ID_LEN];
    int stock_delta;
    int undoable;
} DSAction;

typedef struct DSWarehouse DSWarehouse;

DSWarehouse *ds_warehouse_create(void);
void ds_warehouse_destroy(DSWarehouse *warehouse);
void ds_warehouse_clear(DSWarehouse *warehouse);

int ds_add_product(DSWarehouse *warehouse, const DSProduct *product);
int ds_update_product(DSWarehouse *warehouse, const DSProduct *product);
int ds_remove_product(DSWarehouse *warehouse, const char *id);
int ds_find_product(const DSWarehouse *warehouse, const char *id, DSProduct *out);
int ds_find_product_by_name(const DSWarehouse *warehouse, const char *name, DSProduct *out);
int ds_adjust_stock(DSWarehouse *warehouse, const char *id, int delta);
size_t ds_product_count(const DSWarehouse *warehouse);
DSProduct *ds_collect_products(const DSWarehouse *warehouse, size_t *count);
DSProduct *ds_collect_products_name_order(const DSWarehouse *warehouse, size_t *count);
void ds_sort_products_by_quantity(DSProduct *products, size_t count, int ascending);
void ds_free_products(DSProduct *products);

int ds_add_supplier(DSWarehouse *warehouse, const DSSupplier *supplier);
int ds_update_supplier(DSWarehouse *warehouse, const DSSupplier *supplier);
int ds_remove_supplier(DSWarehouse *warehouse, const char *id);
int ds_find_supplier(const DSWarehouse *warehouse, const char *id, DSSupplier *out);
size_t ds_supplier_count(const DSWarehouse *warehouse);
DSSupplier *ds_collect_suppliers(const DSWarehouse *warehouse, size_t *count);
void ds_free_suppliers(DSSupplier *suppliers);

int ds_enqueue_order(DSWarehouse *warehouse, const DSOrder *order);
int ds_enqueue_priority_order(DSWarehouse *warehouse, const DSOrder *order);
int ds_dequeue_order(DSWarehouse *warehouse, DSOrder *out);
int ds_pop_priority_order(DSWarehouse *warehouse, DSOrder *out);
size_t ds_normal_order_count(const DSWarehouse *warehouse);
size_t ds_priority_order_count(const DSWarehouse *warehouse);
DSOrder *ds_collect_normal_orders(const DSWarehouse *warehouse, size_t *count);
DSOrder *ds_collect_priority_orders(const DSWarehouse *warehouse, size_t *count);
void ds_free_orders(DSOrder *orders);

int ds_push_action(DSWarehouse *warehouse, const DSAction *action);
int ds_pop_action(DSWarehouse *warehouse, DSAction *out);
int ds_peek_action(const DSWarehouse *warehouse, DSAction *out);
size_t ds_action_count(const DSWarehouse *warehouse);
DSAction *ds_collect_actions(const DSWarehouse *warehouse, size_t *count);
void ds_free_actions(DSAction *actions);

#ifdef __cplusplus
}
#endif

#endif
