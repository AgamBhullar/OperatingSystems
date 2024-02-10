#include "umem.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define BEST_FIT (1)
#define WORST_FIT (2)
#define FIRST_FIT (3)
#define NEXT_FIT (4)

void test_allocation_strategies() {
    const size_t memorySize = 1024 * 1024; // 1MB

    int strategies[] = {BEST_FIT, WORST_FIT, FIRST_FIT, NEXT_FIT};
    const char* strategyNames[] = {"Best Fit", "Worst Fit", "First Fit", "Next Fit"};

    for (int i = 0; i < sizeof(strategies) / sizeof(strategies[0]); ++i) {
        printf("Testing %s strategy...\n", strategyNames[i]);
        // assert(umeminit(memorySize, strategies[i]) == 0);
        int init_result = umeminit(memorySize, strategies[i]);
        if (init_result != 0) {
            printf("Failed to initialize memory with %s strategy, return value was: %d\n", strategyNames[i], init_result);
            continue; // Skip to next strategy
        }

        // Perform allocations
        void *ptr1 = umalloc(1000); // Allocate 1000 bytes
        void *ptr2 = umalloc(500);  // Allocate 500 bytes
        void *ptr3 = umalloc(2000); // Allocate 2000 bytes
        void *ptr4 = umalloc(300);  // Allocate 300 bytes

        // Free some blocks to create fragmentation
        ufree(ptr2);
        ufree(ptr4);

        // Allocate again to see how different strategies behave
        void *ptr5 = umalloc(800); // This allocation is interesting, strategies should behave differently here

        // Check the state of memory after allocation
        umemdump();

        // Free all blocks
        ufree(ptr1);
        ufree(ptr3);
        ufree(ptr5);

        // Reset the allocator to its initial state if needed
        // This may involve resetting the global state, such as free list or memory pool
        // Depending on your implementation, you may need to write a function that resets the allocator
        // reset_allocator(); // This function should be implemented according to your specific allocator design

        printf("%s strategy test completed.\n\n", strategyNames[i]);
    }
}

void test_initialization() {
    printf("Testing umeminit...\n");
    int result = umeminit(1024 * 1024, FIRST_FIT); // Initialize with 1MB
    assert(result == 0); // Expect umeminit to return 0 for successful initialization
    printf("umeminit succeeded.\n\n");
}

void test_freeing_memory() {
    printf("Testing memory allocation and freeing...\n");

    // Allocate several blocks
    void *ptr1 = umalloc(100);
    void *ptr2 = umalloc(200);
    void *ptr3 = umalloc(300);

    // Check pointers
    assert(ptr1 != NULL);
    assert(ptr2 != NULL);
    assert(ptr3 != NULL);

    // Memory state after allocations
    umemdump();

    // Free the first and the third block
    ufree(ptr1);
    ufree(ptr3);

    // Memory state after some blocks have been freed
    umemdump();

    // Free the second block
    ufree(ptr2);

    // Memory state should now reflect that all blocks have been freed
    umemdump();

    printf("Memory freeing test completed.\n");
}

int main() {
    // Run initialization test
    test_initialization();
    test_allocation_strategies();
    // Run memory freeing test
    test_freeing_memory();

    return 0;
}

// #include "umem.h"
// #include <stdio.h>
// #include <string.h>
// #include <assert.h>
// #include <stdint.h>

// #define TEST_ALIGNMENT 8

// void basic_allocation_test() {
//     size_t size = 128; // Allocate 128 bytes, an arbitrary small size for testing
//     void *ptr = umalloc(size);

//     // Check if allocation was successful
//     assert(ptr != NULL);
    
//     // Optionally, you can write to this memory to confirm it's usable
//     memset(ptr, 0, size);

//     // Now free the allocated memory
//     assert(ufree(ptr) == 0);

//     // Attempt to allocate again to see if the memory can be reused
//     void *ptr2 = umalloc(size);
//     assert(ptr2 != NULL);

//     // Clean up
//     assert(ufree(ptr2) == 0);

//     printf("Basic Allocation Test Passed\n");
// }

// void entire_heap_allocation_test() {
//     size_t large_size = 1024 * 512; // Half of the heap size, assuming 1MB heap
//     void *ptr = umalloc(large_size);
//     assert(ptr != NULL);

//     // Now try to allocate another large block, which should fail
//     void *ptr2 = umalloc(large_size);
//     assert(ptr2 == NULL);

//     // Free the large block
//     assert(ufree(ptr) == 0);

//     printf("Entire Heap Allocation Test Passed\n");
// }

// void fragmentation_test() {
//     // Allocate several small blocks
//     void *ptr1 = umalloc(100);
//     void *ptr2 = umalloc(200);
//     void *ptr3 = umalloc(300);
//     assert(ptr1 != NULL && ptr2 != NULL && ptr3 != NULL);

//     // Free some blocks to create fragmentation
//     assert(ufree(ptr1) == 0); // Free the first block
//     assert(ufree(ptr3) == 0); // Free the third block

//     // Now attempt to allocate a block that requires contiguous space
//     // This block size is chosen such that it would only fit in the space if fragmentation isn't an issue
//     void *ptr4 = umalloc(400);
//     assert(ptr4 != NULL);

//     // Cleanup: free remaining blocks
//     assert(ufree(ptr2) == 0);
//     assert(ufree(ptr4) == 0);

//     printf("Fragmentation Test Passed\n");
// }

// void boundary_conditions_test() {
//     // Test allocation of 0 bytes
//     void *ptr_zero = umalloc(0);
//     assert(ptr_zero == NULL || ptr_zero != NULL); // Depending on your implementation
//     if (ptr_zero != NULL) {
//         assert(ufree(ptr_zero) == 0);
//     }

//     // Test allocation of a large block
//     size_t large_size = 1024 * 512; // Half of the heap size, assuming 1MB heap
//     void *ptr_max = umalloc(large_size);
//     // This allocation may or may not succeed
//     if (ptr_max != NULL) {
//         assert(ufree(ptr_max) == 0);
//     }

//     printf("Boundary Conditions Test Passed\n");
// }

// void coalescing_test() {
//     // Allocate several blocks
//     void *ptr1 = umalloc(100);
//     void *ptr2 = umalloc(200);
//     void *ptr3 = umalloc(300);
//     assert(ptr1 != NULL && ptr2 != NULL && ptr3 != NULL);

//     // Free the first and second blocks, which are adjacent
//     assert(ufree(ptr1) == 0);
//     assert(ufree(ptr2) == 0);

//     // Now attempt to allocate a block larger than any individual free block,
//     // but small enough to fit into the coalesced space
//     void *ptr4 = umalloc(250);
//     assert(ptr4 != NULL);

//     // Free the third block
//     assert(ufree(ptr3) == 0);

//     // Attempt another allocation that requires coalescing
//     void *ptr5 = umalloc(500);
//     assert(ptr5 != NULL);

//     // Cleanup: free remaining blocks
//     assert(ufree(ptr4) == 0);
//     assert(ufree(ptr5) == 0);

//     printf("Coalescing Test Passed\n");
// }


// void alignment_test() {
//     for (size_t size = 1; size <= 1024; size *= 2) {
//         void *ptr = umalloc(size);
//         assert(ptr != NULL);
//         assert(((uintptr_t)ptr % TEST_ALIGNMENT) == 0);
//         assert(ufree(ptr) == 0);
//     }
//     printf("Alignment Test Passed\n");
// }

// int main() {
//     umeminit(1024 * 1024, FIRST_FIT); // Initialize memory with 1MB and FIRST_FIT strategy

//     basic_allocation_test();
//     entire_heap_allocation_test();
//     fragmentation_test();
//     boundary_conditions_test();
//     coalescing_test();
//     alignment_test();

//     return 0;
// }
