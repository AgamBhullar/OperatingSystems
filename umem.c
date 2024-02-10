#include "umem.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct BlockHeader {
    size_t size;
    struct BlockHeader *next;
    int free;
} BlockHeader;

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define BLOCK_SIZE ALIGN(sizeof(BlockHeader))

static uint8_t fake_heap[1024 * 1024]; // 1MB of fake heap
static BlockHeader *heap_list = NULL; 
static BlockHeader *next_fit_ptr = NULL; 
static int allocation_algorithm = FIRST_FIT; 
static int allocator_initialized = 0;        
static size_t size_of_region = 0;     
static void *memory_region = NULL; 

static BlockHeader *find_best_fit(size_t size);
static BlockHeader *find_worst_fit(size_t size);
static BlockHeader *find_first_fit(size_t size);
static BlockHeader *find_next_fit(size_t size);
static void split_block(BlockHeader *block, size_t size);
static BlockHeader *coalesce(BlockHeader *block);

static void reset_globals() {
    heap_list = NULL;
    next_fit_ptr = NULL;
    allocator_initialized = 0;
    size_of_region = 0;
    memory_region = NULL;
}

int umeminit(size_t sizeOfRegion, int allocationAlgo) {
    // Resetting global variables
    reset_globals();

    if (sizeOfRegion < BLOCK_SIZE) {
        fprintf(stderr, "Requested size is too small\n");
        return -1; 
    }
    
    allocation_algorithm = allocationAlgo;

    // This aligns sizeOfRegion to page size
    size_t page_size = getpagesize();
    sizeOfRegion = (sizeOfRegion + (page_size - 1)) & ~(page_size - 1);

    // Using mmap to allocate memory
    int fd = open("/dev/zero", O_RDWR);  
    void *mapped_area = mmap(NULL, sizeOfRegion, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    close(fd);  

    //Debug line
    //printf("Allocating memory with mmap\n");

    if (mapped_area == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }

    //Debug line
    //printf("Memory allocated successfully\n");

    // Initializing the heap list 
    heap_list = (BlockHeader *)mapped_area;
    heap_list->size = sizeOfRegion - BLOCK_SIZE;
    heap_list->next = NULL;
    heap_list->free = 1;
    next_fit_ptr = heap_list; 

    size_of_region = sizeOfRegion;
    allocator_initialized = 1;
    return 0; 
}

void *umalloc(size_t size) {
    if (heap_list == NULL || size == 0) {
        return NULL;
    }

    size = ALIGN(size);
    BlockHeader *block;

    switch (allocation_algorithm) {
        case BEST_FIT:
            block = find_best_fit(size);
            break;
        case WORST_FIT:
            block = find_worst_fit(size);
            break;
        case FIRST_FIT:
            block = find_first_fit(size);
            break;
        case NEXT_FIT:
            block = find_next_fit(size);
            break;
        default:
            return NULL; 
    }

    if (block == NULL) {
        return NULL; 
    }

    if (block->size >= size + BLOCK_SIZE) {
        split_block(block, size);
    }

    block->free = 0;
    return ((char *)block + BLOCK_SIZE);
}

static BlockHeader *find_best_fit(size_t size) {
    BlockHeader *current = heap_list;
    BlockHeader *best_fit = NULL;

    // Find the best fit
    while (current != NULL) {
        if (current->free && current->size >= size) {
            
            if (best_fit == NULL || current->size < best_fit->size) {
                best_fit = current;
            }
        }
        current = current->next;
    }

    return best_fit; //NULL if no suitable block is found
}

static BlockHeader *find_worst_fit(size_t size) {
    BlockHeader *current = heap_list;
    BlockHeader *worst_fit = NULL;

    // Find the worst fit
    while (current != NULL) {
        if (current->free && current->size >= size) {
            
            if (worst_fit == NULL || current->size > worst_fit->size) {
                worst_fit = current;
            }
        }
        current = current->next;
    }

    return worst_fit; // NULL if no suitable block is found
}


static BlockHeader *find_first_fit(size_t size) {
    BlockHeader *current = heap_list;

    //Find the first fit
    while (current != NULL) {
        if (current->free && current->size >= size) {
            return current; 
        }
        current = current->next;
    }

    return NULL; // No fit found
}


static BlockHeader *find_next_fit(size_t size) {
    static BlockHeader *next = NULL;  
    BlockHeader *start = next;        
    
    if (next == NULL) {
        next = heap_list;
    }
    
    while (next != NULL) {
        if (next->free && next->size >= size) {
            return next; 
        }
        next = next->next;
        if (next == NULL) next = heap_list;
        if (next == start) break;
    }

    return NULL; 
}


static void split_block(BlockHeader *block, size_t size) {
    // Calculate the size of the remaining block
    size_t remaining_size = block->size - size - sizeof(BlockHeader);
    
    
    if (remaining_size > sizeof(BlockHeader)) {
        //For the remaining part of the block, new block header is created
        BlockHeader *new_block = (BlockHeader *)((char *)block + sizeof(BlockHeader) + size);
        new_block->size = remaining_size;
        new_block->free = 1;
        new_block->next = block->next;

        block->size = size;
        block->free = 0;

        new_block->next = block->next;
        block->next = new_block;
    }
}


int ufree(void *ptr) {
    if (ptr == NULL || ptr < (void *)heap_list || ptr >= (void *)heap_list + sizeof(fake_heap)) {
        return -1; 
    }

    BlockHeader *block = (BlockHeader *)((char *)ptr - BLOCK_SIZE);
    block->free = 1;
    
    coalesce(block);

    return 0; 
}

static BlockHeader *coalesce(BlockHeader *block) {
    // if possible, Coalesce with next block
    if (block->next && block->next->free) {
        block->size += sizeof(BlockHeader) + block->next->size;
        block->next = block->next->next;
    }

    BlockHeader *prev = NULL;
    BlockHeader *cur = heap_list;
    while (cur != NULL && cur != block) {
        prev = cur;
        cur = cur->next;
    }
    if (prev && prev->free) {
        prev->size += sizeof(BlockHeader) + block->size;
        prev->next = block->next;
        block = prev;
    }

    return block;
}

void umemdump() {
    BlockHeader *current = heap_list;
    while (current != NULL) {
        printf("Block %p: size %zu, free %d\n", (void *)current, current->size, current->free);
        current = current->next;
    }
}
