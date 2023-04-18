/**
 * @file rb_tcb.c
 * @author Hanjie Wu (hanjiew)
 * @brief rbtree for tcbs
 * @date 2023-02-22
 *
 */

#include <stdbool.h>
#include <stdlib.h>

#include <thr_internals.h>

/**
 * @brief rbtree color
 */
#define RB_BLACK 0
#define RB_RED 1

/**
 * @brief sentinel node
 */
static tcb_t rb_nil = {
    .color = RB_BLACK,
    .parent = &rb_nil,
    .left = &rb_nil,
    .right = &rb_nil,
};

/**
 * @brief rbtree root
 */
static tcb_t* root = &rb_nil;

/**
 * @brief find a tcb by tid
 */
tcb_t* rb_find_tcb(int tid) {
    tcb_t* p = root;
    while (p != &rb_nil) {
        if (tid == p->tid) {
            return p;
        }
        p = (tid > p->tid) ? p->right : p->left;
    }
    return NULL;
}

/**
 * @brief left rotate on a node
 */
static void rb_rotate_left(tcb_t** root, tcb_t* block) {
    tcb_t* r = block->right;
    block->right = r->left;
    if (r->left != &rb_nil) {
        r->left->parent = block;
    }
    r->parent = block->parent;
    if (block->parent == &rb_nil) {
        *root = r;
    } else if (block == block->parent->left) {
        block->parent->left = r;
    } else {
        block->parent->right = r;
    }
    r->left = block;
    block->parent = r;
}

/**
 * @brief right rotate on a node
 */
static void rb_rotate_right(tcb_t** root, tcb_t* block) {
    tcb_t* l = block->left;
    block->left = l->right;
    if (l->right != &rb_nil) {
        l->right->parent = block;
    }
    l->parent = block->parent;
    if (block->parent == &rb_nil) {
        *root = l;
    } else if (block == block->parent->left) {
        block->parent->left = l;
    } else {
        block->parent->right = l;
    }
    l->right = block;
    block->parent = l;
}

/**
 * @brief fixup after insertion
 */
static void rb_fixup(tcb_t** root, tcb_t* block) {
    while (block->parent->color == RB_RED) {
        tcb_t* grand = block->parent->parent;
        if (block->parent == grand->left) {
            tcb_t* uncle = grand->right;
            if (uncle->color == RB_RED) {
                block->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                block->parent->parent->color = RB_RED;
                block = block->parent->parent;
            } else {
                if (block == block->parent->right) {
                    block = block->parent;
                    rb_rotate_left(root, block);
                }
                block->parent->color = RB_BLACK;
                grand->color = RB_RED;
                rb_rotate_right(root, grand);
            }
        } else {
            tcb_t* uncle = grand->left;
            if (uncle->color == RB_RED) {
                block->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;
                block->parent->parent->color = RB_RED;
                block = block->parent->parent;
            } else {
                if (block == block->parent->left) {
                    block = block->parent;
                    rb_rotate_right(root, block);
                }
                block->parent->color = RB_BLACK;
                grand->color = RB_RED;
                rb_rotate_left(root, grand);
            }
        }
    }
    (*root)->color = RB_BLACK;
}

/**
 * @brief insert a node
 */
static void rb_insert(tcb_t** root, tcb_t* block) {
    tcb_t *p = *root, *parent = &rb_nil;
    while (p != &rb_nil) {
        parent = p;
        p = (block->tid > p->tid) ? p->right : p->left;
    }
    block->parent = parent;
    if (parent == &rb_nil) {
        *root = block;
    } else if (block->tid > parent->tid) {
        parent->right = block;
    } else {
        parent->left = block;
    }
    block->left = block->right = &rb_nil;
    block->color = RB_RED;
    rb_fixup(root, block);
}

/**
 * @brief move node v to u
 */
static void rb_transplant(tcb_t** root, tcb_t* u, tcb_t* v) {
    if (u->parent == &rb_nil) {
        *root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

/**
 * @brief find minimum node
 */
static tcb_t* rb_min(tcb_t* block) {
    while (block->left != &rb_nil) {
        block = block->left;
    }
    return block;
}

/**
 * @brief fixup after deletion
 */
static void rb_delete_fixup(tcb_t** root, tcb_t* block) {
    while (block != *root && block->color == RB_BLACK) {
        if (block == block->parent->left) {
            tcb_t* r = block->parent->right;
            if (r->color == RB_RED) {
                r->color = RB_BLACK;
                block->parent->color = RB_RED;
                rb_rotate_left(root, block->parent);
                r = block->parent->right;
            }
            if (r->left->color == RB_BLACK && r->right->color == RB_BLACK) {
                r->color = RB_RED;
                block = block->parent;
            } else {
                if (r->right->color == RB_BLACK) {
                    r->left->color = RB_BLACK;
                    r->color = RB_RED;
                    rb_rotate_right(root, r);
                    r = block->parent->right;
                }
                r->color = block->parent->color;
                block->parent->color = RB_BLACK;
                r->right->color = RB_BLACK;
                rb_rotate_left(root, block->parent);
                block = *root;
            }
        } else {
            tcb_t* l = block->parent->left;
            if (l->color == RB_RED) {
                l->color = RB_BLACK;
                block->parent->color = RB_RED;
                rb_rotate_right(root, block->parent);
                l = block->parent->left;
            }
            if (l->right->color == RB_BLACK && l->left->color == RB_BLACK) {
                l->color = RB_RED;
                block = block->parent;
            } else {
                if (l->left->color == RB_BLACK) {
                    l->right->color = RB_BLACK;
                    l->color = RB_RED;
                    rb_rotate_left(root, l);
                    l = block->parent->left;
                }
                l->color = block->parent->color;
                block->parent->color = RB_BLACK;
                l->left->color = RB_BLACK;
                rb_rotate_right(root, block->parent);
                block = *root;
            }
        }
    }
    block->color = RB_BLACK;
}

/**
 * @brief delete a node
 */
static void rb_delete(tcb_t** root, tcb_t* block) {
    tcb_t *x, *y = block;
    int orig_color = y->color;
    if (block->left == &rb_nil) {
        x = block->right;
        rb_transplant(root, block, x);
    } else if (block->right == &rb_nil) {
        x = block->left;
        rb_transplant(root, block, x);
    } else {
        y = rb_min(block->right);
        orig_color = y->color;
        x = y->right;
        if (y->parent == block) {
            x->parent = y;
        } else {
            rb_transplant(root, y, y->right);
            y->right = block->right;
            y->right->parent = y;
        }
        rb_transplant(root, block, y);
        y->left = block->left;
        y->left->parent = y;
        y->color = block->color;
    }
    if (orig_color == RB_BLACK) {
        rb_delete_fixup(root, x);
    }
}

/**
 * @brief add a tcb to rbtree
 */
void rb_insert_tcb(tcb_t* tcb) {
    rb_insert(&root, tcb);
}

/**
 * @brief remove a tcb from rbtree
 */
void rb_delete_tcb(tcb_t* tcb) {
    rb_delete(&root, tcb);
}
