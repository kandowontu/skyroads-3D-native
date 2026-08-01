#include "allocation.h"

uint16_t sr_allocation_stack_push_bytes(SrAllocationStack *stack, uint32_t bytes) {
    uint16_t segment;
    if (stack->count >= stack->capacity) {
        return 0;
    }
    segment = stack->allocate_bytes(stack->context, bytes);
    stack->segments[stack->count++] = segment;
    return segment;
}

int sr_allocation_scope_push(SrAllocationStack *stack) {
    if (stack->count >= stack->capacity) {
        return 0;
    }
    stack->segments[stack->count++] = 1;
    return 1;
}

int sr_allocation_scope_pop(SrAllocationStack *stack) {
    while (stack->count != 0) {
        uint16_t segment = stack->segments[--stack->count];
        if (segment == 1) {
            return 1;
        }
        stack->free_segment(stack->context, segment);
    }
    return 0;
}

int sr_allocation_stack_pop_one(SrAllocationStack *stack) {
    uint16_t segment;
    if (stack->count == 0) {
        return 0;
    }
    segment = stack->segments[--stack->count];
    stack->free_segment(stack->context, segment);
    return 1;
}
