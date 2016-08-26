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
#include <cstdio>
#include <time.h>
#include <climits>

CacheSim::CacheSim(int a_cache_size, int a_cache_line_size, int a_mapping_ways) {
//如果输入配置不符合要求
    if (a_cache_line_size < 0 || a_mapping_ways < 1) {
        return;
    }
    this->cache_size = (_u64) a_cache_size;
    this->cache_line_size = (_u64) a_cache_line_size;
    // 总的line数 = cache总大小/ 每个line的大小（一般64byte，模拟的时候可配置）
    cache_line_num = (_u64) a_cache_size / a_cache_line_size;
    cache_line_shifts = (_u64) log2(a_cache_line_size);
    // 几路组相联
    cache_mapping_ways = (_u64) a_mapping_ways;
    // 总共有多少set
    cache_set_size = cache_line_num / a_mapping_ways;
    // 其二进制占用位数，同其他shifts
    cache_set_shifts = (_u64) log2(cache_set_size);
    // 空闲块（line）
    cache_free_num = cache_line_num;

    cache_hit_count = 0;
    cache_miss_count = 0;
    cache_r_count = 0;
    cache_w_count = 0;
    // 指令数，主要用来在替换策略的时候提供比较的key，在命中或者miss的时候，相应line会更新自己的count为当时的tick_count;
    tick_count = 0;
    cache_buf = (_u8 *) malloc(this->cache_size);
    memset(cache_buf, 0, this->cache_size);
    // 为每一行分配空间
    caches = (Cache_Line *) malloc(sizeof(Cache_Line) * cache_line_num);
    memset(caches, 0, sizeof(Cache_Line) * cache_line_num);


    //测试时的默认配置
    swap_style = CACHE_SWAP_RAND;
    srand((unsigned) time(NULL));
}

CacheSim::~CacheSim() {
    free(caches);
    free(cache_buf);
}

int CacheSim::check_cache_hit(_u64 set_base, _u64 addr) {
    /**循环查找当前set的所有way（line），通过tag匹配，查看当前地址是否在cache中*/
    _u64 i;
    for (i = 0; i < cache_mapping_ways; ++i) {
        if ((caches[set_base + i].flag & CACHE_FLAG_VALID) &&
            (caches[set_base + i].tag == ((addr >> (cache_set_shifts + cache_line_shifts))))) {
            return set_base + i;
        }
    }
    return -1;
}

/**获取当前set中可用的line，如果没有，就找到要被替换的块*/
int CacheSim::get_cache_free_line(_u64 set_base) {
    _u64 i, min_count, j;
    int free_index;
    /**从当前cache set里找可用的空闲line，可用：脏数据，空闲数据
     * cache_free_num是统计的整个cache的可用块*/
    for (i = 0; i < cache_mapping_ways; ++i) {
        if (!(caches[set_base + i].flag & CACHE_FLAG_VALID)) {
            if (cache_free_num > 0)
                cache_free_num--;
            return set_base + i;
        }
    }
    /**没有可用line，则执行替换算法
     * lock状态的块如何处理？？*/
    free_index = -1;
    if (swap_style == CACHE_SWAP_RAND) {
        // TODO: 随机替换Lock状态的line后面再改
        free_index = rand() % cache_mapping_ways;
    } else {
        // !!!BUG Fixed
        min_count = UINT_MAX;
        for (j = 0; j < cache_mapping_ways; ++j) {
            if (caches[set_base + j].count < min_count && !(caches[set_base + j].flag &CACHE_FLAG_LOCK)) {
                min_count = caches[set_base + j].count;
                free_index = j;
            }
        }
    }
    if(free_index < 0){
        //如果全部被锁定了，应该会走到这里来。那么强制进行替换。强制替换的时候，需要setline?
        min_count = UINT_MAX;
        for (j = 0; j < cache_mapping_ways; ++j) {
            if (caches[set_base + j].count < min_count) {
                min_count = caches[set_base + j].count;
                free_index = j;
            }
        }
    }
    //
    if(free_index >= 0){
        free_index += set_base;
        //如果原有的cache line是脏数据，标记脏位
        if (caches[free_index].flag & CACHE_FLAG_DIRTY) {
            caches[free_index].flag &= ~CACHE_FLAG_DIRTY;
            cache_w_count++;
        }
    }else{
        printf("I should not show\n");
    }
    return free_index;
}

/**将数据写入cache line*/
void CacheSim::set_cache_line(_u64 index, _u64 addr) {
    Cache_Line *line = caches + index;
    // 这里每个line的buf和整个cache类的buf是重复的而且并没有填充内容。
    line->buf = cache_buf + cache_line_size * index;
    // 更新这个line的tag位
    line->tag = addr >> (cache_set_shifts + cache_line_shifts);
    line->flag = (_u8) ~CACHE_FLAG_MASK;
    line->flag |= CACHE_FLAG_VALID;
    line->count = tick_count;
}

void CacheSim::do_cache_op(_u64 addr, char oper_style) {
    _u64 set, set_base;
    int index;
    set = (addr >> cache_line_shifts) % cache_set_size;
    //获得组号的基地址
    set_base = set * cache_mapping_ways;
    //WARNING！！！
    //这里的一个隐患，返回的超过了int上限
    index = check_cache_hit(set_base, addr);
    //命中了
    if (index >= 0) {
        // lock和unlock的指令不算在其内
        if (oper_style == OPERATION_READ || oper_style == OPERATION_WRITE){
            cache_hit_count++;
            //只有在LRU的时候才更新时间戳，第一次设置时间戳是在被放入数据的时候。所以符合FIFO
            if (CACHE_SWAP_LRU == swap_style)
                caches[index].lru_count = tick_count;
            //直接默认配置为写回法，即要替换或者数据脏了的时候才写回。
            //命中了，如果是改数据，不直接写回，而是等下次，即没有命中，但是恰好匹配到了当前line的时候，这时的标记就起作用了，将数据写回内存
            if (oper_style == OPERATION_WRITE)
                caches[index].flag |= CACHE_FLAG_DIRTY;
        }else{
            //一定是hit的load
            if (oper_style == OPERATION_LOCK){
                cache_hit_count++;
                //只有在LRU的时候才更新时间戳，第一次设置时间戳是在被放入数据的时候。所以符合FIFO
                if (CACHE_SWAP_LRU == swap_style)
                    caches[index].lru_count = tick_count;
                lock_cache_line((_u64)index);
            }else{
                // unlock 不计算hit
                unlock_cache_line((_u64)index);
            }
        }
    //miss
    } else {
        if(oper_style == OPERATION_READ || oper_style == OPERATION_WRITE || oper_style == OPERATION_LOCK){
            index = get_cache_free_line(set_base);
            if(index >= 0 ){
                set_cache_line((_u64) index, addr);
                if (oper_style == OPERATION_READ || oper_style == OPERATION_LOCK) {
                    if(oper_style == OPERATION_LOCK){
                        lock_cache_line((_u64)index);
                    }
                    cache_r_count++;
                } else {
                    cache_w_count++;
                }
                cache_miss_count++;
            }else{
                //返回值应该确保是>=0的
            }
        }else{
            // miss的unlock先不用管
        }
        // 如果是进行lock unlock操作时miss，目前先不管，因为现在设置的策略是替换时不能替换lock状态的line
        //Fix BUG:在将Cachesim应用到其他应用中时，发现tickcount没有增加，这里修正下。不然会导致替换算法失效。
        tick_count++;
    }
}

/**从文件读取trace，在我最后的修改目标里，为了适配项目，这里需要改掉*/
void CacheSim::load_trace(char *filename) {
    char buf[128];
    // 添加自己的input路径
    FILE *fin;
    // 记录的是trace中指令的读写，由于cache机制，和真正的读写次数当然不一样。。主要是如果设置的写回法，则写会等在cache中，直到被替换。
    _u64 rcount = 0, wcount = 0;
    fin = fopen(filename, "r");
    if (!fin) {
        printf("load_trace %s failed\n", filename);
        return;
    }
    while (fgets(buf, sizeof(buf), fin)) {
        _u8 style = 0;
        // 原代码中用的指针，感觉完全没必要，而且后面他的强制类型转换实际运行有问题。addr本身就是一个数值，32位unsigned int。
        _u64 addr = 0;
        sscanf(buf, "%c %x", &style, &addr);
        do_cache_op(addr, style);
        switch (style) {
            case 'l' :rcount++;break;
            case 's' :wcount++;break;
            case 'k' :                break;
            case 'u' :                break;

        }
    }
    //TODO 添加printf打印测试结果。
    // 指令统计
    printf("all r/w/sum: %d %d %d \nread rate: %f%%\twrite rate: %f%%\n",
           rcount, wcount, tick_count,
           100.0 * rcount / tick_count,
           100.0 * wcount / tick_count
    );
    // miss率
    printf("miss/hit: %d/%d\t hit rate: %f%%/%f%%\n",
           cache_miss_count, cache_hit_count,
           100.0 * cache_hit_count / (cache_hit_count + cache_miss_count),
           100.0 * cache_miss_count / (cache_miss_count + cache_hit_count));
    // 读写通信
    printf("read : %d Bytes \t %dKB\n write : %d Bytes\t %dKB \n",
           cache_r_count * cache_line_size,
           (cache_r_count * cache_line_size) >> 10,
           cache_w_count * cache_line_size,
           (cache_w_count * cache_line_size) >> 10);
    fclose(fin);

}

void CacheSim::set_swap_style(int a_swap_style) {
    this->swap_style = a_swap_style;
}


int CacheSim::lock_cache_line(_u64 line_index) {
    caches[line_index].flag |= CACHE_FLAG_LOCK;
    return 0;
}

int CacheSim::unlock_cache_line(_u64 line_index) {
    caches[line_index].flag &= ~CACHE_FLAG_LOCK;
    return 0;
}

