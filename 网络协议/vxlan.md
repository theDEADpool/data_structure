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
VXLAN Header：8个字节，标记位1个字节（8bit），VXLAN ID3个字节(24bit)。保留位全部为0。
Outer UDP Header：8个字节，默认监听4789端口。UDP校验和必须为0。
Outer IP Header：20个字节，目的IP是对端VTEP的IP，源IP是本端VTEP的IP。
Outer Ethernet Header：14个字节，目的MAC是对端VTEP的MAC或者是网关的MAC。源MAC是本端VTEP设备的MAC。