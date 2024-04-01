# XDP
XDP不会绕过内核，好处如下：
1. XDP可以复用内核网络驱动，内核的一些基础设施，比如系统路由表、socket等；
2. XDP处理之后数据包仍可以重新给到内核协议栈，容器网络的命名空间等；  
3. XDP支持在运行时原子性的创建新程序，不会中断网络流量，更不需要重启系统；   
4. XDP可以运行在轮询或中断模式下，不需要显示分配CPU，没有特殊的硬件需求，也不需要依赖hugepage；   

## 三种模式
1. Native  
需要网卡支持，在网卡驱动NAPI的poll函数中调用；   、
2. Generic  
如果网卡不支持XDP，就在内核的receive_skb()中调用，这个函数在gro重组报文之后；   
3. Offloaded  
将XDP的程序offload到网卡里，比Native性能更高，需要网卡支持，jit编译器将BPF代码翻译成网卡原生指令；   

## 返回码
1. XDP_DROP，丢弃当前包；  
2. XDP_PASS，把当前包交还给内核处理；  
3. XDP_TX，从收到包的网卡上将该包发出去（hairpin模式），主要用于负载均衡；
4. XDP_REDIRECT，将当前包重定向到另一个网卡发出去。还可以将报文重定向到其他CPU处理，由其他CPU继续完成内核的处理流程。当前CPU可以继续接收新的pkt；  
5. XDP_ABORTED，处理过程出现异常，类似于XDP_DROP，但会通过tracepoint打印日志追踪；  

## 基本概念
数据包在XDP中的表示是

    struct xdp_buff {
        //指向数据包中数据的起始位置
        void *data;
        //指向数据包中数据的结束位置
        void *data_end;
        void *data_meta;
        //指向页面中最大可能的headroom起始位置
        void *data_hard_start;
        struct xdp_rxq_info *rxq;
    }；

对包加自定义header的时候，data会逐渐向data_hard_start靠近。通过bpf_xdp_adjust_head()实现。同样也可以使用这个函数去除自定义header。  
data_meta开始指向与data相同的位置，bpf_xdp_adjust_meta()能够将其朝data_hard_start移动。可以给自定义元数据提供空间，这个空间对协议栈不可见。    data_meta也可以单纯用于尾调用时候传递状态。因此data_hard_start <= data_meta < data < data_end;  
rxq是指向某些额外的和每个接收队列相关的元数据

    struct xdp_rxq_info {
        struct net_device *dev;
        u32 queue_index;
        u32 reg_state;
    }____cacheline_aligned;

XDP程序执行了会有一个返回结果，在linux/bpf.h中定义了以下几种结果：

    enum xdp_action {
        //表示程序异常，包会丢弃，可以通过trace_xdp_exception来监控异常
        XDP_ABORTED = 0,
        //直接丢弃，适用于DDos或通用防火墙
        XDP_DROP,
        //允许将当前包送到网络协议栈，会给包分配一个skb
        //与没有XDP的数据包默认处理行为是一样的
        XDP_PASS,
        //将包直接从收到包的网卡上发出去，可用于高效率负载均衡
        XDP_TX,
        //将包从其他的网卡上发送出去，
        //也可以将包从当前CPU转交给其他CPU，由后者将包转交给上层协议栈
        //这样当前CPU就可以专注于网卡收包和XDP的相关逻辑处理
        XDP_REDIRECT
    };

## AF_XDP
是一个协议族，用于高性能报文处理。通过XDP_REDIRECT返回码，可以将包交给其他CPU处理或其他网卡发送出去。AF_XDP则利用bpf_redirect_map函数将报文重定向到用户空间（UMEM）的内存缓冲区中。UMEM是一块固定大小的内存，被分为一组大小相等的内存块chunk。每个chunk都可以通过描述符（即内存偏移量）来访问。

AF_XDP套接字XSK有两个关联ring，分别是RX RING和TX RING。XSK可以在RX RING上收包，在TX RING上发包。每个XSK至少拥有其中一个RING，就看该XSK是只收包还是只发包。

UMEM也有两个ring，分别是FILL RING和COMPLETION RING。这些ring中保存的是UMEM上的chunk描述符。收发包动作如下：
1. 收包

    a. 用户态程序向FILL RING中写入空白chunk描述符；
    b. 内核从FILL RING上获取空白chunk，并将收到的包写入该chunk。然后将该chunk描述符写入RX RING上；
    c. 用户态程序从RX RING拿到收到的包数据；
2. 发包

    a.用户态程序向空白chunk中写入报数据，然后将该chunk放入TX RING中；
    b.内核从TX RING中消费掉chunk，将其中数据发送到网络中，之后将空白chunk放入到COMPLETE RING中告知用户态程序包发送完成；

用户态代码

    //创建xsk
    xsk_fd = socket(AF_XDP, SOCK_RAM, 0);

    //创建umem
    bufs = mmap(NULL, NUM_FRAMES * opt_xsk_frame_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|opt_mmap_flags, -1, 0);
    if (bufs == MAP_FAILED) {
        exit(EXIT_FAILURE);
    }

    //向xsk注册umem
    struct xdp_umem_reg mr;
    memset(&mr, 0, sizeof(mr));
    //addr是umem的起始地址
    mr.addr = (uintptr_r)umem_area;
    //len是umem的总长度
    mr.len = size;
    //chunk_size是每个chunk的大小
    mr.chunk_size = umem->config.frame_size;
    //如果设置了headroom，那数据包就不是从每个chunk起始地址开始读，而是要预留headroom大小的内存
    mr.headroom = umem->config.frame_headroom;
    mr.flags = umem->config.flags;

    err = setsockopt(umem->fd, SOL_XDP, XDP_UMEM_REG, &mr, sizeof(mr));
    if (err) {
        err = -errno;
        goto out_socket;
    }

    //创建FILL RING和COMPLETION RING，必须设置ring大小
    //FILL RING和COMPLETION RING是必须的，RX RING和TX RING可以二选一
    err = setsockopt(umem->fd, SOL_XDP, XDP_UMEM_FILL_RING, &umem->config.fill_size, sizeof(umem->config.fill_size));
    if (err) {
        err = -errno;
        goto out_socket;
    }

    err = setsockopt(umem->fd, SOL_XDP, XDP_UMEM_COMPLETION_RING, &umem->config.comp_size, sizeof(umem->config.comp_size));
    if (err) {
        err = -errno;
        goto out_socket;
    }
    //上述两个动作只是在内核中创建了两个ring，用户态空间想要访问还需要映射
    //先要获取ring结构体的偏移地址
    //offset的定义是
    struct xdp_ring_offset {
        __u64 producer;
        __u64 consumer;
        __u64 desc;
    }
    err = xsk_get_mmap_offset(umem->fd, &off)
    err = getsockopt(fd, SOL_XDP, XDP_MMAP_OFFSETS, off, &optlen);

    //umem->config.fill_size * sizeof(__u64)是FILL RING数组的长度，off.fr.desc是ring结构体的长度
    map = mmap(NULL, off.fr.desc + umem->config.fill_size * sizeof(__u64), PROT_READ| PROT_WRITE, MAP_SHARED | MAP_POPULATE, umem->fd, XDP_UMEM_PGOFF_FILL_RING);

    //umem->fill是用户态自定义的一个数据结构，用来指向实际ring结构中的对应成员。
    umem->fill = fill;
    fill->mask = umem->config.fill_size - 1;
    fill->size = umem->config.fill_size;
    fill->producer = map + off.fr.producer;
    fill->consumer = map + off.fr.consumer;
    fill->flags = map + off.fr.flags;
    fill->ring = map + off.fr.desc;
    fill->cached_cons = umem->config.fill_size;

    //completion ring的映射方式和上面fill ring类似。

    //创建TX ring和RX ring并映射
    if (rx) {
        err = setsockopt(xsk->fd, SOL_XDP, XDP_RX_RING, &xsk->config.rx_size, sizeof(xsk->config.rx_size));
    }
    if (tx) {
        err = setsockopt(xsk->fd, SOL_XDP, XDP_TX_RING, &xsk->config.tx_size, sizeof(xsk->config.tx_size));
    }
    err = xsk_get_mmap_offsets(xsk->fd, &off);
    if (rx) {
        rx_map = mmap(NULL, off.rx.desc + xsk->config.rx_size * sizeof(struct xdp_desc), PROT_READ| PROT_WRITE,MAP_SHARED | MAP_POPULATE, umem->fd, XDP_PGOFF_RX_RING);
        if (rx_map == MAP_FAILED) {
            ...
        }
        rx->mask = xsk->config.rx_size - 1;
        rx->size = xsk->config.rx_size;
        rx->producer = rx_map + off.rx.producer;
        rx->consumer = rx_map + off.rx.consumer;
        rx->flags = rx_map + off.rx.flags;
        rx->riing = rx_map + off.rx.desc;
    }
    xsk->rx = rx;

    //TX RING也是类似

    //将AF_XDP socket绑定到设备某一队列
    sxdp.sxdp_family = PF_XDP;
    sxdp.sxdp_ifindex = xsk->ifindex;
    sxdp.sxdp_queue_id = xsk->queue_id;
    sxdp.sxdp_flags = xsk->config.bind_flags;

    err = bind(xsk->fd, (struct sockaddr *)&sxdp, sizeof(sxdp));

内核态代码是利用pbf_redirect_map()函数以及BPF_MAP_TYPE_XSKMAP类型的map实现将报文重定向到用户态程序。


内核主要做的是优化的工作，通过热点函数
容器网络，虚拟网卡，没有支持AF_XDP。
cpu相关的优化-- 同核唤醒，绑定在同一个网卡队列的xsk的在同一个cpu核上处理。
nginx master 创建udp 和 对应 xsk，work会fork，相当于xsk有2次应用计数。
因此关闭的时候master要关 work也要关。
支持reload 和 upgrade。