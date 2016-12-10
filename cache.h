#ifndef CACHE_CACHE_H_
#define CACHE_CACHE_H_

#include <stdint.h>
#include <vector>
#include <algorithm>
#include "storage.h"



typedef struct CacheConfig_ {
  int size;
  int associativity;
  int blocksize;
  int set_num; // Number of cache sets
  int write_through; // 0|1 for back|through
  int write_allocate; // 0|1 for no-alc|alc
  bool pre_fetch;
  bool bypass;
} CacheConfig;

typedef struct CacheEntry_{
  bool valid;
  uint64_t addr;   // for write back
  uint64_t tag;
  bool write_back; // whether to write back when doing replacement
  uint64_t latest_visit_offset;

} CacheEntry;

// LRUList fo r LRU
class LRUList{
  public:
    LRUList(){}
    ~LRUList(){}

    // init
    void init(CacheEntry* StartEntry, int associativity);
    // delete entry
    CacheEntry* delete_back();
    // add entry
    void add_front(CacheEntry*);
    // refresh
    void refresh(CacheEntry*);
    // replacement, return whether write back
    uint64_t replacement(uint64_t addr, uint64_t tag, uint64_t &evicted_tag);
  private:
    // store the addr of every entry
    std::vector<CacheEntry*> v;
};

// block state, for LIRS
typedef struct BlockState_{
  uint64_t tag;
  bool LIR;       // 1 LIR 0 HIR
  bool resident;
  CacheEntry* entry;
} BlockState;

class LIRS{
  public:
    LIRS(){}
    ~LIRS(){}
 
    /* LIRSStack */
    void Stack_push(BlockState*);
    BlockState* Stack_pop();
    BlockState* Stack_find(uint64_t tag);
    BlockState* Stack_delete(uint64_t tag);
    void Stack_prune();

    /* ListQ */
    BlockState* ListQ_find(uint64_t tag);
    BlockState* ListQ_delete(uint64_t tag);
    void ListQ_push(BlockState*);
    BlockState* ListQ_pop();

    /* function */
    // init
    void init(CacheEntry* StartEntry, int associativity);
    // refresh when hit
    void refresh(uint64_t tag);
    // replacement, return whether write back
    uint64_t replacement(uint64_t addr, uint64_t tag, uint64_t &evicted_tag);
    // print for debug
    void print();

  private:
    int LIR_num;
    int HIR_num;
    std::vector<BlockState*> Stack;
    std::vector<BlockState*> ListQ;
};

typedef struct CacheSet_{
  unsigned int index;
  CacheEntry* entry;
  int empty_num;
  uint64_t last_evicted_tag;
  // linked list for LRU
  // every time get entry from tail and insert to head
  LRUList lru;

  // LIRS algorithm
  LIRS lirs;
} CacheSet;


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
                     char *content, int &hit, int &time);

  void PrefetchAlgorithm(uint64_t prefetch_addr);
 private:
  // Bypassing
  int BypassDecision();
  // Partitioning
  void PartitionAlgorithm();
  // Replacement
  int ReplaceDecision();
  void ReplaceAlgorithm();
  // Prefetching
  void PrefetchDecision(uint64_t set_index, uint64_t tag, uint64_t current_visit_offset);

  CacheEntry* FindEntry(uint64_t set_index, uint64_t tag);

  // print, for debug
  void printEntry(CacheEntry* entry);
  void printSet(CacheSet* set, int associativity);

  CacheConfig config_;
  Storage *lower_;

  // cache implement
  CacheSet *cacheset;
  uint64_t current_visit_addr;

  DISALLOW_COPY_AND_ASSIGN(Cache);
};

#endif //CACHE_CACHE_H_
