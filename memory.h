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
                     char *content, int &hit, int &time);
  void BuildMemory();
    void PrefetchAlgorithm(uint64_t prefetch_addr);

 private:
  DISALLOW_COPY_AND_ASSIGN(Memory);
};

#endif //CACHE_MEMORY_H_
