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


