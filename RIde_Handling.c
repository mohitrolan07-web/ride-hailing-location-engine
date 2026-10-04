#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_NAME      100
#define MAX_ADDR      200
#define MAX_PHONE      20
#define MAX_ITEMS      50
#define MAX_ITEM_NAME 100
#define MAX_ORDERS    200
#define MAX_PINDIST   500

/* ─── Inventory Item ─── */
typedef struct Item {
    int  item_id;
    char name[MAX_ITEM_NAME];
    int  quantity;
} Item;

/* ─── Order record (stored in orders.dat) ─── */
typedef struct Order {
    int   order_id;
    int   customer_id;
    int   dark_store_id;
    int   delivery_rider_id;
    int   item_id;
    int   num_items;
    float amount;
    int   completed;          /* 0=pending, 1=done */
} Order;

/* ─── Customer ─── */
typedef struct Customer {
    int   account_id;
    char  name[MAX_NAME];
    char  aadhaar[20];
    char  address[MAX_ADDR];
    int   pincode;
    char  phone[MAX_PHONE];
    float account_balance;
    float total_spent;
    int   pending_orders[MAX_ORDERS];
    int   pending_count;
    int   completed_orders[MAX_ORDERS];
    int   completed_count;
} Customer;

/* ─── Delivery Rider ─── */
typedef struct DeliveryRider {
    int   rider_id;
    char  name[MAX_NAME];
    char  phone[MAX_PHONE];
    int   pincode;
    int   dark_store_id;
    float monthly_income[12];   /* index 11 = current month */
    int   orders_last_month;
    float total_earnings;
    int   is_available;         /* 1=free, 0=busy */
    int   completed_orders[MAX_ORDERS];
    int   completed_count;
} DeliveryRider;

/* ─── Dark Store ─── */
typedef struct DarkStore {
    int     store_id;
    int     pincode;
    char    location[MAX_ADDR];
    Item    inventory[MAX_ITEMS];
    int     item_count;
    int     pending_orders[MAX_ORDERS];
    int     pending_count;
    int     completed_orders[MAX_ORDERS];
    int     completed_count;
} DarkStore;

/* ─── Pincode Distance entry ─── */
typedef struct PinDist {
    int   pin1;
    int   pin2;
    float distance;
} PinDist;

/* ─── AVL Tree Node (generic: key=int, data=void*) ─── */
typedef struct AVLNode {
    int            key;
    void          *data;
    int            height;
    struct AVLNode *left;
    struct AVLNode *right;
} AVLNode;

AVLNode *customerTree = NULL;
AVLNode *riderTree    = NULL;
AVLNode *storeTree    = NULL;

int next_customer_id = 1001;
int next_rider_id    = 2001;
int next_store_id    = 3001;
int next_order_id    = 4001;

PinDist distTable[MAX_PINDIST];
int     distCount = 0;

void flush_input(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int max2(int a, int b) { return (a > b) ? a : b; }

int avl_height(AVLNode *n) {
    return n ? n->height : 0;
}

int avl_bf(AVLNode *n) {
    return n ? avl_height(n->left) - avl_height(n->right) : 0;
}

AVLNode *avl_new_node(int key, void *data) {
    AVLNode *node = (AVLNode *)malloc(sizeof(AVLNode));
    if (!node) { perror("malloc"); exit(1); }
    node->key    = key;
    node->data   = data;
    node->height = 1;
    node->left   = node->right = NULL;
    return node;
}

AVLNode *avl_right_rotate(AVLNode *y) {
    AVLNode *x  = y->left;
    AVLNode *T2 = x->right;
    x->right  = y;
    y->left   = T2;
    y->height = 1 + max2(avl_height(y->left), avl_height(y->right));
    x->height = 1 + max2(avl_height(x->left), avl_height(x->right));
    return x;
}

AVLNode *avl_left_rotate(AVLNode *x) {
    AVLNode *y  = x->right;
    AVLNode *T2 = y->left;
    y->left   = x;
    x->right  = T2;
    x->height = 1 + max2(avl_height(x->left), avl_height(x->right));
    y->height = 1 + max2(avl_height(y->left), avl_height(y->right));
    return y;
}

AVLNode *avl_insert(AVLNode *root, int key, void *data) {
    if (!root) return avl_new_node(key, data);

    if      (key < root->key) root->left  = avl_insert(root->left,  key, data);
    else if (key > root->key) root->right = avl_insert(root->right, key, data);
    else { root->data = data; return root; }   /* duplicate: update */

    root->height = 1 + max2(avl_height(root->left), avl_height(root->right));
    int bf = avl_bf(root);

    /* LL */ if (bf >  1 && key < root->left->key)  return avl_right_rotate(root);
    /* RR */ if (bf < -1 && key > root->right->key) return avl_left_rotate(root);
    /* LR */ if (bf >  1 && key > root->left->key)  {
        root->left = avl_left_rotate(root->left); return avl_right_rotate(root);
    }
    /* RL */ if (bf < -1 && key < root->right->key) {
        root->right = avl_right_rotate(root->right); return avl_left_rotate(root);
    }
    return root;
}

AVLNode *avl_min_node(AVLNode *n) {
    while (n && n->left) n = n->left;
    return n;
}

AVLNode *avl_delete(AVLNode *root, int key) {
    if (!root) return NULL;

    if      (key < root->key) root->left  = avl_delete(root->left,  key);
    else if (key > root->key) root->right = avl_delete(root->right, key);
    else {
        if (!root->left || !root->right) {
            AVLNode *tmp = root->left ? root->left : root->right;
            free(root->data);
            free(root);
            return tmp;
        }
        AVLNode *succ  = avl_min_node(root->right);
        root->key      = succ->key;
        root->data     = succ->data;
        succ->data     = NULL;   /* prevent double-free in recursion */
        root->right    = avl_delete(root->right, succ->key);
    }

    root->height = 1 + max2(avl_height(root->left), avl_height(root->right));
    int bf = avl_bf(root);

    if (bf >  1 && avl_bf(root->left)  >= 0) return avl_right_rotate(root);
    if (bf >  1 && avl_bf(root->left)  <  0) {
        root->left = avl_left_rotate(root->left); return avl_right_rotate(root);
    }
    if (bf < -1 && avl_bf(root->right) <= 0) return avl_left_rotate(root);
    if (bf < -1 && avl_bf(root->right) >  0) {
        root->right = avl_right_rotate(root->right); return avl_left_rotate(root);
    }
    return root;
}

AVLNode *avl_search(AVLNode *root, int key) {
    if (!root || root->key == key) return root;
    return (key < root->key)
           ? avl_search(root->left, key)
           : avl_search(root->right, key);
}

void avl_inorder(AVLNode *root, void (*visit)(void *)) {
    if (!root) return;
    avl_inorder(root->left,  visit);
    visit(root->data);
    avl_inorder(root->right, visit);
}

void avl_free(AVLNode *root) {
    if (!root) return;
    avl_free(root->left);
    avl_free(root->right);
    free(root->data);
    free(root);
}

void add_distance(int pin1, int pin2, float dist) {
    if (distCount >= MAX_PINDIST) { printf("Distance table full!\n"); return; }
    distTable[distCount].pin1     = pin1;
    distTable[distCount].pin2     = pin2;
    distTable[distCount].distance = dist;
    distCount++;
}

float get_distance(int pin1, int pin2) {
    if (pin1 == pin2) return 0.0f;
    for (int i = 0; i < distCount; i++) {
        if ((distTable[i].pin1 == pin1 && distTable[i].pin2 == pin2) ||
            (distTable[i].pin1 == pin2 && distTable[i].pin2 == pin1))
            return distTable[i].distance;
    }
    return 9999;
}

/* ─── Store/Rider collector arrays (used across several functions) ─── */
static DarkStore     *storeArr[500]; static int storeArrSz = 0;
static DeliveryRider *riderArr[500]; static int riderArrSz = 0;
static Customer      *customerArr[2000];static int customerArrSz = 0;

static void collect_Store(void *d) { if (storeArrSz < 500)  storeArr[storeArrSz++]  = (DarkStore *)d;     }
static void Collect_Rider(void *d) { if (riderArrSz < 500)  riderArr[riderArrSz++]  = (DeliveryRider *)d; }
static void Collect_Customer(void *d) { if (customerArrSz < 2000) customerArr[customerArrSz++]  = (Customer *)d;      }

/* Find nearest dark-store for a given pincode */
int find_nearest_store(int customer_pincode) {
    storeArrSz = 0;
    avl_inorder(storeTree, collect_Store);
    if (storeArrSz == 0) return -1;

    int   best_id   = -1;
    float best_dist = 9999;
    for (int i = 0; i < storeArrSz; i++) {
        float d = get_distance(customer_pincode, storeArr[i]->pincode);
        if (d < best_dist) { best_dist = d; best_id = storeArr[i]->store_id; }
    }
    return best_id;
}

/* Find nearest FREE rider for a given store */
int find_nearest_free_rider(int store_id, int store_pincode) {
    riderArrSz = 0;
    avl_inorder(riderTree, Collect_Rider);

    /* First preference: rider assigned to this store */
    for (int i = 0; i < riderArrSz; i++)
        if (riderArr[i]->dark_store_id == store_id && riderArr[i]->is_available)
            return riderArr[i]->rider_id;

    /* Second preference: nearest free rider from any store */
    int   best_id   = -1;
    float best_dist = 9999;
    for (int i = 0; i < riderArrSz; i++) {
        if (!riderArr[i]->is_available) continue;
        float d = get_distance(store_pincode, riderArr[i]->pincode);
        if (d < best_dist) { best_dist = d; best_id = riderArr[i]->rider_id; }
    }
    return best_id;
}

static Customer      *sv_cArr[2000]; static int sv_cSz = 0;
static DeliveryRider *sv_rArr[2000]; static int sv_rSz = 0;
static DarkStore     *sv_sArr[2000]; static int sv_sSz = 0;

static void sv_cc(void *d) { if (sv_cSz < 2000) sv_cArr[sv_cSz++] = (Customer *)d;      }
static void sv_cr(void *d) { if (sv_rSz < 2000) sv_rArr[sv_rSz++] = (DeliveryRider *)d; }
static void sv_cs(void *d) { if (sv_sSz < 2000) sv_sArr[sv_sSz++] = (DarkStore *)d;     }

void save_all_data(void) {
    /* counters */
    FILE *fc = fopen("counters.dat", "wb");
    if (fc) {
        fwrite(&next_customer_id, sizeof(int), 1, fc);
        fwrite(&next_rider_id,    sizeof(int), 1, fc);
        fwrite(&next_store_id,    sizeof(int), 1, fc);
        fwrite(&next_order_id,    sizeof(int), 1, fc);
        fclose(fc);
    }

    /* distance table */
    FILE *fd = fopen("distances.dat", "wb");
    if (fd) {
        fwrite(&distCount, sizeof(int),     1,         fd);
        fwrite(distTable,  sizeof(PinDist), distCount, fd);
        fclose(fd);
    }

    /* customers */
    sv_cSz = 0; avl_inorder(customerTree, sv_cc);
    FILE *fcu = fopen("customers.dat", "wb");
    if (fcu) {
        fwrite(&sv_cSz, sizeof(int), 1, fcu);
        for (int i = 0; i < sv_cSz; i++) fwrite(sv_cArr[i], sizeof(Customer), 1, fcu);
        fclose(fcu);
    }

    /* riders */
    sv_rSz = 0; avl_inorder(riderTree, sv_cr);
    FILE *fr = fopen("riders.dat", "wb");
    if (fr) {
        fwrite(&sv_rSz, sizeof(int), 1, fr);
        for (int i = 0; i < sv_rSz; i++) fwrite(sv_rArr[i], sizeof(DeliveryRider), 1, fr);
        fclose(fr);
    }

    /* stores */
    sv_sSz = 0; avl_inorder(storeTree, sv_cs);
    FILE *fs = fopen("stores.dat", "wb");
    if (fs) {
        fwrite(&sv_sSz, sizeof(int), 1, fs);
        for (int i = 0; i < sv_sSz; i++) fwrite(sv_sArr[i], sizeof(DarkStore), 1, fs);
        fclose(fs);
    }

    printf(" Data saved successfully.\n");
}

void load_all_data(void) {
    /* counters */
    FILE *fc = fopen("counters.dat", "rb");
    if (fc) {
        fread(&next_customer_id, sizeof(int), 1, fc);
        fread(&next_rider_id,    sizeof(int), 1, fc);
        fread(&next_store_id,    sizeof(int), 1, fc);
        fread(&next_order_id,    sizeof(int), 1, fc);
        fclose(fc);
    }

    /* distance table */
    FILE *fd = fopen("distances.dat", "rb");
    if (fd) {
        fread(&distCount, sizeof(int),     1,         fd);
        fread(distTable,  sizeof(PinDist), distCount, fd);
        fclose(fd);
    }

    /* customers */
    FILE *fcu = fopen("customers.dat", "rb");
    if (fcu) {
        int cnt; fread(&cnt, sizeof(int), 1, fcu);
        for (int i = 0; i < cnt; i++) {
            Customer *c = (Customer *)malloc(sizeof(Customer));
            fread(c, sizeof(Customer), 1, fcu);
            customerTree = avl_insert(customerTree, c->account_id, c);
        }
        fclose(fcu);
    }

    /* riders */
    FILE *fr = fopen("riders.dat", "rb");
    if (fr) {
        int cnt; fread(&cnt, sizeof(int), 1, fr);
        for (int i = 0; i < cnt; i++) {
            DeliveryRider *r = (DeliveryRider *)malloc(sizeof(DeliveryRider));
            fread(r, sizeof(DeliveryRider), 1, fr);
            riderTree = avl_insert(riderTree, r->rider_id, r);
        }
        fclose(fr);
    }

    /* stores */
    FILE *fs = fopen("stores.dat", "rb");
    if (fs) {
        int cnt; fread(&cnt, sizeof(int), 1, fs);
        for (int i = 0; i < cnt; i++) {
            DarkStore *s = (DarkStore *)malloc(sizeof(DarkStore));
            fread(s, sizeof(DarkStore), 1, fs);
            storeTree = avl_insert(storeTree, s->store_id, s);
        }
        fclose(fs);
    }

    printf(" Data loaded successfully.\n");
}

void print_customer(void *data) {
    Customer *c = (Customer *)data;
    printf("-------------------------------------------------\n");
    printf("  Account ID  : %-6d  Pincode : %-6d         \n", c->account_id, c->pincode);
    printf("  Name        : %-35s\n", c->name);
    printf("  Aadhaar     : %-35s\n", c->aadhaar);
    printf("  Phone       : %-35s\n", c->phone);
    printf("  Address     : %-35s\n", c->address);
    printf("  Balance     : Rs.%-8.2f  Spent: Rs.%-8.2f  \n", c->account_balance, c->total_spent);
    printf("  Pending Orders: %-4d  Completed: %-4d        \n",
           c->pending_count, c->completed_count);
    printf("-------------------------------------------------\n");
}

void print_rider(void *data) {
    DeliveryRider *r = (DeliveryRider *)data;
    float sum3 = 0;
    for (int i = 9; i < 12; i++) sum3 += r->monthly_income[i];
    printf("-------------------------------------------------\n");
    printf("  Rider ID    : %-6d  Status: %-12s    \n",
           r->rider_id, r->is_available ? "Available" : "Busy");
    printf("  Name        : %-35s\n", r->name);
    printf("  Phone       : %-35s\n", r->phone);
    printf("  Pincode     : %-6d  Store ID: %-6d         \n", r->pincode, r->dark_store_id);
    printf("  Total Earn  : Rs.%-8.2f  Orders/Month: %-4d  \n",
           r->total_earnings, r->orders_last_month);
    printf("  Last 3M Earn: Rs.%-8.2f                       \n", sum3);
    printf("-------------------------------------------------\n");
}

void print_store(void *data) {
    DarkStore *s = (DarkStore *)data;
    printf("+-------------------------------------------------+\n");
    printf("|  Store ID  : %-6d  Pincode : %-6d          |\n", s->store_id, s->pincode);
    printf("|  Location  : %-35s|\n", s->location);
    printf("|  Inventory (%d items):                          |\n", s->item_count);
    for (int i = 0; i < s->item_count; i++)
        printf("|    [%3d] %-25s  Qty: %-5d      |\n",
               s->inventory[i].item_id, s->inventory[i].name, s->inventory[i].quantity);
    printf("|  Pending: %-4d  Completed: %-4d                |\n",
           s->pending_count, s->completed_count);
    printf("+-------------------------------------------------+\n");
}

/* ── Operation 1: Register Customer ── */
void register_customer(void) {
    Customer *c = (Customer *)calloc(1, sizeof(Customer));
    if (!c) { perror("calloc"); return; }

    c->account_id = next_customer_id++;
    printf("\n--- Register Customer (ID: %d) ---\n", c->account_id);

    printf("  Name        : "); fgets(c->name,    MAX_NAME,  stdin); c->name[strcspn(c->name,"\n")]       = 0;
    printf("  Aadhaar No  : "); fgets(c->aadhaar, 20,        stdin); c->aadhaar[strcspn(c->aadhaar,"\n")] = 0;
    printf("  Address     : "); fgets(c->address, MAX_ADDR,  stdin); c->address[strcspn(c->address,"\n")] = 0;
    printf("  Pincode     : "); scanf("%d",  &c->pincode);      flush_input();
    printf("  Phone       : "); fgets(c->phone, MAX_PHONE, stdin);   c->phone[strcspn(c->phone,"\n")]     = 0;
    printf("  Acc Balance : Rs."); scanf("%f", &c->account_balance); flush_input();

    customerTree = avl_insert(customerTree, c->account_id, c);
    printf("Customer registered! Account ID = %d\n", c->account_id);
    save_all_data();
}

/* ── Operation 2: Update Customer ── */
void update_customer(void) {
    int id;
    printf("\nEnter Account ID to update: "); scanf("%d", &id); flush_input();

    AVLNode *node = avl_search(customerTree, id);
    if (!node) { printf("Customer not found.\n"); return; }

    Customer *c = (Customer *)node->data;
    printf("  1.Name  2.Address  3.Pincode  4.Phone  5.Balance\n");
    printf("  Choice: ");
    int ch; scanf("%d", &ch); flush_input();

    switch (ch) {
        case 1: printf("  New Name    : "); fgets(c->name, MAX_NAME, stdin);      c->name[strcspn(c->name,"\n")]       = 0; break;
        case 2: printf("  New Address : "); fgets(c->address, MAX_ADDR, stdin);   c->address[strcspn(c->address,"\n")] = 0; break;
        case 3: printf("  New Pincode : "); scanf("%d", &c->pincode);  flush_input(); break;
        case 4: printf("  New Phone   : "); fgets(c->phone, MAX_PHONE, stdin);    c->phone[strcspn(c->phone,"\n")]     = 0; break;
        case 5: printf("  New Balance : Rs."); scanf("%f", &c->account_balance); flush_input(); break;
        default: printf("Invalid.\n"); return;
    }
    printf("Customer updated.\n");
    save_all_data();
}

/* ── Operation 3: Delete Customer ── */
void delete_customer(void) {
    int id;
    printf("\nEnter Account ID to delete: "); scanf("%d", &id); flush_input();

    if (!avl_search(customerTree, id)) { printf("Customer not found.\n"); return; }

    printf("Delete customer %d? (y/n): ", id);
    char ch = getchar(); flush_input();
    if (ch != 'y' && ch != 'Y') { printf("Cancelled.\n"); return; }

    customerTree = avl_delete(customerTree, id);
    printf("Customer %d deleted.\n", id);
    save_all_data();
}

static int cmp_spent_desc(const void *a, const void *b) {    // Customers ko highest spending -> lowest spending order me sort karta hai
    float da = (*(Customer **)b)->total_spent - (*(Customer **)a)->total_spent;
    return (da > 0) ? 1 : (da < 0) ? -1 : 0;
}

static int cmp_pin_asc_c(const void *a, const void *b) {    // Customers ko pincode ke ascending order me sort karta hai
    return (*(Customer **)a)->pincode - (*(Customer **)b)->pincode;
}

/* ── Operation 4a: Display Customers by Spending (descending) ── */
void display_customers_by_spending(void) {
    customerArrSz = 0; avl_inorder(customerTree, Collect_Customer);
    if (customerArrSz == 0) { printf("No customers.\n"); return; }
    qsort(customerArr, customerArrSz, sizeof(Customer *), cmp_spent_desc);
    printf("\n--- Customers - Descending by Spending ---\n");
    for (int i = 0; i < customerArrSz; i++) { printf("Rank %d:\n", i+1); print_customer(customerArr[i]); }
}

/* ── Operation 4b: Display Customers by Pincode ── */
void display_customers_by_pincode(void) {
    customerArrSz = 0; avl_inorder(customerTree, Collect_Customer);
    if (customerArrSz == 0) { printf("No customers.\n"); return; }
    qsort(customerArr, customerArrSz, sizeof(Customer *), cmp_pin_asc_c);
    printf("\n--- Customers - Sorted by Pincode ---\n");
    for (int i = 0; i < customerArrSz; i++) print_customer(customerArr[i]);
}

/* ── Operation 10: Range Search ── */
void range_search_customers(void) {
    int b1, b2;
    printf("\nEnter B1 (start Account ID): "); scanf("%d", &b1); flush_input();
    printf("Enter B2 (end   Account ID): "); scanf("%d", &b2); flush_input();
    if (b1 > b2) { int t = b1; b1 = b2; b2 = t; }

    customerArrSz = 0; avl_inorder(customerTree, Collect_Customer);
    printf("\n=== Customers with Account ID in [%d, %d] ===\n", b1, b2);
    int found = 0;
    for (int i = 0; i < customerArrSz; i++)
        if (customerArr[i]->account_id >= b1 && customerArr[i]->account_id <= b2)
            { print_customer(customerArr[i]); found++; }
    printf(found ? " Total: %d\n" : "No customers found in range.\n", found);
}

/* ── Operation 5: Register Rider ── */
void register_rider(void) {
    DeliveryRider *r = (DeliveryRider *)calloc(1, sizeof(DeliveryRider));
    if (!r) { perror("calloc"); return; }

    r->rider_id = next_rider_id++;
    printf("\n--- Register Delivery Rider (ID: %d) ---\n", r->rider_id);

    printf("  Name     : "); fgets(r->name,  MAX_NAME,  stdin); r->name[strcspn(r->name,"\n")]   = 0;
    printf("  Phone    : "); fgets(r->phone, MAX_PHONE, stdin); r->phone[strcspn(r->phone,"\n")] = 0;
    printf("  Pincode  : "); scanf("%d", &r->pincode); flush_input();

    int sid = find_nearest_store(r->pincode);
    if (sid == -1) {
        printf("[!] No dark-stores registered yet. Register a store first.\n");
        free(r); next_rider_id--; return;
    }
    r->dark_store_id = sid;
    r->is_available  = 1;

    riderTree = avl_insert(riderTree, r->rider_id, r);
    printf("[OK] Rider registered! ID=%d, Assigned Store=%d\n", r->rider_id, r->dark_store_id);
    save_all_data();
}

/* ── Operation 6: Delete Rider ── */
void delete_rider(void) {
    int id;
    printf("\nEnter Rider ID to delete: "); scanf("%d", &id); flush_input();
    if (!avl_search(riderTree, id)) { printf("[X] Rider not found.\n"); return; }

    printf(" Delete rider %d? (y/n): ", id);
    char ch = getchar(); flush_input();
    if (ch != 'y' && ch != 'Y') { printf("Cancelled.\n"); return; }

    riderTree = avl_delete(riderTree, id);
    printf("Rider %d deleted.\n", id);
    save_all_data();
}

static float rider_earn3(DeliveryRider *r) {
    float s = 0; for (int i = 9; i < 12; i++) s += r->monthly_income[i]; return s;
}
static int cmp_earn3_desc(const void *a, const void *b) {
    float ea = rider_earn3(*(DeliveryRider **)a), eb = rider_earn3(*(DeliveryRider **)b);
    return (eb > ea) ? 1 : (eb < ea) ? -1 : 0;
}
static int cmp_orders_desc(const void *a, const void *b) {
    return (*(DeliveryRider **)b)->orders_last_month - (*(DeliveryRider **)a)->orders_last_month;
}
static int cmp_pin_earn(const void *a, const void *b) {
    DeliveryRider *ra = *(DeliveryRider **)a, *rb = *(DeliveryRider **)b;
    if (ra->pincode != rb->pincode) return ra->pincode - rb->pincode;
    float ea = rider_earn3(ra), eb = rider_earn3(rb);
    return (eb > ea) ? 1 : (eb < ea) ? -1 : 0;
}

/* ── Operation 7a: Riders by earnings (last 3 months) ── */
void display_riders_by_earnings_3months(void) {
    riderArrSz = 0; avl_inorder(riderTree, Collect_Rider);
    if (riderArrSz == 0) { printf("No riders.\n"); return; }    
    qsort(riderArr, riderArrSz, sizeof(DeliveryRider *), cmp_earn3_desc);
    printf("\n=== Riders - Descending by Last-3-Month Earnings ===\n");
    for (int i = 0; i < riderArrSz; i++) {
        printf("Rank %d (3Months Earn: Rs.%.2f):\n", i+1, rider_earn3(riderArr[i]));
        print_rider(riderArr[i]);
    }
}

/* ── Operation 7b: Riders by orders last 1 month ── */
void display_riders_by_orders_1month(void) {
    riderArrSz = 0; avl_inorder(riderTree, Collect_Rider);
    if (riderArrSz == 0) { printf("No riders.\n"); return; }
    qsort(riderArr, riderArrSz, sizeof(DeliveryRider *), cmp_orders_desc);
    printf("\n=== Riders - Descending by Orders (Last 1 Month) ===\n");
    for (int i = 0; i < riderArrSz; i++) {
        printf("Rank %d (Orders: %d):\n", i+1, riderArr[i]->orders_last_month);
        print_rider(riderArr[i]);
    }
}

/* ── Operation 7c: Riders by pincode then earnings ── */
void display_riders_by_pincode_earnings(void) {
    riderArrSz = 0; avl_inorder(riderTree, Collect_Rider);
    if (riderArrSz == 0) { printf("No riders.\n"); return; }
    qsort(riderArr, riderArrSz, sizeof(DeliveryRider *), cmp_pin_earn);
    printf("\n=== Riders - Pincode ASC, Earnings DESC ===\n");
    for (int i = 0; i < riderArrSz; i++) {
        printf("[Pin: %d | 3Months Earn: Rs.%.2f]\n", riderArr[i]->pincode, rider_earn3(riderArr[i]));
        print_rider(riderArr[i]);
    }
}

void register_store(void) {
    DarkStore *s = (DarkStore *)calloc(1, sizeof(DarkStore));
    if (!s) { perror("calloc"); return; }

    s->store_id = next_store_id++;
    printf("\n--- Register Dark Store (ID: %d) ---\n", s->store_id);
    printf("  Location : "); fgets(s->location, MAX_ADDR, stdin); s->location[strcspn(s->location,"\n")] = 0;
    printf("  Pincode  : "); scanf("%d", &s->pincode); flush_input();

    printf("  How many items in inventory? "); scanf("%d", &s->item_count); flush_input();
    if (s->item_count > MAX_ITEMS) s->item_count = MAX_ITEMS;
    for (int i = 0; i < s->item_count; i++) {
        s->inventory[i].item_id = 100 + i;
        printf("  Item %d Name : ", i+1);
        fgets(s->inventory[i].name, MAX_ITEM_NAME, stdin);
        s->inventory[i].name[strcspn(s->inventory[i].name,"\n")] = 0;
        printf("  Item %d Qty  : ", i+1); scanf("%d", &s->inventory[i].quantity); flush_input();
    }

    storeTree = avl_insert(storeTree, s->store_id, s);
    printf("[OK] Dark Store registered! ID = %d\n", s->store_id);
    save_all_data();
}

void display_store_inventory(int store_id) {
    AVLNode *n = avl_search(storeTree, store_id);
    if (!n) { printf(" Store %d not found.\n", store_id); return; }
    print_store(n->data);
}

/* ── Helper: find item index in store inventory ── */
static int store_find_item(DarkStore *s, int item_id) {
    for (int i = 0; i < s->item_count; i++)
        if (s->inventory[i].item_id == item_id) return i;
    return -1;
}

/* ── Operation 8: Place an Order ── */
void place_order(void) {
    int cust_id, item_id, num;

    printf("\n--- Place an Order ---\n");
    printf("  Customer Account ID : "); scanf("%d", &cust_id); flush_input();

    AVLNode *cn = avl_search(customerTree, cust_id);
    if (!cn) { printf("Customer not found.\n"); return; }
    Customer *cust = (Customer *)cn->data;

    printf("  Item ID             : "); scanf("%d", &item_id); flush_input();
    printf("  Number of Items     : "); scanf("%d", &num);     flush_input();

    /* Collect all stores and find nearest that has stock */
    storeArrSz = 0; avl_inorder(storeTree, collect_Store);

    int   best_store = -1;
    float best_dist  = 1e9f;
    int   best_idx   = -1;

    for (int i = 0; i < storeArrSz; i++) {
        int idx = store_find_item(storeArr[i], item_id);
        if (idx == -1 || storeArr[i]->inventory[idx].quantity < num) continue;
        float d = get_distance(cust->pincode, storeArr[i]->pincode);
        if (d < best_dist) { best_dist = d; best_store = i; best_idx = idx; }
    }

    if (best_store == -1) {
        printf("No dark-store can fulfil this order (item not available / insufficient stock).\n");
        return;
    }

    DarkStore *store = storeArr[best_store];
    int rider_id = find_nearest_free_rider(store->store_id, store->pincode);
    if (rider_id == -1)
        printf("No free rider right now — order queued (rider will be assigned on delivery).\n");

    /* Build order */
    Order *ord = (Order *)calloc(1, sizeof(Order));
    ord->order_id          = next_order_id++;
    ord->customer_id       = cust_id;
    ord->dark_store_id     = store->store_id;
    ord->delivery_rider_id = rider_id;
    ord->item_id           = item_id;
    ord->num_items         = num;
    ord->amount            = num * 50.0f;   /* Rs.50 per unit (demo price) */
    ord->completed         = 0;

    /* Update customer pending list */
    if (cust->pending_count < MAX_ORDERS)
        cust->pending_orders[cust->pending_count++] = ord->order_id;

    /* Update store pending list */
    if (store->pending_count < MAX_ORDERS)
        store->pending_orders[store->pending_count++] = ord->order_id;

    /* Deduct inventory */
    store->inventory[best_idx].quantity -= num;

    /* Mark rider busy */
    if (rider_id != -1) {
        AVLNode *rn = avl_search(riderTree, rider_id);
        if (rn) ((DeliveryRider *)rn->data)->is_available = 0;
    }

    /* Append order to orders.dat */
    FILE *fo = fopen("orders.dat", "ab");
    if (fo) { fwrite(ord, sizeof(Order), 1, fo); fclose(fo); }

    printf("\n Order placed!\n");
    printf("     Order ID : %d\n",  ord->order_id);
    printf("     Store    : %d  (%.1f km away)\n", store->store_id, best_dist);
    printf("     Rider ID : %d\n",  rider_id);
    printf("     Amount   : Rs.%.2f\n", ord->amount);

    free(ord);
    save_all_data();
}

/* ── Operation 9: Deliver an Order ── */
void deliver_order(void) {
    int order_id;
    printf("\n--- Deliver Order ---\n");
    printf("  Enter Order ID: "); scanf("%d", &order_id); flush_input();

    /* Read order from file */
    FILE *fo = fopen("orders.dat", "rb");
    if (!fo) { printf(" No orders file found.\n"); return; }

    Order ord;
    int found = 0;
    while (fread(&ord, sizeof(Order), 1, fo)) {
        if (ord.order_id == order_id) { found = 1; break; }
    }
    fclose(fo);

    if (!found)    { printf(" Order %d not found.\n", order_id); return; }
    if (ord.completed) { printf(" Order already completed.\n"); return; }

    AVLNode *cn = avl_search(customerTree, ord.customer_id);
    AVLNode *sn = avl_search(storeTree,    ord.dark_store_id);
    AVLNode *rn = (ord.delivery_rider_id != -1)
                  ? avl_search(riderTree, ord.delivery_rider_id) : NULL;

    if (!cn || !sn) { printf(" Customer or Store missing.\n"); return; }

    Customer      *cust  = (Customer *)cn->data;
    DarkStore     *store = (DarkStore *)sn->data;
    DeliveryRider *rider = rn ? (DeliveryRider *)rn->data : NULL;

    /* Check balance */
    if (cust->account_balance < ord.amount) {
        printf(" Insufficient balance. Required: Rs.%.2f, Available: Rs.%.2f\n",
               ord.amount, cust->account_balance);
        return;
    }

    /* Deduct from customer */
    cust->account_balance -= ord.amount;
    cust->total_spent     += ord.amount;

    /* Rider gets 10% commission */
    float commission = ord.amount * 0.10f;
    if (rider) {
        rider->total_earnings      += commission;
        rider->monthly_income[11]  += commission;
        rider->orders_last_month++;
        rider->is_available         = 1;
        if (rider->completed_count < MAX_ORDERS)
            rider->completed_orders[rider->completed_count++] = order_id;
    }

    /* Customer: pending → completed */
    for (int i = 0; i < cust->pending_count; i++) {
        if (cust->pending_orders[i] == order_id) {
            cust->pending_orders[i] = cust->pending_orders[--cust->pending_count]; break;
        }
    }
    if (cust->completed_count < MAX_ORDERS)
        cust->completed_orders[cust->completed_count++] = order_id;

    /* Store: pending → completed */
    for (int i = 0; i < store->pending_count; i++) {
        if (store->pending_orders[i] == order_id) {
            store->pending_orders[i] = store->pending_orders[--store->pending_count]; break;
        }
    }
    if (store->completed_count < MAX_ORDERS)
        store->completed_orders[store->completed_count++] = order_id;

    /* Mark order completed — rewrite orders.dat */
    static Order allOrders[5000];
    int orderCnt = 0;
    FILE *fo2 = fopen("orders.dat", "rb");
    if (fo2) {
        Order tmp;
        while (fread(&tmp, sizeof(Order), 1, fo2)) {
            if (tmp.order_id == order_id) tmp.completed = 1;
            allOrders[orderCnt++] = tmp;
        }
        fclose(fo2);
    }
    fo2 = fopen("orders.dat", "wb");
    if (fo2) { fwrite(allOrders, sizeof(Order), orderCnt, fo2); fclose(fo2); }

    printf("\n[OK] Order %d delivered!\n", order_id);
    printf("     Customer paid    : Rs.%.2f\n", ord.amount);
    printf("     Rider commission : Rs.%.2f (10%%)\n", commission);
    printf("     Customer balance : Rs.%.2f\n", cust->account_balance);

    save_all_data();
}

void print_banner(void) {
    printf("\n");
    printf(" --------------------------------------------------------\n");
    printf("      BLINKIT  -  Inventory-led Delivery System         \n");
    printf("      Data Structure: AVL Tree  |  Storage: File I/O    \n");
    printf("---------------------------------------------------------\n");
}

void customer_menu(void) {
    int ch;
    do {
        printf("\n  -- Customer Menu --\n");
        printf("  1. Register Customer\n");
        printf("  2. Update Customer\n");
        printf("  3. Delete Customer\n");
        printf("  4. Display (Descending by Spending)\n");
        printf("  5. Display (By Pincode)\n");
        printf("  0. Back\n");
        printf("  Choice: "); scanf("%d", &ch); flush_input();
        switch (ch) {
            case 1: register_customer();            break;
            case 2: update_customer();              break;
            case 3: delete_customer();              break;
            case 4: display_customers_by_spending();break;
            case 5: display_customers_by_pincode(); break;
            case 0: break;
            default: printf("Invalid.\n");
        }
    } while (ch != 0);
}

void rider_menu(void) {
    int ch;
    do {
        printf("\n  -- Delivery Rider Menu --\n");
        printf("  1. Register Rider\n");
        printf("  2. Delete Rider\n");
        printf("  3. Display (Descending Earnings, Last 3 Months)\n");
        printf("  4. Display (Descending Orders, Last 1 Month)\n");
        printf("  5. Display (By Pincode, then Earnings)\n");
        printf("  0. Back\n");
        printf("  Choice: "); scanf("%d", &ch); flush_input();
        switch (ch) {
            case 1: register_rider();                     break;
            case 2: delete_rider();                       break;
            case 3: display_riders_by_earnings_3months(); break;
            case 4: display_riders_by_orders_1month();    break;
            case 5: display_riders_by_pincode_earnings(); break;
            case 0: break;
            default: printf("Invalid.\n");
        }
    } while (ch != 0);
}

void store_menu(void) {
    int ch;
    do {
        printf("\n  -- Dark Store Menu --\n");
        printf("  1. Register Dark Store\n");
        printf("  2. View Store Inventory\n");
        printf("  0. Back\n");
        printf("  Choice: "); scanf("%d", &ch); flush_input();
        switch (ch) {
            case 1: register_store(); break;
            case 2: {
                int sid;
                printf("  Store ID: "); scanf("%d", &sid); flush_input();
                display_store_inventory(sid);
                break;
            }
            case 0: break;
            default: printf("Invalid.\n");
        }
    } while (ch != 0);
}

void admin_menu(void) {
    int ch;
    do {
        printf("\n  -- Admin / Distance Table --\n");
        printf("  1. Add PinCode Distance\n");
        printf("  2. Show Distance Table\n");
        printf("  0. Back\n");
        printf("  Choice: "); scanf("%d", &ch); flush_input();
        switch (ch) {
            case 1: {
                int p1, p2; float dist;
                printf("  Pincode 1     : "); scanf("%d", &p1);   flush_input();
                printf("  Pincode 2     : "); scanf("%d", &p2);   flush_input();
                printf("  Distance (km) : "); scanf("%f", &dist); flush_input();
                add_distance(p1, p2, dist);
                save_all_data();
                printf("[OK] Distance added.\n");
                break;
            }
            case 2:
                printf("\n  Pincode Distance Table:\n");
                for (int i = 0; i < distCount; i++)
                    printf("    %d <-> %d : %.2f km\n",
                           distTable[i].pin1, distTable[i].pin2, distTable[i].distance);
                break;
            case 0: break;
            default: printf("Invalid.\n");
        }
    } while (ch != 0);
}

void main_menu(void) {
    int ch;
    do {
        print_banner();
        printf("  1.  Customer Operations\n");
        printf("  2.  Delivery Rider Operations\n");
        printf("  3.  Dark Store Operations\n");
        printf("  4.  Place an Order           [Op 8]\n");
        printf("  5.  Deliver an Order         [Op 9]\n");
        printf("  6.  Range Search Customers   [Op 10]\n");
        printf("  7.  Admin / Distance Table\n");
        printf("  8.  Save Data\n");
        printf("  0.  Exit\n");
        printf(" -------------------------------------------------\n");
        printf("  Your Choice: "); scanf("%d", &ch); flush_input();

        switch (ch) {
            case 1: customer_menu();          break;
            case 2: rider_menu();             break;
            case 3: store_menu();             break;
            case 4: place_order();            break;
            case 5: deliver_order();          break;
            case 6: range_search_customers(); break;
            case 7: admin_menu();             break;
            case 8: save_all_data();          break;
            case 0:
                printf("\nGoodbye! Saving data...\n");
                save_all_data();
                break;
            default:
                printf("  Invalid choice. Try again.\n");
        }
    } while (ch != 0);
}


int main(void) {
    printf("Loading saved data...\n");
    load_all_data();
    main_menu();
    avl_free(customerTree);
    avl_free(riderTree);
    avl_free(storeTree);
    return 0;
}