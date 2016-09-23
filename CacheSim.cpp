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

CacheSim::CacheSim() {}
void CacheSim::init(_u64 a_cache_size[3], _u64 a_cache_line_size[3], _u64 a_mapping_ways[3]) {
//如果输入配置不符合要求
    if (a_cache_line_size < 0 || a_mapping_ways[0] < 1 || a_mapping_ways[1] < 1) {
        return;
    }
    cache_size[0] =  a_cache_size[0];
    cache_size[1] =  a_cache_size[1];
    cache_line_size[0] =  a_cache_line_size[0];
    cache_line_size[1] =  a_cache_line_size[1];
    // 总的line数 = cache总大小/ 每个line的大小（一般64byte，模拟的时候可配置）
    cache_line_num[0] = (_u64) a_cache_size[0] / a_cache_line_size[0];
    cache_line_num[1] = (_u64) a_cache_size[1] / a_cache_line_size[1];
    cache_line_shifts[0] = (_u64)log2(a_cache_line_size[0]);
    cache_line_shifts[1] = (_u64)log2(a_cache_line_size[1]);
    // 几路组相联
    cache_mapping_ways[0]=  a_mapping_ways[0];
    cache_mapping_ways[1]=  a_mapping_ways[1];
    // 总共有多少set
    cache_set_size[0] = cache_line_num[0] / cache_mapping_ways[0];
    cache_set_size[1] = cache_line_num[1] / cache_mapping_ways[1];
    // 其二进制占用位数，同其他shifts
    cache_set_shifts[0] = (_u64) log2(cache_set_size[0]);
    cache_set_shifts[1] = (_u64) log2(cache_set_size[1]);
    // 空闲块（line）
    cache_free_num[0] = cache_line_num[0];
    cache_free_num[1] = cache_line_num[1];

    cache_r_count = 0;
    cache_w_count = 0;
    // 指令数，主要用来在替换策略的时候提供比较的key，在命中或者miss的时候，相应line会更新自己的count为当时的tick_count;
    tick_count = 0;
//    cache_buf = (_u8 *) malloc(cache_size);
//    memset(cache_buf, 0, this->cache_size);
    // 为每一行分配空间
    for (int i = 0; i < 2; ++i) {
        caches[i] = (Cache_Line *) malloc(sizeof(Cache_Line) * cache_line_num[i]);
        memset(caches[i], 0, sizeof(Cache_Line) * cache_line_num[i]);
    }
    //测试时的默认配置
    swap_style[0] = CACHE_SWAP_LRU;
    swap_style[1] = CACHE_SWAP_LRU;
    re_init();
    srand((unsigned) time(NULL));
}

/**顶部的初始化放在最一开始，如果中途需要对tick_count进行清零和caches的清空，执行此。主要因为tick_count的自增可能会超过unsigned long long，而且一旦tick_count清零，caches里的count数据也就出现了错误。*/
void CacheSim:: re_init(){
    tick_count = 0;
    memset(cache_hit_count,0,sizeof(cache_hit_count));
    memset(cache_miss_count,0,sizeof(cache_miss_count));
    cache_free_num[0] = cache_line_num[0];
    cache_free_num[1] = cache_line_num[1];
    memset(caches[0], 0, sizeof(Cache_Line) * cache_line_num[0]);
    memset(caches[1], 0, sizeof(Cache_Line) * cache_line_num[1]);
//    memset(cache_buf, 0, this->cache_size);
}
CacheSim::~CacheSim() {
    free(caches[0]);
    free(caches[1]);
//    free(cache_buf);
}

int CacheSim::check_cache_hit(_u64 set_base, _u64 addr, int level) {
    /**循环查找当前set的所有way（line），通过tag匹配，查看当前地址是否在cache中*/
    _u64 i;
    for (i = 0; i < cache_mapping_ways[level]; ++i) {
        if ((caches[level][set_base + i].flag & CACHE_FLAG_VALID) &&
            (caches[level][set_base + i].tag == ((addr >> (cache_set_shifts[level] + cache_line_shifts[level]))))) {
            return set_base + i;
        }
    }
    return -1;
}

/**获取当前set中可用的line，如果没有，就找到要被替换的块*/
int CacheSim::get_cache_free_line(_u64 set_base, int level) {
    _u64 i, min_count, j;
    int free_index;
    /**从当前cache set里找可用的空闲line，可用：脏数据，空闲数据
     * cache_free_num是统计的整个cache的可用块*/
    for (i = 0; i < cache_mapping_ways[level]; ++i) {
        if (!(caches[level][set_base + i].flag & CACHE_FLAG_VALID)) {
            if (cache_free_num[level] > 0)
                cache_free_num[level]--;
            return set_base + i;
        }
    }
    /**没有可用line，则执行替换算法
     * lock状态的块如何处理？？*/
    free_index = -1;
    if (swap_style[level] == CACHE_SWAP_RAND) {
        // TODO: 随机替换Lock状态的line后面再改
        free_index = rand() % cache_mapping_ways[level];
    } else {
        // !!!BUG Fixed
        min_count = ULONG_LONG_MAX;
        for (j = 0; j < cache_mapping_ways[level]; ++j) {
            if (caches[level][set_base + j].count < min_count && !(caches[level][set_base + j].flag &CACHE_FLAG_LOCK)) {
                min_count = caches[level][set_base + j].count;
                free_index = j;
            }
        }
    }
    if(free_index < 0){
        //如果全部被锁定了，应该会走到这里来。那么强制进行替换。强制替换的时候，需要setline?
        min_count = ULONG_LONG_MAX;
        for (j = 0; j < cache_mapping_ways[level]; ++j) {
            if (caches[level][set_base + j].count < min_count) {
                min_count = caches[level][set_base + j].count;
                free_index = j;
            }
        }
    }
    //
    if(free_index >= 0){
        free_index += set_base;
        //如果原有的cache line是脏数据，标记脏位
        if (caches[level][free_index].flag & CACHE_FLAG_DIRTY) {
            // TODO: 写回到L2 cache中。
            caches[level][free_index].flag &= ~CACHE_FLAG_DIRTY;
            cache_w_count++;
        }
    }else{
        printf("I should not show\n");
    }
    return free_index;
}

/**将数据写入cache line*/
void CacheSim::set_cache_line(_u64 index, _u64 addr, int level) {
    Cache_Line *line = caches[level] + index;
    // 这里每个line的buf和整个cache类的buf是重复的而且并没有填充内容。
//    line->buf = cache_buf + cache_line_size * index;
    // 更新这个line的tag位
    line->tag = addr >> (cache_set_shifts[level] + cache_line_shifts[level]);
    line->flag = (_u8) ~CACHE_FLAG_MASK;
    line->flag |= CACHE_FLAG_VALID;
    line->count = tick_count;
}

/**不需要分level*/
void CacheSim::do_cache_op(_u64 addr, char oper_style) {
    _u64 set_l1, set_l2, set_base_l1,set_base_l2;
    long long hit_index_l1, hit_index_l2, free_index_l1, free_index_l2;
    tick_count++;
    if(oper_style == OPERATION_READ) cache_r_count++;
    if(oper_style == OPERATION_WRITE) cache_w_count++;
    set_l2 = (addr >> cache_line_shifts[1]) % cache_set_size[1];
    set_base_l2 = set_l2 * cache_mapping_ways[1];
    hit_index_l2 = check_cache_hit(set_base_l2, addr, 1);
    set_l1 = (addr >> cache_line_shifts[0]) % cache_set_size[0];
    set_base_l1 = set_l1 * cache_mapping_ways[0];
    hit_index_l1 = check_cache_hit(set_base_l1, addr, 0);
    // lock操作现在只关心L2的。
    if(oper_style == OPERATION_LOCK || oper_style == OPERATION_UNLOCK){
        if(hit_index_l2 >= 0){
            if(oper_style == OPERATION_LOCK){
                cache_hit_count[1]++;
                if(CACHE_SWAP_LRU == swap_style[1]){
                    caches[1][hit_index_l2].lru_count = tick_count;
                }
                lock_cache_line((_u64)hit_index_l2, 1);
                // TODO: 不加载到L1上
                // 如果L1miss，则一定需要加载上去
//                if( hit_index_l1 < 0 ){
//                    free_index_l1 = get_cache_free_line(set_base_l1, 0);
//                    if(free_index_l1 >= 0){
//                        set_cache_line((_u64)free_index_l1, addr, 0);
//                        //需要miss++吗？
//                    }else{
//                        printf("I should not be here");
//                    }
//                }
            }else{
                unlock_cache_line((_u64)hit_index_l2, 1);
            }
        }else {
            // lock miss
            if (oper_style == OPERATION_LOCK) {
                cache_miss_count[1]++;
                free_index_l2 = get_cache_free_line(set_base_l2, 1);
                if (free_index_l2 >= 0) {
                    set_cache_line((_u64) free_index_l2, addr, 1);
                    lock_cache_line((_u64) free_index_l2, 1);
                    // TODO: 不管L1
                    // 同时需要查看L1是否hit
//                    if(hit_index_l1 < 0){
//                        free_index_l1 = get_cache_free_line(set_base_l1, 0);
//                        if(free_index_l1 >= 0){
//                            set_cache_line((_u64)free_index_l1, addr, 0);
//                            //需要miss++吗？
//                        }else{
//                            printf("I should not be here.");
//                        }
//                    }
                } else {
                    //返回值应该确保是>=0的
                    printf("I should not be here.");
                }
            } else {
                // miss的unlock 先不管
            }
        }
    }else{
        // L1命中了
        if (hit_index_l1 >= 0) {
            // 一定是read或者write。lock的已经在前面处理过了。
            cache_hit_count[0]++;
            //只有在LRU的时候才更新时间戳，第一次设置时间戳是在被放入数据的时候。所以符合FIFO
            if (CACHE_SWAP_LRU == swap_style[0])
                caches[0][hit_index_l1].lru_count = tick_count;
            //直接默认配置为写回法，即要替换或者数据脏了的时候才写回。
            //命中了，如果是改数据，不直接写回，而是等下次，即没有命中，但是恰好匹配到了当前line的时候，这时的标记就起作用了，将数据写回内存
            //TODO: error :先不用考虑写回的问题，这里按设想，不应该直接从L1写回到内存
            if (oper_style == OPERATION_WRITE) {
                // 修正上面的问题
//                caches[0][hit_index_l1].flag |= CACHE_FLAG_DIRTY;
                // L2命中，则将新数据写入到L2
                if(hit_index_l2 >= 0){
                    cache_hit_count[1]++;
                    caches[1][hit_index_l2].flag |= CACHE_FLAG_DIRTY;
                //如果L2miss，那么找到一个新块，将数据写入L2
                }else{
                     //需要添加cache2 miss count吗？先不加了
                    cache_miss_count[1]++;
                    free_index_l2 = get_cache_free_line(set_base_l2, 1);
                    set_cache_line((_u64) free_index_l2, addr, 1);
                }
            }
        } else {
            // L1 miss
            cache_miss_count[0]++;
            // 先查看L2是否hit，如果hit，直接取数据上来，否则
            if(hit_index_l2 >= 0 ){
                // 在L2中hit, 需要写回到L1中
                cache_hit_count[1]++;
                free_index_l1 = get_cache_free_line(set_base_l1, 0);
                set_cache_line((_u64) free_index_l1, addr, 0);
            }else{
                // not in L2,从内存中取
                cache_miss_count[1]++;
                free_index_l2 = get_cache_free_line(set_base_l2, 1);
                set_cache_line((_u64) free_index_l2, addr, 1);
                free_index_l1 = get_cache_free_line(set_base_l1, 0);
                set_cache_line((_u64) free_index_l1, addr, 0);
            }
        }
        //Fix BUG:在将Cachesim应用到其他应用中时，发现tickcount没有增加，这里修正下。不然会导致替换算法失效。
        // Bug Fix: 在hm中，需要通过外部单独调用tickcount++,现在还不明白为什么。
//    tick_count++;
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
    // 指令统计
    printf("all r/w/sum: %lld %lld %lld \nread rate: %f%%\twrite rate: %f%%\n",
           rcount, wcount, tick_count,
           100.0 * rcount / tick_count,
           100.0 * wcount / tick_count
    );
    // miss率
    printf("L1 miss/hit: %lld/%lld\t hit rate: %f%%/%f%%\n",
           cache_miss_count[0], cache_hit_count[0],
           100.0 * cache_hit_count[0] / (cache_hit_count[0] + cache_miss_count[0]),
           100.0 * cache_miss_count[0] / (cache_miss_count[0] + cache_hit_count[0]));
    printf("L2 miss/hit: %lld/%lld\t hit rate: %f%%/%f%%\n",
           cache_miss_count[1], cache_hit_count[1],
           100.0 * cache_hit_count[1] / (cache_hit_count[1] + cache_miss_count[1]),
           100.0 * cache_miss_count[1] / (cache_miss_count[1] + cache_hit_count[1]));
    // 读写通信
//    printf("read : %d Bytes \t %dKB\n write : %d Bytes\t %dKB \n",
//           cache_r_count * cache_line_size,
//           (cache_r_count * cache_line_size) >> 10,
//           cache_w_count * cache_line_size,
//           (cache_w_count * cache_line_size) >> 10);
    fclose(fin);

}



int CacheSim::lock_cache_line(_u64 line_index, int level) {
    caches[level][line_index].flag |= CACHE_FLAG_LOCK;
    return 0;
}

int CacheSim::unlock_cache_line(_u64 line_index, int level) {
    caches[level][line_index].flag &= ~CACHE_FLAG_LOCK;
    return 0;
}

