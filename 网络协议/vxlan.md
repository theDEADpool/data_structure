服务器虚拟化迁移，大二层网络
冷迁移 热迁移
热迁移，访问不能中断，ip和mac都不能变；为了保证这些，就要求迁移时候必须在一个大二层网络内；


VLAN只有4096个租户，VXLAN可以有1600w的租户；
mac in UDP,把二层网络报重新封装在一个udp包里；
[outhdr](ethhdr|iphdr|udphdr)vxlanId[Innerhdr](ethdhr|...)

网络架构
vm -- vtep -vxlan隧道- vtep --vm
如果vni和bd相同，说明在一个二层网关体系内；
如果vin和bd不同，则需要三层网关；





VXLAN术语
VTEP
VXLAN Tunnel EndPoint。VTEP负责VXLAN报文的封装与解封装，包括ARP请求报文和正常的VXLAN数据报文。
VNI
Virtual Network ID。VNI封装在VXLAN头部，共24位。
VXLAN网关
用于连接VXLAN网络和传统VLAN网络，实现VNI和VLAN ID之间的映射。VXLAN网关也算是VTEP的一种。
VXLAN封装
VXLAN使用UDP封装原始的二层报文，封装报文头部共50字节。

Inner MAC：原始的以太网帧的MAC地址。


vxlan报文字段
vxlan flags(1Byte)固定为00001000
Reserved(3Byte)
vni(3Byte):表示租户，一个租户可以有多个vni，不同vni的租户之间不能进行二层通告；
Reserved(1Byte)
VXLAN Header：8个字节，标记位1个字节（8bit），VXLAN ID3个字节(24bit)。保留位全部为0。
Outer UDP Header：8个字节，默认监听4789端口。UDP校验和必须为0。
Outer IP Header：20个字节，目的IP是对端VTEP的IP，源IP是本端VTEP的IP。
Outer Ethernet Header：14个字节，目的MAC是对端VTEP的MAC或者是网关的MAC。源MAC是本端VTEP设备的MAC。


同一个大二层的vtep设备两两之间都需要建立vxlan隧道；
如何划分一个大二层区域
bridge domain，简称BD，表示一个大二层广播域；每一个BD需要一个与之对应的vni；
有了bd有了vni同时建立了vxlan隧道相当于完成了组网，但vm数据包到达vtep设备时候如何判断属于哪个bd或vni呢？
有两种方法，一个是基于vlan的方式，一个是基于报文封装方式。
基于vlan的方式，即建立连接在vtep设备上的vm都划分不同的vlan，然后再在vtep设备上建立vlan与bd之间的对应关系。当vtep收到一个数据包，根据其所属的vlan找到bd从而找到对应的vni；
多个vlan可以映射到相同的bd中；

主流是基于报文封装方式
在物理二层接口上创建子接口，如何创建子接口？交换机会配置truck接口，vm通过交换机连接vtep设备。
vm1（vlan10）/vm2（vlan30） -- (trunk1)switch(trunk2) -- vtep
在trunk上会配置pvid vlan：30；allow pass vlan：10 30
那vm1的包经过switch会继续保留vlan10的标签，而vm2的包经过switch之后则会剥离掉vlan30的标签。
vtep上会创建子接口，比如子接口1用dot1q的形式，那vlan10的包就回命中这个接口配置，走BD10；
比如子接口2用untag，那没标签的包过来就会命中这个接口配置，走BD30；

封装类型
untag，不允许携带vlan
dot1q，只允许特定一层vlan
qinq，只允许特定两层vlan
default，允许所有

vxlan报文转发时候如何知道对端的ip和mac？
通过泛洪机制，vtep会有自己的转发表，当vtep发现目标ip的mac地址本地没有的时候，会把报文泛洪给其他所有的vtep，目标vtep相应之后，源vtep就会学习到mac、vni和vtep的映射关系，并加入转发表；