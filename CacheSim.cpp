//
// Created by find on 16-7-19.
//
#include "CacheSim.h"

CacheSim::CacheSim() {

}

int CacheSim::check_cache_hit(CacheSim *cache, _u32 set_base, _u32 addr) {
    /**循环查找当前set，通过tag匹配，查看当前地址是否在cache中*/
    _u32 i, tag;
    for (i = 0; i < cache->cache_set_size; ++i) {
        tag = cache->caches[set_base + i].tag;
        if ((tag & CACHE_FLAG_VAILD) && ((tag & cache->cache_tag_mask) == (addr & cache->cache_tag_mask))) {
            return set_base + i;
        }
    }
    return -1;
}

int CacheSim::get_cache_free_line(CacheSim *cache, _u32 set_base, _u32 addr) {
    _u32 i, tag;
    // TODO: min_count??
    _u32 free_index;
    /**从当前cache set里找可用的空闲line，可用：脏数据，空闲数据
     * cache_free_num是统计的整个cache的可用块*/
    for (i = 0; i < cache_set_size; ++i) {
        tag = cache->caches[set_base + i].tag;
        //这里是正确的，，肯定要找invaild cache line...
        if (!(tag & CACHE_FLAG_VAILD)) {
            if (cache->cache_free_num > 0)
                cache->cache_free_num--;
            return set_base + i;
        }
    }
    /**没有可用line，则执行替换算法*/
    


}