# DPDK
## 大页内存
应用程序访问的是虚拟内存，按照页表粒度（4KB）转换到物理内存。为了提高性能，最近一次使用的若干页面地址被保存在一个称为转换检测缓冲区(TLB)的高速缓存中。TLB空间很小，不适用于DPDK这种数据量大内存使用多的应用。因此DPDK依赖标准大页，英特尔架构有两种大小2M和1G；  
### linux内存管理
linux内存管理mmu，采用多级页表，虚拟地址分为三部分，0-11位是页内偏移，12-21位是二级页表号；22-31位是一级页表号；在一级页表中根据一级页表号找到二级页表的位置，然后在该二级页表中通过二级页表号找到物理页表的位置，再通过页内偏移找到物理地址；  
为了减少查询过程，会将最近若干次的插叙到的虚拟内存和物理内存的对应关系保存到TLB中，下次再访问虚拟地址时，CPU首先根据12-31位查询TLB，如果命中则直接使用即可，不命中则会产生缺页中断，通过上面多级查找的过程更新TLB的内容；TLB容量有限，通常只能存放512条虚拟内存和物理内存的对应关系；  
TLB表只有一个，位于CPU的高速缓存中，CPU需要知道当前TLB表项中是哪个进程的虚拟地址和物理地址映射，就需要用到CR3寄存器。寄存器存放的是某个进程一级页表的地址，此时TLB存放的就是该进程虚拟内存和物理内存的映射；  
### 大页内存的优点
1. 避免swap；多级页表如果内存不足会被放入磁盘的swap分区，大页内存不会；  
2. 减少页表开销，减轻TLB压力；所有进程共享一个大页内存表，而大页内存能有效的减少虚拟地址与物理地址之间映射关系的表项，增加TLB的命中率；  
大页一般是2M或1G；  
### 如何激活大页
开启hugetlbfs、default_hugepagesz=1G hugepagesz=1G hugepage=4  
对numa系统，申请的大页会平均分给每个numa node，比如有2个node，要4G大页，就是每个node 2G；  
DPDK应用程序从/sys/kernel/mm/hugepage/和/proc/mounts等目录或文件中获取系统的所有大页资源信息，通过mmap给大页建立虚拟地址和物理地址的映射；  
rte_eal_hugepage_init初始化

## cache一致性
数据读从内存-cache-处理器，数据写处理器-cache-内存；  
多个cpu对某个内存块同时读写，会产生cache一致性问题。每个处理器会有自己独占的cache，比如A\B两个cpu同时访问一个变量，A修改了变量之后是保存在cache，在同步到内存。但B则是直接从自己的cache中读取该变量的值，没有感知到A对变量的改动，产生一致性问题；   
DPDK的解决办法是避免每个核与其他核共享数据，对于多网卡多队列，DPDK让每个核在网卡上都有独立的收发队列；  

## NUMA
每个cpu都有自己的本地内存，也可以跨处理器访问远端内存；  
dpdk的原则就是：   
1. 确保DPDK线程、内存分配和网络设备都位于相同的NUMA节点上，以避免跨节点内存访问带来的性能损失。   
​2. 使用DPDK提供的API（如rte_malloc_socket）为NUMA节点分配内存。   
### 如何获取pci网卡所在的numa节点
可以通过rte_eth_dev_socket_id(uint8_t port_id)来查询接口所在的numa节点。port_id就是网卡id；    
### 如何获取线程所在的numa节点
dpdk初始化会扫描设备cpu信息，为每个核分配一个lcore_config结构，相当于一个逻辑核，逻辑核所在的numa节点可以通过rte_socket_id(void)查询；  
eal_cpu_socket_id用来扫描/sys目录下文件，确定当前逻辑核所在的numa节点；  
后续创建内存、网卡收发队列都需要传入socket id也就是numa节点；  

## 如何使用DPDK进行数据包的收发处理？
收包  
网卡通过DMA把数据写入收包队列，两个环形队列一个rx_ring一个是sw_ring，rx_ring存储报文的物理地址，提供DMA使用；sw_ring存储其对应的虚拟地址，提供应用使用；
rte_rx_queue_setup会创建rx_ring和sw_ring;  
rx_ring里面的每个描述符中的地址填的是rte_mbuf的buf data地址，而sw_ring的每个描述符存放的是rte_mbuf的地址，DMA把数据写入rx_ring中实际上就是将数据填充到了mbuf的data部分；rx_ring和sw_ring通过rte_eth_dev_start建立映射关系；   
每次收到新数据，会检查rx_ring当前的buf DD位是否为0，为0表示buf可用，把数据填入后DD改为1。如果发现DD为1，说明网卡队列已满，丢弃报文；  
应用通过rte_eth_rx_burst从指定的网卡和指定的队列中读取数据。从队列尾部开始读，将数据写入mbuf，并把队列buf的DD位置0；  

发包   
发送队列初始化，和接收队列类似tx_queue_setup，创建tx_ring和sw_ring，建立mempool、queue、DMA、ring之间的关系


## 环形队列rte_ring
无锁环形队列，支持单生产者入队、单消费者出队、多生产者入队、多消费者出队；  
两个数组，consumer和producer每个数组都有head和tail两个指针；
1. 单生产者入队  
11. 先将生产者header和消费者tail交给临时变量（cpu cache中的值），同时将prod_next指向下一个对象；此时local prod_head = global prod_head = global prod_tail；  
12. 把临时变量local prod next指向head的下一个，然后将global prod head = local prod next, 并将数据写入local prod_head指向的位置；  
13. 最后一步将global prod tail指向gobal prod head的位置，此时global prod head = global prod tail = local prod next，而local prod head指向的位置一直没有变化，此时指向的是写入的数据；  
14. 生产者的头尾部索引都没有变化，保持global cons head = global cons tail = local con head；  

2. 单消费者出队  
21. 将生产者head和消费者tail交给临时变量local cons head和local prod tail，同时将local cons next指向cons head的下一个，此时global cons head = global cons tail = local cons head；  
22. 将global cons head指向local cons next，并取出local cons head指向的对象，此时global cons tail = local cons head，global cons head = local cons next；  
23. 将global cons tail指向global cons head，出队完成；  

3. 多生产者入队  
31. 和单生产者类似，也是将生产者head和消费者tail交给临时变量，既然是多生产者，就会有多组临时变量local prod head1、local prod head2、local cons tail1、local cons tail2；local prod next1指向local prod head1下一个，local prod next2也一样；此时local prod head1 == local prod head2 == global prod head == global prod tail，local prod next1 == local prod next2；  
32. 通过CAS比较交换指令修改global prod head指向local prod next，在core1执行成功，core2失败，那结果是，global prod head == local prod next1，local prod head == global prod tail；而core2 local prod head2 = local prod next1 = global prod head， local prod next2 = local prod head2的下一个，相当于core2的local prod head和local prod next都移动了；  
33. 后续CAS在core2执行成功，core1更新obj1，core更新obj2，此时obj1和obj2就刚好形成先后关系；global prod head指向obj1，global prod head指向obj2；  
34. 此时更新global prod tail，必须是local prod head == global prod tail才更新，也就是core1更新，global prod tail == local prod next1，也就是obj2；  
35. 最后core2检查发现，也满足global prod tail == local prod head的条件，因此core2也更新，最终global prod tail == global prod head == obj2的next；  

## 内存池
可以创建固定大小的内存池，这个就是一块完整的内存：  
```
struct rte_mempool * rte_mempool_create(const char           name,             //mempool名字
    unsigned             n,                 //元素个数
    unsigned             elt_size,          //元素大小
    unsigned             cache_size, 
    unsigned             private_data_size, 
    rte_mempool_ctor_t   mp_init, 
    void*                mp_init_arg, 
    rte_mempool_obj_cb_t obj_init, 
    void*                obj_init_arg, 
    int                  socket_id, //在NUMA情况下，socket_id参数是套接字标识符。如果预留区域没有NUMA约束，取值为SOCKET_ID_ANY。
    unsigned             flags      //单/多生产者，单/多消费者
    )
```
也可以创建mbuf内存池，每块内存前面都有一个struct rte_mbuf的结构，然后是data；  
```
struct rte_mempool * rte_pktmbuf_pool_create(const char * 	name,
        unsigned            n,  
        unsigned 	        cache_size,
        uint16_t 	        priv_size,
        uint16_t 	        data_room_size,
        int 	        socket_id 
    )
```
内存池中的对象是通过上面说到的环形队列ring来管理的；  
mempool不是直接在大页上申请的，而是要依赖memzone，memzone就是一段连续的物理空间，一个mempool可以包含多个memzone。每个mempool有一个local cache数组，数组成员是指向rte_mbuf的指针，数组长度和cpu的核数对等；  
一个mempool至少需要3个memzone，其中mempool的local cache和头结构占用1个，内存池中的对象占用1个，而管理对象的rte_ring占用一个；  
local cache本身是由关联的核独占，其指针指向mempool中的对象，而mempool是多核共享的，即从mempool申请内存会由竞争；  

## mbuf

## PMD轮询驱动和uio


## 多队列
以run to completion模型为例
rte_eth_tx_queue_setup可以建立网卡、队列与cpu核数之间的对应关系；  
将网卡的某个接收队列分配给某个核，从该队列中收到的所有报文都应当在该指定的核上处理结束。  
从核对应的本地存储中分配内存池，接收报文和对应的报文描述符都位于该内存池。  
为每个核分配一个单独的发送队列，发送报文和对应的报文描述符都位于该核和发送队列对应的本地内存池中。  

## 负载均衡
1. 包类型
mbuf中有packet_type字段标识包的类型，包括二层、三层、四层，tunnel的类型；  
2. RSS  
网卡根据不同的包使用不同的key来计算hash值，tcp、udp是用四元组、ipv4的其他包是用src ip和dst ip等；算出来hash值之后就可以确定某一个队列；可以按照四元组hash，也可以按照二元组hash，或者四元组+tag进行hash；   
最终分配的是否均匀就看哈希算法，一般是用托普利兹算法、还有对称哈希；  
3. Flow Director  
根据包字段做精确匹配，把特定的包分配到特定的队列；维护了一个FLow Director表，表中规定了不同的包类型需要check哪些关键字，以及后续采用什么样的动作（分配队列、丢弃等）；  
4. 服务质量QoS流量类别   
根据不同的流量类别分配不同的队列，不同业务类型（TC，traffic class）根据加权严格优先级（比如VLAN的UP user priority）来调度，同一个TC内的不同队列则采用轮询的方式调度；  
5. 虚拟化分流  

针对诸多的负载均衡技术，sdn有一个可重构匹配表RMT，表定义了三样内容，一是使用那种分类方式，二是每种分类方式需要哪些匹配信息，三是匹配之后采取什么样的动作；  

## CPU亲和
linux内核提供的CPU亲和，针对某个线程，将线程与某个cpu绑定，提升cache命中率；  
lcore初始化过程：
1. rte_eal_cpu_init，获取系统当前cpu信息；  
2. eal_parse_args，确定哪些cpu可用并设置master cpu；  
3. 

## 两种模式
报文输入-报文预处理-报文分流-ingress队列->调度到某个cpu处理->egress队列->post处理->报文输出；  
### run to complete
一个报文从始至终都在1个CPU核上处理；
### pipeline
将一个大功能细分成多个独立的阶段，不同阶段通过队列传递报文，某个CPU单独负责其中某一个阶段的工作；  


