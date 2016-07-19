//
// Created by find on 16-7-19.
//

#ifndef CACHE_SIM
#define CACHE_SIM
typedef unsigned char _u8;
typedef unsigned int _u32;

/**tag的最后四位作为标志位，
 * 0x1: 有效位
 * 0x2: 脏位
 * 0x4: 是否上锁*/
#define CACHE_FLAG_VAILD 0x1;
#define CACHE_FLAG_WRITE_BACK 0x2;
#define CACHE_FLAG_LOCK 0x4;
#define CACHE_FLAG_MASK 0xf;
// 替换算法
enum cache_swap_style {
    CACHE_SWAP_FIFO,
    CACHE_SWAP_LRU,
    CACHE_SWAP_RAND,
    CACHE_SWAP_MAX
};
//写内存方法就默认写回吧。
class Cache_Item{
    _u32 tag;
    union{
        _u32 count;
        _u32 lru_count;
        _u32 fifo_count;
    };
    _u8 *buf;
};

class CacheSim {
    /**cache的总大小，单位byte*/
    _u32 cache_size;
    /**cache line(Cache block)cache块的大小*/
    _u32 cache_line_size;
    /**总的行数*/
    _u32 cache_line_num;
    /**cache way的数量，总数*/
    _u32 cache_mapping_ways;
    /**一个caceh set内含有的way（line）数量，即几路组相连*/
    _u32 cache_set_size;
    /**2的多少次方是set的数量，用于匹配地址时，进行位移比较*/
    _u32 cache_set_shifts;
    /**2的多少次方是line的长度，用于匹配地址*/
    _u32 caceh_line_shifts;
    /**真正的cache地址列*/
    Cache_Item * caches;

    /**指令计数器*/
    _u32 tick_count;
    /**cache缓冲区*/
    _u8 * cache_buf;
    /**缓存替换算法*/
    int swap_style;
    /**读写内存的计数*/
    _u32 cache_r_count,cache_w_count;
    _u32 cache_hit_count;
    _u32 cache_miss_count;

    /**空闲cache line的index记录，在寻找时，返回空闲line的index*/
    _u32 cache_free_num;
    /***/
};

#endif