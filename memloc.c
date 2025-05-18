#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winnt.h>

typedef char ALIGN[16];

union header {
  struct {
    size_t size;
    unsigned is_free;
    union header *next;
  } s;
  ALIGN stub;
};
typedef union header header_t;

header_t *head, *tail;

int main(int argc, char *argv[]) { return EXIT_SUCCESS; }

header_t *get_free_block(size_t size) {
  header_t *curr = head;
  while (curr) {
    if (curr->s.is_free && curr->s.size >= size)
      return curr;
    curr = curr->s.next;
  }
  return NULL;
}

// This is a simple windows malloc based on
// https://arjunsreedharan.org/post/148675821737/memory-allocators-101-write-a-simple-memory
// This is my best attemp at porting the article to windows
// I don't expect this to work and you shouldn't as well
// Use at your own risk
// returns the byte right after the header, the headers shouldn't even be hinted
// to exist
void *wmalloc(size_t size) {
  size_t total_size;
  header_t *header;
  LPVOID block;

  if (!size)
    return NULL;

  header = get_free_block(size);

  if (header) {
    header->s.is_free = 0;
    return (void *)(header + 1);
  }
  total_size = sizeof(header_t) + size;

  block =
      VirtualAlloc(NULL, total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

  if (block == (void *)-1) {
    return NULL;
  }
  header = block;
  header->s.size = size;
  header->s.is_free = 0;
  header->s.next = NULL;
  if (!head)
    head = header;
  if (tail)
    tail->s.next = header;
  tail = header;

  return (void *)(header + 1);
}

void wfree(void *block) {
  header_t *header, *tmp;
  void *programbreak; // probably won't even work, windows has it's own methods

  if (!block)
    return;
  header = (header_t *)block - 1; // cool
  programbreak =
      VirtualAlloc(NULL, 0, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  if ((char *)block + header->s.size == programbreak) {
    if (head == tail)
      head = tail = NULL;
    else {
      tmp = head;
      while (tmp) {
        if (tmp->s.next == tail) {
          tmp->s.next = NULL;
          tail = tmp;
        }
        tmp = tmp->s.next;
      }
    }

    VirtualAlloc(NULL, 0 - sizeof(header_t) - header->s.size,
                 MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    return;
  }
  header->s.is_free = 1;
}

void *wcalloc(size_t num, size_t nsize) {
  size_t size;
  void *block;
  if (!num || !nsize)
    return NULL;
  size = num * nsize;
  // check mul overflow
  if (nsize != size / num)
    return NULL;
  block = wmalloc(size);
  if (!block)
    return NULL;
  memset(block, 0, size);
  return block;
}

void *realloc(void *block, size_t size) {
  header_t *header;
  void *ret;
  if (!block || !size)
    return wmalloc(size);
  header = (header_t *)block - 1;
  if (header->s.size == size)
    return block;
  ret = wmalloc(size);
  if (ret) {
    memcpy(ret, block, header->s.size);
    wfree(block);
  }
  return ret;
}
