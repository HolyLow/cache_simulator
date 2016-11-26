#ifndef CACHE_CACHE_H_
#define CACHE_CACHE_H_

#include <stdint.h>
#include "storage.h"

typedef struct CacheConfig_ {
  int size;
  int associativity;
  int blocksize;
  int set_num; // Number of cache sets
  int write_through; // 0|1 for back|through
  int write_allocate; // 0|1 for no-alc|alc
} CacheConfig;

typedef struct CacheEntry_{
  bool valid;
  unsigned int tag;
  bool write_back; // whether to write back when doing replacement
  char *block;
  // linked list for LRU algorithm
  CacheEntry_* pre;
  CacheEntry_* next;
} CacheEntry;

typedef struct CacheSet_{
  unsigned int index;
  CacheEntry* entry;
  // linked list for LRU
  // every time get entry from tail and insert to head
  CacheEntry* head;
  CacheEntry* tail;
}CacheSet;


class Cache: public Storage {
 public:
  Cache() {}
  ~Cache() {}

  // Sets & Gets
  void SetConfig(CacheConfig cc){config_ = cc;}
  void GetConfig(CacheConfig& cc){cc = config_;}
  void SetLower(Storage *ll) { lower_ = ll; }
  // Build cache
  void BuildCache();
  // Main access process
  void HandleRequest(uint64_t addr, int bytes, int read,
                     char *content, int &hit, int &time, char* &block);

 private:
  // Bypassing
  int BypassDecision();
  // Partitioning
  void PartitionAlgorithm();
  // Replacement
  int ReplaceDecision();
  void ReplaceAlgorithm();
  // Prefetching
  int PrefetchDecision();
  void PrefetchAlgorithm();

  CacheEntry* FindEntry(uint64_t set_index, uint64_t tag);
  // LRU replacement algorithm, assume that block size are the same in cache
  char* LRUreplacement(uint64_t set_index, uint64_t tag, char* &block);

  // print, for debug
  void printEntry(CacheEntry* entry);
  void printSet(CacheSet* set, int associativity);

  CacheConfig config_;
  Storage *lower_;

  // cache implement
  CacheSet *cacheset;

  DISALLOW_COPY_AND_ASSIGN(Cache);
};

#endif //CACHE_CACHE_H_
