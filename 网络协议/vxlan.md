# VXLAN
## 产生背景 
1. 大型二层网络中 MAC 地址表项存在数量瓶颈问题，网络设备的 MAC 表装不下那么多的 MAC 地址表项；提到大型二层网络就会让人想到数据中心的网络，这其中除了大规模的物理设备之外，每个物理设备还会运行着数量不小的虚拟机，这些虚拟机也拥有自己的 MAC 地址。这样一来无形之中就将 MAC 地址的数量“堆”到了一种极致;  
2. VLAN 只有 4096个。二层网络隔离能力有限，在上文描述的大型二层网络环境中，仅靠 4096 个 VLAN 无法再满足大型二层网络的隔离需求；
3. 虚拟机迁移范围受限，无法跨越三层网络进行迁移。如果虚拟机迁移改变了 IP地址就会造成业务中断，所以虚拟机迁移必须在二层网络中进行。例如虚拟机无法在不改变 IP、不中断业务的前提下从华南的机房迁入华北的机房;  

VLAN只有4096个租户，VXLAN可以有1600w的租户；
mac in UDP,把二层网络报重新封装在一个udp包里；
[outhdr](ethhdr|iphdr|udphdr)vxlanId[Innerhdr](ethdhr|...)

VXLAN术语
1. VTEP  
VXLAN Tunnel EndPoint。VTEP负责VXLAN报文的封装与解封装，包括ARP请求报文和正常的VXLAN数据报文；  
vtep设备可以是独立的网络设备也可以是虚拟交换机；  
vtep具备两个接口，一个是本地桥接接口，负责原始报文的发送和接收，一个是ip接口，负责vxlan报文的发送和接收；  

2. VNI  
Virtual Network ID。VNI封装在VXLAN头部，共24位。  

3. VXLAN网关  
用于连接VXLAN网络和传统VLAN网络，实现VNI和VLAN ID之间的映射。VXLAN网关也算是VTEP的一种。
VXLAN封装
VXLAN使用UDP封装原始的二层报文，封装报文头部共50字节。  
Inner MAC：原始的以太网帧的MAC地址。  

## vxlan报文字段
vxlan flags(1Byte)固定为00001000
Reserved(3Byte)
vni(3Byte):表示租户，一个租户可以有多个vni，不同vni的租户之间不能进行二层通告；
Reserved(1Byte)
VXLAN Header：8个字节，标记位1个字节（8bit），VXLAN ID3个字节(24bit)。保留位全部为0。
Outer UDP Header：8个字节，默认监听4789端口。UDP校验和必须为0。
Outer IP Header：20个字节，目的IP是对端VTEP的IP，源IP是本端VTEP的IP。
Outer Ethernet Header：14个字节，目的MAC是对端VTEP的MAC或者是网关的MAC。源MAC是本端VTEP设备的MAC。

## 报文进入vxlan隧道
同一个大二层的vtep设备两两之间都需要建立vxlan隧道；
如何划分一个大二层区域?  
bridge domain，简称BD，表示一个大二层广播域；每一个BD需要一个与之对应的vni；

首先要回答一个问题，哪些报文需要进入vxlan隧道？  
通过接口来决定，传统网络定义的三种接口：  
1. access接口，PC和交换机相连的接口；  
接口收到带Tag的报文，如果Tag与PVID一致，则允许进入，否则丢弃；发送时，会剥离掉Tag，并允许此报文通过此接口发出；  
PVID是port base vlan id，一个port只能拥有一个PVID；  
接收收到不带Tag的报文，允许进入，并打上PVID，并发出；  
2. trunk接口，一般用于交换机之间的互联接口；  
只允许所属vlan报文进入和发出交换机；允许多个带tag的报文从交换机发出，但只允许一个untag报文从交换机发出；  
接口收到带Tag报文，如果Tag和接口vlan一致则允许进入，否则丢弃；收到untag报文，当pvid在接口所属vlan中，允许进入并打上pvid，否则丢弃；  
要发送的报文带tag，但tag不在接口所属vlan中，则不允许从此接口发出；  
如果tag在接口所属vlan中且与pvid一致，则剥离tag并从此接口发出；但如果tag与pvid不一致，则不剥离tag并从此接口发出；  
3. hybrid接口，一般不适用；  
只允许所属vlan报文进入和发出交换机；允许多个带tag的报文从交换机发出，也允许多个untag报文从交换机发出；  

接口实际上做了两件事情，一是判断哪些报文需要进入vxlan隧道，二是判断需要对报文做怎样的处理；  
接口是根据不同的报文封装类型来做出不同处理的，有四种封装类型：  
dot1q：对于带有一层VLAN Tag的报文，该类型接口只接收与指定VLAN Tag匹配的报文；对于带有两层VLAN Tag的报文，该类型接口只接收外层VLAN Tag与指定VLAN Tag匹配的报文。  
untag：该类型接口只接收不带VLAN Tag的报文。  
qinq：该类型接口只接收带有指定两层VLAN Tag的报文。  
default：允许接口接收所有报文，不区分报文中是否带VLAN Tag。不论是对原始报文进行VXLAN封装，还是解封装VXLAN报文，该类型接口都不会对原始报文进行任何VLAN Tag处理，包括添加、替换或剥离。  

有了上面的信息，就可以判断出哪些报文属于哪个BD了（实际上就是业务如何接入vxlan隧道）；  
有两种方法，一个是基于vlan的方式，一个是基于报文封装方式。
基于vlan的方式，即建立连接在vtep设备上的vm都划分不同的vlan，然后再在vtep设备上建立vlan与bd之间的对应关系。当vtep收到一个数据包，根据其所属的vlan找到bd从而找到对应的vni。多个vlan可以映射到相同的bd中；

举例：  
vm1（vlan10）/vm2（vlan30） -- (trunk1)switch(trunk2) -- vtep
在trunk上会配置pvid vlan：30；allow pass vlan：10 30
那vm1的包经过switch会继续保留vlan10的标签，而vm2的包经过switch之后则会剥离掉vlan30的标签。
vtep上会创建子接口，比如子接口1用dot1q的形式，那vlan10的包就回命中这个接口配置，走BD10；
比如子接口2用untag，那没标签的包过来就会命中这个接口配置，走BD30；

在知道报文属于哪个BD之后，就能够根据BD中对应的peer list，将报文发给对应的peer；如果是广播&组播报文，就发给所有的peer，如果是单播，则vtep根据mac表匹配报文要从哪个接口发出；  

## 如何建立vxlan隧道
那这里产生的新问题就是BD的peer list怎么来的，也就是vxlan隧道是如何建立的？  
两种方式，一是手动配置，二是依靠EVPN自动建立隧道；  

## vxlan网关种类
vxlan二层网关，用于终端接入，也可用于同一vxlan网络的子网通信；  
vxlan三层网关，用于不同子网或与外部网络通信；  
集中式网关，将三层网关部署在同一台设备上，所有设备都需要与三层网关建立vxlan隧道，所有跨子网的流量都经过这个三层网关转发；可以对流量进行集中式管理，但所有转发路径不是最优，且三层网关ARP表的大小会限制整个网络的规模；  
分布式网关，spine-leaf架构，leaf节点就是vtep设备，leaf节点之间互相建立vxlan隧道，spine不感知隧道，只是做报文转发；leaf节点是二层网关，即连接到同一leaf节点的物理机or虚拟机都通过该节点进行二层互通，同时又是三层网关，即不同子网与外部网络的通信也是通过leaf节点完成；  

## 参考
https://support.huawei.com/enterprise/zh/doc/EDOC1100087027#ZH-CN_TOPIC_0254803605

## EVPN
最初的vxlan没有控制面，依赖于手工配置和流量泛洪来学习主机地址，导致流量浪费；  
通过EVPN可实现VTEP自动发现、VXLAN隧道自动建立，从而降低网络部署、扩展的难度；  
EVPN可以同时发布二层MAC和三层路由信息，可以减少网络中泛洪流量；  

传统的BPG-4只能管理ipv4单播路由，通过对NLRI的扩展，衍生出MP-MGP。NLRI扩展了多个新的地址族，比如ipv4、vpn等，EVPN就是在L2VPN地址族下扩展了一个子地址族evpn，定义了五种新的路由类型：  
Type1 Ethernet auto-discovery (AD) route，以太自动发现路由  
Type2 MAC/IP advertisement route，MAC/IP路由  
Type3 Inclusive multicast Ethernet tag route，Inclusive Multicast路由  
Type4 Ethernet segment route，以太网段路由  
Type5 IP prefix route，IP前缀路由  
其中EVPN vxlan主要关注type2、3、5三种路由；  

### Type2 路由报文格式
1. Route Distinguisher  
该字段为EVPN实例下设置的RD（Route Distinguisher）值。作用类似于L3VPN的RD值，这里的RD是区分不同的EVPN实例。一个二层广播域BD就对应一个EVPN实例。  
2.Ethernet Segment Identifier  
该字段为当前设备与对端连接定义的唯一标识。  
3. Ethernet Tag ID
该字段为当前设备上实际配置的VLAN ID。  
4. MAC Address Length  
该字段为此路由携带的主机MAC地址的长度。  
5. MAC Address  
该字段为此路由携带的主机MAC地址。  
6. IP Address Length  
该字段为此路由携带的主机IP地址的掩码长度。  
7.IP Address   
该字段为此路由携带的主机IP地址。  
8. MPLS Label1  
该字段为此路由携带的二层VNI，用于标识不同的BD。  
9. MPLS Label2  
该字段为此路由携带的三层VNI，用于标识不同的VRF。VXLAN网络中为了实现不同租户之间的隔离，需要通过不同的VRF（L3VPN）来隔离不同租户的路由表，从而将不同租户的路由存放在不同的私网路由表中，而三层VNI就是用来标识这些VRF的。  

### Type2 路由应用场景
1. 通告主机MAC地址  
要实现同子网主机的二层互访，两端VTEP需要相互学习主机MAC。作为BGP EVPN对等体的VTEP之间借助Type2路由，可以相互通告已经获取到的主机MAC。详细的说明请参见本文的使用EVPN学习MAC地址。  
2. 通告主机ARP
MAC/IP路由可以同时携带主机MAC地址+主机IP地址，因此该路由可以用来在VTEP之间传递主机ARP表项，实现主机ARP通告，可应用于以下两个子场景：  
场景1：ARP广播抑制。当三层网关学习到其子网下的主机ARP时，生成主机信息（包含主机IP地址、主机MAC地址、二层VNI、网关VTEP IP地址），然后通过传递ARP类型路由将主机信息同步到二层网关上。这样当二层网关再收到ARP请求时，先查找是否存在目的IP地址对应的主机信息，如果存在，则直接将ARP请求报文中的广播MAC地址替换为目的单播MAC地址，实现广播变单播，达到ARP广播抑制的目的。ARP广播抑制的详情可参见本文中的VXLAN BGP EVPN网络中的ARP广播抑制。  
场景2：分布式网关场景下的虚拟机迁移。当一台虚拟机从当前网关迁移到另一个网关下之后，新网关学习到该虚拟机的ARP（一般通过虚拟机发送免费ARP实现），并生成主机信息（包含主机IP地址、主机MAC地址、二层VNI、网关VTEP IP地址），然后通过传递ARP类型路由将主机信息发送给虚拟机的原网关。原网关收到后，感知到虚拟机的位置发生变化，触发ARP探测，当探测不到原位置的虚拟机时，撤销原位置虚拟机的ARP和主机路由。  
3. 通告主机IP路由  
在分布式网关场景中，要实现跨子网主机的三层互访，两端VTEP（作为三层网关）需要互相学习主机IP路由。作为BGP EVPN对等体的VTEP之间通过交换Type2路由，可以相互通告已经获取到的主机IP路由。详细的说明请参见本文的主机路由发布。  
4. ND表项扩散  
Type2路由可以同时携带主机MAC地址+主机IPv6地址，因此该路由可以用来在VTEP之间传递ND表项，实现ND表项扩散，可用于实现NS组播抑制、防止ND欺骗攻击、分布式网关场景下的IPv6虚拟机迁移。  
5. 通告主机IPv6路由  
在分布式网关场景中，要实现跨子网IPv6主机的三层互访，网关设备需要互相学习主机IPv6路由。作为BGP EVPN对等体的VTEP之间通过交换Type2路由，可以相互通告已经获取到的主机IPv6路由。

### Type3路由
主要用于在VTEP之间相互通告二层VNI、VTEP IP信息，以建立头端复制列表，即用于VTEP的自动发现和VXLAN隧道的动态建立。如果对端VTEP IP地址是三层路由可达的，则建立一条到对端的VXLAN隧道。同时，如果对端VNI与本端相同，则创建一个头端复制表，用于后续BUM报文转发；  
路由字段：
1. Route distinguisher：EVPN实例下配置的RD值；  
2. Ether Tag ID：当前设备的vlan id，为全0；  
3. IP address length：vtep设备ip长度；  
4. origin route ip address：vtep设备ip；  
5. flags：vxlan场景没用；  
6. Tunnel type：隧道类型，vxlan场景是6，头端复制，用于BUM报文转发；  
7. MPLS label：二层vni信息；  
8. Tunnel identifier：vxlan场景也是本端vtep的ip；   

### Type5路由
IP前缀路由，主要用于传递网段路由。不同于Type2路由只传递32（IPv4）/128（IPv6）位的主机路由，Type5路由可传递0～32/0～128掩码长度的网段路由，也可以传递主机路由；  
当携带主机IP地址时，主要用于分布式网关场景中的主机/网段路由通告；当携带网段地址时，通过传递该类型路由，可以实现VXLAN网络中的主机访问外部网络；   

## BGP EVPN的工作过程
### EVPN学习MAC地址
host1 -- vtep1 --vxlan tunnel-- vtep2 -- host2  
1. host1连接至vtep1时会触发arp、dhcp等行为，vtep1就能学习到host1的mac地址，记录在mac表中；  
2. 然后vtep1向vtep2（两者是BGP邻居关系）发送type2路由，通告本端的vtep ip、host1的mac地址、vni、RT值等信息；  
3. vtep2根据RT值与本端RT进行比较选择是否接纳路由，两者相同则接纳，vtep2会建立对应关系，下一跳是vtep1的ip对应host1的mac地址；  
4. 同理vtep1也可以学习到host2的mac地址；  
### 建立EVPN头端复制表和vxlan隧道
1. vtep之间通过type3路由建立头端复制表，会携带vni、本端vtep ip、RD、出方向vpn-target等信息； 
2. vtep2收到该路由后，如果vtep1 ip三层可达，则建立vxlan隧道，如果vni相同，则建立头端复制表，用于后续广播、组播、未知单播报文的转发；   
### 发布主机路由
1. 主机路由可以指导分布式网关场景下vtep设备进行三层转发，即一台vtep设备通告其他设备，本端由哪些主机路由信息，其他设备才知道哪些报文要发送给该vtep设备。这一部分是通过type2路由完成的；  
2. 其他vtep设备收到该路由后，会记录该vtep设备的主机ip信息到路由表，下一跳为该vtep设备ip；  
### 发布网段路由
1.通过type5路由发布，如果vtep设备是某一网络中的唯一网关，就可以发布该路由信息；  

## vxlan流量转发
### 同子网单播报文转发
1. host1向host2发送报文，原始报文src ip=host1 ip，dst ip = host2 ip，src mac = host1 mac，dst mac = host2 mac；   
2. 报文到vtep1后，封装udp头部src ip = vtep1 ip，dst ip = vtep2 ip，src mac = vtep1 mac，dst mac = nexthop mac（也可能是vtep2 mac）；根据报文入接口或vlan信息判断其所属BD，通过BD找到vxlan隧道接口；  
3. vtep2收到报文会取出vxlan的udp头部，根据原始报文dst mac在本地mac表中到对应出接口转发给host2；  

### 同子网BUM报文转发
1. 如果是发送广播、组播、未知单播，会向子网内所有vtep设备发送；  
2. vtep设备收到报文后会找到该BD内所有vxlan隧道接口进行报文转发；  

## vxlan ARP抑制方法
1. ARP广播变单播   
还是上面的网络，vtep2收到host2的ARP报文后可以学习到host2的mac，同时生产ARP广播抑制表通过evpn向vtep1发布；  
当host1首次请求host2时，发送ARP广播请求host2的mac，vtep1收到请求后查询本地的抑制表，将ARP请求报文的dst mac从广播替换成host2的mac，然后再将ARP转发给vtep2-host2；  

2. ARP二层代答   
a. vtep2上开启ARP二层代答功能后，vtep2会侦听主机发送的ARP报文；  
b. 当2接收到Host2的ARP报文后，可以根据ARP生成相应的ARP广播抑制表项，并通过EVPN向vtep1发布，这样vtep1也可以学习到Host2的主机信息；  
c. Host1初次访问Host2，发送ARP广播请求来获取Host2的MAC地址；vtep1收到ARP广播请求后，查询ARP广播抑制表，因为已经有Host2的主机信息，所以vtep1直接对ARP请求进行代答；  
