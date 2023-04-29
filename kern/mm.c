/** @file mm.c
 *
 *  @brief memory management functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <common_kern.h>
#include <x86/asm.h>

#include <asm_instr.h>
#include <common.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <sync.h>

/** header of a free page */
typedef struct page_head_s {
    int size;
    pa_t prev;
    pa_t next;
} page_head_t;

/** footer of a free page */
typedef struct page_foot_s {
    char padding[PAGE_SIZE - sizeof(int)];
    int size;
} page_foot_t;

/** total user pages */
static int num_user_pages;

/** bitmap to track page usage */
static unsigned char* user_page_bitmap;

/** bins from 4k to 1m, which is normal request size */
#define NUM_BINS 9
/** minimum size of last bin */
#define LAST_BIN_SIZE (1 << (NUM_BINS - 1))
/** bins for free chunks */
static pa_t bins[NUM_BINS];

mutex_t mm_lock = MUTEX_INIT;

/**
 * @brief is a page inuse
 * @param pn physical page number
 * @return 1 inuse, 0 free
 */
static int is_page_inuse(int pn) {
    return (user_page_bitmap[(pn + 1) / 8] & (1 << ((pn + 1) % 8))) != 0;
}

/**
 * @brief mark a page inuse
 * @param pn physical page number
 */
static void set_page_inuse(int pn) {
    user_page_bitmap[(pn + 1) / 8] |= (1 << ((pn + 1) % 8));
}

/**
 * @brief mark a page free
 * @param pn physical page number
 */
static void set_page_free(int pn) {
    user_page_bitmap[(pn + 1) / 8] &= (~(1 << ((pn + 1) % 8)));
}

/**
 * @brief convert physical address to page number
 * @param pa physical address
 * @return physical page number
 */
static int pa_to_pn(pa_t pa) {
    return (pa - USER_MEM_START) / PAGE_SIZE;
}

/**
 * @brief convert page number to physical address
 * @param pn physical page number
 * @return physical address
 */
static int pn_to_pa(int pn) {
    return pn * PAGE_SIZE + USER_MEM_START;
}

/**
 * @brief find which bin a free chunk belongs to
 * @param size size of chunk
 * @return bin
 */
static int find_bin(int size) {
    if (size >= LAST_BIN_SIZE) {
        return NUM_BINS - 1;
    }
    int i;
    for (i = NUM_BINS - 2; i > 0; i--) {
        if ((size & (1 << i)) != 0) {
            return i;
        }
    }
    return 0;
}

/**
 * @brief add a chunk to bins
 * @param pn physical page number
 * @param size size of chunk
 */
static void add_to_bins(int pn, int size);

/**
 * @brief remove a chunk from bins
 * @param pn physical page number
 */
static void remove_from_bins(int pn);

/**
 * @brief allocate a memory from a bin
 * @param bin bin
 * @param size size of chunk
 * @return address of chunk, BAD_PA on failure
 */
static pa_t alloc_from_bin(pa_t* bin, int size);

void mm_init() {
    num_user_pages = machine_phys_frames() - (USER_MEM_START / PAGE_SIZE);
    assert(num_user_pages > 0);
    user_page_bitmap = smalloc((num_user_pages + 2) / 8);
    assert(user_page_bitmap != NULL);
    memset(user_page_bitmap, 0, (num_user_pages + 2) / 8);
    memset(bins, 0, sizeof(bins));

    set_page_inuse(-1);
    set_page_inuse(num_user_pages);
    add_to_bins(pa_to_pn(USER_MEM_START), num_user_pages);
}

pa_t alloc_user_pages(int num_pages) {
    mutex_lock(&mm_lock);
    int bn = find_bin(num_pages);
    while (bn < NUM_BINS) {
        pa_t result = alloc_from_bin(&bins[bn], num_pages);
        if (result != BAD_PA) {
            mutex_unlock(&mm_lock);
            return result;
        }
        bn++;
    }
    mutex_unlock(&mm_lock);
    return BAD_PA;
}

void free_user_pages(pa_t pa, int num_pages) {
    mutex_lock(&mm_lock);
    int pn = pa_to_pn(pa);
    int final_pn = pn, final_size = num_pages;

    /* merge with prev */
    int prev_inuse = is_page_inuse(pn - 1);
    if (prev_inuse == 0) {
        int old_if = save_clear_if();
        page_foot_t* prev_foot =
            (page_foot_t*)map_phys_page(pn_to_pa(pn - 1), NULL);
        int prev_size = prev_foot->size;
        restore_if(old_if);
        int prev_pn = pn - prev_size;
        remove_from_bins(prev_pn);
        final_pn = prev_pn;
        final_size += prev_size;
    }

    /* merge with next */
    int next_pn = pn + num_pages;
    int next_inuse = is_page_inuse(next_pn);
    if (next_inuse == 0) {
        int old_if = save_clear_if();
        page_head_t* next_head =
            (page_head_t*)map_phys_page(pn_to_pa(next_pn), NULL);
        int next_size = next_head->size;
        restore_if(old_if);
        remove_from_bins(next_pn);
        final_size += next_size;
    }

    add_to_bins(final_pn, final_size);
    mutex_unlock(&mm_lock);
}

va_t map_phys_page(pa_t pa, pa_t* old_pa) {
    pte_t* pte = get_mapped_phys_page_pte();
    if (old_pa != NULL) {
        *old_pa = get_page_base(*pte);
    }
    *pte = make_pte(pa, 0, PTE_SUPERVISOR, PTE_RW, PTE_PRESENT);
    va_t va = get_mapped_phys_page();
    invlpg(va);
    return va;
}

/**
 * @brief insert a chunk to a bin
 * @param bin bin
 * @param this_chunk chunk
 * @param size size of chunk
 */
static void bin_insert(pa_t* bin, pa_t this_chunk, int size) {
    int old_if = save_clear_if();
    if (*bin == BAD_PA) {
        page_head_t* this_head = (page_head_t*)map_phys_page(this_chunk, NULL);
        this_head->next = this_head->prev = this_chunk;
        this_head->size = size;
    } else {
        pa_t head_chunk = *bin;
        page_head_t* head_head = (page_head_t*)map_phys_page(head_chunk, NULL);
        pa_t tail_chunk = head_head->prev;
        head_head->prev = this_chunk;
        page_head_t* this_head = (page_head_t*)map_phys_page(this_chunk, NULL);
        this_head->next = head_chunk;
        this_head->prev = tail_chunk;
        this_head->size = size;
        page_head_t* tail_head = (page_head_t*)map_phys_page(tail_chunk, NULL);
        tail_head->next = this_chunk;
    }
    restore_if(old_if);
    *bin = this_chunk;
}

/**
 * @brief remove a chunk from a bin
 * @param bin bin
 * @param this_chunk chunk
 */
static void bin_delete(pa_t* bin, pa_t this_chunk) {
    int old_if = save_clear_if();
    page_head_t* this_head = (page_head_t*)map_phys_page(this_chunk, NULL);
    pa_t next_chunk = this_head->next;
    if (*bin == this_chunk) {
        *bin = (next_chunk == this_chunk ? BAD_PA : next_chunk);
    }
    if (next_chunk != this_chunk) {
        pa_t prev_chunk = this_head->prev;
        page_head_t* next_head = (page_head_t*)map_phys_page(next_chunk, NULL);
        next_head->prev = prev_chunk;
        page_head_t* prev_head = (page_head_t*)map_phys_page(prev_chunk, NULL);
        prev_head->next = next_chunk;
    }
    restore_if(old_if);
}

static void add_to_bins(int pn, int size) {
    set_page_free(pn);
    set_page_free(pn + (size - 1));
    pa_t this_chunk = pn_to_pa(pn);
    pa_t* bin = &bins[find_bin(size)];

    bin_insert(bin, this_chunk, size);

    int old_if = save_clear_if();
    page_foot_t* this_foot =
        (page_foot_t*)map_phys_page(pn_to_pa(pn + (size - 1)), NULL);
    this_foot->size = size;
    restore_if(old_if);
}

static void remove_from_bins(int pn) {
    pa_t this_chunk = pn_to_pa(pn);
    int old_if = save_clear_if();
    page_head_t* this_head = (page_head_t*)map_phys_page(this_chunk, NULL);
    int size = this_head->size;
    restore_if(old_if);
    pa_t* bin = &bins[find_bin(size)];
    bin_delete(bin, this_chunk);
}

static pa_t alloc_from_bin(pa_t* bin, int size) {
    if (*bin == BAD_PA) {
        return BAD_PA;
    }
    pa_t r_pa = BAD_PA;
    pa_t r_pn = 0;
    pa_t this_chunk = *bin, end = *bin;
    do {
        int old_if = save_clear_if();
        page_head_t* this_head = (page_head_t*)map_phys_page(this_chunk, NULL);
        int this_size = this_head->size;
        pa_t next_chunk = this_head->next;
        restore_if(old_if);
        if (this_size > size) {
            bin_delete(bin, this_chunk);
            r_pa = this_chunk;
            r_pn = pa_to_pn(r_pa);
            add_to_bins(r_pn + size, this_size - size);
            break;
        } else if (this_size == size) {
            bin_delete(bin, this_chunk);
            r_pa = this_chunk;
            r_pn = pa_to_pn(r_pa);
            break;
        }
        this_chunk = next_chunk;
    } while (this_chunk != end);
    if (r_pa == BAD_PA) {
        return BAD_PA;
    }
    set_page_inuse(r_pn);
    set_page_inuse(r_pn + (size - 1));
    return r_pa;
}
