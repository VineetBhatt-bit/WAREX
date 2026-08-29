#include "warehouse_ds.h"
#include "warehouse_service.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": " #condition "\n"; ++failures; } } while (0)

DSProduct product(const char* id, const char* name, int quantity = 10) { DSProduct p{}; std::snprintf(p.id,sizeof p.id,"%s",id);std::snprintf(p.sku,sizeof p.sku,"SKU-%s",id);std::snprintf(p.name,sizeof p.name,"%s",name);std::snprintf(p.category,sizeof p.category,"General");p.quantity=quantity;p.reorder_level=5;p.price=3.5;return p; }
DSSupplier supplier(const char* id) { DSSupplier s{};std::snprintf(s.id,sizeof s.id,"%s",id);std::snprintf(s.name,sizeof s.name,"Supplier %s",id);std::snprintf(s.contact,sizeof s.contact,"Contact");std::snprintf(s.email,sizeof s.email,"a@example.test");return s; }
DSOrder order(const char* id, const char* product_id, int qty, int priority) { DSOrder o{};std::snprintf(o.id,sizeof o.id,"%s",id);std::snprintf(o.product_id,sizeof o.product_id,"%s",product_id);std::snprintf(o.customer,sizeof o.customer,"Customer");o.quantity=qty;o.priority=priority;return o; }

void data_structure_tests() {
    DSWarehouse* w = ds_warehouse_create(); CHECK(w != nullptr); CHECK(ds_product_count(w) == 0); CHECK(!ds_remove_product(w,"none"));
    auto a=product("A","Alpha",8), b=product("B","Beta",3), c=product("C","Gamma",12);
    CHECK(ds_add_product(w,&a)); CHECK(ds_add_product(w,&b)); CHECK(ds_add_product(w,&c)); CHECK(!ds_add_product(w,&a)); // hash duplicate
    DSProduct found{}; CHECK(ds_find_product(w,"B",&found) && found.quantity==3); CHECK(ds_find_product_by_name(w,"Beta",&found) && std::string(found.id)=="B"); CHECK(!ds_find_product_by_name(w,"Missing",&found)); CHECK(!ds_find_product(w,"missing",&found)); CHECK(!ds_adjust_stock(w,"B",-4)); CHECK(ds_adjust_stock(w,"B",2)); CHECK(ds_find_product(w,"B",&found)&&found.quantity==5);
    size_t count=0;auto ordered=ds_collect_products_name_order(w,&count);CHECK(count==3&&std::string(ordered[0].name)=="Alpha");ds_free_products(ordered);auto sortable=ds_collect_products(w,&count);ds_sort_products_by_quantity(sortable,count,1);CHECK(sortable[0].quantity==5);ds_sort_products_by_quantity(sortable,count,0);CHECK(sortable[0].quantity==12);ds_free_products(sortable);
    auto s=supplier("S1");CHECK(ds_add_supplier(w,&s));CHECK(!ds_add_supplier(w,&s));CHECK(ds_remove_supplier(w,"S1"));
    auto n1=order("N1","A",1,1),n2=order("N2","A",1,1);CHECK(ds_enqueue_order(w,&n1)); CHECK(ds_enqueue_order(w,&n2)); DSOrder out{};CHECK(ds_dequeue_order(w,&out)&&std::string(out.id)=="N1");CHECK(ds_dequeue_order(w,&out)&&std::string(out.id)=="N2");CHECK(!ds_dequeue_order(w,&out));
    auto p2=order("P2","A",1,2),p5=order("P5","A",1,5),p3=order("P3","A",1,3);CHECK(ds_enqueue_priority_order(w,&p2));CHECK(ds_enqueue_priority_order(w,&p5));CHECK(ds_enqueue_priority_order(w,&p3));CHECK(ds_pop_priority_order(w,&out)&&std::string(out.id)=="P5");CHECK(ds_pop_priority_order(w,&out)&&std::string(out.id)=="P3");CHECK(ds_pop_priority_order(w,&out)&&std::string(out.id)=="P2");CHECK(!ds_pop_priority_order(w,&out));
    DSAction act{};std::snprintf(act.description,sizeof act.description,"received");CHECK(ds_push_action(w,&act));CHECK(ds_peek_action(w,&act));CHECK(ds_pop_action(w,&act));CHECK(!ds_pop_action(w,&act));
    CHECK(ds_remove_product(w,"B"));CHECK(!ds_find_product(w,"B",nullptr));ds_warehouse_destroy(w);
}

void service_tests() {
    auto path = (std::filesystem::temp_directory_path() / "warex_test_data.wrx").string(); std::filesystem::remove(path);
    { warex::WarehouseService service(path);auto s=supplier("S1");CHECK(service.add_supplier(s).ok);auto removable=supplier("S2");CHECK(service.add_supplier(removable).ok);CHECK(service.delete_supplier("S2").ok);std::snprintf(s.name,sizeof s.name,"Updated Supplier");CHECK(service.update_supplier(s).ok);auto p=product("P1","Paper",12);std::snprintf(p.supplier_id,sizeof p.supplier_id,"S1");CHECK(service.add_product(p).ok);CHECK(!service.delete_supplier("S1").ok);p.quantity=13;CHECK(service.update_product(p).ok);CHECK(service.find_product_by_name("Paper").has_value());CHECK(!service.add_product(p).ok);CHECK(!service.adjust_stock("P1",-14).ok);CHECK(service.adjust_stock("P1",4).ok);CHECK(service.undo_last_action().ok);CHECK(service.add_order(order("O1","P1",3,1)).ok);CHECK(!service.add_order(order("O1","P1",3,1)).ok);CHECK(service.add_order(order("O2","P1",2,5)).ok);CHECK(service.process_next_order().ok);CHECK(service.products().front().quantity==11); }
    { warex::WarehouseService restored(path);CHECK(restored.load().ok);CHECK(restored.products().size()==1);CHECK(restored.normal_orders().size()==1);CHECK(restored.priority_orders().empty());CHECK(restored.snapshot().suppliers==1); }
    std::filesystem::remove(path);
}
}
int main(){data_structure_tests();service_tests();if(failures){std::cerr<<failures<<" test assertion(s) failed.\n";return EXIT_FAILURE;}std::cout<<"All WAREX tests passed.\n";}
