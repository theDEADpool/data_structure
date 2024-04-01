# netfilter&iptalbes
## netfilter
netfilter是内核的一个框架，用于进行数据包的过滤和操作。比如防火墙、NAT、流量控制等。

netfilter主要组件包括：
Hook：控制在哪个阶段来处理包；  
表：组织和存储规则，规则就是包的处理方式，比如filter表、net表、mangle表等；  
链：每个表里都有多个链，链表示包的处理过程，常见的链包括INPUT、OUTPUT、FORWARD；  
匹配器：检查包是否满足设定的规则条件；  
目标：当包满足了规则条件后，就按照规则指定的目标对包进行操作，比如ACCEPT、DROP、REJECT等；  

hook支持哪些钩子点：
prerouting：包进入IP层之后，在经过路由匹配之前；  
local_in：经过路由判定是发给本地的数据包，在交给上层协议之前；  
forward：经过路由判定不是给本地的数据包，在转发出去之前；  
local_out：发出的包在没有经过路由判定之前；
postrouting：发出的包在经过路由判定之后；

数据包流向：
1. 发给本地  
pre_routing - local_in;   
2. 转发  
pre_routing - forward - post_routing;  
3. 本地发出  
local_out - post_routing;

hook点链表
prerouting - prerouting链  
local_in - input链  
forward - forward链  
local_out - output链  
postrouting - postrouting链  

注册钩子函数
nf_register_hook(struct nf_hook_ops *reg);  
nf_unregister_hook(struct nf_hook_ops *reg);

    struct nf_hook_ops {
        struct list_head;
        nf_hookfn *hook;  //对应的回调函数  
        u_int8_t pf;  //处理协议类型
        unsigned int hooknum; //钩子函数注册到哪个hook点
        int priority; //钩子函数优先级
    }
返回值
NF_ACCEPT: 在处理路径上正常继续，继续调用下一个hook函数；  
NF_DROP: 丢弃数据包，终止处理；  
NF_STOLEN: 数据包由hook函数进行处理，而不交给协议栈处理；  
NF_QUEUE: 将数据包入队后供用户态程序处理；  
NF_REPEAT: 重新调用当前hook，要注意如果hook函数处理错误，一直返回REPEAT会造成死循环；   
NF_STOP：直接跳过后续的hook；  

NF_QUEUE，数据包会被插入到指定的queue队列里，用户态程序从队列里接收数据包并进行处理，完成后该数据包将被释放； 
用户态程序通过nfqueue.Open来监听一个队列，并向队列中注册处理函数handle func；  
```
    func handlePacket(q *nfqueue.Nfqueue, a nfqueue.Attribute) int {
        if a.Payload != nil && len(*a.Payload) != 0 {
            pkt := packet(*a.Payload)
            fmt.Printf("tcp connect: %s:%d -> %s:%d\n", pkt.srcIP(), pkt.srcPort(), pkt.dstIP(), pkt.dstPort())
        }
        _ = q.SetVerdict(*a.PacketID, nfqueue.NfAccept)
        return 0
    }
```
NF_QUEUE支持的选项
1. --queue-num，指定使用哪个队列（0~65535）；  
2. --queue-balacne，指定一个队列范围，数据包就会在这些队列中进行负载均衡，同一个连接的数据包会分配到相同的queue中；  
3. --queue-bypass，默认情况，如果没有用户态程序监听某个队列，那这个队列上排队的包都会被丢弃，这个选项可以让这些包正常的进入到后续的处理流程；  
4. --queue-cpu-fanout，这个需要和--queue-balance一起使用，使用CPU id作为索引来把包进行排队； 

### conntrack
连接跟踪是实现SNAT和DNAT的前提，四个hook点对包进行跟踪；  
prerouting和local_out，是新连接第一个包最先到达的地方，一个是外部向本机建连，一个是本机向外部建连；将conntrack entry加入到unconfirm list中；  
postrouting和local_in，这是新连接第一个包离开netfilter最后的hook点，说明新连接第一个包没有被丢弃；    
struct nf_hook_ops {} //注册hook函数;  
struct nf_conntrack_tuple {} //连接跟踪的基本元素，表示特定方向的流；  
```
    struct nf_conn{
        struct nf_conntrack ct_general;   
        spinlock_t lock;  
        u16 cpu;  
        struct nf_conntrack_tuple_hash tuplehash[IP_CT_DIR_MAX];  
        unsigned long status;  
        struct timer_list timeout;  //超时定时器，包含超时处理函数；  
        counters;  //统计计数，包个数计数、字节数计数；  
        ...
    }
```
hash_conntrack_raw()：根据tuple计算一个32的哈希；  
nf_conntrack_in()：连接跟踪模块的核心，包进入连接跟踪的地方，包括以下步骤：
resolve_normal_ct()中计算元组hash，将其加入未确认tuplehash->nf_ct_timeout_lookup()判断是否超时；  
nf_conntrack_confirm()：确认前面通过nf_conntrack_in()创建新连接，将元组从未确认列表中删除；  
nf_ct_get()：获取连接跟踪数据，没建连返回null； 

连接跟踪表
是记录所有连接的hash表，全局变量nf_conntrack_hash指针指向；  
数据包进入netfilter之后，首先会进入nf_conntrack_in()，首先根据协议类型找到对应的协议模块，然后调用resolve_normal_ct()进一步处理，这个函数的主要作用就是判断数据包是不是已经在连接跟踪表中，如果不在，则创建nf_conntrack并初始化；  

以本端发起连接为例：
1.本地主机创建了一个数据包：skb_0(202.2.2.1：120 - 202.10.10.1：30，TCP);  
2.数据包在经过链 NF_IP_LOCAL_OUT 时，会首先进入链接跟踪模块。模块根据skb_0(202.2.2.1：120 - 202.10.10.1：30，TCP)调用函数ip_ct_get_tuple 将数据包转换成一个tuple；  
3.然后调用ct = tuplehash_to_ctrack() 尝试获取它的连接跟踪记录项，发现不存在该记录项，于是新建一个和它关联的连接跟踪记录，并将连接跟踪记录的状态标识为 NEW。要注意的地方时当新建一个连接跟踪记录时，连接跟踪记录中会生成两个tuple，一个是 原始方向的tuple[ORIGINAL] ={ 202.2.2.1：120 - 202.10.10.1：30，TCP };于此同时，系统会自动生成应答方向的tuple[REPLY] ={ 202.10.10.1：30 - 202.2.2.1：120，TCP }；记住要点，一个连接跟踪记录会持有两个方向上的tuple记录；  
4.该数据包被发送给目的主机，目的主机收到后，发送恢复数据包skb_1(202.10.10.1：30 - 202.2.2.1：120，TCP);给源主机；  
5.数据包skb_1到达主机的NF_IP_PRE_ROUNTING 链，首先进入连接跟踪模块，调用函数ip_ct_get_tuple 将数据包转换成一个tuple，可以知道这个tuple={ 202.10.10.1：30 - 202.2.2.1：120，TCP }；
7.对上面得到的tuple调用ct = tuplehash_to_ctrack() ，系统变得到了该数据包属于哪一个连接记录，然后将该连接记录项中的状态改成ESTABLISHED；  
8.后面从本机发送的和从外面主机发过来的数据包都可以根据这个机制查找到相应的属于自己的连接记录项，如果不能查找到连接记录项，系统会丢弃该数据包。这边是简单的防火墙的实现；  

### netfilter NAT
实现了SNAT和DNAT，源地址转换和目的地址转换；  
主要关注prerouting和postrouting两个hook点的ip_nat_fn()

## iptables
运行在用户态，有四张表，每个表下面有不同的链；  
1. Filter
过滤数据包，实现防火墙功能的表；  
input forward output

2. NAT
负责网络地址转换；
prerouting（主要用来做DNAT） postrouting（主要用来做SNAT） output

3. Managle  
专门用来改变数据包;  
input forward prerouting output postrouting
  
4. Raw  
优先级最高，设置raw时一般是为了不再让iptables做数据包的链接跟踪处理，提高性能
prerouting output

五条链  
a.input:进入到本地设备的包；  
b.forward:对路由后的数据包做修改；  
c.prerouting:在路由之前更改数据包；  
d.output:本地创建的数据包在路由之前做修改；  
e.postrouting:在数据包离开时更改路由信息；  

iptable命令
iptables -t table -operation chain -j target match
iptables -t filter -a input -j accept -p tcp –dport 80
iptables -D INPUT 2 //删除规则
-operation  
-a 在链尾添加一条规则  
-i 插入规则  
-d 删除规则  
-r 替代一条规则  
-l 列出规则  

-j target  
accept 接收该数据报
drop 丢弃该数据报
queue 排队该数据报到用户空间
return 返回到前面调用的链
foobar 用户自定义链

## NAT实例
iptables -t nat -a postrouting -j snat –to-source 1.2.3.4
iptables -t nat -a prerouting -j dnat –to-destination 1.2.3.4:8080 -p tcp –dport 80 -i eth1
iptables -t nat -a prerouting -j redirect –to-port 3128 -i eth1 -p tcp –dport 80

1. snat
client：10.10.10.8
server:192.168.10.4
client访问server时，在client上有一条SNAT规则如下：
iptables -A POSTROUTING -t nat -s 10.10.10.8 -j SNAT --to-source 192.168.10.2
在client主机上，数据流如下：10.10.10.8:2222 > 192.168.10.4:22
经过conntrack模块后，ct的tuple如下：
original tuple：10.10.10.8:2222 -> 192.168.10.4:22
reply tuple: 192.168.10.4:22 ->10.10.10.8:2222

经过POSTROUING的nat hook点时，查找到这条SNAT规则，执行其target，设置ct->status为IPS_SRC_NAT，复制一份original的tuple，将此tuple源ip换成--to-source指定的ip，再将此tuple revert后的值赋值给reply的tuple，此时ct的tuple如下，original的tuple是没变化的，reply的目的ip变成 --to-source指定的ip
original tuple：10.10.10.8:2222 -> 192.168.10.4:22
reply tuple: 192.168.10.4:22 ->192.168.10.2:2222

修改完ct的reply tuple后，在函数nf_nat_packet中，mtype为NF_NAT_MANIP_SRC，所以statusbit = IPS_SRC_NAT，dir为IP_CT_DIR_ORIGINAL，所以ct->status & statusbit 为1，所以需要修改数据包。
首先获取reply方向tuple的revert tuple：192.168.10.2:2222->192.168.10.4:22，
调用 manip_pkt 修改数据包，因为mtype为NF_NAT_MANIP_SRC，所以修改源ip为 192.168.10.2

if (ct->status & statusbit) {
        struct nf_conntrack_tuple target;

        /* We are aiming to look like inverse of other direction. */
        nf_ct_invert_tuplepr(&target, &ct->tuplehash[!dir].tuple);

        l3proto = __nf_nat_l3proto_find(target.src.l3num);
        l4proto = __nf_nat_l4proto_find(target.src.l3num,
                        target.dst.protonum);
        if (!l3proto->manip_pkt(skb, 0, l4proto, &target, mtype))
            return NF_DROP;
    }
所以最后从client发出去的数据流为：192.168.10.2:2222 -> 192.168.10.4:22。

从server返回的数据流为：192.168.10.4:22 -> 192.168.10.2:2222
在client上经过conntrack时，可以查找到ct表项，并且为reply方向，所以设置ctinfo = IP_CT_ESTABLISHED_REPLY，并且设置 ct->status 为 IPS_SEEN_REPLY_BIT。
然后在PREROUTING的nat hook点，此时ctinfo为IP_CT_ESTABLISHED_REPLY，所以不用查找nat表，直接执行nf_nat_packet，
此函数中，mtype为NF_NAT_MANIP_DST，所以statusbit = IPS_DST_NAT，dir为IP_CT_DIR_REPLY，所以执行 statusbit ^= IPS_NAT_MASK 后 statusbit为IPS_SRC_NAT，所以ct->status & statusbit 为1，所以需要修改数据包。
首先获取orginal方向tuple的revert tuple：192.168.10.4:22->10.10.10.8:2222，
调用 manip_pkt 修改数据包,因为mtype为NF_NAT_MANIP_DST，所以修改目的ip为 10.10.10.8.
最后进入client的数据流为：192.168.10.4:22 -> 10.10.10.8:2222.

2. dnat
client：10.10.10.8
server:10.10.10.12
client访问server时，在client上有一条DNAT规则如下：
iptables -A OUTPUT -t nat -d 192.168.10.4 -j DNAT --to-destination 10.10.10.12
在client主机上，数据流如下：10.10.10.8:2222 > 192.168.10.4:22
经过conntrack模块后，ct的tuple如下：
original tuple：10.10.10.8:2222 -> 192.168.10.4:22
reply tuple: 192.168.10.4:22 ->10.10.10.8:2222

经过OUTPUT的nat hook点时，查找到这条DNAT规则，执行其target，设置ct->status为IPS_DST_NAT，复制一份original的tuple，将此tuple目的ip换成--to-destination指定的ip，再将此tuple revert后的值赋值给reply的tuple，此时ct的tuple如下，original的tuple是没变化的，reply的源ip变成 --to-destination指定的ip
original tuple：10.10.10.8:2222 -> 192.168.10.4:22
reply tuple: 10.10.10.12:22 ->10.10.10.8:2222

修改完ct的reply tuple后，在函数nf_nat_packet中，mtype为NF_NAT_MANIP_DST，所以statusbit = IPS_DST_NAT，dir为IP_CT_DIR_ORIGINAL，statusbit 仍然为IPS_DST_NAT，所以ct->status & statusbit 的结果为1，所以需要修改数据包。
首先获取reply方向tuple的revert tuple：10.10.10.8:2222->10.10.10.12:22，
调用 manip_pkt 修改数据包，因为mtype为NF_NAT_MANIP_DST，所以修改目的ip为 10.10.10.12

if (ct->status & statusbit) {
        struct nf_conntrack_tuple target;

        /* We are aiming to look like inverse of other direction. */
        nf_ct_invert_tuplepr(&target, &ct->tuplehash[!dir].tuple);

        l3proto = __nf_nat_l3proto_find(target.src.l3num);
        l4proto = __nf_nat_l4proto_find(target.src.l3num,
                        target.dst.protonum);
        if (!l3proto->manip_pkt(skb, 0, l4proto, &target, mtype))
            return NF_DROP;
    }
所以最后从client发出去的数据流为：10.10.10.8:2222 -> 10.10.10.12:22。

从server返回的数据流为：10.10.10.12:22 -> 10.10.10.8:2222
在client上经过conntrack时，可以查找到ct表项，并且为reply方向，所以设置ctinfo = IP_CT_ESTABLISHED_REPLY，并且设置 ct->status 为 IPS_SEEN_REPLY_BIT。
然后在INPUT的nat hook点，此时ctinfo为IP_CT_ESTABLISHED_REPLY，所以不用查找nat表，直接执行nf_nat_packet，
此函数中，mtype为NF_NAT_MANIP_SRC，所以statusbit = IPS_SRC_NAT，dir为IP_CT_DIR_REPLY，所以执行 statusbit ^= IPS_NAT_MASK 后 statusbit为IPS_DST_NAT，所以ct->status & statusbit 为1，所以需要修改数据包。
首先获取orginal方向tuple的revert tuple：192.168.10.4:22->10.10.10.8:2222，
调用 manip_pkt 修改数据包,因为mtype为NF_NAT_MANIP_SRC，所以修改目的ip为 192.168.10.4
最后进入client的数据流为：192.168.10.4:22 -> 10.10.10.8:2222.

## mangle修改报文
iptables -t mangle -a prerouting -j mark –set-mark 0x0a -p tcp
iptables -t mangle -a prerouting -j tos –set-tos 0x10 -p tcp –dport ssh
