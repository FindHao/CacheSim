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
