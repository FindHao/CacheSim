//
// Created by find on 16-7-19.
// Cache architect
// memory address  format:
// |tag|组号 log2(组数)|组内块号log2(mapping_ways)|块内地址 log2(cache line)|
//
#include "CacheSim.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <time.h>
#include <climits>
#include "map"

CacheSim::CacheSim() {}

/**@arg a_cache_size[] 多级cache的大小设置
 * @arg a_cache_line_size[] 多级cache的line size（block size）大小
 * @arg a_mapping_ways[] 组相连的链接方式*/

_u32 CacheSim::pow_int(int base, int exponent) {
    _u32 sum = 1;
    for (int i = 0; i < exponent; i++) {
        sum *= base;
    }
    return sum;
}

_u64 CacheSim::pow_64(_u64 base, _u64 exponent) {
    _u64 sum = 1;
    for (_u64 i = 0; i < exponent; i++) {
        sum *= base;
    }
    return sum;
}

void CacheSim::init(_u64 a_cache_size[3], _u64 a_cache_line_size[3], _u64 a_mapping_ways[3]) {
//如果输入配置不符合要求
    if (a_cache_line_size[0] < 0 || a_cache_line_size[1] < 0 || a_mapping_ways[0] < 1 || a_mapping_ways[1] < 1) {
        return;
    }
    cache_size[0] = a_cache_size[0];
    cache_size[1] = a_cache_size[1];
//    在这里屏蔽两级cache 仅仅作为一个cache与share mem
    cache_line_size[0] = a_cache_line_size[0];
    cache_line_size[1] = a_cache_line_size[1];

    // 总的line数 = cache总大小/ 每个line的大小（一般64byte，模拟的时候可配置）
    cache_line_num[0] = (_u64) a_cache_size[0] / a_cache_line_size[0];
    cache_line_num[1] = (_u64) a_cache_size[1] / a_cache_line_size[1];
    cache_line_shifts[0] = (_u64) log2(a_cache_line_size[0]);
    cache_line_shifts[1] = (_u64) log2(a_cache_line_size[1]);
    // 几路组相联
    cache_mapping_ways[0] = a_mapping_ways[0];
    cache_mapping_ways[1] = a_mapping_ways[1];
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
    cache_w_memory_count = 0;
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
    swap_style[1] = CACHE_SWAP_DRRIP;
    // 用于SRRIP算法
    SRRIP_M = 2;
    SRRIP_2_M_1 = pow_int(2, SRRIP_M) - 1;
    SRRIP_2_M_2 = pow_int(2, SRRIP_M) - 2;
    PSEL = 0;
    cur_win_repalce_policy = CACHE_SWAP_SRRIP;

    re_init();
    srand((unsigned) time(NULL));
}

/**顶部的初始化放在最一开始，如果中途需要对tick_count进行清零和caches的清空，执行此。*/
void CacheSim::re_init() {
    tick_count = 0;
    memset(cache_hit_count, 0, sizeof(cache_hit_count));
    memset(cache_miss_count, 0, sizeof(cache_miss_count));
    cache_free_num[0] = cache_line_num[0];
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
    switch (swap_style[level]) {
        case CACHE_SWAP_RAND:
            free_index = rand() % cache_mapping_ways[level];
            break;
        case CACHE_SWAP_LRU:
            min_count = ULONG_LONG_MAX;
            for (j = 0; j < cache_mapping_ways[level]; ++j) {
                if (caches[level][set_base + j].count < min_count &&
                    !(caches[level][set_base + j].flag & CACHE_FLAG_LOCK)) {
                    min_count = caches[level][set_base + j].count;
                    free_index = j;
                }
            }
            break;
        case CACHE_SWAP_SRRIP:
            while (free_index < 0) {
                for (_u64 k = 0; k < cache_mapping_ways[level]; ++k) {
                    if (caches[level][set_base + k].RRPV == SRRIP_2_M_1) {
                        free_index = k;
                        // break the for-loop
                        break;
                    }
                }
                // increment all RRPVs

                if (free_index < 0) {
                    // increment all RRPVs
                    for (_u64 k = 0; k < cache_mapping_ways[level]; ++k) {
                        caches[level][set_base + k].RRPV++;
                    }
                } else {
                    // break the while-loop
                    break;
                }
            }

//                for (_u64 k = 0; k < cache_mapping_ways[level]; ++k) {
//                    if (caches[level][set_base + k].RRPV == SRRIP_2_M_1) {
//                        free_index = k;
//                        // break the for-loop
//                        break;
//                    }
//                }
//                // increment all RRPVs
//
//                if (free_index < 0) {
//                    // increment all RRPVs
//                    for (_u64 k = 0; k < cache_mapping_ways[level]; ++k) {
//                        caches[level][set_base + k].RRPV++;
//                    }
//                }

            break;
    }
    //如果没有使用锁，那么这个if应该是不会进入的
    if (free_index < 0) {
        //如果全部被锁定了，应该会走到这里来。那么强制进行替换。强制替换的时候，需要setline?
        min_count = ULONG_LONG_MAX;
        for (j = 0; j < cache_mapping_ways[level]; ++j) {
            if (caches[level][set_base + j].count < min_count) {
                min_count = caches[level][set_base + j].count;
                free_index = j;
            }
        }
    }
    if (free_index >= 0) {
        free_index += set_base;
        //如果原有的cache line是脏数据，标记脏位
        if (caches[level][free_index].flag & CACHE_FLAG_DIRTY) {
            // TODO: 写回到L2 cache中。
            // TODO: 写延迟 ： mem， l2.
            caches[level][free_index].flag &= ~CACHE_FLAG_DIRTY;
            cache_w_memory_count++;
        }
    } else {
        printf("I should not show\n");
    }
    return free_index;
}


/**获取当前set中可用的line，如果没有，就找到要被替换的块*/
int CacheSim::get_cache_free_line_specific(_u64 set_base, int level, int a_swap_style) {
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
    switch (a_swap_style) {
        case CACHE_SWAP_RAND:
            free_index = rand() % cache_mapping_ways[level];
            break;
        case CACHE_SWAP_LRU:
            min_count = ULONG_LONG_MAX;
            for (j = 0; j < cache_mapping_ways[level]; ++j) {
                if (caches[level][set_base + j].count < min_count &&
                    !(caches[level][set_base + j].flag & CACHE_FLAG_LOCK)) {
                    min_count = caches[level][set_base + j].count;
                    free_index = j;
                }
            }
            break;
        case CACHE_SWAP_SRRIP:
            while (free_index < 0) {
                for (_u64 k = 0; k < cache_mapping_ways[level]; ++k) {
                    if (caches[level][set_base + k].RRPV == SRRIP_2_M_1) {
                        free_index = k;
                        // break the for-loop
                        break;
                    }
                }
                // increment all RRPVs

                if (free_index < 0) {
                    // increment all RRPVs
                    for (_u64 k = 0; k < cache_mapping_ways[level]; ++k) {
                        caches[level][set_base + k].RRPV++;
                    }
                } else {
                    // break the while-loop
                    break;
                }
            }

//                for (_u64 k = 0; k < cache_mapping_ways[level]; ++k) {
//                    if (caches[level][set_base + k].RRPV == SRRIP_2_M_1) {
//                        free_index = k;
//                        // break the for-loop
//                        break;
//                    }
//                }
//                // increment all RRPVs
//
//                if (free_index < 0) {
//                    // increment all RRPVs
//                    for (_u64 k = 0; k < cache_mapping_ways[level]; ++k) {
//                        caches[level][set_base + k].RRPV++;
//                    }
//                }

            break;
    }
    //如果没有使用锁，那么这个if应该是不会进入的
    if (free_index < 0) {
        //如果全部被锁定了，应该会走到这里来。那么强制进行替换。强制替换的时候，需要setline?
        min_count = ULONG_LONG_MAX;
        for (j = 0; j < cache_mapping_ways[level]; ++j) {
            if (caches[level][set_base + j].count < min_count) {
                min_count = caches[level][set_base + j].count;
                free_index = j;
            }
        }
    }
    if (free_index >= 0) {
        free_index += set_base;
        //如果原有的cache line是脏数据，标记脏位
        if (caches[level][free_index].flag & CACHE_FLAG_DIRTY) {
            // TODO: 写回到L2 cache中。
            // TODO: 写延迟 ： mem， l2.
            caches[level][free_index].flag &= ~CACHE_FLAG_DIRTY;
            cache_w_memory_count++;
        }
    } else {
        printf("I should not show\n");
    }
    return free_index;
}

/**返回这个set是否是sample set。*/
int CacheSim::get_set_flag(_u64 set_base) {
    // size >> 10 << 5 = size * 32 / 1024 ，参照论文中的sample比例
    int K = cache_set_size[1] >> 5;
    int log2K = (int) log2(K);
    int log2N = (int) log2(cache_set_size[1]);
    // 使用高位的几位，作为筛选.比如需要32 = 2^5个，则用最高的5位作为mask
    _u64 mask = pow_64(2, (_u64) (log2N - log2K)) - 1;
    _u64 residual = set_base & mask;
    return residual;
}


/**将数据写入cache line，只有在miss的时候才会执行*/
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
    _u64 set_l1, set_l2, set_base_l1, set_base_l2;
    long long hit_index_l1, hit_index_l2, free_index_l1, free_index_l2;
    tick_count++;
    if (oper_style == OPERATION_READ) cache_r_count++;
    if (oper_style == OPERATION_WRITE) cache_w_count++;
    set_l2 = (addr >> cache_line_shifts[1]) % cache_set_size[1];
    set_base_l2 = set_l2 * cache_mapping_ways[1];
    hit_index_l2 = check_cache_hit(set_base_l2, addr, 1);
    set_l1 = (addr >> cache_line_shifts[0]) % cache_set_size[0];
    set_base_l1 = set_l1 * cache_mapping_ways[0];
    hit_index_l1 = check_cache_hit(set_base_l1, addr, 0);
    int set_flag = get_set_flag(set_l2);
    int temp_swap_style = swap_style[1];
    if (swap_style[1] == CACHE_SWAP_DRRIP) {
        /**是否是sample set*/
        switch (set_flag) {
            case 0:
                temp_swap_style = CACHE_SWAP_BRRIP;
                break;
            case 1:
                temp_swap_style = CACHE_SWAP_SRRIP;
                break;
            default:
                if (PSEL > 0) {
                    cur_win_repalce_policy = CACHE_SWAP_BRRIP;
                } else {
                    cur_win_repalce_policy = CACHE_SWAP_SRRIP;
                }
                temp_swap_style = cur_win_repalce_policy;
        }
    }


    //是否写操作
    if (oper_style == OPERATION_WRITE) {
        if (hit_index_l2 >= 0) {
            cache_hit_count[1]++;
            caches[1][hit_index_l2].count = tick_count;
            caches[1][hit_index_l2].flag |= CACHE_FLAG_DIRTY;
            switch (temp_swap_style) {
                case CACHE_SWAP_BRRIP:
                case CACHE_SWAP_SRRIP:
                    caches[1][hit_index_l2].RRPV = 0;
                    break;
                case CACHE_SWAP_SRRIP_FP:
                    if (caches[1][hit_index_l2].RRPV != 0) {
                        caches[1][hit_index_l2].RRPV -= 1;
                    }
                    break;
//                case CACHE_SWAP_DRRIP:
//                    break;
            }
        } else {
            cache_miss_count[1]++;
            free_index_l2 = get_cache_free_line_specific(set_base_l2, 1, temp_swap_style);
            set_cache_line((_u64) free_index_l2, addr, 1);
            caches[1][free_index_l2].flag |= CACHE_FLAG_DIRTY;
            switch (temp_swap_style) {
                case CACHE_SWAP_SRRIP_FP:
                case CACHE_SWAP_SRRIP:
                    caches[1][free_index_l2].RRPV = SRRIP_2_M_2;
                    break;
                case CACHE_SWAP_BRRIP:
                    caches[1][free_index_l2].RRPV = rand() / RAND_MAX > EPSILON ? SRRIP_2_M_1 : SRRIP_2_M_2;
                    break;
            }
            // 如果是动态策略，则还需要更新psel
            if (swap_style[1] == CACHE_SWAP_DRRIP) {
                if (set_flag == 1) {
                    PSEL++;
                } else if (set_flag == 0) {
                    PSEL--;
                }
            }
        }
    } else {
//        cache命中则直接返回
        if (hit_index_l2 >= 0) {
            cache_hit_count[1]++;
            caches[1][hit_index_l2].count = tick_count;
            switch (temp_swap_style) {
                case CACHE_SWAP_BRRIP:
                case CACHE_SWAP_SRRIP:
                    caches[1][hit_index_l2].RRPV = 0;
                    break;
                case CACHE_SWAP_SRRIP_FP:
                    if (caches[1][hit_index_l2].RRPV != 0) {
                        caches[1][hit_index_l2].RRPV -= 1;
                    }
                    break;
            }
        } else {
            cache_miss_count[1]++;
            free_index_l2 = get_cache_free_line_specific(set_base_l2, 1, temp_swap_style);
            set_cache_line((_u64) free_index_l2, addr, 1);
            switch (temp_swap_style) {
                case CACHE_SWAP_SRRIP_FP:
                case CACHE_SWAP_SRRIP:
                    caches[1][free_index_l2].RRPV = SRRIP_2_M_2;
                    break;
                case CACHE_SWAP_BRRIP:
                    caches[1][free_index_l2].RRPV = rand() / RAND_MAX > EPSILON ? SRRIP_2_M_1 : SRRIP_2_M_2;
                    break;
            }
            // 如果是动态策略，则还需要更新psel
            if (swap_style[1] == CACHE_SWAP_DRRIP) {
                if (set_flag == 1) {
                    PSEL++;
                } else if (set_flag == 0) {
                    PSEL--;
                }
            }
        }
    }
}


/**从文件读取trace，在我最后的修改目标里，为了适配项目，这里需要改掉*/
void CacheSim::load_trace(const char *filename) {
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
        char tmp_style[5];
        char style;
        // 原代码中用的指针，感觉完全没必要，而且后面他的强制类型转换实际运行有问题。addr本身就是一个数值，32位unsigned int。
        _u64 addr = 0;
        int datalen = 0;
        int burst = 0;
        int mid = 0;
        float delay = 0;
        float ATIME = 0;
        int ch = 0;
        int qos = 0;

        sscanf(buf, "%s %x %d %d %x %f %f %d %d", tmp_style, &addr, &datalen, &burst, &mid, &delay, &ATIME, &ch, &qos);
        if (strcmp(tmp_style, "nr") == 0 || strcmp(tmp_style, "wr") == 0) {
            style = 'l';
        } else if (strcmp(tmp_style, "nw") == 0 || strcmp(tmp_style, "naw") == 0) {
            style = 's';
        } else {
            printf("%s", tmp_style);
            return;
        }
        do_cache_op(addr, style);
        switch (style) {
            case 'l' :
                rcount++;
                break;
            case 's' :
                wcount++;
                break;
            case 'k' :
                break;
            case 'u' :
                break;

        }
    }
    // 文件中的指令统计
    printf("all r/w/sum: %lld %lld %lld \nread rate: %f%%\twrite rate: %f%%\n",
           rcount, wcount, tick_count,
           100.0 * rcount / tick_count,
           100.0 * wcount / tick_count
    );
    // miss率
//    printf("L1 miss/hit: %lld/%lld\t hit/miss rate: %f%%/%f%%\n",
//           cache_miss_count[0], cache_hit_count[0],
//           100.0 * cache_hit_count[0] / (cache_hit_count[0] + cache_miss_count[0]),
//           100.0 * cache_miss_count[0] / (cache_miss_count[0] + cache_hit_count[0]));
    printf("L2 miss/hit t: %lld/%lld\t hit/miss rate: %f%%/%f%% \n",
           cache_miss_count[1], cache_hit_count[1],
           100.0 * cache_hit_count[1] / (cache_hit_count[1] + cache_miss_count[1]),
           100.0 * cache_miss_count[1] / (cache_miss_count[1] + cache_hit_count[1]));
    printf("SM_in is %lld\t and cache in is %lld\n", cache_miss_count[1]);
//    char a_swap_style[100];
//    switch (swap_style[1]){
//        case CACHE_SWAP_LRU:
//
//    }
//    printf("\n=======Cache Policy=======\n%s", );
    printf("\n=======Bandwidth=======\nMemory --> Cache:\t%.4fGB\nCache --> Memory:\t%.4fMB\n",
           cache_miss_count[1] * cache_line_size[1] * 1.0 / 1024 / 1024 / 1024,
           cache_w_memory_count * cache_line_size[1] * 1.0 / 1024 / 1024);
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

