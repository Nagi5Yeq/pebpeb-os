/** @file common.c
 *
 *  @brief common data structures.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */
#include <common.h>

#include <string.h>

void queue_insert_head(queue_t** queue, queue_t* t) {
    if (*queue == NULL) {
        t->next = t->prev = t;
    } else {
        t->next = *queue;
        t->prev = (*queue)->prev;
        (*queue)->prev->next = t;
        (*queue)->prev = t;
    }
    *queue = t;
}

void queue_insert_tail(queue_t** queue, queue_t* t) {
    if (*queue == NULL) {
        t->next = t->prev = t;
        *queue = t;
    } else {
        t->next = *queue;
        t->prev = (*queue)->prev;
        (*queue)->prev->next = t;
        (*queue)->prev = t;
    }
}

queue_t* queue_remove_head(queue_t** queue) {
    queue_t* t = *queue;
    if (t->next == t) {
        *queue = NULL;
    } else {
        t->next->prev = t->prev;
        t->prev->next = t->next;
        *queue = t->next;
    }
    return t;
}

queue_t* queue_remove_tail(queue_t** queue) {
    queue_t* t = (*queue)->prev;
    if (t->next == t) {
        *queue = NULL;
    } else {
        t->next->prev = t->prev;
        t->prev->next = t->next;
    }
    return t;
}

void queue_detach(queue_t** queue, queue_t* t) {
    if (*queue == t) {
        if (t->next == t) {
            *queue = NULL;
        } else {
            *queue = t->next;
        }
    }
    t->next->prev = t->prev;
    t->prev->next = t->next;
}

int heap_init(heap_t* heap) {
    return vector_init(heap, sizeof(heap_node_t), INITIAL_HEAP_SIZE);
}

int heap_insert(heap_t* heap, heap_node_t* node) {
    if (vector_push(heap, node) != 0) {
        return -1;
    }
    int cur = vector_size(heap) - 1;
    while (cur != 0) {
        int parent = ((cur - 1) >> 1);
        heap_node_t* cn = (heap_node_t*)vector_at(heap, cur);
        heap_node_t* pn = (heap_node_t*)vector_at(heap, parent);
        if (cn->key >= pn->key) {
            break;
        }
        heap_node_t t = *pn;
        *pn = *cn;
        *cn = t;
        cur = parent;
    }
    return 0;
}

heap_node_t* heap_peak(heap_t* heap) {
    return (vector_size(heap) == 0 ? NULL : (heap_node_t*)vector_at(heap, 0));
}

void heap_pop(heap_t* heap) {
    int size = vector_size(heap);
    if (size == 0) {
        return;
    }
    *(heap_node_t*)vector_at(heap, 0) =
        *(heap_node_t*)vector_at(heap, size - 1);
    vector_pop(heap);
    size--;
    int cur = 0;
    while (1) {
        int lc = cur * 2 + 1;
        int rc = lc + 1;
        int small = cur;
        heap_node_t* cn = (heap_node_t*)vector_at(heap, cur);
        heap_node_t *ln, *rn, *sn = cn;
        if (lc < size) {
            ln = (heap_node_t*)vector_at(heap, lc);
            if (ln->key < sn->key) {
                small = lc;
                sn = ln;
            }
        }
        if (rc < size) {
            rn = (heap_node_t*)vector_at(heap, rc);
            if (rn->key < sn->key) {
                small = rc;
                sn = rn;
            }
        }
        if (small == cur) {
            break;
        }
        if (small == lc) {
            heap_node_t t = *ln;
            *ln = *cn;
            *cn = t;
            cur = lc;
        } else {
            heap_node_t t = *rn;
            *rn = *cn;
            *cn = t;
            cur = rc;
        }
    }
}

rb_t rb_nil = {
    .color = RB_BLACK,
    .parent = &rb_nil,
    .left = &rb_nil,
    .right = &rb_nil,
};

rb_t* rb_find(rb_t* root, int key) {
    rb_t* p = root;
    while (p != &rb_nil) {
        if (key == p->key) {
            return p;
        }
        p = (key > p->key) ? p->right : p->left;
    }
    return NULL;
}

/**
 * @brief left rotate on a node
 * @param root rbtree root
 * @param node node
 */
static void rb_rotate_left(rb_t** root, rb_t* node) {
    rb_t* r = node->right;
    node->right = r->left;
    if (r->left != &rb_nil) {
        r->left->parent = node;
    }
    r->parent = node->parent;
    if (node->parent == &rb_nil) {
        *root = r;
    } else if (node == node->parent->left) {
        node->parent->left = r;
    } else {
        node->parent->right = r;
    }
    r->left = node;
    node->parent = r;
}

/**
 * @brief right rotate on a node
 * @param root rbtree root
 * @param node node
 */
static void rb_rotate_right(rb_t** root, rb_t* node) {
    rb_t* l = node->left;
    node->left = l->right;
    if (l->right != &rb_nil) {
        l->right->parent = node;
    }
    l->parent = node->parent;
    if (node->parent == &rb_nil) {
        *root = l;
    } else if (node == node->parent->left) {
        node->parent->left = l;
    } else {
        node->parent->right = l;
    }
    l->right = node;
    node->parent = l;
}

/**
 * @brief fixup after insertion
 * @param root rbtree root
 * @param node node
 */
static void rb_fixup(rb_t** root, rb_t* node) {
    while (node->parent->color == RB_RED) {
        rb_t* grand = node->parent->parent;
        if (node->parent == grand->left) {
            rb_t* uncle = grand->right;
            if (uncle->color == RB_RED) {
                node->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                node->parent->parent->color = RB_RED;
                node = node->parent->parent;
            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    rb_rotate_left(root, node);
                }
                node->parent->color = RB_BLACK;
                grand->color = RB_RED;
                rb_rotate_right(root, grand);
            }
        } else {
            rb_t* uncle = grand->left;
            if (uncle->color == RB_RED) {
                node->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                node->parent->parent->color = RB_RED;
                node = node->parent->parent;
            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    rb_rotate_right(root, node);
                }
                node->parent->color = RB_BLACK;
                grand->color = RB_RED;
                rb_rotate_left(root, grand);
            }
        }
    }
    (*root)->color = RB_BLACK;
}

void rb_insert(rb_t** root, rb_t* node) {
    rb_t *p = *root, *parent = &rb_nil;
    while (p != &rb_nil) {
        parent = p;
        p = (node->key > p->key) ? p->right : p->left;
    }
    node->parent = parent;
    if (parent == &rb_nil) {
        *root = node;
    } else if (node->key > parent->key) {
        parent->right = node;
    } else {
        parent->left = node;
    }
    node->left = node->right = &rb_nil;
    node->color = RB_RED;
    rb_fixup(root, node);
}

/**
 * @brief move v to u's position
 * @param root rbtree root
 * @param u u
 * @param v v
 */
static void rb_transplant(rb_t** root, rb_t* u, rb_t* v) {
    if (u->parent == &rb_nil) {
        *root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

rb_t* rb_min(rb_t* root) {
    while (root->left != &rb_nil) {
        root = root->left;
    }
    return root;
}

rb_t* rb_next(rb_t* node) {
    if (node->right != &rb_nil) {
        return rb_min(node->right);
    }
    rb_t* parent = node->parent;
    while (parent != &rb_nil && node == parent->right) {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}

/**
 * @brief fixup after deletion
 * @param root rbtree root
 * @param node node
 */
static void rb_delete_fixup(rb_t** root, rb_t* node) {
    while (node != *root && node->color == RB_BLACK) {
        if (node == node->parent->left) {
            rb_t* r = node->parent->right;
            if (r->color == RB_RED) {
                r->color = RB_BLACK;
                node->parent->color = RB_RED;
                rb_rotate_left(root, node->parent);
                r = node->parent->right;
            }
            if (r->left->color == RB_BLACK && r->right->color == RB_BLACK) {
                r->color = RB_RED;
                node = node->parent;
            } else {
                if (r->right->color == RB_BLACK) {
                    r->left->color = RB_BLACK;
                    r->color = RB_RED;
                    rb_rotate_right(root, r);
                    r = node->parent->right;
                }
                r->color = node->parent->color;
                node->parent->color = RB_BLACK;
                r->right->color = RB_BLACK;
                rb_rotate_left(root, node->parent);
                node = *root;
            }
        } else {
            rb_t* l = node->parent->left;
            if (l->color == RB_RED) {
                l->color = RB_BLACK;
                node->parent->color = RB_RED;
                rb_rotate_right(root, node->parent);
                l = node->parent->left;
            }
            if (l->right->color == RB_BLACK && l->left->color == RB_BLACK) {
                l->color = RB_RED;
                node = node->parent;
            } else {
                if (l->left->color == RB_BLACK) {
                    l->right->color = RB_BLACK;
                    l->color = RB_RED;
                    rb_rotate_left(root, l);
                    l = node->parent->left;
                }
                l->color = node->parent->color;
                node->parent->color = RB_BLACK;
                l->left->color = RB_BLACK;
                rb_rotate_right(root, node->parent);
                node = *root;
            }
        }
    }
    node->color = RB_BLACK;
}

void rb_delete(rb_t** root, rb_t* node) {
    rb_t *x, *y = node;
    int orig_color = y->color;
    if (node->left == &rb_nil) {
        x = node->right;
        rb_transplant(root, node, x);
    } else if (node->right == &rb_nil) {
        x = node->left;
        rb_transplant(root, node, x);
    } else {
        y = rb_min(node->right);
        orig_color = y->color;
        x = y->right;
        if (y->parent == node) {
            x->parent = y;
        } else {
            rb_transplant(root, y, y->right);
            y->right = node->right;
            y->right->parent = y;
        }
        rb_transplant(root, node, y);
        y->left = node->left;
        y->left->parent = y;
        y->color = node->color;
    }
    if (orig_color == RB_BLACK) {
        rb_delete_fixup(root, x);
    }
}

/** do not shink small vectors as this do not bring big benefit */
#define VECTOR_MINIMUM_SIZE 8
/** don't shink too fast so we divide by 3 */
#define VECTOR_SHINK_RATIO 3

int vector_init(vector_t* v, int elem_size, int init_size) {
    v->elem_size = elem_size;
    v->size = init_size;
    v->array = (char*)smalloc(init_size * elem_size);
    v->current = 0;
    return (v->array == NULL ? -1 : 0);
}

int vector_size(vector_t* v) {
    return v->current;
}

int vector_push(vector_t* v, void* elem) {
    if (v->current == v->size) {
        char* result = (char*)smalloc(v->size * v->elem_size * 2);
        if (result == NULL) {
            return -1;
        }
        memcpy(result, v->array, v->current * v->elem_size);
        sfree(v->array, v->size * v->elem_size);
        v->size <<= 1;
        v->array = result;
    }
    memcpy(v->array + v->current * v->elem_size, elem, v->elem_size);
    v->current++;
    return 0;
}

/**
 * @brief check if a vector should shrink
 * @param v vector
 */
static void vector_try_shink(vector_t* v) {
    if (v->current < v->size / VECTOR_SHINK_RATIO) {
        if ((v->size / 2) < VECTOR_MINIMUM_SIZE) {
            return;
        }
        char* result = (char*)smalloc((v->size / 2) * v->elem_size);
        if (result == NULL) {
            return;
        }
        memcpy(result, v->array, v->current * v->elem_size);
        sfree(v->array, v->size * v->elem_size);
        v->size >>= 1;
        v->array = result;
    }
}

void vector_pop(vector_t* v) {
    v->current--;
    vector_try_shink(v);
}

void vector_remove(vector_t* v, int index) {
    memmove(v->array + index * v->elem_size,
            v->array + (index + 1) * v->elem_size,
            (v->current - index - 1) * v->elem_size);
    v->current--;
    vector_try_shink(v);
}

void* vector_at(vector_t* v, int index) {
    return (void*)(v->array + index * v->elem_size);
}

void vector_free(vector_t* v) {
    sfree(v->array, v->size * v->elem_size);
}
