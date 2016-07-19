//
// Created by find on 16-7-19.
// cache line = cache block = 原代码里的cache item ~= cache way
//

#ifndef CACHE_SIM
#define CACHE_SIM
typedef unsigned char _u8;
typedef unsigned int _u32;
/**tag的最后四位作为标志位，
 * 0x1: 有效位
 * 0x2: 脏位
 * 0x4: 是否上锁*/
// .... define不能加;忘了。。
#define CACHE_FLAG_VAILD 0x1
#define CACHE_FLAG_WRITE_BACK 0x2
#define CACHE_FLAG_LOCK 0x4
#define CACHE_FLAG_MASK 0xf
// 替换算法
enum cache_swap_style {
    CACHE_SWAP_FIFO,
    CACHE_SWAP_LRU,
    CACHE_SWAP_RAND,
    CACHE_SWAP_MAX
};

//写内存方法就默认写回吧。
class Cache_Line {
    _u32 tag;
    /**计数，记录上一次访问的时间*/
    union {
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
    /**每个set有多少way*/
    _u32 cache_mapping_ways;
    /**组内块号的位移*/
    _u32 cache_mapping_ways_shifts;
    /**整个cache有多少组*/
    _u32 cache_set_size;
    /**2的多少次方是set的数量，用于匹配地址时，进行位移比较*/
    _u32 cache_set_shifts;
    /**2的多少次方是line的长度，用于匹配地址*/
    _u32 cache_line_shifts;
    /**获取出tag部分，主要是通过和1与*/
    _u32 cache_tag_mask;
    /**真正的cache地址列*/
    Cache_Item *caches;

    /**指令计数器*/
    _u32 tick_count;
    /**cache缓冲区*/
    _u8 *cache_buf;
    /**缓存替换算法*/
    int swap_style;
    /**读写内存的计数*/
    _u32 cache_r_count, cache_w_count;
    /**cache hit和miss的计数*/
    _u32 cache_hit_count, cache_miss_count;

    /**空闲cache line的index记录，在寻找时，返回空闲line的index*/
    _u32 cache_free_num;

    void reset_cache_sim(CacheSim* cache, int cache_line_size, int mapping_ways);
    /**原代码中addr的处理有些问题，导致我没有成功运行他的代码。
     * 检查是否命中
     * @args:
     * cache: 模拟的cache
     * set_base: 当前地址属于哪一个set，其基址是什么。
     * addr: 要判断的内存地址
     * TODO: check the addr */
    int check_cache_hit(CacheSim * cache, _u32 set_base, _u32 addr);
    /**获取cache当前set中空余的line*/
    int get_cache_free_line(CacheSim *cache, _u32 set_base, _u32 addr);
    /**找到合适的line之后，将数据写入cache line中*/
    void set_cache_line(CacheSim * cache, _u32 index, _u32 addr);
    /**对一个指令进行分析*/
    int do_cache_op(CacheSim * cache, _u32 addr, bool is_read);
    CacheSim * init_cache_sim(int cache_size);
    void free_cache_sim(CacheSim * cache);
    /**读入trace文件*/
    void load_trace(CacheSim * cache, char * filename);
    void do_test(CacheSim * cache, char * filename);
};

#endif