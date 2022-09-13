////////////////////////////////////////////////////////////////////////////////
// COMP1521 22T1 --- Assignment 2: `Allocator', a simple sub-allocator        //
// <https://www.cse.unsw.edu.au/~cs1521/22T1/assignments/ass2/index.html>     //
//                                                                            //
// Written by Micheal Makhoul on 20/04/2022.                                  //
//                                                                            //
// 2021-04-06   v1.0    Team COMP1521 <cs1521 at cse.unsw.edu.au>             //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/**
 * This Project was developed for uni training purpose.
 * This is a sub-allocator program used for helping complicated programs to allocate 
 * a required number of smaller chunks within their heap.
 * 
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allocator.h"

/** minimum total space for heap */
#define MIN_HEAP 4096

/** minimum amount of space to split for a free chunk (excludes header) */
#define MIN_CHUNK_SPLIT 32

/** the size of a chunk header (in bytes) */
#define HEADER_SIZE (sizeof(struct header))

/** constants for chunk header's status */
#define ALLOC 0x55555555
#define FREE 0xAAAAAAAA

typedef unsigned char byte;

/** The header for a chunk. */
typedef struct header {
    uint32_t status; /**< the chunk's status -- shoule be either ALLOC or FREE */
    uint32_t size;   /**< number of bytes, including header */
    byte     data[]; /**< the chunk's data -- not interesting to us */
} header_type;


/** The heap's state */
typedef struct heap_information {
    byte      *heap_mem;      /**< space allocated for Heap */
    uint32_t   heap_size;     /**< number of bytes in heap_mem */
    byte     **free_list;     /**< array of pointers to free chunks */
    uint32_t   free_capacity; /**< maximum number of free chunks (maximum elements in free_list[]) */
    uint32_t   n_free;        /**< current number of free chunks */
} heap_information_type;

// Footnote:
// The type unsigned char is the safest type to use in C for a raw array of bytes
//
// The use of uint32_t above limits maximum heap size to 2 ** 32 - 1 == 4294967295 bytes
// Using the type size_t from <stdlib.h> instead of uint32_t allowing any practical heap size,
// but would make struct header larger.

/** Global variable holding the state of the heap */
static struct heap_information my_heap;

/**
 * Updates `my_heap.free_list' after freeing an allocated chunk.
 * The function adds a pointer to the deallocated chunk to the array,
 * and keeps the array sorted in ascending order.
 * 
 * @param ptr A pointer to the deallocated chunk.
 * @return The position of the added pointer inside `my_heap.free_list' array.
 */
int update_free_list(void *ptr);

/**
 * Checks if any adjacent chunk is free for a given index in the `my_heap.free_list' array.
 * If so, it merges the free chunks, and updates the list.
 * 
 * @param position The position of the freed chunk in the `my_heap.free_list' array.
 */
void merge_free_chunks(int position);

/**
 * Shifts the elements of `my_heap.free_list' array one position to the left 
 * starting from the element `my_heap.n_free' - 1 till a given index.
 * It was used to delete an element from the list after allocation or 
 * merging the chunk that the element points to.
 *
 * @param index The position of the array element to delete from the list.
 */
void shift_left(int index);

/**
 * Shifts the elements of `my_heap.free_list' array one position to the right 
 * starting from a given index till the last non-zero element in the array (`my_heap.n_free' - 1).
 * It was used inside update_free_list() function, to allow adding a new element to the list in the appropriate position.
 * 
 * @param index The position of the array element to start shifting from.
 */
void shift_right(int index);

/**
 * Check if the required `size' is less than MIN_HEAP size then set it to MIN_HEAP.
 * It also rounds the required `size' value up to the nearest multiple of 4 if it is not.
 * 
 * @param size The required submemory size to allocate.
 * @return int 
 */
int roundSize(int size)

// Initialise my_heap
int init_heap(uint32_t size) { 
    
    size = roundSize(size);
    
    // Allocate `size' bytes memory to my_heap.heap_mem.
    my_heap.heap_mem = malloc(size);
    if (my_heap.heap_mem == NULL) {
        return -1;
    }
 
    my_heap.heap_size = size;
    my_heap.free_capacity = (size / HEADER_SIZE);

    // Allocate the `free_list' of size `size/HEADER_SIZE'.
    my_heap.free_list = calloc(my_heap.free_capacity, HEADER_SIZE);
    if (my_heap.free_list == NULL) {
        return -1;
    }
    
    // Set the first element in the `free_list' array to a pointer to `my_heap.heap_mem' address.
    header_type *chunk = (header_type *)(my_heap.heap_mem);
    
    my_heap.n_free = 1;
    my_heap.free_list[0] = (byte *) chunk;
    
    chunk->size = my_heap.heap_size;
    chunk->status = FREE;

    return 0;
}

// Allocate a chunk of memory large enough to store `size' bytes.
void *my_malloc(uint32_t size) {

    size = roundSize(size);
    
    // The size of the entire required chunk.
    uint32_t required_chunk_size = size + HEADER_SIZE;
    
    // Searching the free_list array for a big enough free chunk to allocate entirely or partially. 
    for (int i = 0; i < my_heap.n_free; i++) {
        
        // To get the size and the status of the free chunks
        header_type *available_chunk = (header_type *)(my_heap.free_list[i]);
        uint32_t current_size = available_chunk->size;
        
        // The remaining value of the available chunk size - required chunk size
        uint32_t remaining_size = current_size - required_chunk_size;
        
        // Check if the available chunk is enough to allocate `size' bytes and the remaining is less than MIN_CHUNK_SPLIT.
        if ((remaining_size < MIN_CHUNK_SPLIT) && (remaining_size >= 0)){
            
            // Allocate the whole chunk.
            available_chunk->status = ALLOC;            
            shift_left(i);            
            my_heap.n_free--;
            
            // Return a pointer to the first usable byte of data in the allocated chunk.
            return (byte *) available_chunk + HEADER_SIZE;
            
            // Check if the available chunk is enough to allocate `size' bytes and the remaining is greater than MIN_CHUNK_SPLIT.
        } else if (current_size >= (required_chunk_size + MIN_CHUNK_SPLIT)) {
            
            // Allocate the lower chunk.
            available_chunk->size = required_chunk_size;
            available_chunk->status = ALLOC;
            
            // Next free chunk.
            header_type *next_chunk = (header_type *)(my_heap.free_list[i] + required_chunk_size);
            next_chunk->size = remaining_size;
            next_chunk->status = FREE;
            
            // Add the new chunk to the list.
            my_heap.free_list[i] = (byte *) next_chunk;
            
            // Return a pointer to the first usable byte of data in the allocated chunk.
            return (byte *) available_chunk + HEADER_SIZE;
        } 
    }
    
    // Return NULL if it was not able to allocate `size' bytes.
    return NULL;
}


// Deallocate chunk of memory referred to by `ptr'
void my_free(void *ptr) {
    
    // Check if the `ptr' represents an allocated chunk in the heap.
    if (ptr == NULL) {
        fprintf(stderr, "Attempt to free unallocated chunk\n");
        exit(1);
    }
    
    // Subtract HEADER_SIZE from the given pointer to get the address of the first byte of the chunk.
    ptr -= HEADER_SIZE;
    
    // To access the details of the chunk we want to free.
    header_type *chunk_to_free = (header_type *)(ptr);
    
    // Check if the `ptr' represents an allocated chunk in the heap.
    if (chunk_to_free->status != ALLOC) {
        fprintf(stderr, "Attempt to free unallocated chunk\n");
        exit(1);
    } else {
        chunk_to_free->status = FREE;
        
        // Add the chunk to my_heap.free_list array.
        int position = update_free_list(ptr);  
        
        // Merge the chunk with the adjacent free chunks.
        merge_free_chunks(position);
    }    
    
}


// DO NOT CHANGE CHANGE THiS FUNCTION
//
// Release resources associated with the heap
void free_heap(void) {
    free(my_heap.heap_mem);
    free(my_heap.free_list);
}


// DO NOT CHANGE CHANGE THiS FUNCTION

// Given a pointer `obj'
// return its offset from the heap start, if it is within heap
// return -1, otherwise
// note: int64_t used as return type because we want to return a uint32_t bit value or -1
int64_t heap_offset(void *obj) {
    if (obj == NULL) {
        return -1;
    }
    int64_t offset = (byte *)obj - my_heap.heap_mem;
    if (offset < 0 || offset >= my_heap.heap_size) {
        return -1;
    }

    return offset;
}


// DO NOT CHANGE CHANGE THiS FUNCTION
//
// Print the contents of the heap for testing/debugging purposes.
// If verbosity is 1 information is printed in a longer more readable form
// If verbosity is 2 some extra information is printed
void dump_heap(int verbosity) {

    if (my_heap.heap_size < MIN_HEAP || my_heap.heap_size % 4 != 0) {
        printf("ndump_heap exiting because my_heap.heap_size is invalid: %u\n", my_heap.heap_size);
        exit(1);
    }

    if (verbosity > 1) {
        printf("heap size = %u bytes\n", my_heap.heap_size);
        printf("maximum free chunks = %u\n", my_heap.free_capacity);
        printf("currently free chunks = %u\n", my_heap.n_free);
    }

    // We iterate over the heap, chunk by chunk; we assume that the
    // first chunk is at the first location in the heap, and move along
    // by the size the chunk claims to be.

    uint32_t offset = 0;
    int n_chunk = 0;
    while (offset < my_heap.heap_size) {
        struct header *chunk = (struct header *)(my_heap.heap_mem + offset);

        char status_char = '?';
        char *status_string = "?";
        switch (chunk->status) {
        case FREE:
            status_char = 'F';
            status_string = "free";
            break;

        case ALLOC:
            status_char = 'A';
            status_string = "allocated";
            break;
        }

        if (verbosity) {
            printf("chunk %d: status = %s, size = %u bytes, offset from heap start = %u bytes",
                    n_chunk, status_string, chunk->size, offset);
        } else {
            printf("+%05u (%c,%5u) ", offset, status_char, chunk->size);
        }

        if (status_char == '?') {
            printf("\ndump_heap exiting because found bad chunk status 0x%08x\n",
                    chunk->status);
            exit(1);
        }

        offset += chunk->size;
        n_chunk++;

        // print newline after every five items
        if (verbosity || n_chunk % 5 == 0) {
            printf("\n");
        }
    }

    // add last newline if needed
    if (!verbosity && n_chunk % 5 != 0) {
        printf("\n");
    }

    if (offset != my_heap.heap_size) {
        printf("\ndump_heap exiting because end of last chunk does not match end of heap\n");
        exit(1);
    }

}

// ADD YOUR EXTRA FUNCTIONS HERE

/**
 * Updates `my_heap.free_list' after freeing an allocated chunk.
 * The function adds a pointer to the deallocated chunk to the array,
 * and keeps the array sorted in ascending order.
 * 
 * @param ptr A pointer to the deallocated chunk.
 * @return The position of the added pointer inside `my_heap.free_list' array.
 */
int update_free_list(void *ptr) {
    
    // Checks if the list is empty.
    if (my_heap.n_free == 0) {
        my_heap.free_list[0] = (byte *) ptr;
        my_heap.n_free++;
        return 0;
    }
    
    // If the list is not empty.
    for (int i = 0; i < my_heap.n_free; i ++) {
        // Add `ptr' value to the left of the first bigger pointer value in the list.
        if (my_heap.free_list[i] > (byte *) ptr) {
            // Shift the first bigger value than `ptr' to the right.
            shift_right(i);
            my_heap.n_free++;
            my_heap.free_list[i] = (byte *) ptr;
            
            // Return the index where `ptr' was added to the list.
            return i;
        } 
    }
    return 0;
}

/**
 * Checks if any adjacent chunk is free for a given index in the `my_heap.free_list' array.
 * If so, it merges the free chunks, and updates the list.
 * 
 * @param position The position of the freed chunk in the `my_heap.free_list' array.
 */
void merge_free_chunks(int position) {
    // The chunk we want to check if the adjacent chunks are free.
    header_type *chunk = (header_type *)(my_heap.free_list[position]);
    
    // Check if next chunk is free.
    header_type *next_chunk = (header_type *)(my_heap.free_list[position + 1]);
    if ((byte *) chunk + chunk->size == (byte *) next_chunk) {
        chunk->size += next_chunk->size;
        my_heap.n_free--;
        shift_left(position + 1);
    }
    
    // Check if previous chunk is free.
    if (position != 0) {
        header_type *previous_chunk = (header_type *)(my_heap.free_list[position - 1]);
        if ((byte *) previous_chunk + previous_chunk->size == (byte *) chunk) {
            previous_chunk->size += chunk->size;
            my_heap.n_free--;
            shift_left(position);
        }
    }
}

/**
 * Shifts the elements of `my_heap.free_list' array one position to the left 
 * starting from the element `my_heap.n_free' - 1 till a given index.
 *
 * @param index The position of the array element to delete from the list.
 */
void shift_left(int index) {
    for (int position = index; position < my_heap.n_free; position++) {
        my_heap.free_list[position] = my_heap.free_list[position + 1];
    }
}

/**
 * Shifts the elements of `my_heap.free_list' array one position to the right 
 * starting from a given index till the last non-zero element in the array (`my_heap.n_free' - 1).
 * 
 * @param index The position of the array element to start shifting from.
 */
void shift_right(int index) {
    for (int position = my_heap.n_free; position > index; position--) {
        my_heap.free_list[position] = my_heap.free_list[position - 1];
    }
}

/**
 * Check if the required `size' is less than MIN_HEAP size then set it to MIN_HEAP.
 * It also rounds the required `size' value up to the nearest multiple of 4 if it is not.
 * @param size The required submemory size to allocate.
 * @return int 
 */
int roundSize(int size){
    if (size < MIN_HEAP) {
        size = MIN_HEAP;
    }
    
    for (int i = 1; i < 4; i++) {
        if (size % 4 == i) {
            size+= 4 - i;
        }
    }
}
