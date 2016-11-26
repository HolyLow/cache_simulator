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

void Cache::printEntry(CacheEntry* entry){
  printf("addr=%lx, tag=%lx, valid=%d, block[0]=%d\n", entry, entry->tag, entry->valid, entry->block[0]);
}

void Cache::printSet(CacheSet* set, int associativity){
  printf("set_index = %d, associativity=%d\n", set->index, associativity);
  printf("---------------------------------------------\n");
  CacheEntry *current = set->head;
  while(current != NULL){
    this->printEntry(current);
    current = current->next;
  }
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
    this->cacheset[i].head  = &this->cacheset[i].entry[0];
    this->cacheset[i].tail  = &this->cacheset[i].entry[associativity-1];
    // for every cache entry
    for(int j = 0; j < associativity; j++){
      this->cacheset[i].entry[j].valid = FALSE;
      this->cacheset[i].entry[j].write_back = FALSE;
      // pre
      if (j == 0)
         this->cacheset[i].entry[j].pre = NULL;
      else
         this->cacheset[i].entry[j].pre = &this->cacheset[i].entry[j-1];
      // next
      if (j == associativity-1)
         this->cacheset[i].entry[j].next = NULL;
      else
         this->cacheset[i].entry[j].next = &this->cacheset[i].entry[j+1];
      this->cacheset[i].entry[j].block = new char[block_size];
    }
  }
}

void Cache::HandleRequest(uint64_t addr, int bytes, int read,
                          char *content, int &hit, int &time, char* &block) {
  hit = 0;
  time = 0;
  stats_.access_counter++;
  // parse address
  int block_offset_bits = log2(this->config_.blocksize);
  int set_index_bits    = log2(this->config_.set_num);
  uint64_t block_offset = ONES(block_offset_bits-1, 0) & addr;
  uint64_t set_index    = (ONES(block_offset_bits+set_index_bits-1, block_offset_bits) & addr) >> block_offset_bits;
  uint64_t tag          = (ONES(31, block_offset_bits+set_index_bits) & addr) >> (block_offset_bits+set_index_bits);

  //printf("block_offset_bits=%d, set_index_bits=%d\n", block_offset_bits, set_index_bits);
  //printf("read=%d, block_offset=%lx, set_index=%lx, tag=%lx\n", read, block_offset, set_index, tag);

  //printSet(&this->cacheset[set_index], this->config_.associativity);

  // read
  if(read == TRUE){

    CacheEntry* entry = FindEntry(set_index, tag);
    // miss
    if(entry == NULL){
      // Fetch from lower layer
      int lower_hit, lower_time;
      lower_->HandleRequest(addr, bytes, read, content,
                          lower_hit, lower_time, block);
      hit = 0;
      time += latency_.bus_latency + lower_time;
      stats_.access_time += latency_.bus_latency;
      stats_.miss_num++;
    }
    // hit
    else {
      // copy to content

      for(int i = 0; i < bytes; i++)
        content[i] = entry->block[i];
      block = entry->block;
      hit = 1;
      time += latency_.bus_latency + latency_.hit_latency;
      stats_.access_time += time;
      return;
    }

    // return back, write content(block from the lower layer) to this layer
    if(hit == 0){
      char *temp_block = LRUreplacement(set_index, tag, block);

      //printf("temp_block=%x\n", temp_block);
      // write back
      if(temp_block != NULL){
        int lower_hit, lower_time;
        lower_->HandleRequest(addr, this->config_.blocksize, FALSE, temp_block,
                          lower_hit, lower_time, block);
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
      if (this->config_.write_allocate == TRUE){
        // read
        char update_content[64];  // avoid data lost of content
        this->HandleRequest(addr, bytes, TRUE, update_content,
                          hit, time, block);
        // write with hit
        this->HandleRequest(addr, bytes, FALSE, content,
                          hit, time, block);
        hit = 0;
        stats_.miss_num++;
      }
      // no write-allocate
      else{
        // directly write lower layer
        int lower_hit, lower_time;
        lower_->HandleRequest(addr, bytes, FALSE, content,
                          lower_hit, lower_time, block);
        hit = 0;
        time += latency_.bus_latency + lower_time;
        stats_.access_time += latency_.bus_latency;
      }
    }
    // hit
    else{
      // write-through
      if (this->config_.write_through){
        // write current layer
        for(int i = 0; i < bytes; i++)
          entry->block[block_offset+i] = content[i];
        // write to lower layer
        int lower_hit, lower_time;
        lower_->HandleRequest(addr, bytes, read, content,
                          lower_hit, lower_time, block);
        hit = 1;
        time += latency_.bus_latency + latency_.hit_latency + lower_time;
        stats_.access_time += latency_.bus_latency + latency_.hit_latency;
      }
      // write-back
      else{
        // write current layer
        for(int i = 0; i < bytes; i++)
          entry->block[block_offset+i] = content[i];
        hit = 1;
        entry->write_back = TRUE;
        time += latency_.bus_latency + latency_.hit_latency;
        stats_.access_time += latency_.bus_latency + latency_.hit_latency;
      }

    }
  }

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

int Cache::PrefetchDecision() {
  return FALSE;
}

void Cache::PrefetchAlgorithm() {
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

// get from tail, save to head
char* Cache::LRUreplacement(uint64_t set_index, uint64_t tag, char* &block){

  CacheSet *replace_set = &this->cacheset[set_index];
  CacheEntry *replace_entry = replace_set->tail;

  char *temp_block = new char[this->config_.blocksize];
  //memcpy(temp_block, replace_entry->block, this->config_.blocksize);
  //memcpy(replace_entry->block, block, this->config_.blocksize);

  // linked list arrangement
  replace_set->tail = replace_entry->pre;
  replace_entry->pre->next = NULL;
  replace_entry->pre = NULL;
  replace_entry->next = replace_set->head;
  replace_set->head->pre = replace_entry;
  replace_set->head = replace_entry;
  // set tag valid
  replace_entry->valid = TRUE;
  replace_entry->tag = tag;
  block = replace_entry->block;
  //check whether write back
  if (replace_entry->write_back == TRUE){
    replace_entry->write_back == FALSE;
    return temp_block;
  }
  return NULL;
}
