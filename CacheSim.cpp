//
// Created by find on 16-7-19.
// Cache architect
// memory address  format:
// |tag|组号 log2(组数)|组内块号log2(mapping_ways)|块内地址 log2(cache line)|
//
#include "CacheSim.h"
#include <cstdlib>
#include <cstring>
#include <math.h>

int CacheSim::check_cache_hit(CacheSim *cache, _u32 set_base, _u32 addr) {
    /**循环查找当前set的所有way（line），通过tag匹配，查看当前地址是否在cache中*/
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
    for (i = 0; i < cache_mapping_ways; ++i) {
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
     * FIFO：这个原作者的实现是正确的，是我理解错他的意思了。
     * */
    free_index = 0;
    switch (cache->swap_style) {
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
    if (cache->caches[free_index].tag & CACHE_FLAG_WRITE_BACK) {
        cache->caches[free_index].tag &= ~CACHE_FLAG_WRITE_BACK;
        cache->cache_w_count++;
    }
    return free_index;
}

/**将数据写入cache line*/
void CacheSim::set_cache_line(CacheSim *cache, _u32 index, _u32 addr) {
    Cache_Line *line = cache->caches + index;
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
    _u32 set, set_base;
    int index;
    // TODO: add the init of cache_mapping_ways_shifts
    set = (addr >> (cache->cache_line_shifts + cache->cache_mapping_ways_shifts)) % cache->cache_set_size;
    //获得组号的基地址
    set_base = set * cache->cache_mapping_ways;
    index = check_cache_hit(cache, set_base, addr);
    //命中了
    if (index >= 0) {
        cache->cache_hit_count++;
        //只有在LRU的时候才更新时间戳，所以
        if (CACHE_SWAP_LRU == cache->swap_style)
            cache->caches[index].lru_count = cache->tick_count;
        //直接默认配置为写回法，即要替换或者数据脏了的时候才写回。
        //命中了，如果是改数据，不直接写回，而是等下次，即没有命中，但是恰好匹配到了当前line的时候，这时的标记就起作用了，将数据写回内存
        if (!is_read)
            cache->caches[index].tag |= CACHE_FLAG_WRITE_BACK;
        //miss
    } else {
        index = get_cache_free_line(cache, set_base, addr);
        //由于我暂时不需要统计缺失原因，所以下面关于addr list的都可以先不用实现。
        set_cache_line(cache, index, addr);
        if (is_read) {
            cache->cache_r_count++;
        } else {
            cache->cache_w_count++;
        }
        cache->cache_miss_count++;
    }
}

/**初始化cache*/
CacheSim *CacheSim::init_cache_sim(int cache_size) {
    CacheSim *a_cache_line = (CacheSim *) malloc(sizeof(*a_cache_line));
    memset(a_cache_line, 0, sizeof(CacheSim));
    a_cache_line->cache_size = cache_size;
    reset_cache_sim(a_cache_line, 32, 2);
}

/**初始化cache*/
void CacheSim::reset_cache_sim(CacheSim *cache, int cache_line_size, int mapping_ways) {
    //如果输入配置不符合要求
    if (!cache || cache_line_size < 0 || mapping_ways < 1) {
        return;
    }
    cache->cache_line_size = cache_line_size;
    // 总的line数 = cache总大小/ 每个line的大小（一般64byte，模拟的时候可配置）
    cache->cache_line_num = cache->cache_line_size / cache_line_size;
    // 地址格式中的第三部分，获取设置的几路组相联，是2的多少次方，以便对地址进行位移，获取tag等。
    cache->cache_mapping_ways_shifts = log2(mapping_ways);
    // 地址格式中的第四部分，
    cache->cache_line_shifts = log2(cache_line_size);
    // 几路组相联
    cache->cache_mapping_ways = mapping_ways;
    // 总共有多少set
    cache->cache_set_size = cache->cache_line_num / mapping_ways;
    // 其二进制占用位数，同其他shifts
    cache->cache_set_shifts = log2(cache->cache_set_size);
    // 获取用来提取tag位的mask，比如0b1011 & 0b1100(0xB) = 0b(1000)
    cache->cache_tag_mask =
            0xffffffff << (cache->cache_set_shifts + cache->cache_line_shifts + cache->cache_mapping_ways_shifts);
    // 空闲块（line）
    cache->cache_free_num = cache->cache_line_num;

    cache->cache_hit_count = 0;
    cache->cache_miss_count = 0;
    cache->cache_r_count = 0;
    cache->cache_w_count = 0;
    // 指令数，主要用来在替换策略的时候提供比较的key，在命中或者miss的时候，相应line会更新自己的count为当时的tick_count;
    cache->tick_count = 0;
    //这个buf用来模拟存数
    if(!cache->cache_buf){
        cache->cache_buf = (_u8*) malloc(cache->cache_size);
        memset(cache->cache_buf, 0 , cache->cache_size);
    }
    if(cache->caches){
        free(cache->caches);
        cache->caches = NULL;
    }
    // 为每一行分配空间
    cache->caches = (Cache_Line*)malloc(sizeof(Cache_Line) * cache->cache_line_num);
    memset(cache->caches, 0, sizeof(Cache_Line) * cache->cache_line_num);

}

void CacheSim::free_cache_sim(CacheSim *cache) {
    if (cache) {
        if (cache->cache_buf)
            free(cache->cache_buf);
        if (cache->caches)
            free(cache->caches);
        free(cache);
    }
}




































