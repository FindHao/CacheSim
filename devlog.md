## 星期一, 18. 七月 2016 04:48下午 
原代码里没有实现LRU的替换策略，只实现了随机替换。
## July 20, 2016 10:09 AM
```
|tag|组号 log2(组数)|组内块号log2(mapping_ways)|块内地址 log2(cache line)|
```
TODO：
替换策略
+ LRU
+ FIFO

原作者的写法是使用结构体，所以需要传入cachesim指针，但是现在用的是class，所以传入参数里其实把cache去掉，放到外面？？
也就是说，完全不用传参数表里这个CacheSim * cache...
我日，，这又是原作者一个隐隐的大坑啊。
修改所有的传参，
~~TODO：
补充析构函数~~
~~初始化没用？~~
初始化以后才能进行swap_style的设置。
而且总的cache大小，只有在此时赋值了。

fuckkkkk,,
原来clion的cmakelist build以后，二进制默认是放在clion的临时目录里。。
没有malloc就进行使用。。
构造函数可以替代初始化代码
析构函数替代自己写的free函数。
先设置默认组相联数为4,cache line大小为32.
基本完成，测试不同line size以及组相联数对命中率等的影响。
还有LRU和FIFO的完善。。


FIFO:采用那个count计数。先进先出，即找到tick count最小的line，作为替换line
LRU：priority_quque，每个set维护一个queue，决定替换哪个line，也就是queue的数量是set的数量，而每个set里元素的个数是mapping ways。


**TODOList**

+ ~~FIFO替换算法~~
+ ~~LRU替换算法~~
+ tick_count会不会超出int？？
+ 确认union的fifo count和LRU count等

LRU的关键是要记得更新优先级队列。何时更新？有更改的时候就更新？？？
需要用优先级队列吗？？
![](http://my.csdn.net/uploads/201205/24/1337859321_3597.png)
需要吗？
现在的实现是直接分配了
**直接从记录里找最远访问的那个line，即count比较小的那个。和FIFO的不同是，再次访问的时候，FIFO的时间戳不能更新。**
原作者的实现并没有错误，，是我傻逼。。
设置不同的替换策略，不同的cache line size 和mapping ways进行对比实验，检验代码的有效性。


按照我的思路，每次new一个cachesim，但是貌似代码执行出来，每次都是同一个地址啊。不过由于有delete操作，所以每次都在那个虚拟地址上分配？？？
那么为什么在fgets那里会出现段错误？？
无奈，只好一个一个测试了。。
bug找到
```
    if(this->swap_style == CACHE_SWAP_RAND){
        free_index = rand() % this->cache_mapping_ways;
```
随机替换算法，% 的应该是mapping ways。。

影响因素：
line size， ways， 替换算法。
要做的比较（折线图）

||ways1|ways2|
|-|-|-|
|line size 64|fdfdfd|fdfd

主要看在这些元素变化的时候，miss率和读写数量的变化。
## July 21, 2016 9:03 AM

| 变量              | 固定值                          | 实验结果                                     | Q                           |
| --------------- | ---------------------------- | ---------------------------------------- | --------------------------- |
| mapping ways    | 替换策略 cache_line_size         | 随着mapping ways的增加，miss率降低，但是降低的幅度逐渐下降    | 设置更大的mapping ways，查看后面的图像走势 |
| 替换策略            | mapping_ways cache_line_size | 不同替换策略的miss rate差距不大，FIFO 0.245306, LRU 0.238325, RAND0.241427，写数据FIFO最高，LRU和RAND差距非常小，读数据FIFO和RAND差距非常小，LRU最低 |                             |
| cache_line_size | 替换策略，mapping_ways            | cache_line_size 越大，miss率越低               | 同Q1什么时候变平缓？                 |

数据基本符合预期。

## July 21, 2016 3:28 PM
~~tag的设置应该是在set line的时候，而且应该是右移，~~
tag_mask在一开始就是u32的，并没有右移，而是通过设置后面位为0达到目的。不对，就是应该右移。
fuck....
## July 22, 2016 12:35 PM
的确想错了，tag的划分：
```
|tag|组号 log2(组数)|块内地址 log2(cache line)|
```
肯定没有组内块号，不然还要替换算法干啥。。。
所以应该从内存地址划分和cache的划分两个方面来说明。
终于明白了。。
所以cache line那里，还需要添加单独的标记位。
为何我改了gitignore文件仍然检测input和workspace.xml文件的修改？？？
TODO：
graphy.py文件实现根据输入展示不同的图标

## July 23, 2016 3:37 PM
LRU，FIFO时间戳的说明单独说明
## July 24, 2016 8:27 AM
实验数据统计
添加cache locking的支持：
随机选取块
## July 27, 2016 10:14 AM
cache locking：

+ 使用locking标记位


每次肯定是锁一个line 64byte的数据。（16个int）
替换的时候，需要判断是否是处于lock状态的，
unlock操作怎么办。
HM中何时lock
## July 29, 2016 8:36 AM
增加加锁和解锁指令：
指令只包含一个地址，解锁的时候是解锁整个line。如果miss，则将命中的整个line的所有内容标记解锁。
解锁的时候需要替换出去吗？需要置为无效吗？锁定的时候，到底能不能被强制替换出去？如果能，那么后续的解锁操作该如何做？如果不能，后续就全部miss吗？是否需要强制留下1个line可用？
先实现不能被强制替换的。且可以锁满的。
应该是先指定要锁的内容，然后再执行load这个步骤？？？NO，先load，再立刻锁，则lock的时候，一定是命中的，只需要设置cache line的标志位即可。
对比测试：
cache line 32byte ,mapping ways 8 ,cache size 64KB
柱状图？？？横坐标：

我傻逼了啊。。。
我的测试数据并没有使用cache locking的数据，但是miss率仍旧降低了。。。。。。。。。。。。。。。。。说明我的代码写的有问题

## August 25, 2016 8:32 AM
 加锁和解锁的函数的传入参数应该是地址，而不是index。
原来的计划是将lock作为辅助指令，即必须先load，然后才能lock，现在准备将lock做成平级的，'k'，传入的是要锁的数据的地址。
## August 26, 2016 9:44 AM
替换策略应该修改，目前是直接不替换。
后续还有时间代价也要算上。
注意，如果单独提取出来用，tickcount并没有自增。
## 星期三, 31. 八月 2016 08:48上午 
lock早已完成。
在加入别的项目里时，出现了很多问题，不在构造函数里初始化了，放在了init里。
单独提出了reinit，因为发现当指令非常非常多时，tickcount自增，超过了_u64上限，导致替换策略的失效。因此在调用cachesim的一些地方，每次需要进行reinit。
把tickcount自增的地方也提出来了。
## 星期一, 19. 九月 2016 07:26下午 
应该是re_init没有调用导致的L2的miss很低很低。
gcc.trace
all r/w/sum: 318197 197486 515683 
read rate: 61.703993%	write rate: 38.296007%
L1 miss/hit: 6651/509032	 hit rate: 98.710254%/1.289746%
L2 miss/hit: 3373/3278	 hit rate: 49.285822%/50.714178%
 mcf.trace
all r/w/sum: 5972 721258 727230 
read rate: 0.821198%	write rate: 99.178802%
L1 miss/hit: 180081/547149	 hit rate: 75.237408%/24.762592%
L2 miss/hit: 90118/89963	 hit rate: 49.956964%/50.043036%

Process finished with exit code 0
## 星期五, 23. 九月 2016 03:22下午 
修正L1的脏数据写回问题，修正查找free cache line时，初始最小值为UINT_MAX问题。

## 2017-01-17 23:14:53

添加了读取命令行参数的功能，尚未完善。同时准备添加L1 lock 和unlock功能