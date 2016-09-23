//
// Created by find on 16-7-19.
// cache line = cache block = 原代码里的cache item ~= cache way
//
#ifndef CACHE_SIM
#define CACHE_SIM
typedef unsigned char _u8;
typedef unsigned long long _u64;

const unsigned char CACHE_FLAG_VALID = 0x01;
const unsigned char CACHE_FLAG_DIRTY = 0x02;
const unsigned char CACHE_FLAG_LOCK = 0x04;
const unsigned char CACHE_FLAG_MASK = 0xff;
/**最多多少层cache*/
const int MAXLEVEL = 3;
const char OPERATION_READ = 'l';
const char OPERATION_WRITE = 's';
const char OPERATION_LOCK = 'k';
const char OPERATION_UNLOCK = 'u';
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
    _u64 tag;
    /**计数，FIFO里记录最一开始的访问时间，LRU里记录上一次访问的时间*/
    union {
        _u64 count;
        _u64 lru_count;
        _u64 fifo_count;
    };
    _u8 flag;
    _u8 *buf;
};

class CacheSim {
    // 隐患
public:
    /**cache的总大小，单位byte*/
    _u64 cache_size[MAXLEVEL];
    /**cache line(Cache block)cache块的大小*/
    _u64 cache_line_size[MAXLEVEL];
    /**总的行数*/
    _u64 cache_line_num[MAXLEVEL];
    /**每个set有多少way*/
    _u64 cache_mapping_ways[MAXLEVEL];
    /**整个cache有多少组*/
    _u64 cache_set_size[MAXLEVEL];
    /**2的多少次方是set的数量，用于匹配地址时，进行位移比较*/
    _u64 cache_set_shifts[MAXLEVEL];
    /**2的多少次方是line的长度，用于匹配地址*/
    _u64 cache_line_shifts[MAXLEVEL];
    /**真正的cache地址列。指针数组*/
    Cache_Line *caches[MAXLEVEL];

    /**指令计数器*/
    _u64 tick_count;
    /**cache缓冲区,由于并没有数据*/
//    _u8 *cache_buf[MAXLEVEL];
    /**缓存替换算法*/
    int swap_style[MAXLEVEL];
    /**读写内存的计数*/
    _u64 cache_r_count, cache_w_count;
    /**cache hit和miss的计数*/
    _u64 cache_hit_count[MAXLEVEL], cache_miss_count[MAXLEVEL];
    /**空闲cache line的index记录，在寻找时，返回空闲line的index*/
    _u64 cache_free_num[MAXLEVEL];

    CacheSim();
    ~CacheSim();
    void init(_u64 a_cache_size[],_u64 a_cache_line_size[], _u64 a_mapping_ways[]);
    /**原代码中addr的处理有些问题，导致我没有成功运行他的代码。
     * 检查是否命中
     * @args:
     * cache: 模拟的cache
     * set_base: 当前地址属于哪一个set，其基址是什么。
     * addr: 要判断的内存地址
     * @return:
     * 由于cache的地址肯定不会超过int（因为cache大小决定的）
     * TODO: check the addr */
    int check_cache_hit(_u64 set_base, _u64 addr, int level);
    /**获取cache当前set中空余的line*/
    int get_cache_free_line(_u64 set_base, int level);
    /**找到合适的line之后，将数据写入cache line中*/
    void set_cache_line(_u64 index, _u64 addr, int level);
    /**对一个指令进行分析*/
    void do_cache_op(_u64 addr, char oper_style);
    /**读入trace文件*/
    void load_trace(char * filename);

    /**lock a cache line*/
    int lock_cache_line(_u64 addr, int level);
    /**unlock a cache line*/
    int unlock_cache_line(_u64 addr, int level);
    /**@return 返回miss率*/
    double get_miss_rate(int level){
        return 100.0 * cache_miss_count[level] / (cache_miss_count[level] + cache_hit_count[level]);
    }

    void re_init();
};

#endif