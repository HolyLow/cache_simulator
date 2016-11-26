#include "memory.h"


void Memory::HandleRequest(uint64_t addr, int bytes, int read,
                          char *content, int &hit, int &time, char* &block) {
    stats_.access_counter++;
  	// read
 	if(read == TRUE){
/*	    for(int i = 0; i < bytes; i++)
	        content[i] = this->memory[addr+i];
	    block = &this->memory[addr];
*/		hit = 1;
	  	time = latency_.hit_latency + latency_.bus_latency;
	  	stats_.access_time += time;
 	}
 	// write
 	else {
/* 	    for(int i = 0; i < bytes; i++)
	        this->memory[addr+i] = content[i];
	    block = &this->memory[addr];
*/		hit = 1;
	  	time = latency_.hit_latency + latency_.bus_latency;
	  	stats_.access_time += time;
 	}


}

void Memory::BuildMemory(){
	this->memory = new char[1<<27];
}
