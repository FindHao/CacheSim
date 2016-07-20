#include <iostream>
#include "CacheSim.h"
using namespace std;


int main() {
    CacheSim cache =CacheSim(0x8000,32,4);
    char  test_case [100]= "/home/find/ddown/traces/gcc.trace";
    //32KB的cache大小，这里的初始化
    // 默认采用一个测试文件，采用写回法
//    for (int k = CACHE_SWAP_FIFO; k < CACHE_SWAP_MAX; ++k) {
//        cache->swap_style = k;
//
//    }
    cache.load_trace(test_case);

    return 0;
}