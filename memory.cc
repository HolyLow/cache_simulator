#include "memory.h"


void Memory::HandleRequest(uint64_t addr, int bytes, int read,
                          char *content, int &hit, int &time) {
    stats_.access_counter++;
  	// read
 	if(read == TRUE){
 		hit = 1;
	  	time = latency_.hit_latency + latency_.bus_latency;
	  	stats_.access_time += time;
 	}
 	// write
 	else {
 		hit = 1;
	  	time = latency_.hit_latency + latency_.bus_latency;
	  	stats_.access_time += time;
 	}


}

void Memory::BuildMemory(){

}

void Memory::PrefetchAlgorithm(uint64_t prefetch_addr)
{
    return;
}
