#include "cache.h"
#include "def.h"


#define ONES(x,y) (((1<<(x+1))-1)-((1<<y)-1))

// only for the number that is the power of 2
int log2(int temp){
    int num = 0;
    while((temp & 1) == 0){
        temp = (unsigned int)temp >> 1;
        num += 1;
    }
    return num;
}

/************************************************/
/*                                              */
/*                                              */
/*          LRU List                            */
/*                                              */
/*                                              */
/************************************************/


void LRUList::init(CacheEntry* StartEntry, int associativity){
  for(int i = 0; i < associativity; i++){
    this->v.push_back(&StartEntry[i]);
  }
}

CacheEntry* LRUList::delete_back(){
  CacheEntry *entry = this->v.back();
  this->v.pop_back();
  return entry;
}

void LRUList::add_front(CacheEntry* entry){
  this->v.insert(this->v.begin(), entry);
}

void LRUList::refresh(CacheEntry* entry){
  std::vector<CacheEntry*>::iterator iter = std::find(this->v.begin(), this->v.end(), entry);
  if(iter == this->v.end()){
    printf("cannot find entry in LRU refresh!\n");
  } 
  else{
    this->v.erase(iter);
    this->add_front(entry);
  }
}

uint64_t LRUList::replacement(uint64_t addr, uint64_t tag, uint64_t &evicted_tag){
  // get from back
  CacheEntry* entry = this->delete_back();
  uint64_t write_back_addr = entry->addr;
  evicted_tag = entry->tag;
  // set addr, tag, valid
  entry->tag = tag;
  entry->addr = addr;
  entry->valid = TRUE;
  // add to front
  this->add_front(entry);
  // check write back
  if(entry->write_back){
    entry->write_back = FALSE;
    return write_back_addr;
  }
  entry->write_back = FALSE;
  return -1;
}

/************************************************/
/*                                              */
/*                                              */
/*          LIRS                                */
/*                                              */
/*                                              */
/************************************************/


void LIRS::Stack_push(BlockState* state){
  this->Stack.insert(this->Stack.begin(), state);
}

BlockState* LIRS::Stack_pop(){
  BlockState* state = this->Stack.back();
  this->Stack.pop_back();
  return state;
}

BlockState* LIRS::Stack_find(uint64_t tag){
  for(std::vector<BlockState*>::iterator iter = this->Stack.begin(); iter != this->Stack.end(); iter++){
    if((*iter)->tag == tag)
      return *iter;
  } 
  printf("cannot find tag=%lx in LIRS Stack\n", tag);
  return NULL;
}

BlockState* LIRS::Stack_delete(uint64_t tag){
  BlockState* state = this->Stack_find(tag);
  std::vector<BlockState*>::iterator iter = std::find(this->Stack.begin(), this->Stack.end(), state);
  if(iter != this->Stack.end())
    this->Stack.erase(iter);
  return state;
}

void LIRS::Stack_prune(){
  //this->print();
  for(std::vector<BlockState*>::reverse_iterator iter = this->Stack.rbegin(); iter != this->Stack.rend(); iter++ ){
    // if HIR delete
    if(!(*iter)->LIR)
      this->Stack_pop();
    else
      break;
  }

}

BlockState* LIRS::ListQ_find(uint64_t tag){
  for(std::vector<BlockState*>::iterator iter = this->ListQ.begin(); iter != this->ListQ.end(); iter++){
    if((*iter)->tag == tag)
      return *iter;
  } 
  printf("cannot find tag=%lx in LIRS ListQ\n", tag);
  return NULL;
}

BlockState* LIRS::ListQ_delete(uint64_t tag){
  BlockState* state = this->ListQ_find(tag);
  std::vector<BlockState*>::iterator iter = std::find(this->ListQ.begin(), this->ListQ.end(), state);
  if(iter != this->ListQ.end())
    this->ListQ.erase(iter);
  return state;
}

void LIRS::ListQ_push(BlockState* state){
  this->ListQ.insert(this->ListQ.begin(), state);
}

BlockState* LIRS::ListQ_pop(){
  if(this->ListQ.empty())
    return NULL;
  BlockState* state = this->ListQ.back();
  this->ListQ.pop_back();
  return state;  
}

void LIRS::init(CacheEntry* StartEntry, int associativity){
  this->LIR_num = associativity * 0.7;
  this->HIR_num = associativity - this->LIR_num;

  // generate LIR
  for(int i = 0; i < this->LIR_num; i++){
    BlockState* state = new BlockState();
    state->entry = StartEntry;
    StartEntry++;
    state->resident = TRUE;
    state->LIR = TRUE;
    this->Stack_push(state);
  }

  // generate HIR
  for(int i = 0; i < this->HIR_num; i++){
    BlockState* state = new BlockState();
    state->entry = StartEntry;
    StartEntry++;
    state->resident = TRUE;
    state->LIR = FALSE;
    this->Stack_push(state);
    this->ListQ_push(state);
  }

  return;
}

void LIRS::refresh(uint64_t tag){
  BlockState* state = this->Stack_delete(tag);
  // resident-HIR in ListQ
  // push to Stack top
  if(state == NULL){
    state = this->ListQ_delete(tag);
    if(state == NULL){
      printf("state not in Stack and ListQ\n");
      exit(1);
    }
    this->ListQ_push(state);
    this->Stack_push(state);
    return;
  }
  // LIR
  // repush tp Stack top
  if(state->LIR){
    this->Stack_push(state);
    this->Stack_prune();
  }
  // resident-HIR in stack -> LIR
  // replaced LIR -> HIR, removed from Stack, push to ListQ
  else{
    state->LIR = TRUE;
    this->Stack_push(state);
    this->Stack_prune();
    this->ListQ_delete(state->tag);
    BlockState* state = this->Stack_pop();
    state->LIR = FALSE;
    this->ListQ_push(state);
  }

}

uint64_t LIRS::replacement(uint64_t addr, uint64_t tag, uint64_t &evicted_tag){
  // get the block to replace
  BlockState* state = this->ListQ_pop();
  CacheEntry* entry = state->entry;
  uint64_t write_back_addr = entry->addr;
  evicted_tag = entry->tag;
  // replace in
  entry->addr = addr;
  entry->tag = tag;
  entry->valid = TRUE;

  // to-be-replaced block also in Stack
  if(this->Stack_find(state->tag) != NULL){
    state->resident = FALSE;
  }
  
  // new state
  BlockState* new_state = this->Stack_find(tag);
  // if new state not in Stack
  if(new_state == NULL){
    new_state = new BlockState();
    new_state->entry = entry;
    new_state->tag = tag;
    new_state->LIR = FALSE;
    new_state->resident = TRUE;
    this->Stack_push(new_state);
    this->ListQ_push(new_state);
  }
  // new state already in Stack
  else{
    new_state->LIR = TRUE;
    new_state->resident = TRUE;
    this->Stack_delete(new_state->tag);
    this->Stack_push(new_state);
    // pop LIR -> HIR
    BlockState* trans_state = Stack_pop();
    trans_state->LIR = FALSE;
    this->ListQ_push(trans_state);
  }

  // check write back
  if(entry->write_back){
    entry->write_back = FALSE;
    return write_back_addr;
  }
  entry->write_back = FALSE;
  return -1;
}

void LIRS::print(){
  printf("Stack: ");
  for(std::vector<BlockState*>::reverse_iterator iter = this->Stack.rbegin(); iter != this->Stack.rend(); iter++ ){
    printf("%lx|%d ", (*iter)->tag, (*iter)->LIR);
  }
  printf("\n");
  printf("ListQ: ");
  for(std::vector<BlockState*>::reverse_iterator iter = this->ListQ.rbegin(); iter != this->ListQ.rend(); iter++){
    printf("%lx|%d ", (*iter)->tag, (*iter)->LIR);
  }
  printf("\n");
}

/************************************************/
/*                                              */
/*                                              */
/*         Cache                                */
/*                                              */
/*                                              */
/************************************************/

void Cache::printEntry(CacheEntry* entry){
  printf("addr=%lx, tag=%lx, valid=%d\n", entry, entry->tag, entry->valid);
}

void Cache::printSet(CacheSet* set, int associativity){
  printf("set_index = %d, associativity=%d\n", set->index, associativity);
  printf("---------------------------------------------\n");
  
  printf("---------------------------------------------\n");
}


void Cache::BuildCache(){
  int block_size = this->config_.blocksize;    // byte
  int cache_size = this->config_.size * 1024;  // byte
  int associativity = this->config_.associativity;
  int set_num = cache_size / (block_size * associativity);
  this->config_.set_num = set_num;

  this->cacheset = new CacheSet[set_num];
  // for every cache set
  for(int i = 0; i < set_num; i++){
    this->cacheset[i].index = i;
    this->cacheset[i].entry = new CacheEntry[associativity];
    this->cacheset[i].last_evicted_tag = 0;
    this->cacheset[i].empty_num = associativity;
    this->cacheset[i].lru.init(this->cacheset[i].entry, associativity);
    this->cacheset[i].lirs.init(this->cacheset[i].entry, associativity);
    // for every cache entry
    for(int j = 0; j < associativity; j++){
      this->cacheset[i].entry[j].valid = FALSE;
      this->cacheset[i].entry[j].write_back = FALSE;
      this->cacheset[i].entry[j].latest_visit_offset = -1;
    }
  }
}

void Cache::HandleRequest(uint64_t addr, int bytes, int read,
                          char *content, int &hit, int &time) {
  hit = 0;
  time = 0;
  stats_.access_counter++;
  // parse address
  int block_offset_bits = log2(this->config_.blocksize);
  int set_index_bits    = log2(this->config_.set_num);
  uint64_t block_offset = ONES(block_offset_bits-1, 0) & addr;
  uint64_t set_index    = (ONES(block_offset_bits+set_index_bits-1, block_offset_bits) & addr) >> block_offset_bits;
  uint64_t tag          = (ONES(63, block_offset_bits+set_index_bits) & addr) >> (block_offset_bits+set_index_bits);

  //printf("block_offset_bits=%lx, set_index_bits=%lx\n", block_offset_bits, set_index_bits);
  //printf("read=%d, block_offset=%lx, set_index=%lx, tag=%lx\n", read, block_offset, set_index, tag);

  // read
  if(read == TRUE){

    CacheEntry* entry = FindEntry(set_index, tag);
    // miss
    if(entry == NULL){
      // Fetch from lower layer
      int lower_hit, lower_time;
      lower_->HandleRequest(addr, bytes, read, content,
                          lower_hit, lower_time);
      hit = 0;
      time += latency_.bus_latency + latency_.hit_latency + lower_time;
      stats_.access_time += latency_.bus_latency;
      stats_.miss_num++;

      // bypass
      if(config_.bypass && cacheset[set_index].empty_num < 0 && tag != cacheset[set_index].last_evicted_tag){
        time -= latency_.hit_latency;
        // return directely
        return;
      }
 
    }
    // hit
    else {
      // read content
      // copy to content
      hit = 1;
      time += latency_.bus_latency + latency_.hit_latency;
      stats_.access_time += time;
      //this->cacheset[set_index].lru.refresh(entry);
      this->cacheset[set_index].lirs.refresh(tag);
    }

    // return back, write content(block from the lower layer) to this layer
    if(hit == 0){
      
      //uint64_t write_back_addr = this->cacheset[set_index].lru.replacement(addr, tag, cacheset[set_index].last_evicted_tag);
      uint64_t write_back_addr = this->cacheset[set_index].lirs.replacement(addr, tag, cacheset[set_index].last_evicted_tag);
      if(write_back_addr > 0){
        int lower_hit, lower_time;
        lower_->HandleRequest(write_back_addr, this->config_.blocksize, FALSE, content,
                           lower_hit, lower_time);
        // write-back time don't care?
        //time += lower_time;
      }
    }
      
  }

  // write
  else{
    CacheEntry* entry = FindEntry(set_index, tag);
  
    // miss
    if(entry == NULL){
      // write-allocate
      stats_.miss_num++;
      
      // bypass
      if(config_.bypass && cacheset[set_index].empty_num < 0 && tag != cacheset[set_index].last_evicted_tag){
        // directly write lower layer
        int lower_hit, lower_time;
        lower_->HandleRequest(addr, bytes, FALSE, content,
                         lower_hit, lower_time);
        hit = 0;
        time += latency_.bus_latency + lower_time;
        stats_.access_time += latency_.bus_latency;
        // return directely
        return;
      }
  
      if (this->config_.write_allocate == TRUE){
        // read
        char update_content[64];  // avoid data lost of content
        this->HandleRequest(addr, bytes, TRUE, update_content,
                           hit, time);
        stats_.access_counter--;
        stats_.miss_num--;
        // write with hit
        this->HandleRequest(addr, bytes, FALSE, content,
                           hit, time);
        stats_.access_counter--;
        hit = 0;
      }
      // no write-allocate
      else{
        // directly write lower layer
        int lower_hit, lower_time;
        lower_->HandleRequest(addr, bytes, FALSE, content,
                         lower_hit, lower_time);
        hit = 0;
        time += latency_.bus_latency + latency_.hit_latency + lower_time;
        stats_.access_time += latency_.bus_latency;
      }
    }
    // hit
    else{
      // write-through
      if (this->config_.write_through){
        // write current layer

        // write to lower layer
        int lower_hit, lower_time;
        lower_->HandleRequest(addr, bytes, read, content,
                          lower_hit, lower_time);
        hit = 1;
        time += latency_.bus_latency + latency_.hit_latency + lower_time;
        stats_.access_time += latency_.bus_latency + latency_.hit_latency;
      }
      // write-back
      else{
        // write current layer
        hit = 1;
        entry->write_back = TRUE;
        time += latency_.bus_latency + latency_.hit_latency;
        stats_.access_time += latency_.bus_latency + latency_.hit_latency;
      }
      //this->cacheset[set_index].lru.refresh(entry);
      this->cacheset[set_index].lirs.refresh(tag);
    }
  }

  this->cacheset[set_index].empty_num--;

  if(config_.pre_fetch)
    PrefetchDecision(set_index, tag, block_offset);
  CacheEntry* entry = FindEntry(set_index, tag);
  entry->latest_visit_offset = block_offset;


  //printSet(&this->cacheset[set_index], this->config_.associativity);

/*
  // Bypass?
  if (!BypassDecision()) {
    PartitionAlgorithm();
    // Miss?
    if (ReplaceDecision()) {
      // Choose victim
      ReplaceAlgorithm();
    } else {
      // return hit & time
      hit = 1;
      time += latency_.bus_latency + latency_.hit_latency;
      stats_.access_time += time;
      return;
    }
  }
  // Prefetch?
  if (PrefetchDecision()) {
    PrefetchAlgorithm();
  } else {
    // Fetch from lower layer
    int lower_hit, lower_time;
    lower_->HandleRequest(addr, bytes, read, content,
                          lower_hit, lower_time);
    hit = 0;
    time += latency_.bus_latency + lower_time;
    stats_.access_time += latency_.bus_latency;
  }

*/
}

int Cache::BypassDecision() {
  return FALSE;
}

void Cache::PartitionAlgorithm() {
}

int Cache::ReplaceDecision() {
  return TRUE;
  //return FALSE;
}

void Cache::ReplaceAlgorithm(){
}

void Cache::PrefetchDecision(uint64_t set_index, uint64_t tag, uint64_t current_visit_offset) {
    CacheEntry* entry = FindEntry(set_index, tag);
    if(entry == NULL)
    {
        printf("sth has got wrong in prefetch.\n");
        exit(0);
    }
    // int block_offset_bits = log2(this->config_.blocksize);
    //     int set_index_bits    = log2(this->config_.set_num);
    // printf("decide to prefetch down!\n");
    //         // printf("step = %d\n", step);
    // printf("set_index = %d\n", set_index);
    // uint64_t prefetch_addr = (set_index << block_offset_bits) | (tag << (block_offset_bits+set_index_bits));
    // prefetch_addr += 1 << block_offset_bits;
    // PrefetchAlgorithm(prefetch_addr);
    if(entry->latest_visit_offset != -1)
    {
        int step = current_visit_offset - entry->latest_visit_offset;
        int block_offset_bits = log2(this->config_.blocksize);
        int set_index_bits    = log2(this->config_.set_num);
        if(step >= config_.blocksize || step <= -1 * config_.blocksize)
            printf("error: step = %d\n", step);
        if(step + current_visit_offset >= config_.blocksize && FindEntry((set_index + 1) % this->config_.set_num, tag) == NULL)
        {
            printf("decide to prefetch down!\n");
            printf("step = %d\n", step);
            printf("set_index = %ld\n", set_index);
            uint64_t prefetch_addr = (set_index << block_offset_bits) | (tag << (block_offset_bits+set_index_bits));
            prefetch_addr += 1 << block_offset_bits;
            PrefetchAlgorithm(prefetch_addr);
            this->cacheset[set_index].empty_num;
        }
        else if(step + current_visit_offset < 0 && FindEntry((set_index - 1) % this->config_.set_num, tag) == NULL)
        {
            printf("decide to prefetch up!\n");
            printf("step = %d\n", step);
            printf("set_index = %ld\n", set_index);
            uint64_t prefetch_addr = (set_index << block_offset_bits) | (tag << (block_offset_bits+set_index_bits));
            prefetch_addr -= 1 << block_offset_bits;
            PrefetchAlgorithm(prefetch_addr);
            this->cacheset[set_index].empty_num--;
        }

    }
}

void Cache::PrefetchAlgorithm(uint64_t prefetch_addr) {
    int block_offset_bits = log2(this->config_.blocksize);
    int set_index_bits    = log2(this->config_.set_num);
    // uint64_t block_offset = ONES(block_offset_bits-1, 0) & addr;
    uint64_t set_index    = (ONES(block_offset_bits+set_index_bits-1, block_offset_bits) & prefetch_addr) >> block_offset_bits;
    uint64_t tag          = (ONES(63, block_offset_bits+set_index_bits) & prefetch_addr) >> (block_offset_bits+set_index_bits);
    if(FindEntry(set_index, tag) != NULL)
        return;
    stats_.prefetch_num++;
    //this->cacheset[set_index].lru.replacement(prefetch_addr, tag, cacheset[set_index].last_evicted_tag);
    this->cacheset[set_index].lirs.replacement(prefetch_addr, tag, cacheset[set_index].last_evicted_tag);
    //printf("temp_block=%x\n", temp_block);
    // write back
    // if(temp_block != NULL){
    //   int lower_hit, lower_time;
    //   lower_->HandleRequest(this->current_visit_addr, this->config_.blocksize, FALSE, temp_block,
    //                     lower_hit, lower_time);
    // }
    lower_->PrefetchAlgorithm(prefetch_addr);
}

// if not find, return NULL
CacheEntry* Cache::FindEntry(uint64_t set_index, uint64_t tag){
  CacheEntry* target_entry = NULL;
  CacheSet* target_set = &this->cacheset[set_index];
  for(int i = 0; i < this->config_.associativity; i++){
    if(tag == target_set->entry[i].tag && target_set->entry[i].valid == TRUE){
      target_entry = &target_set->entry[i];
      break;
    }
  }
  return target_entry;
}


