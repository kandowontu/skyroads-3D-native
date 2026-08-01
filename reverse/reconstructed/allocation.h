#ifndef SKYROADS_RECOVERED_ALLOCATION_H
#define SKYROADS_RECOVERED_ALLOCATION_H

#include <stddef.h>
#include <stdint.h>

typedef struct SrAllocationStack {
    uint16_t *segments;       /* DS:9344 */
    size_t capacity;
    size_t count;             /* DS:4572 */
    void *context;
    uint16_t (*allocate_bytes)(void *context, uint32_t bytes);
    void (*free_segment)(void *context, uint16_t segment);
} SrAllocationStack;

/* Reconstructions of 1000:3ED8, 1000:3F04, 1000:3F1F, and 1000:3F56. */
uint16_t sr_allocation_stack_push_bytes(SrAllocationStack *stack, uint32_t bytes);
int sr_allocation_scope_push(SrAllocationStack *stack);
int sr_allocation_scope_pop(SrAllocationStack *stack);
int sr_allocation_stack_pop_one(SrAllocationStack *stack);

#endif
