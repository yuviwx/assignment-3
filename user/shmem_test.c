#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/riscv.h"
#include <stddef.h>


void shmem_test(int disable_unmapping)
{
    char* buffer = malloc(PGSIZE);
    if (buffer == NULL) {
        printf("Failed to allocate shared memory\n");
        exit(1);
    }
    int daddy = getpid();
    int pid = fork();
    if(pid < 0) {
        printf("Fork failed\n");
        free(buffer);
        exit(1);
    } else if (pid == 0) {
        // Child process
        printf("Child size: %d\n", sbrk(0));
        char* shared_memory = map_shared_pages(daddy, getpid(), buffer, PGSIZE);
        printf("Child size after shared memory: %d\n", sbrk(0));

        // Use the shared memory
        strcpy(shared_memory, "Hello daddy");
        if(!disable_unmapping) {
            unmap_shared_pages(getpid(), shared_memory, PGSIZE);
            printf("Child size after unmap: %d\n", sbrk(0));
        }
        char* new_buffer = malloc(PGSIZE);
        if (new_buffer == NULL) {
            printf("Failed to allocate new buffer\n");
            exit(1);
        }
        printf("Child size after malloc: %d\n", sbrk(0));
        free(new_buffer);        
        exit(0);
    }
    else {
        // Parent process
        wait(0); 
        printf("%s\n", buffer);
        free(buffer);
    }
}

// Test 2: Multiple mappings and edge cases
void test2() {
    printf("\n=== Test 2: Multiple Mappings Test ===\n");
    // Allocate buffers    
    char* buffer1 = malloc(PGSIZE);
    char* buffer2 = malloc(PGSIZE);
    if (buffer1 == NULL || buffer2 == NULL) {
        printf("Failed to allocate shared memory\n");
        exit(1);
    }
    // Fork
    int daddy = getpid();
    int pid = fork();
    if(pid < 0) {
        printf("Fork failed\n");
        free(buffer1);
        free(buffer2);
        exit(1);
    } 
    // Share memory
    else if (pid == 0) {
        // Child process
        char* shared_memory1 = map_shared_pages(daddy, getpid(), buffer1, PGSIZE);
        char* shared_memory2 = map_shared_pages(daddy, getpid(), buffer2, PGSIZE);
        if((uint64)shared_memory1 == (uint64)-1 || (uint64)shared_memory2 == (uint64)-1){
            printf("failed to share buffers\n");
            exit(1);
        }else{
            printf("successfully shared 2 buffers\n");
            printf("Child shared1: %p, shared2: %p\n", shared_memory1, shared_memory2);
        }

        // Use the shared memory
        strcpy(shared_memory1, "Hello daddy");
        strcpy(shared_memory2, "Hello daddy 2");
        unmap_shared_pages(getpid(), shared_memory2, PGSIZE);
        printf("buffer2 unshared\n");
        unmap_shared_pages(getpid(), shared_memory1, PGSIZE);
        printf("buffer1 unshared\n");
    }
    else {
        // Parent process
        wait(0); 
        printf("buffer1: %s\n", buffer1);
        printf("buffer2: %s\n", buffer2);
        free(buffer1);
        free(buffer2);
    }
}

int main(int argc, char *argv[])
{  

    shmem_test(0);
    shmem_test(1);
    exit(0);
}