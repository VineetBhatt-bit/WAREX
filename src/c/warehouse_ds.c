#include "warehouse_ds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASH_BUCKETS 53

typedef struct ProductNode { DSProduct value; struct ProductNode *next; } ProductNode;
typedef struct SupplierNode { DSSupplier value; struct SupplierNode *next; } SupplierNode;
typedef struct HashNode { char key[WAREX_ID_LEN]; ProductNode *product; struct HashNode *next; } HashNode;
typedef struct BstNode { ProductNode *product; struct BstNode *left, *right; } BstNode;
typedef struct OrderNode { DSOrder value; struct OrderNode *next; } OrderNode;
typedef struct ActionNode { DSAction value; struct ActionNode *next; } ActionNode;

struct DSWarehouse {
    ProductNode *products; size_t products_count;
    SupplierNode *suppliers; size_t suppliers_count;
    HashNode *hash[HASH_BUCKETS]; BstNode *name_index;
    OrderNode *normal_head, *normal_tail; size_t normal_count;
    DSOrder *heap; size_t heap_count, heap_capacity;
    ActionNode *actions; size_t action_count;
};

static void copy_text(char *to, size_t size, const char *from) { if (!from) from = ""; snprintf(to, size, "%s", from); }
static unsigned hash_key(const char *key) { unsigned h = 5381; for (; *key; ++key) h = ((h << 5) + h) ^ (unsigned char)*key; return h % HASH_BUCKETS; }
static int valid_id(const char *s) { return s && *s && strlen(s) < WAREX_ID_LEN; }
static int product_order(const DSProduct *a, const DSProduct *b) { int n = strcmp(a->name, b->name); return n ? n : strcmp(a->id, b->id); }

static void free_bst(BstNode *node) { if (!node) return; free_bst(node->left); free_bst(node->right); free(node); }
static ProductNode *bst_find_name(BstNode *root, const char *name) { while (root) { int compare = strcmp(name, root->product->value.name); if (compare == 0) return root->product; root = compare < 0 ? root->left : root->right; } return NULL; }
static BstNode *bst_insert(BstNode *root, ProductNode *product) {
    if (!root) { BstNode *n = calloc(1, sizeof(*n)); if (n) n->product = product; return n; }
    if (product_order(&product->value, &root->product->value) < 0) root->left = bst_insert(root->left, product);
    else root->right = bst_insert(root->right, product);
    return root;
}
static void hash_clear(DSWarehouse *w) { for (size_t i=0;i<HASH_BUCKETS;i++) { HashNode *n=w->hash[i]; while(n){HashNode *next=n->next;free(n);n=next;} w->hash[i]=NULL; } }
static int hash_put(DSWarehouse *w, ProductNode *p) { unsigned b=hash_key(p->value.id); HashNode *n=calloc(1,sizeof(*n)); if(!n) return 0; copy_text(n->key,sizeof(n->key),p->value.id); n->product=p; n->next=w->hash[b]; w->hash[b]=n; return 1; }
static ProductNode *hash_get(const DSWarehouse *w, const char *id) { if(!valid_id(id)) return NULL; for(HashNode*n=w->hash[hash_key(id)];n;n=n->next) if(!strcmp(n->key,id)) return n->product; return NULL; }
static void rebuild_indexes(DSWarehouse *w) { hash_clear(w); free_bst(w->name_index); w->name_index=NULL; for(ProductNode*p=w->products;p;p=p->next){hash_put(w,p); w->name_index=bst_insert(w->name_index,p);} }

DSWarehouse *ds_warehouse_create(void) { return calloc(1,sizeof(DSWarehouse)); }
void ds_warehouse_clear(DSWarehouse *w) {
    if(!w)return; ProductNode*p=w->products;while(p){ProductNode*n=p->next;free(p);p=n;} w->products=NULL;w->products_count=0; hash_clear(w);free_bst(w->name_index);w->name_index=NULL;
    SupplierNode*s=w->suppliers;while(s){SupplierNode*n=s->next;free(s);s=n;}w->suppliers=NULL;w->suppliers_count=0;
    OrderNode*o=w->normal_head;while(o){OrderNode*n=o->next;free(o);o=n;}w->normal_head=w->normal_tail=NULL;w->normal_count=0;free(w->heap);w->heap=NULL;w->heap_count=w->heap_capacity=0;
    ActionNode*a=w->actions;while(a){ActionNode*n=a->next;free(a);a=n;}w->actions=NULL;w->action_count=0;
}
void ds_warehouse_destroy(DSWarehouse *w) { if(w){ds_warehouse_clear(w);free(w);} }

int ds_add_product(DSWarehouse *w,const DSProduct *v){if(!w||!v||!valid_id(v->id)||!v->name[0]||v->quantity<0||v->reorder_level<0||v->price<0||hash_get(w,v->id))return 0;ProductNode*n=calloc(1,sizeof(*n));if(!n)return 0;n->value=*v;n->next=w->products;w->products=n;w->products_count++;rebuild_indexes(w);return 1;}
int ds_update_product(DSWarehouse *w,const DSProduct *v){ProductNode*n=w&&v?hash_get(w,v->id):NULL;if(!n||!v->name[0]||v->quantity<0||v->reorder_level<0||v->price<0)return 0;n->value=*v;rebuild_indexes(w);return 1;}
int ds_remove_product(DSWarehouse*w,const char*id){if(!w||!valid_id(id))return 0;ProductNode**p=&w->products;while(*p&&strcmp((*p)->value.id,id))p=&(*p)->next;if(!*p)return 0;ProductNode*gone=*p;*p=gone->next;free(gone);w->products_count--;rebuild_indexes(w);return 1;}
int ds_find_product(const DSWarehouse*w,const char*id,DSProduct*out){ProductNode*n=w?hash_get(w,id):NULL;if(!n)return 0;if(out)*out=n->value;return 1;}
int ds_find_product_by_name(const DSWarehouse*w,const char*name,DSProduct*out){ProductNode*n=(w&&name&&*name)?bst_find_name(w->name_index,name):NULL;if(!n)return 0;if(out)*out=n->value;return 1;}
int ds_adjust_stock(DSWarehouse*w,const char*id,int delta){ProductNode*n=w?hash_get(w,id):NULL;if(!n||(delta<0&&n->value.quantity< -delta))return 0;n->value.quantity+=delta;return 1;}
size_t ds_product_count(const DSWarehouse*w){return w?w->products_count:0;}
static void bst_collect(BstNode*n,DSProduct*items,size_t*at){if(!n)return;bst_collect(n->left,items,at);items[(*at)++]=n->product->value;bst_collect(n->right,items,at);}
DSProduct *ds_collect_products(const DSWarehouse*w,size_t*count){if(count)*count=0;if(!w)return NULL;DSProduct*a=calloc(w->products_count,sizeof(*a));if(w->products_count&&!a)return NULL;size_t i=0;for(ProductNode*n=w->products;n;n=n->next)a[i++]=n->value;if(count)*count=i;return a;}
DSProduct *ds_collect_products_name_order(const DSWarehouse*w,size_t*count){if(count)*count=0;if(!w)return NULL;DSProduct*a=calloc(w->products_count,sizeof(*a));if(w->products_count&&!a)return NULL;size_t at=0;bst_collect(w->name_index,a,&at);if(count)*count=at;return a;}
static void merge(DSProduct*a,DSProduct*temp,size_t l,size_t m,size_t r,int asc){size_t i=l,j=m,k=l;while(i<m&&j<r){int left=a[i].quantity<=a[j].quantity;if(!asc)left=!left;temp[k++]=left?a[i++]:a[j++];}while(i<m)temp[k++]=a[i++];while(j<r)temp[k++]=a[j++];for(i=l;i<r;i++)a[i]=temp[i];}
static void merge_sort(DSProduct*a,DSProduct*t,size_t l,size_t r,int asc){if(r-l<2)return;size_t m=l+(r-l)/2;merge_sort(a,t,l,m,asc);merge_sort(a,t,m,r,asc);merge(a,t,l,m,r,asc);}
void ds_sort_products_by_quantity(DSProduct*a,size_t n,int asc){if(!a||n<2)return;DSProduct*t=malloc(n*sizeof(*t));if(!t)return;merge_sort(a,t,0,n,asc);free(t);}
void ds_free_products(DSProduct*p){free(p);}

int ds_add_supplier(DSWarehouse*w,const DSSupplier*v){if(!w||!v||!valid_id(v->id)||!v->name[0]||ds_find_supplier(w,v->id,NULL))return 0;SupplierNode*n=calloc(1,sizeof(*n));if(!n)return 0;n->value=*v;n->next=w->suppliers;w->suppliers=n;w->suppliers_count++;return 1;}
int ds_update_supplier(DSWarehouse*w,const DSSupplier*v){if(!w||!v)return 0;for(SupplierNode*n=w->suppliers;n;n=n->next)if(!strcmp(n->value.id,v->id)){if(!v->name[0])return 0;n->value=*v;return 1;}return 0;}
int ds_remove_supplier(DSWarehouse*w,const char*id){if(!w||!valid_id(id))return 0;for(ProductNode*p=w->products;p;p=p->next)if(!strcmp(p->value.supplier_id,id))return 0;SupplierNode**s=&w->suppliers;while(*s&&strcmp((*s)->value.id,id))s=&(*s)->next;if(!*s)return 0;SupplierNode*g=*s;*s=g->next;free(g);w->suppliers_count--;return 1;}
int ds_find_supplier(const DSWarehouse*w,const char*id,DSSupplier*out){if(!w||!valid_id(id))return 0;for(SupplierNode*n=w->suppliers;n;n=n->next)if(!strcmp(n->value.id,id)){if(out)*out=n->value;return 1;}return 0;}
size_t ds_supplier_count(const DSWarehouse*w){return w?w->suppliers_count:0;}
DSSupplier *ds_collect_suppliers(const DSWarehouse*w,size_t*count){if(count)*count=0;if(!w)return NULL;DSSupplier*a=calloc(w->suppliers_count,sizeof(*a));if(w->suppliers_count&&!a)return NULL;size_t i=0;for(SupplierNode*n=w->suppliers;n;n=n->next)a[i++]=n->value;if(count)*count=i;return a;}
void ds_free_suppliers(DSSupplier*s){free(s);}

static int earlier(const DSOrder*a,const DSOrder*b){return a->priority>b->priority||(a->priority==b->priority&&strcmp(a->id,b->id)<0);}
static void heap_up(DSWarehouse*w,size_t i){while(i){size_t p=(i-1)/2;if(!earlier(&w->heap[i],&w->heap[p]))break;DSOrder t=w->heap[i];w->heap[i]=w->heap[p];w->heap[p]=t;i=p;}}
static void heap_down(DSWarehouse*w,size_t i){for(;;){size_t l=i*2+1,r=l+1,b=i;if(l<w->heap_count&&earlier(&w->heap[l],&w->heap[b]))b=l;if(r<w->heap_count&&earlier(&w->heap[r],&w->heap[b]))b=r;if(b==i)break;DSOrder t=w->heap[i];w->heap[i]=w->heap[b];w->heap[b]=t;i=b;}}
static int valid_order(const DSWarehouse*w,const DSOrder*o){return w&&o&&valid_id(o->id)&&valid_id(o->product_id)&&o->customer[0]&&o->quantity>0&&o->priority>=1&&o->priority<=5&&ds_find_product(w,o->product_id,NULL);}
int ds_enqueue_order(DSWarehouse*w,const DSOrder*o){if(!valid_order(w,o)||o->priority!=1)return 0;OrderNode*n=calloc(1,sizeof(*n));if(!n)return 0;n->value=*o;if(w->normal_tail)w->normal_tail->next=n;else w->normal_head=n;w->normal_tail=n;w->normal_count++;return 1;}
int ds_enqueue_priority_order(DSWarehouse*w,const DSOrder*o){if(!valid_order(w,o)||o->priority<=1)return 0;if(w->heap_count==w->heap_capacity){size_t cap=w->heap_capacity?w->heap_capacity*2:8;DSOrder*n=realloc(w->heap,cap*sizeof(*n));if(!n)return 0;w->heap=n;w->heap_capacity=cap;}w->heap[w->heap_count++]=*o;heap_up(w,w->heap_count-1);return 1;}
int ds_dequeue_order(DSWarehouse*w,DSOrder*out){if(!w||!w->normal_head)return 0;OrderNode*n=w->normal_head;if(out)*out=n->value;w->normal_head=n->next;if(!w->normal_head)w->normal_tail=NULL;free(n);w->normal_count--;return 1;}
int ds_pop_priority_order(DSWarehouse*w,DSOrder*out){if(!w||!w->heap_count)return 0;if(out)*out=w->heap[0];w->heap[0]=w->heap[--w->heap_count];if(w->heap_count)heap_down(w,0);return 1;}
size_t ds_normal_order_count(const DSWarehouse*w){return w?w->normal_count:0;}size_t ds_priority_order_count(const DSWarehouse*w){return w?w->heap_count:0;}
DSOrder *ds_collect_normal_orders(const DSWarehouse*w,size_t*count){if(count)*count=0;if(!w)return NULL;DSOrder*a=calloc(w->normal_count,sizeof(*a));if(w->normal_count&&!a)return NULL;size_t i=0;for(OrderNode*n=w->normal_head;n;n=n->next)a[i++]=n->value;if(count)*count=i;return a;}
DSOrder *ds_collect_priority_orders(const DSWarehouse*w,size_t*count){if(count)*count=0;if(!w)return NULL;DSOrder*a=calloc(w->heap_count,sizeof(*a));if(w->heap_count&&!a)return NULL;memcpy(a,w->heap,w->heap_count*sizeof(*a));if(count)*count=w->heap_count;return a;}void ds_free_orders(DSOrder*o){free(o);}

int ds_push_action(DSWarehouse*w,const DSAction*a){if(!w||!a||!a->description[0])return 0;ActionNode*n=calloc(1,sizeof(*n));if(!n)return 0;n->value=*a;n->next=w->actions;w->actions=n;w->action_count++;return 1;}
int ds_pop_action(DSWarehouse*w,DSAction*out){if(!w||!w->actions)return 0;ActionNode*n=w->actions;if(out)*out=n->value;w->actions=n->next;free(n);w->action_count--;return 1;}
int ds_peek_action(const DSWarehouse*w,DSAction*out){if(!w||!w->actions)return 0;if(out)*out=w->actions->value;return 1;}
size_t ds_action_count(const DSWarehouse*w){return w?w->action_count:0;}
DSAction*ds_collect_actions(const DSWarehouse*w,size_t*count){if(count)*count=0;if(!w)return NULL;DSAction*a=calloc(w->action_count,sizeof(*a));if(w->action_count&&!a)return NULL;size_t i=0;for(ActionNode*n=w->actions;n;n=n->next)a[i++]=n->value;if(count)*count=i;return a;}void ds_free_actions(DSAction*a){free(a);}
