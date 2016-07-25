#include <iostream>
#include "CacheSim.h"
using namespace std;


int main() {
    char  test_case [100]= "/home/find/ddown/traces/gcc.trace";

//    int line_size[] = {8, 16, 32, 64, 128};
    int line_size[] = {32};
    int ways[] = {1, 2, 4, 8, 12, 16, 32};
//    int ways[] = {8};
    int cache_size[] = {0x800,0x1000,0x2000,0x4000,0x8000,0x10000,0x20000,0x40000,0x80000};
    int i,j,m;
    CacheSim *cache;
    for (m = 0;m<6;m ++){
        for (i=0; i<sizeof(line_size)/sizeof(int); i++){
            for (j=0; j<sizeof(ways)/sizeof(int); j++){
                for (int k = CACHE_SWAP_FIFO; k < CACHE_SWAP_MAX; ++k) {
                    printf("\ncache_size: %d Bytes\tline_size: %d\t mapping ways %d \t swap_style %d \n", cache_size[m],line_size[i], ways[j], k);
                    cache = new CacheSim(cache_size[m], line_size[i], ways[j]);
                    cache->set_swap_style(k);
                    cache->load_trace(test_case);
                    delete cache;
                }
            }
        }
    }
    return 0;
}