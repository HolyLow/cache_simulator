#ifndef CACHE_MEMORY_H_
#define CACHE_MEMORY_H_


#include <stdint.h>
#include "storage.h"
#include "def.h"

class Memory: public Storage {
 public:
  Memory() {}
  ~Memory() {}

  // Main access process
  void HandleRequest(uint64_t addr, int bytes, int read,
                     char *content, int &hit, int &time, char* &block);
  void BuildMemory();

 private:
  // Memory implement
 char *memory;

  DISALLOW_COPY_AND_ASSIGN(Memory);
};

#endif //CACHE_MEMORY_H_
