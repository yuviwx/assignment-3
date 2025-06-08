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

int main(int argc, char *argv[])
{  

    shmem_test(0);
    shmem_test(1);
    exit(0);
}