#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/stat.h"
#include "user.h"

#define NCHILDREN 4
typedef unsigned int uint32;

// search for a free segment and write to it
int write_log(char *buf, int index, const char *msg) {
  int len = strlen(msg);
  char *p = buf;

  //if (len >= (1 << 16)) return -1;

  while (p + 4 + len < buf + PGSIZE) {
    uint32 *header = (uint32 *)p;
    uint32 expected = 0;
    uint32 new_header = (index << 16) | (uint16)len;  // casting to prevent too big len(overflow to index's bits)

    if (__sync_val_compare_and_swap(header, expected, new_header) == expected) {
      memmove(p + 4, msg, len);
      *((uint32 *)p) = new_header | 0x80000000; // Mark the msb with 1 - finish writing
      return 0;
    }
    uint32 jump = *((uint32 *)p) & 0xFFFF;
    p += 4 + jump;
    p = (char *)(((uint64)p + 3) & ~3); // align
  }
  return -1;
}

void read_log(char *buf) {
  char *p = buf;
  uint32 header;
  uint16 child_id, len;

  while (1) {
    if (p + 4 >= buf + PGSIZE)
      break;

    header = *(uint32 *)p;
    child_id = header >> 16;
    len = header & 0xFFFF;

    if (header != 0 && (header >> 16) != 0xFFFF && (header & 0x80000000) == 0) {

      if (p + 4 + len >= buf + PGSIZE)
        break;
      
      char msg[len+1];
      memmove(msg, p + 4, len);
      msg[len] = '\0';
      printf("Parent received from child %d: %s\n", child_id, msg);

      *((uint32 *)p) = (0xFFFF << 16) | len;
    }
    p += 4 + len;
    p = (char *)(((uint64)p + 3) & ~3); // align to 4 bytes
  }
}
/*
void custom_exit(char* buf){
  uint32 *p = (uint32 *)buf;
  uint32 expected, new_val;
  do{
    expected = *p;
    new_val = expected-1;
  }while(__sync_val_compare_and_swap(p, expected, new_val) != expected);
}
*/
void custom_exit(char *buf) {
  __sync_fetch_and_sub((uint32 *)buf, 1);
}

int test1(){
  char* buffer = malloc(PGSIZE/8);
  memset(buffer, 0, PGSIZE);
  int dad_pid = getpid();
  int index, my_pid;

  // Use the first header to flag the number of alive children - len == 0
  *((uint32 *)buffer) = NCHILDREN << 16;

  for(index = 0; index < NCHILDREN; index++){
      my_pid = fork();
      if(my_pid < 0) return -1;
      if(my_pid == 0) break;
  }
  // child - share buffer
  if(my_pid == 0){
      char* sh_buffer = map_shared_pages(dad_pid, my_pid, buffer, PGSIZE);
      write_log(sh_buffer, index,"hello my name is migel" );
  }
  else {
    while (*(uint32*)buffer != 0) {
      read_log(buffer);
    }
  }
  return 0;
  
}

int main(int argc, char *argv[]){
    return test1();
}
