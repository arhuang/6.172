/**
 * Copyright (c) 2015 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "./allocator_interface.h"
#include "./memlib.h"

// Don't call libc malloc!
#define malloc(...) (USE_MY_MALLOC)
#define free(...) (USE_MY_FREE)
#define realloc(...) (USE_MY_REALLOC)

// We use a variant of binned free lists and coalescing.
// Free blocks can have different sizes. A free block of size k is put into
// free_list i if k in [SIZEi, SIZE(i+1)-1], and into free_list if it is too large.
//
// If p is the start location of a block, then we store its size right before p.
// We also store a mark right after the block. If p is free, then the mark stores
// the size of the block. Otherwise, the mark is set to NON_FREE_BLOCK.
//
// We use doubly-linked list for free lists because this enables easier remove.
// We use data stored before and after a block (especially the mark) to perform
// coalescing.


// free_list structure
// size = SIZE_T_SIZE*4 = 32 bytes
typedef struct free_list_t {
  size_t size;  // size of the free block
  struct free_list_t *next, *prev;  // prev and next in the free list
  struct free_list_t **head;  // head of the free list
} free_list_t;

// free_list(i) is for free blocks of size in [SIZEi, SIZE(i+1)-1].
// free_list is for free blocks of size larger than SIZE9.
static free_list_t *free_list;
static free_list_t *free_list0;
static free_list_t *free_list1;
static free_list_t *free_list2;
static free_list_t *free_list3;
static free_list_t *free_list4;
static free_list_t *free_list5;
static free_list_t *free_list6;
static free_list_t *free_list7;
static free_list_t *free_list8;

// Because next, prev, head of a free_list_t is stored inside the block,
// we need to ensure SIZE0 >= sizeof(free_list_t) - sizeof(SIZE_T_SIZE).
// SIZEs are generated using a similar way as Fibonacci.
#ifndef SIZE0
  #define SIZE0 24
#endif
#ifndef SIZE1
  #define SIZE1 56
#endif
#ifndef SIZE2
  #define SIZE2 88
#endif
#ifndef SIZE3
  #define SIZE3 152
#endif
#ifndef SIZE4
  #define SIZE4 248
#endif
#ifndef SIZE5
  #define SIZE5 408
#endif
#ifndef SIZE6
  #define SIZE6 664
#endif
#ifndef SIZE7
  #define SIZE7 1080
#endif
#ifndef SIZE8
  #define SIZE8 1752
#endif
#ifndef SIZE9
  #define SIZE9 2840
#endif

// recover start location from free_list_t*
#define FREE_LIST_T_TO_PTR(p) ((void*)(p) + SIZE_T_SIZE)
// FREE_MARK(p) is p->size for a free block and NON_FREE_BLOCK for a non-free block
#define FREE_MARK(p) (*(size_t*)((void*)(p) + SIZE_T_SIZE + p->size))
// mark for a non-free block
#define NON_FREE_BLOCK 0

// real_heap_hi is the highest address that is used by a non-free block.
// i.e. free blocks at the end of heap is ignored.
// heap_rem = mem_heap_hi() - real_heap_hi.
static void * real_heap_hi;
static size_t heap_rem;

// All blocks must have a specified minimum alignment.
// The alignment requiheap_rement (from config.h) is >= 8 bytes.
#ifndef ALIGNMENT
#define ALIGNMENT 8
#endif

// Rounds up to the nearest multiple of ALIGNMENT.
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

// The smallest aligned size that will hold a size_t value.
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// check - This checks our invariant that the size_t header before every
// block points to either the beginning of the next block, or the end of the
// heap.
int my_check() {
  char *p;
  char *lo = (char*)mem_heap_lo() + SIZE_T_SIZE;
  char *hi = real_heap_hi + 1;
  size_t size = 0;

  p = lo;
  while (lo <= p && p < hi) {
    size = ALIGN(*(size_t*)p + SIZE_T_SIZE + SIZE_T_SIZE);
    p += size;
  }

  if (p != hi) {
    printf("Bad headers did not end at heap_hi!\n");
    printf("heap_lo: %p, heap_hi: %p, size: %lu, p: %p\n", lo, hi, size, p);
    return -1;
  }

  return 0;
}

// reset real_heap_hi to be mem_heap_hi()
void reset_real_heap_hi() {
  real_heap_hi = mem_heap_hi();
  heap_rem = 0;
}

// same as mem_sbrk, but handle the case where heap_rem is nonzero
void * my_sbrk(size_t size) {
  void * p = real_heap_hi + 1;
  // check if we need to call mem_sbrk
  if (heap_rem >= size) {
    // if heap_rem is large enough, only need to change real_heap_hi
    real_heap_hi += size;
    heap_rem -= size;
  } else {
    // otherwise we need to call mem_sbrk
    mem_sbrk(size - heap_rem);
    reset_real_heap_hi();
  }
  return p;
}

// init - Initialize the malloc package.  Called once before any other
// calls are made.
int my_init() {
  // initialize all free lists
  free_list = NULL;
  free_list0 = NULL;
  free_list1 = NULL;
  free_list2 = NULL;
  free_list3 = NULL;
  free_list4 = NULL;
  free_list5 = NULL;
  free_list6 = NULL;
  free_list7 = NULL;
  free_list8 = NULL;

  // reset real_heap_hi
  reset_real_heap_hi();
  // Because in coalescing, we look at the entry before a block, we need
  // to prevent going below mem_heap_lo.
  my_sbrk(SIZE_T_SIZE);
  *(size_t*)(real_heap_hi + 1 - SIZE_T_SIZE) = NON_FREE_BLOCK;
  return 0;
}

// remove a block from free list
void remove_from_free_list(free_list_t* p) {
  // if p is head of a free list, change the head
  if (p == *(p->head)) {
    *(p->head) = p->next;
  }
  if (p->next != NULL) {
    p->next->prev = p->prev;
  }
  if (p->prev != NULL) {
    p->prev->next = p->next;
  }
  // set FREE_MARK(p)
  FREE_MARK(p) = NON_FREE_BLOCK;
}

// add a block to free list
void add_to_free_list(free_list_t* p) {
  // the free list p should be in
  free_list_t** free_list_head;

  if (p->size < SIZE1) free_list_head = &free_list0; else
  if (p->size < SIZE2) free_list_head = &free_list1; else
  if (p->size < SIZE3) free_list_head = &free_list2; else
  if (p->size < SIZE4) free_list_head = &free_list3; else
  if (p->size < SIZE5) free_list_head = &free_list4; else
  if (p->size < SIZE6) free_list_head = &free_list5; else
  if (p->size < SIZE7) free_list_head = &free_list6; else
  if (p->size < SIZE8) free_list_head = &free_list7; else
  if (p->size < SIZE9) free_list_head = &free_list8; else
    free_list_head = &free_list;
  // set fields of p
  p->head = free_list_head;
  p->next = *free_list_head;
  p->prev = NULL;
  if (p->next != NULL) p->next->prev = p;
  *free_list_head = p;
  // set FREE_MARK(p)
  FREE_MARK(p) = p->size;
}

// perform coalescing
// repeatedly check whether the current block can be merged with the previous
// block or the next block.
void coalesce(free_list_t* p) {
  bool done = false;
  while (!done) {
    done = true;
    // check FREE_MARK of previous block
    size_t prev_size = *(size_t*)((void*)p - SIZE_T_SIZE);
    if (prev_size != NON_FREE_BLOCK) {
      // merge with previous block
      done = false;
      free_list_t* prev = (free_list_t*)((void*)p - SIZE_T_SIZE - SIZE_T_SIZE - prev_size);
      remove_from_free_list(prev);
      remove_from_free_list(p);
      prev->size += p->size + SIZE_T_SIZE + SIZE_T_SIZE;
      add_to_free_list(prev);
      p = prev;
    }
    if ((void*)p + SIZE_T_SIZE + SIZE_T_SIZE + p->size < real_heap_hi) {
      free_list_t* next = (free_list_t*)((void*)p + SIZE_T_SIZE + SIZE_T_SIZE + p->size);
      // check FREE_MARK of next block
      if (FREE_MARK(next) != NON_FREE_BLOCK) {
        // merge with next block
        done = false;
        remove_from_free_list(next);
        remove_from_free_list(p);
        p->size += next->size + SIZE_T_SIZE + SIZE_T_SIZE;
        add_to_free_list(p);
      }
    }
  }
}

// alloc from free lists. If fail, return NULL. If success, return start
// of the block.
void * alloc_free_list(size_t size) {
  free_list_t* ptr = NULL;
  if (size <= SIZE0 && free_list0 != NULL) ptr = free_list0; else
  if (size <= SIZE1 && free_list1 != NULL) ptr = free_list1; else
  if (size <= SIZE2 && free_list2 != NULL) ptr = free_list2; else
  if (size <= SIZE3 && free_list3 != NULL) ptr = free_list3; else
  if (size <= SIZE4 && free_list4 != NULL) ptr = free_list4; else
  if (size <= SIZE5 && free_list5 != NULL) ptr = free_list5; else
  if (size <= SIZE6 && free_list6 != NULL) ptr = free_list6; else
  if (size <= SIZE7 && free_list7 != NULL) ptr = free_list7; else
  if (size <= SIZE8 && free_list8 != NULL) ptr = free_list8; else {
    // go over the out-of-range free list.
    for (free_list_t* p = free_list; p != NULL; p = p->next) {
      if (p->size >= size) {
        ptr = p;
        break;
      }
    }
  }

  if (ptr == NULL) return NULL;
  remove_from_free_list(ptr);
  // check whether we can create a new free block in the unused space
  if (ptr->size - size >= SIZE0 + SIZE_T_SIZE + SIZE_T_SIZE) {
    // start position of free_list_t* of the new block
    free_list_t* q = (free_list_t*)((void*)(ptr) + SIZE_T_SIZE + SIZE_T_SIZE + size);
    // size of new block = total size (ptr->size)
    // minus size used by current block (size)
    // minus FREE_MARK of ptr (SIZE_T_SIZE)
    // minus size field of q (SIZE_T_SIZE)
    q->size = ptr->size - size - SIZE_T_SIZE - SIZE_T_SIZE;
    add_to_free_list(q);
    ptr->size = size;
    // set FREE_MARK
    FREE_MARK(ptr) = NON_FREE_BLOCK;
  }
  return FREE_LIST_T_TO_PTR(ptr);
}

//  malloc - Allocate a block by incheap_rementing the brk pointer.
//  Always allocate a block whose size is a multiple of the alignment.
void * my_malloc(size_t size) {
  // always use aligned size
  size = ALIGN(size);
  // if size is too small, set it to be SIZE0
  if (size <= SIZE0) size = SIZE0;
  void *p = alloc_free_list(size);
  // if we successfully allocated from free list, return
  if (p != NULL) return p;

  // We allocate a little bit of extra memory so that we can store the
  // size of the block we've allocated.  Take a look at realloc to see
  // one example of a place where this can come in handy.
  int aligned_size = ALIGN(size + SIZE_T_SIZE + SIZE_T_SIZE);

  // Expands the heap by the given number of bytes and returns a pointer to
  // the newly-allocated area.  This is a slow call, so you will want to
  // make sure you don't wind up calling it on every malloc.
  p = my_sbrk(aligned_size);

  if (p == (void *)-1) {
    // Whoops, an error of some sort occurred.  We return NULL to let
    // the client code know that we weren't able to allocate memory.
    return NULL;
  } else {
    // We store the size of the block we've allocated in the first
    // SIZE_T_SIZE bytes.
    *(size_t*)p = size;
    // set FREE_MARK to be NON_FREE_BLOCK
    *(size_t*)(p + SIZE_T_SIZE + size) = NON_FREE_BLOCK;

    // Then, we return a pointer to the rest of the block of memory,
    // which is at least size bytes long.  We have to cast to uint8_t
    // before we try any pointer arithmetic because voids have no size
    // and so the compiler doesn't know how far to move the pointer.
    // Since a uint8_t is always one byte, adding SIZE_T_SIZE after
    // casting advances the pointer by SIZE_T_SIZE bytes.
    return p + SIZE_T_SIZE;
  }
}

// make sure that real_heap_hi is the end of a non-free block
void collapse() {
  // FREE_MARK of the block at end
  size_t size = *(size_t*)(real_heap_hi + 1 - SIZE_T_SIZE);
  if (size != NON_FREE_BLOCK) {
    real_heap_hi -= size + SIZE_T_SIZE + SIZE_T_SIZE;
    heap_rem += size + SIZE_T_SIZE + SIZE_T_SIZE;
    remove_from_free_list((free_list_t*)(real_heap_hi + 1));
  }
  // because we do coalescing, we do not need to collapse repeatedly
}

// free - put into free list and then coalesce; handle special cases where
// blocks are at the end of heap
void my_free(void *ptr) {
  free_list_t* q = (free_list_t*)(ptr - SIZE_T_SIZE);
  // if the block to be freed is at the end, change real_heap_hi
  if (ptr + q->size + SIZE_T_SIZE - 1 == real_heap_hi) {
    real_heap_hi = ptr - SIZE_T_SIZE - 1;
    heap_rem += q->size + SIZE_T_SIZE + SIZE_T_SIZE;
    // the previous block may be a free block, check that
    collapse();
    return;
  }
  // otherwise put into free list and coalesce
  add_to_free_list(q);
  coalesce(q);
}

// realloc - Implemented simply in terms of malloc and free; handle special
// cases where blocks are at the end of heap
void * my_realloc(void *ptr, size_t size) {
  // always use aligned size
  size = ALIGN(size);
  // Get the size of the old block of memory.  Take a peek at my_malloc(),
  // where we stashed this in the SIZE_T_SIZE bytes directly before the
  // address we returned.  Now we can back up by that many bytes and read
  // the size.
  size_t copy_size = *(size_t*)(ptr - SIZE_T_SIZE);

  // if block to be reallocated is at the end of heap, do not need to move
  if (ptr + copy_size - 1 + SIZE_T_SIZE == real_heap_hi) {
    if (size > copy_size) {
      // size becomes larger, need sbrk
      my_sbrk(size - copy_size);
    } else {
      // size becomes smaller, modify real_heap_hi
      real_heap_hi -= copy_size - size;
      heap_rem += copy_size - size;
    }
    // set size
    *(size_t*)(ptr - SIZE_T_SIZE) = size;
    // set FREE_MARK
    *(size_t*)(ptr + size) = NON_FREE_BLOCK;
    return ptr;
  }

  // Allocate a new chunk of memory, and fail if that allocation fails.
  void *newptr = my_malloc(size);
  if (NULL == newptr)
    return NULL;

  // If the new block is smaller than the old one, we have to stop copying
  // early so that we don't write off the end of the new block of memory.
  if (size < copy_size)
    copy_size = size;

  // This is a standard library call that performs a simple memory copy.
  memcpy(newptr, ptr, copy_size);

  // Release the old block.
  my_free(ptr);

  // Return a pointer to the new block.
  return newptr;
}

// call mem_reset_brk.
void my_reset_brk() {
  mem_reset_brk();
}

// call mem_heap_lo
void * my_heap_lo() {
  return mem_heap_lo();
}

// call mem_heap_hi
void * my_heap_hi() {
  return mem_heap_hi();
}
