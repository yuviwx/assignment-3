#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NCHILDREN 16
// #define MAX_READ_CYCLES 200000  // Stop reading after this many cycles with no new messages
// #define IDLE_THRESHOLD 500000    // Consider "no activity" after this many empty cycles

typedef unsigned int uint32;
typedef unsigned short uint16;

void build_msg(int i, int child_index, char* msg) {
  char template[] = "Message from child X msg YY";
  memmove(msg, template, sizeof(template));
  msg[19] = '0' + child_index;  // Replace 'X'

  // Replace 'YY' with actual number
  if (i >= 10) {
      msg[25] = '0' + (i / 10);
      msg[26] = '0' + (i % 10);
  } else {
      msg[25] = '0' + i;
      msg[26] = '\0';  // Terminate early for single digit
  }
}

int write_log(char *buf, int child_index, const char *msg) {
    int len = strlen(msg);
    char *p = buf;

    if (len >= (1 << 16)) return -1;

    // Search for free space in buffer
    while (p + 4 + len < buf + PGSIZE) {
        uint32 *header = (uint32 *)p;
        uint32 expected = 0;  
        uint32 new_header = (child_index << 16) | (uint16)len;

        if (__sync_val_compare_and_swap(header, expected, new_header) == expected) {
            memmove(p + 4, msg, len);
            
            // Mark as complete with MSB=1 (atomic completion flag)
            *((uint32 *)p) = new_header | 0x80000000;
            return 0;  // Success
        }
        uint32 current_header = *((uint32 *)p);
        uint16 current_len = current_header & 0xFFFF;
        p += 4 + current_len;
        
        // Align to 4-byte boundary
        p = (char *)(((uint64)p + 3) & ~3);
    }
    
    return -1;
}

void read_log(char *buf) {
    char *p = buf;
    // int messages_found = 0;
    // int debug_scan_count = 0;

    while (p + 4 < buf + PGSIZE) {
        uint32 header = *(uint32 *)p;
        // debug_scan_count++;
        
        // Skip empty slots
        if (header == 0) {
            p += 4;
            p = (char *)(((uint64)p + 3) & ~3);
            continue;
        }

        uint16 child_id = (header >> 16) & 0x7FFF;  // Mask out MSB flag
        uint16 len = header & 0xFFFF;
        
        // Check if message is complete (MSB=1) and not already read
        if ((header & 0x80000000) && child_id != 0x7FFF && child_id < NCHILDREN) {
            // Validate buffer bounds
            if (p + 4 + len >= buf + PGSIZE) break;
            
            char msg[len + 1];
            memmove(msg, p + 4, len);
            msg[len] = '\0';
            printf("Parent received from child %d: %s\n", child_id, msg);
            
            // Mark as read (set child_id to 0x7FFF)
            uint32 new_header = (0x7FFF << 16) | len | 0x80000000;
            *((uint32 *)p) = new_header;
            // messages_found++;
        }

        // Move to next message location
        p += 4 + len;
        p = (char *)(((uint64)p + 3) & ~3);
        
        // if (debug_scan_count > 100) break;
    }
    
    // return messages_found;
}

void custom_exit(char* buf){
  uint32 *header;
  uint32 expected, num_of_children, new_header;

  do {
    header = (uint32 *)buf;
    expected = *header;
    num_of_children = expected >> 16;
    new_header = (num_of_children - 1) << 16;
  } while (__sync_val_compare_and_swap(header, expected, new_header) != expected);
}
void test1() {
  char *buffer = malloc(PGSIZE);
    if (buffer == 0) {
        printf("ERROR: Failed to allocate buffer\n");
        exit(1);
    }
    memset(buffer, 0, PGSIZE);
    *((uint32 *)buffer) = NCHILDREN << 16; 
    int dad_pid = getpid();
    int child_index, my_pid;

    for (child_index = 0; child_index < NCHILDREN; child_index++) {
        my_pid = fork();
        if (my_pid < 0) {
            printf("ERROR: Fork failed\n");
            exit(1);
        }
        if (my_pid == 0) break;  
    }

    if (my_pid == 0) {
        char *sh_buffer = map_shared_pages(dad_pid, getpid(), buffer, PGSIZE);
        if (sh_buffer == (char *)-1) {
            printf("ERROR: Child %d failed to map shared memory\n", child_index);
            exit(1);
        }

        // Add small delays for different children to create concurrency
        if (child_index > 0) {
           // sleep(child_index);  // Stagger child start times
        }

        int message_count = (child_index == 0) ? 20 : 5;
        
        for (int i = 0; i < message_count; i++) {
            char msg[28];
            build_msg(i, child_index,msg);            
            int result = write_log(sh_buffer, child_index, msg);
            if (result == -1) {
                break;
            }
            
            // Small delay between messages to allow interleaving
            // if (i % 3 == 0) sleep(1);
        }
        custom_exit(sh_buffer);
        unmap_shared_pages(getpid(), sh_buffer, PGSIZE);

        exit(0);
        
    } else {
        // Parent process - continuous reading
      uint32  num_of_children;
      do {
        num_of_children = *(uint32 *)buffer >> 16;
        read_log(buffer);

     } while (num_of_children);
          
        
        
        // int consecutive_empty_reads = 0;
        // int total_messages_read = 0;
        // while (consecutive_empty_reads < IDLE_THRESHOLD) {
        //     int messages_read = read_log(buffer);
        //     total_messages_read += messages_read;
            
        //     if (messages_read > 0) {
        //         consecutive_empty_reads = 0;  // Reset counter
        //     } else {
        //         consecutive_empty_reads++;
        //     }
            
        // }

        printf("Parent finished reading total messages\n");
        
    }

    free(buffer);
    exit(0);
}

void test2() {
  printf("============= test 2 =================================\n");
  char *buffer = malloc(PGSIZE);
    if (buffer == 0) {
        printf("ERROR: Failed to allocate buffer\n");
        exit(1);
    }
    memset(buffer, 0, PGSIZE);
    *((uint32 *)buffer) = 1 << 16; 
    int dad_pid = getpid();
    int my_pid;
    printf("ready\n");
    my_pid = fork();
    if (my_pid < 0) {
        printf("ERROR: Fork failed\n");
        exit(1);
    } 
    

    if (my_pid == 0) {
        char *sh_buffer = map_shared_pages(dad_pid, getpid(), buffer, PGSIZE);
        if (sh_buffer == (char *)-1) {
            printf("ERROR: Child failed to map shared memory\n");
            exit(1);
        }
        char msg[1001];
        memset(msg,1,1001);
        write_log(sh_buffer,1,msg);
        custom_exit(sh_buffer);
        unmap_shared_pages(getpid(), sh_buffer, PGSIZE);

        exit(0);
        
    } else {
        // Parent process - continuous reading
      uint32  num_of_children;
      do {
        num_of_children = *(uint32 *)buffer >> 16;
        read_log(buffer);

     } while (num_of_children);

        printf("Parent finished reading total messages\n");
        
    }

    free(buffer);
    exit(0);
}

int main(int argc, char *argv[]) {
  printf("main\n");
  // test1();
  test2();

    
  return 0;
}