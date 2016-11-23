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
    while(tmp & 1 == 0)
        tmp = (unsigned int)tmp >> 1;
    return (tmp >> 1 == 0);
}

// get all kinds of settings from the command line
void GetSettings(int& argc, char *argv[], int& levelNum, CacheConfig* cache_config, FILE*& input)
{
    int i, j, argCount = 1;
    for(argc--, argv++; argc > 0 ; argc -= argCount, argv -= argCount)
    {
        argCount = 1;
        if(!strcmp(*argv, "-f")) // set cache level num
        {
            CHECK(argc > 1, "no input file following -f");
            input = fopen(*(argv + 1), "r");
            CHECK(input != NULL, "invalid input file");
            argCount = 2;
        }
        if(!strcmp(*argv, "-l")) // set cache level num
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
            CHECK(argc > levelNum, "no enough associativity num following -s");
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
                cache_config[i].write_allocate = j & 2; // second least significant bit represents whether to write allocate
            }
            argCount = levelNum + 1;
        }
    }
    CHECK(input != NULL, "input file not provided");
}
int main(int argc, char *argv[])
{

    CacheConfig cache_config[MAXLEVEL + 1];
    int i, levelNum = 1;
    FILE* input = NULL;
    GetSettings(argc, argv, levelNum, cache_config, input);

    Memory m;
    Cache l[MAXLEVEL + 1];         // for convenience, l[1] is the highest cache, and l[0] is not used
    for(i = 1; i < levelNum; i++)
        l[i].SetLower(&l[i + 1]);
    l[levelNum].SetLower(&m);

    // set storageStatus and Configulation of memory and cache
    StorageStats s;
    s.access_time = 0;
    m.SetStats(s);
    for(i = 1; i <= levelNum; i++)
    {
        l[i].SetStats(s);
        l[i].SetConfig(cache_config[i]);
    }

    // set latency of memory and cache
    StorageLatency ml;
    ml.bus_latency = 6;
    ml.hit_latency = 100;
    m.SetLatency(ml);

    StorageLatency ll;
    ll.bus_latency = 3;
    ll.hit_latency = 10;

    for(i = 1; i <= levelNum; i++)
    {
        l[i].SetLatency(ll);
    }
    int hit, time;
    char content[64];
    char ch_wORr;
    uint64_t addr;
    printf("still ok here, before fscanf!\n");
    while(fscanf(input, "%c%I64u", &ch_wORr, &addr) != EOF)
    {
        int bl_wORr = (ch_wORr == 'w' ? 0 : 1);
        l[1].HandleRequest(addr, 1, bl_wORr, content, hit, time);
        printf("Request access time: %dns\n", time);
        // l1.HandleRequest(1024, 0, 1, content, hit, time);
        // printf("Request access time: %dns\n", time);
    }

    l[1].GetStats(s);
    printf("Total L1 access time: %dns\n", s.access_time);
    m.GetStats(s);
    printf("Total Memory access time: %dns\n", s.access_time);
    fclose(input);
    return 0;
}
