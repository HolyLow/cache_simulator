#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cache.h"
#include "memory.h"
#define MAXLEVEL 3
void CHECK(bool tmp, const char* message)
{
    if(!tmp)
    {
        printf("%s\n", message);
        printf("program exits with error\n");
        exit(0);
    }
}


// check if it's the power of 2
bool is2power(int tmp)
{
    if(tmp == 0)
        return false;
    while((tmp & 1) == 0)
        tmp = (unsigned int)tmp >> 1;
    return ((tmp >> 1) == 0);
}

// init the default settings
void InitDefaultSettings(StorageStats& storage_stats, StorageLatency& latency_m, StorageLatency* latency_c, CacheConfig* cache_config, int& levelNum)
{
    storage_stats.access_counter = 0;
    storage_stats.miss_num = 0;
    storage_stats.access_time = 0;
    storage_stats.replace_num = 0;
    storage_stats.fetch_num = 0;
    storage_stats.prefetch_num = 0;

    latency_m.bus_latency = 6;
    latency_m.hit_latency = 100;

    int i;
    for(i = 1; i <= MAXLEVEL; i++)
    {
        latency_c[i].bus_latency = 3;
        latency_c[i].hit_latency = 10;
    }
    latency_c[1].bus_latency = 2;
	latency_c[2].bus_latency = 2;
	latency_c[3].bus_latency = 4;

	latency_c[1].hit_latency = 4;
	latency_c[2].hit_latency = 5;
	latency_c[3].hit_latency = 11;

    for(i = 1; i <= MAXLEVEL; i++)
    {
        cache_config[i].size = 32;
        cache_config[i].associativity = 8;
        cache_config[i].write_through = 0;
        cache_config[i].write_allocate = 0;
        cache_config[i].blocksize = 64;
    }

    cache_config[1].size = 32;
	cache_config[2].size = 256;
	cache_config[3].size = 8 * 1024;

	cache_config[1].associativity = 8;
	cache_config[2].associativity = 8;
	cache_config[3].associativity = 8;

	cache_config[1].blocksize = 64;
	cache_config[2].blocksize = 64;
	cache_config[3].blocksize = 64;

    levelNum = 1;


}

// get all kinds of settings from the command line
void GetSettings(int& argc, char *argv[], int& levelNum, CacheConfig* cache_config, StorageLatency* latency_c, FILE*& input)
{
    int i, j, argCount = 1;
    for(argc--, argv++; argc > 0 ; argc -= argCount, argv += argCount)
    {
        argCount = 1;
        if(!strcmp(*argv, "-f")) // set cache level num
        {
            CHECK(argc > 1, "no input file following -f");
            input = fopen(*(argv + 1), "r");
            CHECK(input != NULL, "invalid input file");
            argCount = 2;
        }
        else if(!strcmp(*argv, "-l")) // set cache level num
        {
            CHECK(argc > 1, "no level num following -l");
            levelNum = atoi(*(argv + 1));
            CHECK(levelNum <= MAXLEVEL && levelNum > 0, "invalid cache level num");
            argCount = 2;
        }
        else if(!strcmp(*argv, "-s")) // set cache size
        {
            CHECK(argc > levelNum, "no enough size num following -s");
            for(i = 1; i <= levelNum; i++)
            {
                cache_config[i].size = atoi(*(argv + i));
                CHECK(is2power(cache_config[i].size), "invalid cache size");
            }
            argCount = levelNum + 1;
        }
        else if(!strcmp(*argv, "-a")) // set cache associativity
        {
            CHECK(argc > levelNum, "no enough associativity num following -a");
            for(i = 1; i <= levelNum; i++)
            {
                cache_config[i].associativity = atoi(*(argv + i));
                CHECK(is2power(cache_config[i].associativity), "invalid associativity");
            }
            argCount = levelNum + 1;
        }
        else if(!strcmp(*argv, "-p")) // set cache policy
        {
            CHECK(argc > levelNum, "no enough policy num following -p");
            for(i = 1; i <= levelNum; i++)
            {
                j = atoi(*(argv + i));
                CHECK(j < 4 && j >= 0, "invalid policy");
                cache_config[i].write_through = j & 1;  // least significant bit represents whether to write through
                cache_config[i].write_allocate = (j>>1) & 1; // second least significant bit represents whether to write allocate
            }
            argCount = levelNum + 1;
        }
        else if(!strcmp(*argv, "-b")) // set block size
        {
            CHECK(argc > levelNum, "no enough block size num following -b");
            for(i = 1; i <= levelNum; i++)
            {
                cache_config[i].blocksize = atoi(*(argv + i));
                CHECK(is2power(cache_config[i].blocksize), "invalid block size");
            }
            argCount = levelNum + 1;
        }
        else if(!strcmp(*argv, "-hl")) // set hit latency
        {
            CHECK(argc > levelNum, "no enough hit latency num following -hl");
            for(i = 1; i <= levelNum; i++)
            {
                latency_c[i].hit_latency = atoi(*(argv + i));
                CHECK(latency_c[i].hit_latency > 0, "invalid cache hit latency");
            }
            argCount = levelNum + 1;
        }
        else if(!strcmp(*argv, "-bl")) // set hit latency
        {
            CHECK(argc > levelNum, "no enough bus latency num following -bl");
            for(i = 1; i <= levelNum; i++)
            {
                latency_c[i].bus_latency = atoi(*(argv + i));
                CHECK(latency_c[i].bus_latency > 0, "invalid cache bus latency");
            }
            argCount = levelNum + 1;
        }
        else
            CHECK(false, "invalid command!");
    }
    CHECK(input != NULL, "input file not provided");
    for(i = 1; i <= levelNum; i++)
    {
        CHECK(cache_config[i].blocksize * cache_config[i].associativity <= cache_config[i].size * 1024,
            "invalid cache setting, as cache size can't match the associativity and block size");
    }
}

// set settings to get the wanted cache and memory
void SetSettings(Memory& m, Cache* l, StorageStats& storage_stats, CacheConfig* cache_config, StorageLatency& latency_m, StorageLatency* latency_c, int levelNum)
{
    // connect
    int i;
    for(i = 1; i < levelNum; i++)
        l[i].SetLower(&l[i + 1]);
    l[levelNum].SetLower(&m);

    // set storageStatus and Configulation of memory and cache
    m.SetStats(storage_stats);
    m.BuildMemory();
    for(i = 1; i <= levelNum; i++)
    {
        l[i].SetStats(storage_stats);
        l[i].SetConfig(cache_config[i]);
        l[i].BuildCache();
    }

    // set latency of memory and cache
    m.SetLatency(latency_m);

    for(i = 1; i <= levelNum; i++)
    {
        l[i].SetLatency(latency_c[i]);
    }
}

int main(int argc, char *argv[])
{

    CacheConfig cache_config[MAXLEVEL + 1];
    int i, levelNum = 1;
    StorageStats storage_stats;
    StorageLatency latency_m, latency_c[MAXLEVEL + 1];
    InitDefaultSettings(storage_stats, latency_m, latency_c, cache_config, levelNum);

    FILE* input = NULL;
    GetSettings(argc, argv, levelNum, cache_config, latency_c, input);

    Memory m;
    Cache l[MAXLEVEL + 1];         // for convenience, l[1] is the highest cache, and l[0] is not used
    SetSettings(m, l, storage_stats, cache_config, latency_m, latency_c, levelNum);


    int hit, time, total_time = 0;
    char content[64];
    char ch_wORr;
    uint64_t addr;
    printf("still ok here, before fscanf!\n");
    int num = 0;
    while(fscanf(input, "%c %lx\n", &ch_wORr, &addr) != EOF)
    {
        content[0] = 0;

        //printf("*************************************************************************\n");
        int bl_wORr;
        if(ch_wORr == 'w'){
            bl_wORr = 0;
            printf("addr=%lx(%lu), read=%c\n",addr, addr, ch_wORr);
            l[1].HandleRequest(addr, 1, bl_wORr, content, hit, time);
            printf("Request access time: %dns\n", time);
            total_time += time;
        }
        else if(ch_wORr == 'r'){
            bl_wORr = 1;
            printf("addr=%lx(%lu), read=%c\n",addr, addr, ch_wORr);
            l[1].HandleRequest(addr, 1, bl_wORr, content, hit, time);
            printf("Request access time: %dns\n", time);
            total_time += time;
        }
        //printf("*************************************************************************\n");

        // l1.HandleRequest(1024, 0, 1, content, hit, time);
        // printf("Request access time: %dns\n", time);

        num++;
    }
    StorageStats s;
    l[1].GetStats(s);
    printf("Total L1 access cycle: %d\n", s.access_counter);
    m.GetStats(s);
    printf("Total Memory access cycle: %d\n", s.access_counter);

    for(i = 1; i <= levelNum; i++)
    {
        printf("-----------CACHE LEVEL %d----------------\n", i);
        l[i].OutputStorage();
        printf("\n");
    }
    printf("total time:%d\n", total_time);
    printf("access num: %d\n", num);
    fclose(input);
    return 0;
}
