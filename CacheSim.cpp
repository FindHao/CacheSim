//
// Created by find on 16-7-19.
// Cache architect
// memory address  format:
// |tag|组号|组内块号log2()|块内地址 log2(cache line)|
//
#include "CacheSim.h"

CacheSim::CacheSim() {

}

int CacheSim::check_cache_hit(CacheSim *cache, _u32 set_base, _u32 addr) {
    /**循环查找当前set，通过tag匹配，查看当前地址是否在cache中*/
    _u32 i, tag;
    for (i = 0; i < cache->cache_mapping_ways; ++i) {
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
    /**没有可用line，则执行替换算法
     * 原作者完成了随机替换和FIFO替换，LRU的并没有完成。
     * TODO: LRU替换算法
     * FIFO
     * */
    free_index = 0;
    switch (cache->swap_style){
        case CACHE_SWAP_RAND:
            free_index = rand() % cache->cache_set_size;
            break;
        //TODO: 后续再补充这俩算法吧。
        case CACHE_SWAP_FIFO:

            break;
        // TODO
        case CACHE_SWAP_LRU:
            break;
    }
    free_index += set_base;
    //如果原有的cache line是脏数据，写回内存
    if (cache->caches[free_index].tag & CACHE_FLAG_WRITE_BACK){
        cache->caches[free_index].tag &= ~CACHE_FLAG_WRITE_BACK;
        cache->cache_w_count++;
    }
    return free_index;
}
/**将数据写入cache line*/
void CacheSim::set_cache_line(CacheSim *cache, _u32 index, _u32 addr) {
    Cache_Line * line = cache->caches + index;
    // 这里每个line的buf和整个cache类的buf是重复的而且并没有填充内容。
    line->buf = cache->cache_buf + cache->cache_line_size * index;
    // 更新这个line的tag位
    line->tag = addr & ~CACHE_FLAG_MASK;
    // 置有效位为有效。
    line->tag |= CACHE_FLAG_VAILD;
    line->count = cache->tick_count;
}
/**这里是真正操作地址的函数，原作者的代码写错了，内存地址的划分都没有明确了解。
 * 内存地址划分格式应该如下：
 * |tag|组号（属于哪一个set）|组内块号log2(cache_mapping_ways)|块内地址log2(cache_line_size)|
 * 所以，应该先获得当前地址属于哪一个set，在check的时候，对这个set的line进行验证。
 * */
void CacheSim::do_cache_op(CacheSim *cache, _u32 addr, bool is_read) {
    _u32 set,set_base;
    int index;
    // TODO: add the init of cache_mapping_ways_shifts
    set = (addr >> (cache->cache_line_shifts + cache->cache_mapping_ways_shifts)) % cache->cache_set_size;
    //获得组号的基地址
    set_base = set * cache->cache_mapping_ways;
    // 命中情况
    index = check_cache_hit(cache, set_base, addr);
    if(index >= 0){
        
    }
}
