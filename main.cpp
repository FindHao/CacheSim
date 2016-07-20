#include <iostream>
#include "CacheSim.h"
using namespace std;


int main() {
    char  test_case [100]= "/home/find/ddown/traces/gcc.trace";

    int line_size[] = {32, 64, 128};
    int ways[] = {1, 2, 4, 8, 12, 16};
    int i,j;
    CacheSim *cache;
    int k = 0;
//    for (i=0; i<sizeof(line_size)/sizeof(int); i++){
//        for (j=0; j<sizeof(ways)/sizeof(int); j++){
//            for (int k = CACHE_SWAP_FIFO; k < CACHE_SWAP_MAX; ++k) {
    printf("line_size: %d\t mapping ways %d \t swap_style %d \n", line_size[1], ways[3], CACHE_SWAP_FIFO);
    cache = new CacheSim(0x8000, line_size[0], ways[3]);
    cache->set_swap_style(CACHE_SWAP_FIFO);
    cache->load_trace(test_case);
    delete cache;
//            }
//        }
//    }
    return 0;
}