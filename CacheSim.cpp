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

CacheSim::CacheSim(int cache_size,int cache_line_size, int mapping_ways) {
//如果输入配置不符合要求
    if (cache_line_size < 0 || mapping_ways < 1) {
        return;
    }
    this->cache_size = cache_size;
    this->cache_line_size = (_u32) cache_line_size;
    // 总的line数 = cache总大小/ 每个line的大小（一般64byte，模拟的时候可配置）
    this->cache_line_num = this->cache_size / cache_line_size;
    // 地址格式中的第三部分，获取设置的几路组相联，是2的多少次方，以便对地址进行位移，获取tag等。
    this->cache_mapping_ways_shifts = (_u32) log2(mapping_ways);
    // 地址格式中的第四部分，
    this->cache_line_shifts = (_u32) log2(cache_line_size);
    // 几路组相联
    this->cache_mapping_ways = (_u32) mapping_ways;
    // 总共有多少set
    this->cache_set_size = this->cache_line_num / mapping_ways;
    // 其二进制占用位数，同其他shifts
    this->cache_set_shifts = (_u32) log2(this->cache_set_size);
    // 获取用来提取tag位的mask，比如0b1011 & 0b1100(0xB) = 0b(1000)
    this->cache_tag_mask =
            0xffffffff << (this->cache_set_shifts + this->cache_line_shifts + this->cache_mapping_ways_shifts);
    // 空闲块（line）
    this->cache_free_num = this->cache_line_num;

    this->cache_hit_count = 0;
    this->cache_miss_count = 0;
    this->cache_r_count = 0;
    this->cache_w_count = 0;
    // 指令数，主要用来在替换策略的时候提供比较的key，在命中或者miss的时候，相应line会更新自己的count为当时的tick_count;
    this->tick_count = 0;
    this->cache_buf = (_u8*) malloc(this->cache_size);
    memset(this->cache_buf, 0 , this->cache_size);
    // 为每一行分配空间
    this->caches = (Cache_Line*)malloc(sizeof(Cache_Line) * this->cache_line_num);
    memset(this->caches, 0, sizeof(Cache_Line) * this->cache_line_num);

    //测试时的默认配置
    this->swap_style = CACHE_SWAP_RAND;


    srand((unsigned)time(NULL));
}

CacheSim::~CacheSim(){
    free(this->caches);
    free(this->cache_buf);
}
int CacheSim::check_cache_hit(_u32 set_base, _u32 addr) {
    /**循环查找当前set的所有way（line），通过tag匹配，查看当前地址是否在cache中*/
    _u32 i, tag;
    for (i = 0; i < this->cache_mapping_ways; ++i) {
        tag = this->caches[set_base + i].tag;
        if ((tag & CACHE_FLAG_VAILD) && (tag == (addr >> (this->cache_set_shifts + this->cache_line_shifts + this->cache_mapping_ways_shifts)))) {
            return set_base + i;
        }
    }
    return -1;
}
/**获取当前set中可用的line，如果没有，就找到要被替换的块*/
int CacheSim::get_cache_free_line(_u32 set_base, _u32 addr) {
    _u32 i, tag, min_count;
    // TODO: min_count??
    _u32 free_index;
    /**从当前cache set里找可用的空闲line，可用：脏数据，空闲数据
     * cache_free_num是统计的整个cache的可用块*/
    for (i = 0; i < cache_mapping_ways; ++i) {
        tag = this->caches[set_base + i].tag;
        //这里是正确的，，肯定要找invaild cache line...
        if (!(tag & CACHE_FLAG_VAILD)) {
            if (this->cache_free_num > 0)
                this->cache_free_num--;
            return set_base + i;
        }
    }
    /**没有可用line，则执行替换算法
     * 原作者完成了随机替换和FIFO替换，LRU的并没有完成。
     * TODO: LRU替换算法
     * FIFO：这个原作者的实现是正确的，是我理解错他的意思了。
     * */
    free_index = 0;
    if(this->swap_style == CACHE_SWAP_RAND){
        free_index = rand() % this->cache_mapping_ways;
    }else{
        min_count = this->caches[set_base].count;
        for (int j = 1; j < this->cache_mapping_ways; ++j) {
            if(this->caches[set_base + j].count < min_count ){
                min_count = caches[set_base + j].count;
                free_index = j;
            }
        }
    }
    free_index += set_base;
    //如果原有的cache line是脏数据，写回内存
    if (this->caches[free_index].tag & CACHE_FLAG_WRITE_BACK) {
        this->caches[free_index].tag &= ~CACHE_FLAG_WRITE_BACK;
        this->cache_w_count++;
    }
    return free_index;
}

/**将数据写入cache line*/
void CacheSim::set_cache_line(_u32 index, _u32 addr) {
    Cache_Line *line = this->caches + index;
    // 这里每个line的buf和整个cache类的buf是重复的而且并没有填充内容。
    line->buf = this->cache_buf + this->cache_line_size * index;
    // 更新这个line的tag位
//    line->tag = addr & ~CACHE_FLAG_MASK;
    line->tag = (addr >> (this->cache_set_shifts + this->cache_line_shifts + this->cache_mapping_ways_shifts)) & ~CACHE_FLAG_MASK;
    // 置有效位为有效。
    line->tag |= CACHE_FLAG_VAILD;
    line->count = this->tick_count;
}

/**这里是真正操作地址的函数，原作者的代码写错了，内存地址的划分都没有明确了解。
 * 内存地址划分格式应该如下：
 * |tag|组号（属于哪一个set）|组内块号log2(cache_mapping_ways)|块内地址log2(cache_line_size)|
 * 所以，应该先获得当前地址属于哪一个set，在check的时候，对这个set的line进行验证。
 * */
void CacheSim::do_cache_op(_u32 addr, bool is_read) {
    _u32 set, set_base;
    int index;
    // TODO: add the init of cache_mapping_ways_shifts
    set = (addr >> (this->cache_line_shifts + this->cache_mapping_ways_shifts)) % this->cache_set_size;
    //获得组号的基地址
    set_base = set * this->cache_mapping_ways;
    index = check_cache_hit(set_base, addr);
    //命中了
    if (index >= 0) {
        this->cache_hit_count++;
        //只有在LRU的时候才更新时间戳?????
        if (CACHE_SWAP_LRU == this->swap_style)
            this->caches[index].lru_count = this->tick_count;
        //直接默认配置为写回法，即要替换或者数据脏了的时候才写回。
        //命中了，如果是改数据，不直接写回，而是等下次，即没有命中，但是恰好匹配到了当前line的时候，这时的标记就起作用了，将数据写回内存
        if (!is_read)
            this->caches[index].tag |= CACHE_FLAG_WRITE_BACK;
        //miss
    } else {
        index = get_cache_free_line(set_base, addr);
        //由于我暂时不需要统计缺失原因，所以下面关于addr list的都可以先不用实现。
        set_cache_line(index, addr);
        if (is_read) {
            this->cache_r_count++;
        } else {
            this->cache_w_count++;
        }
        this->cache_miss_count++;
    }
}

/**从文件读取trace，在我最后的修改目标里，为了适配项目，这里需要改掉*/
void CacheSim::load_trace(char *filename) {
    char buf[128];
    // 添加自己的input路径
    FILE* fin ;
    // 记录的是trace中指令的读写，由于cache机制，和真正的读写次数当然不一样。。主要是如果设置的写回法，则写会等在cache中，直到被替换。
    _u32 rcount = 0, wcount =0;
    fin = fopen(filename, "r");
    if(!fin){
        printf("load_trace %s failed\n", filename);
        return;
    }
    while(fgets(buf, sizeof(buf), fin)){
        _u8 style = 0;
        // 原代码中用的指针，感觉完全没必要，而且后面他的强制类型转换实际运行有问题。addr本身就是一个数值，32位unsigned int。
        _u32 addr =0;
        sscanf(buf, "%c %x", &style, &addr);
        if(style == 'l'){
            do_cache_op(addr, 1);
            rcount++;
        }else{
            do_cache_op(addr, 0);
            wcount++;
        }
        this->tick_count ++;
    }
    //TODO 添加printf打印测试结果。
    // 指令统计
    printf("all r/w/sum: %d %d %d \nread rate: %f%%\twrite rate: %f%%\n",
    rcount,wcount,this->tick_count,
           100.0*rcount/this->tick_count,
           100.0*wcount/this->tick_count
    );
    // miss率
    printf("miss/hit: %d/%d\t hit rate: %f%%/%f%%\n",
    this->cache_miss_count,this->cache_hit_count,
    100.0*this->cache_hit_count/(this->cache_hit_count+this->cache_miss_count),
    100.0*this->cache_miss_count/(this->cache_miss_count+this->cache_hit_count));
    // 读写通信
    printf("read : %d Bytes \t %dKB\n write : %d Bytes\t %dKB \n",
    this->cache_r_count*this->cache_line_size,
    (this->cache_r_count*this->cache_line_size)>>10,
   this->cache_w_count*this->cache_line_size,
   (this->cache_w_count*this->cache_line_size)>>10);
    fclose(fin);

}

void CacheSim::set_swap_style(int swap_style) {
    this->swap_style = swap_style;
}
