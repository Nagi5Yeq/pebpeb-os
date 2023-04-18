/** @file common.h
 *
 *  @brief common data structure
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdlib.h>

/**
 * a queue node
 */
typedef struct queue_s {
    struct queue_s* next;
    struct queue_s* prev;
} queue_t;

/**
 * @brief insert a node to the head of a queue
 * @param queue queue
 * @param t the node
 */
void queue_insert_head(queue_t** queue, queue_t* t);

/**
 * @brief insert a node to the tail of a queue
 * @param queue queue
 * @param t the node
 */
void queue_insert_tail(queue_t** queue, queue_t* t);

/**
 * @brief remove a node from the head of a queue
 * @param queue queue
 * @return the node removed
 */
queue_t* queue_remove_head(queue_t** queue);

/**
 * @brief remove a node from the tail of a queue
 * @param queue queue
 * @return the node removed
 */
queue_t* queue_remove_tail(queue_t** queue);

/**
 * @brief detach a node from the a queue
 * @param queue queue
 * @param t the node to be detached
 */
void queue_detach(queue_t** queue, queue_t* t);

/** get the enclose object of a queue node */
#define queue_data(queue, type, member) \
    (type*)((char*)queue - offsetof(type, member))

/**
 * a rbtree node
 */
typedef struct rb_s {
    int key;
    int color;
    struct rb_s* parent;
    struct rb_s* left;
    struct rb_s* right;
} rb_t;

/** this color can have childs of any color */
#define RB_BLACK 0
/** this color can only have childs of the color mentioned above */
#define RB_RED 1

/** used to represent NULL in rbtrees */
extern rb_t rb_nil;

/**
 * @brief find a node with key
 * @param root rbtree root
 * @param key key to find
 * @return rb_nil if not found, else the node
 */
rb_t* rb_find(rb_t* root, int key);

/**
 * @brief insert a node to rbtree
 * @param root rbtree root
 * @param node the node to insert
 */
void rb_insert(rb_t** root, rb_t* node);

/**
 * @brief delete a node from rbtree
 * @param root rbtree root
 * @param node the node to delete
 */
void rb_delete(rb_t** root, rb_t* node);

/**
 * @brief find the node with minimum key in rbtree
 * @param root rbtree root
 * @return rb_nil if rbtree is empty, otherwise the node
 */
rb_t* rb_min(rb_t* root);

/**
 * @brief find the node with the samllest key which is greater than node's key
 * @param root rbtree root
 * @param node the node to compare
 * @return rb_nil if no such node exist, otherwise the node
 */
rb_t* rb_next(rb_t* node);

/** get the enclose object of a rbtree node */
#define rb_data(rb, type, member) (type*)((char*)rb - offsetof(type, member))

/**
 * a vector
 */
typedef struct vector_s {
    int current;
    int size;
    int elem_size;
    char* array;
} vector_t;

/**
 * @brief initializa a vector
 * @param v the vector
 * @param elem_size size of element
 * @param init_size initial size of vector
 * @return -1 on failure, 0 on success
 */
int vector_init(vector_t* v, int elem_size, int init_size);

/**
 * @brief get the size of a vector
 * @param v the vector
 * @return size
 */
int vector_size(vector_t* v);

/**
 * @brief push an element to the back of v
 * @param v vector
 * @param elem element
 * @return -1 on failure, 0 on success
 */
int vector_push(vector_t* v, void* elem);

/**
 * @brief remove an element from the back of v
 * @param v vector
 */
void vector_pop(vector_t* v);

/**
 * @brief remove an element at index index
 * @param v vector
 * @param index index
 */
void vector_remove(vector_t* v, int index);

/**
 * @brief return a pointer to the element at index in v
 * @param v vector
 * @param index index
 * @return the element
 */
void* vector_at(vector_t* v, int index);

/**
 * @brief free a vector's resource
 * @param v vector
 */
void vector_free(vector_t* v);

/**
 * a node in heap
 */
typedef struct heap_node_s {
    int key;
    void* value;
} heap_node_t;

/**
 * a minimum heap
 */
typedef vector_t heap_t;

/**
 * heap is only used for timer and we choose this size because 8 is a common
 * number in computer science and no other size make more sense than this one
 */
#define INITIAL_HEAP_SIZE (8)

/**
 * @brief initializa a heap
 * @param heap the heap
 * @return -1 on failure, 0 on success
 */
int heap_init(heap_t* heap);

/**
 * @brief insert a node into heap
 * @param heap the heap
 * @param node the node
 * @return -1 on failure, 0 on success
 */
int heap_insert(heap_t* heap, heap_node_t* node);

/**
 * @brief peak the top of a heap
 * @param heap the heap
 * @return top node of the heap or NULL if heap is empty
 */
heap_node_t* heap_peak(heap_t* heap);

/**
 * @brief remove the top node from heap
 * @param heap the heap
 */
void heap_pop(heap_t* heap);

#endif
