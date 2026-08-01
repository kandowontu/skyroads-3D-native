#include "allocation.h"

#include <stdio.h>

typedef struct FakeAllocator {
    uint16_t next;
    uint16_t freed[4];
    size_t freed_count;
} FakeAllocator;

static uint16_t allocate_bytes(void *context, uint32_t bytes) {
    FakeAllocator *allocator = (FakeAllocator *)context;
    (void)bytes;
    return allocator->next++;
}

static void free_segment(void *context, uint16_t segment) {
    FakeAllocator *allocator = (FakeAllocator *)context;
    allocator->freed[allocator->freed_count++] = segment;
}

int main(void) {
    uint16_t entries[8] = {0};
    FakeAllocator allocator = {0x2000, {0}, 0};
    SrAllocationStack stack = {
        entries, 8, 0, &allocator, allocate_bytes, free_segment};

    sr_allocation_stack_push_bytes(&stack, 100);
    sr_allocation_scope_push(&stack);
    sr_allocation_stack_push_bytes(&stack, 200);
    sr_allocation_stack_push_bytes(&stack, 300);
    if (!sr_allocation_scope_pop(&stack) || stack.count != 1 ||
        allocator.freed_count != 2 || allocator.freed[0] != 0x2002 ||
        allocator.freed[1] != 0x2001) {
        fputs("allocation scope vector failed\n", stderr);
        return 1;
    }
    if (!sr_allocation_stack_pop_one(&stack) || stack.count != 0 ||
        allocator.freed_count != 3 || allocator.freed[2] != 0x2000) {
        fputs("allocation pop-one vector failed\n", stderr);
        return 1;
    }
    puts("Recovered allocation-stack scopes passed LIFO vectors");
    return 0;
}
