/*
 * validator.h - 6.172 Malloc Validator
 *
 * Validates a malloc/free/realloc implementation.
 *
 * Copyright (c) 2010, R. Bryant and D. O'Hallaron, All rights reserved.
 * May not be used, modified, or copied without permission.
 */

#ifndef MM_VALIDATOR_H
#define MM_VALIDATOR_H

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "./config.h"
#include "./mdriver.h"
#include "./memlib.h"

// Returns true if p is R_ALIGNMENT-byte aligned
#if (__WORDSIZE == 64 )
#define IS_ALIGNED(p)  ((((uint64_t)(p)) % R_ALIGNMENT) == 0)
#else
#define IS_ALIGNED(p)  ((((uint32_t)(p)) % R_ALIGNMENT) == 0)
#endif

static size_t min(size_t a, size_t b) {
  if (a > b) return b;
  return a;
}

static size_t max(size_t a, size_t b) {
  if (a > b) return a;
  return b;
}

// Range list data structure

// Records the extent of each block's payload
typedef struct range_t {
  char *lo;              // low payload address
  char *hi;              // high payload address
  struct range_t *next;  // next list element
} range_t;

// The following routines manipulate the range list, which keeps
// track of the extent of every allocated block payload. We use the
// range list to detect any overlapping allocated blocks.


// add_range - As directed by request opnum in trace tracenum,
// we've just called the student's malloc to allocate a block of
// size bytes at addr lo. After checking the block for correctness,
// we create a range struct for this block and add it to the range list.
static int add_range(const malloc_impl_t *impl, range_t **ranges, char *lo,
    int size, int tracenum, int opnum) {
  char *hi = lo + size - 1;
  range_t *p;

  // You can use this as a buffer for writing messages with sprintf.
  // char msg[MAXLINE];

  assert(size > 0);

  // Payload addresses must be R_ALIGNMENT-byte aligned
  if (!IS_ALIGNED(lo)) {
    malloc_error(tracenum, opnum, "Payload addresses must be R_ALIGNMENT-byte aligned\n");
    return 0;
  }

  // The payload must lie within the extent of the heap
  if (!(impl->heap_lo() <= (void*) lo && (void*) hi <= impl->heap_hi())) {
    malloc_error(tracenum, opnum, "The payload must lie within the extent of the heap\n");
    return 0;
  }

  // The payload must not overlap any other payloads
  for (p = *ranges; p != NULL; p = p->next) {
    if (!(p->hi < lo || hi < p->lo)) {
      malloc_error(tracenum, opnum, "The payload must not overlap any other payloads\n");
      return 0;
    }
  }

  // Everything looks OK, so remember the extent of this block by creating a
  // range struct and adding it the range list.
  p = malloc(sizeof(range_t));
  p->lo = lo;
  p->hi = hi;
  p->next = *ranges;
  *ranges = p;

  return 1;
}

// remove_range - Free the range record of block whose payload starts at lo
static void remove_range(range_t **ranges, char *lo) {
  // Iterate the linked list until you find the range with a matching lo
  // payload and remove it.  Remember to properly handle the case where the
  // payload is in the first node, and to free the node after unlinking it.
  if (*ranges == NULL) return;
  range_t *p = *ranges;

  // check if payload is head
  if (p->lo == lo) {
    *ranges = p->next;
    free(p);
    return;
  }

  // go over the list
  for (range_t *prevp = *ranges; prevp != NULL; prevp = prevp->next) {
    p = prevp->next;
    if (p != NULL && p->lo == lo) {
      prevp->next = p->next;
      free(p);
      return;
    }
  }
}

// clear_ranges - free all of the range records for a trace
static void clear_ranges(range_t **ranges) {
  range_t *p;
  range_t *pnext;

  for (p = *ranges; p != NULL; p = pnext) {
    pnext = p->next;
    free(p);
  }
  *ranges = NULL;
}

#define MAX_NUM_ENTRY 100
#define SMALL_PRIME 137

// fill region p of size size with random numbers
static void fill_random(char* p, size_t size) {
  // for efficiency, only fill the first and last MAX_NUM_ENTRY entries
  size_t left = min(MAX_NUM_ENTRY, size);
  size_t right = max(left, size - MAX_NUM_ENTRY);
  for (size_t j = 0; j < left; j++) {
    *(p+j) = (char) rand();
  }
  for (size_t j = right; j < size; j++) {
    *(p+j) = (char) rand();
  }
}

// compute a checksum for region p of size size
static char checksum(char* p, size_t size) {
  // for efficiency, only consider the first and last MAX_NUM_ENTRY entries
  size_t left = min(MAX_NUM_ENTRY, size);
  size_t right = max(left, size - MAX_NUM_ENTRY);
  char ans = 0;
  // multiply by SMALL_PRIME so that ans is not simply the sum
  for (size_t j = 0; j < left; j++) {
    ans *= SMALL_PRIME;
    ans += *(p + j);
  }
  for (size_t j = right; j < size; j++) {
    ans *= SMALL_PRIME;
    ans += *(p + j);
  }
  return ans;
}


// eval_mm_valid - Check the malloc package for correctness
int eval_mm_valid(const malloc_impl_t *impl, trace_t *trace, int tracenum) {
  int i = 0;
  int index = 0;
  int size = 0;
  int oldsize = 0;
  char *newp = NULL;
  char *oldp = NULL;
  char *p = NULL;
  range_t *ranges = NULL;

  // Reset the heap.
  impl->reset_brk();

  // Call the mm package's init function
  if (impl->init() < 0) {
    malloc_error(tracenum, 0, "impl init failed.");
    return 0;
  }

  // Interpret each operation in the trace in order
  for (i = 0; i < trace->num_ops; i++) {
    index = trace->ops[i].index;
    size = trace->ops[i].size;
    switch (trace->ops[i].type) {
      case ALLOC:  // malloc

        // Call the student's malloc
        if ((p = (char *) impl->malloc(size)) == NULL) {
          malloc_error(tracenum, i, "impl malloc failed.");
          return 0;
        }

        // Test the range of the new block for correctness and add it
        // to the range list if OK. The block must be  be aligned properly,
        // and must not overlap any currently allocated block.
        if (add_range(impl, &ranges, p, size, tracenum, i) == 0)
          return 0;

        // Fill the allocated region with some unique data that you can check
        // for if the region is copied via realloc.
        fill_random(p, size);

        // Remember region
        trace->blocks[index] = p;
        trace->block_sizes[index] = size;
        break;

      case REALLOC:  // realloc

        // Call the student's realloc
        oldp = trace->blocks[index];
        oldsize = trace->block_sizes[index];
        if (size < oldsize)
          oldsize = size;

        char old_checksum = checksum(oldp, oldsize);

        if ((newp = (char *) impl->realloc(oldp, size)) == NULL) {
          malloc_error(tracenum, i, "impl realloc failed.");
          return 0;
        }

        // Remove the old region from the range list
        remove_range(&ranges, oldp);

        // Check new block for correctness and add it to range list
        if (add_range(impl, &ranges, newp, size, tracenum, i) == 0)
          return 0;

        // Make sure that the new block contains the data from the old block,
        // and then fill in the new block with new data that you can use to
        // verify the block was copied if it is resized again.
        char new_checksum = checksum(newp, oldsize);

        if (old_checksum != new_checksum) {
          malloc_error(tracenum, i, "Make sure that the new block contains the data from the old block\n");
          return 0;
        }

        fill_random(newp, size);

        // Remember region
        trace->blocks[index] = newp;
        trace->block_sizes[index] = size;
        break;

      case FREE:  // free

        // Remove region from list and call student's free function
        p = trace->blocks[index];
        remove_range(&ranges, p);
        impl->free(p);
        break;

      case WRITE:  // write

        break;

      default:
        app_error("Nonexistent request type in eval_mm_valid");
    }
  }

  // Free ranges allocated and reset the heap.
  impl->reset_brk();
  clear_ranges(&ranges);

  // As far as we know, this is a valid malloc package
  return 1;
}
#endif  // MM_VALIDATOR_H
