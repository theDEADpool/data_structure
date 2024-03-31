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
        nf_hookfn *hook;
        u_int8_t pf;  //处理协议类型
        unsigned int hooknum; //钩子函数注册到哪个hook点
        int priority; //钩子函数优先级
    }

## iptables
运行在用户态

有四张表
1. Filter
实现防火墙功能的表；  

2. NAT
负责网络地址转换；

3. Managle  
专门用来改变数据包;  
  
4. Raw

五条链  
a.input:进入到本地设备的包；  
b.forward:对路由后的数据包做修改；  
c.prerouting:在路由之前更改数据包；  
d.output:本地创建的数据包在路由之前做修改；  
e.postrouting:在数据包离开时更改路由信息；