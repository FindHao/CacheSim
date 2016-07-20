#include <iostream>
#include "CacheSim.h"
using namespace std;


int main() {
    CacheSim * cache;
    char  test_case [100]= "gcc.trace";
    int i,j,k;
    //32KB的cache大小，这里的初始化
    cache->init_cache_sim(0x8000);
    // 默认采用一个测试文件，采用写回法
//    for (int k = CACHE_SWAP_FIFO; k < CACHE_SWAP_MAX; ++k) {
//        cache->swap_style = k;
//
//    }
    cache->swap_style = CACHE_SWAP_RAND;
    cache->do_test(test_case);
    //写好析构后删除
    cache->free_cache_sim();
    return 0;
}