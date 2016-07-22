#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char _u8;
typedef unsigned int  _u32;

//#ifdef _DEBUG
#define dbg_log                 printf
//#endif

#define CACHE_FLAG_VALID        0x1
#define CACHE_FLAG_WRITE_BACK   0x2
#define CACHE_FLAG_MASK         0xf

const char* swap_style_description[] = {
        "FIFO替换",
        "LRU替换",
        "随机替换"
};

const char* write_style_description[] = {
        "写直达",
        "写回"
};

enum cache_swap_style{
    CACHE_SWAP_FIFO,
    CACHE_SWAP_LRU,
    CACHE_SWAP_RAND,
    CACHE_SWAP_MAX
};

enum cache_write_style{
    CACHE_WRITE_THROUGH,
    CACHE_WRITE_BACK,
    CACHE_WRITE_MAX
};

struct cache_item {
    _u32 tag;                       /* cache标记缓存- 最低4bit为标志位 */
    union {
        _u32 count;
        _u32 lru_count;         /* 读写计数 */
        _u32 fifo_count;        /* 先入先出 = tickcount */
    };
    _u8* buf;                       /* 指向实际数据 */
};

struct cache_sim {
    _u32 cache_size;                 /* cache缓存容量 */
    _u32 cache_line_size;            /* cache块大小 */
    _u32 cache_item_num;             /* cache总入口数 */
    _u32 cache_mapping_ways;         /* cache总组数 */
    _u32 cache_group_size;           /* cache组大小 */

    _u32 cache_group_shifts;
    _u32 cache_block_shifts;
    _u32 cache_tag_mask;

    struct cache_item* caches;

    _u32* addr_list;                /* 地址列表-以便统计强制缺失 */
    _u32 addr_list_size;
    _u32 addr_num;

    _u32 tick_count;                /* 总指令计数器 */
    _u8* cache_buf;                 /* cache缓存区 */
    int swap_style;
    int write_style;

    _u32 cache_r_count;             /* cache读主存块数 */
    _u32 cache_w_count;             /* cache写主存块数 */
    _u32 cache_hit_count;
    _u32 cache_miss_count;

    _u32 cache_free_num;            /* 空闲cache条目 */
    _u32 force_miss_count;          /* 强制缺失 */
    _u32 capacity_miss_count;       /* 容量缺失 */
    _u32 conflict_miss_count;       /* 冲突缺失 */
};

/* 返回插入位置 */
int get_addr_index(struct cache_sim* cache, void* addr)
{
    int lo = 0;
    int hi = cache->addr_num - 1;
    int mid = (lo + hi) / 2;

    if (cache->addr_num < 1)
        return -1;
    if ((_u32)addr <= cache->addr_list[lo])
        return lo;
    if ((_u32)addr >= cache->addr_list[hi])
        return hi;

    while (hi > lo + 1) {
        mid = (lo + hi) / 2;
        if ((_u32)addr > cache->addr_list[mid])
            lo = mid;
        else if ((_u32)addr < cache->addr_list[mid])
            hi = mid;
        else
            return mid;
    }

    return mid;
}

int add_addr_list(struct cache_sim* cache, void* addr)
{
    int index;

    if (!cache->addr_list || cache->addr_num < 1
        || cache->addr_num >= cache->addr_list_size) {
        cache->addr_list_size += 0x400;
        cache->addr_list = realloc(cache->addr_list, cache->addr_list_size * sizeof(_u32));
    }

    if (cache->addr_num < 1) {
        cache->addr_list[0] = (_u32)addr;
        cache->addr_num = 1;
        return 0;
    }

    index = get_addr_index(cache, addr);

    if ((_u32)addr == cache->addr_list[index])
        return -1;
    else if ((_u32)addr > cache->addr_list[index])
        index += 1;

    memmove(cache->addr_list + index + 1,
            cache->addr_list + index,
            sizeof(_u32)*(cache->addr_num - index));

    cache->addr_list[index] = (_u32)addr;
    cache->addr_num++;

    return 0;
}

int get_power_num(int num)
{
    int ret = -1;
    while (num > 0){
        ret++;
        num = num >> 1;
    }
    return ret;
}

void reset_cache_sim(struct cache_sim* cache, int cache_line_size, int mapping_ways)
{
    if (!cache || cache_line_size < 16 || mapping_ways < 1)
        return;

    cache->cache_line_size = cache_line_size;
    cache->cache_item_num = cache->cache_size / cache_line_size;
    cache->cache_block_shifts = get_power_num(cache_line_size);
    cache->cache_mapping_ways = mapping_ways;
    cache->cache_group_shifts = get_power_num(mapping_ways);
    cache->cache_group_size = cache->cache_item_num / mapping_ways;
    cache->cache_tag_mask = 0xffffffff << (cache->cache_group_shifts + cache->cache_block_shifts);
    cache->cache_free_num = cache->cache_item_num;

    cache->addr_num = 0;
    cache->cache_hit_count = 0;
    cache->cache_miss_count = 0;
    cache->cache_r_count = 0;
    cache->cache_w_count = 0;
    cache->capacity_miss_count = 0;
    cache->conflict_miss_count = 0;
    cache->force_miss_count = 0;
    cache->tick_count = 0;

    if (!cache->cache_buf){
        cache->cache_buf = malloc(cache->cache_size);
        memset(cache->cache_buf, 0, cache->cache_size);
    }

    if (cache->caches){
        free(cache->caches);
        cache->caches = NULL;
    }

    /* 这里直接设置cache-tags为32bits-简化 */
    cache->caches = malloc(sizeof(struct cache_item) * cache->cache_item_num);
    memset(cache->caches, 0, sizeof(struct cache_item) * cache->cache_item_num);
}

static int chk_cache_hit(struct cache_sim* cache, _u32 group_base, void* addr)
{
    _u32 i, tag;
    for (i=0; i<cache->cache_group_size; i++) {
        tag = cache->caches[group_base + i].tag;
        if ((tag & CACHE_FLAG_VALID)
            && (tag & cache->cache_tag_mask) == ((_u32)addr & cache->cache_tag_mask)){
            return group_base + i;
        }
    }

    return -1;
}

static int get_cache_free_item(struct cache_sim* cache,  _u32 group_base, void* addr)
{
    _u32 i, tag;
    _u32 min_count, free_index;
    for (i=0; i<cache->cache_group_size; i++) {
        tag = cache->caches[group_base + i].tag;
        if (!(tag & CACHE_FLAG_VALID)){
            if (cache->cache_free_num > 0)
                cache->cache_free_num--;
            return group_base + i;
        }
    }

    /* 执行缓存切换算法 */
    free_index = 0;
    if (CACHE_SWAP_RAND == cache->swap_style){
        free_index = rand() % cache->cache_group_size;
    } else {
        min_count = cache->caches[group_base].count;
        for (i=0; i<cache->cache_group_size; i++) {
            if (cache->caches[group_base + i].count < min_count){
                min_count = cache->caches[group_base + i].count;
                free_index = i;
            }
        }
    }

    free_index += group_base;

    /* 检查cache回写标志 */
    if (cache->caches[free_index].tag & CACHE_FLAG_WRITE_BACK){
        cache->caches[free_index].tag &= ~CACHE_FLAG_WRITE_BACK;
        /*
        dbg_log("cache write back to block %p\n",
                cache->caches[free_index].tag & (~(cache->cache_line_size - 1))
                );
         */
        cache->cache_w_count++;
    }

    /*
    dbg_log("cache swaped %d %p->%p\n",
            free_index,
            (_u32)addr & (~(cache->cache_line_size - 1)),
            cache->caches[free_index].tag & (~(cache->cache_line_size - 1))
            );
     */
    return free_index;
}

static void set_cache_item(struct cache_sim* cache, _u32 index, void* addr)
{
    struct cache_item* item = cache->caches + index;

    item->buf = cache->cache_buf + cache->cache_line_size * index;
    item->tag = (_u32)addr & ~CACHE_FLAG_MASK;
    item->tag |= CACHE_FLAG_VALID;
    item->count = cache->tick_count;
}

static int do_cache_op(struct cache_sim* cache, void* addr, int is_read)
{
    _u32 group;
    _u32 group_base;
    int index;

    group = ((_u32)addr >> cache->cache_block_shifts) % cache->cache_mapping_ways;
    group_base = group * cache->cache_group_size;

    index = chk_cache_hit(cache, group_base, addr);
    if (index >= 0){
        cache->cache_hit_count++;
        /* LRU更新时间戳 */
        if (CACHE_SWAP_LRU == cache->swap_style)
            cache->caches[index].lru_count = cache->tick_count;
        if (!is_read && cache->write_style == CACHE_WRITE_THROUGH)
            cache->cache_w_count++;
        if (!is_read && cache->write_style == CACHE_WRITE_BACK)
            cache->caches[index].tag |= CACHE_FLAG_WRITE_BACK;
    } else {
        index = get_cache_free_item(cache, group_base, addr);
        /*
         * 统计缺失原因：强制缺失-第一次访问某块
         */
        if (add_addr_list(cache, (void*)addr) == 0)
            cache->force_miss_count++;
        else if (cache->cache_free_num > 0)
            cache->conflict_miss_count++;
        else
            cache->capacity_miss_count++;

        set_cache_item(cache, index, addr);
        /* 从主存读取到缓存 */
        if (is_read){
            /*
            dbg_log("read memory block %p -> cache:%d\n",
                    (_u32)addr & (~(cache->cache_line_size - 1)), index);
             */
            cache->cache_r_count++;
        } else {
            /*
            dbg_log("write memory block %p -> cache:%d\n",
                    (_u32)addr & (~(cache->cache_line_size - 1)), index);
             */
            cache->cache_w_count++;
        }

        cache->cache_miss_count++;
    }

    return 0;
}

struct cache_sim* init_cache_sim(int cache_size)
{
    struct cache_sim* cache_obj = (struct cache_sim*)malloc(sizeof(*cache_obj));

    memset(cache_obj, 0, sizeof(struct cache_sim));

    cache_obj->cache_size = cache_size;
    reset_cache_sim(cache_obj, 32, 2);

    return cache_obj;
}

void free_cache_sim(struct cache_sim* cache)
{
    if (cache) {
        if (cache->cache_buf)
            free(cache->cache_buf);
        if (cache->caches)
            free(cache->caches);
        free(cache);
    }
}

void load_trace(struct cache_sim* cache, char* filename)
{
    char buf[128];
    FILE* fp = fopen(filename, "r");
    _u32 rcount = 0, wcount = 0;

    if (!fp){
        dbg_log("load_trace %s failed...\n", filename);
        return;
    }

    while (fgets(buf, sizeof(buf), fp)){
        _u8 style = 0;
        _u32 addr = 0, instruct_deltha = 0;
        //printf("%d %s", cache->addr_num, buf);
        sscanf(buf, "%c %x %d", &style, &addr, &instruct_deltha);
        //add_addr_list(cache, (void*)addr);
        if (style == 'l'){
            do_cache_op(cache, (void*)addr, 1);
            rcount++;
        } else {
            do_cache_op(cache, (void*)addr, 0);
            wcount++;
        }

        cache->tick_count += instruct_deltha + 1;
    }

    dbg_log("%s\t%s/%s\t行大小:%d\t%d路相联\n",
            filename,
            swap_style_description[cache->swap_style],
            write_style_description[cache->write_style],
            cache->cache_line_size,
            cache->cache_mapping_ways
    );

    dbg_log("指令统计 读/写/合计:%d/%d/%d\n读指令比例:%f%%\t写指令比例:%f%%\n",
            rcount, wcount, cache->tick_count,
            100.0*rcount/cache->tick_count,
            100.0*wcount/cache->tick_count
    );

    dbg_log("miss/hit:%d/%d\t命中率:%f%%/%f%%\n",
            cache->cache_miss_count,
            cache->cache_hit_count,
            100.0*cache->cache_hit_count/(cache->cache_miss_count + cache->cache_hit_count),
            100.0*cache->cache_miss_count/(cache->cache_miss_count + cache->cache_hit_count)
    );

    dbg_log("强制缺失:%d/%d/%f%%\n容量缺失:%d/%d/%f%%\n冲突缺失:%d/%d/%f%%\n",
            cache->force_miss_count, cache->cache_miss_count,
            100.0*cache->force_miss_count/cache->cache_miss_count,
            cache->capacity_miss_count, cache->cache_miss_count,
            100.0*cache->capacity_miss_count/cache->cache_miss_count,
            cache->conflict_miss_count, cache->cache_miss_count,
            100.0*cache->conflict_miss_count/cache->cache_miss_count
    );

    dbg_log("读通信:%d bytes\t%dKB\n写通信:%d bytes\t%dKB\n\n",
            cache->cache_r_count*cache->cache_line_size,
            (cache->cache_r_count*cache->cache_line_size)>>10,
            cache->cache_w_count*cache->cache_line_size,
            (cache->cache_w_count*cache->cache_line_size)>>10
    );

    fclose(fp);
}

static do_test(struct cache_sim* cache, char* filename)
{
    int line_size[] = {32, 64, 128};
    int ways[] = {1, 2, 4, 8};
    int i,j;

    for (i=0; i<sizeof(line_size)/sizeof(int); i++){
        for (j=0; j<sizeof(ways)/sizeof(int); j++){
            reset_cache_sim(cache, line_size[i], ways[j]);
            load_trace(cache, filename);
        }
    }

}

int main(int argc, char** argv)
{
    struct cache_sim* cache_obj;
    char* test_cases[] = {"gcc.trace","gzip.trace","mcf.trace","swim.trace","twolf.trace"};
    int i, j, k;

    cache_obj = init_cache_sim(0x8000);

    for (k=0; k<sizeof(test_cases)/sizeof(char*); k++){
        for(i=CACHE_SWAP_FIFO; i<CACHE_SWAP_MAX; i++){
            for (j=CACHE_WRITE_THROUGH; j<CACHE_WRITE_MAX; j++) {
                cache_obj->swap_style = i;
                cache_obj->write_style = j;
                do_test(cache_obj, test_cases[k]);

            }
        }
    }
    free_cache_sim(cache_obj);

    return 0;
}