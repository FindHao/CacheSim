//
// Created by find on 16-7-19.
// cache line = cache block = 原代码里的cache item ~= cache way
//

#ifndef CACHE_SIM
#define CACHE_SIM
typedef unsigned char _u8;
typedef unsigned int _u32;

const unsigned char CACHE_FLAG_VAILD = 0x01;
const unsigned char CACHE_FLAG_DIRTY = 0x02;
const unsigned char CACHE_FLAG_LOCK = 0x04;
const unsigned char CACHE_FLAG_MASK = 0xff;
// 替换算法
enum cache_swap_style {
    CACHE_SWAP_FIFO,
    CACHE_SWAP_LRU,
    CACHE_SWAP_RAND,
    CACHE_SWAP_MAX
};

//写内存方法就默认写回吧。
class Cache_Line {
public:
    _u32 tag;
    /**计数，FIFO里记录最一开始的访问时间，LRU里记录上一次访问的时间*/
    union {
        _u32 count;
        _u32 lru_count;
        _u32 fifo_count;
    };
    _u8 flag;
    _u8 *buf;
};

class CacheSim {
    // 隐患
public:
    /**cache的总大小，单位byte*/
    _u32 cache_size;
    /**cache line(Cache block)cache块的大小*/
    _u32 cache_line_size;
    /**总的行数*/
    _u32 cache_line_num;
    /**每个set有多少way*/
    _u32 cache_mapping_ways;
    /**整个cache有多少组*/
    _u32 cache_set_size;
    /**2的多少次方是set的数量，用于匹配地址时，进行位移比较*/
    _u32 cache_set_shifts;
    /**2的多少次方是line的长度，用于匹配地址*/
    _u32 cache_line_shifts;
    /**真正的cache地址列*/
    Cache_Line *caches;

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

    CacheSim(int cache_size,int cache_line_size, int mapping_ways);
    ~CacheSim();

    void set_swap_style(int swap_style);
    /**原代码中addr的处理有些问题，导致我没有成功运行他的代码。
     * 检查是否命中
     * @args:
     * cache: 模拟的cache
     * set_base: 当前地址属于哪一个set，其基址是什么。
     * addr: 要判断的内存地址
     * TODO: check the addr */
    int check_cache_hit(_u32 set_base, _u32 addr);
    /**获取cache当前set中空余的line*/
    _u32 get_cache_free_line(_u32 set_base);
    /**找到合适的line之后，将数据写入cache line中*/
    void set_cache_line(_u32 index, _u32 addr);
    /**对一个指令进行分析*/
    void do_cache_op(_u32 addr, bool is_read);
    /**读入trace文件*/
    void load_trace(char * filename);
};

#endif