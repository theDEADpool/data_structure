# ospf
属于IGP内部网关协议，BGP属于EGP外部网关协议；

优点：  
1. 基于SPF算法，以累计链路开销作为参考计算最短路径；  
2. 采用组播形式发送报文，减少不必要的开销；相比RIP采用广播的方式，BGP是单播；  
3. 支持区域划分；  
4. 支持等价路由进行负载分担；  
5. 支持报文认证；  

## 基本概念
Route id：在ospf区域中唯一标识一台路由器，32位无符号整数，以ip格式来定义；比如route id是1.1.1.1，但不代表路由器就持有这个ip；  
1. 手动配置；  
2. 使用lo接口中最大的地址作为route id；  
3. 没有lo接口就用最大的ip地址作为route id；  

ospf区域类型  
AREA0：骨干区域，一定有，且骨干区域必须连续（虚拟链路）。其他区域必须与骨干区域相连；  
AREA1...N：子区域，区域之间的通信都要经过骨干区域；   
stub area：末梢区域，不接受自治系统以外的路由信息。该区域的ABR不允许发送Type 5类型的LSA；   
完全stub area：完全末梢区域，不接受自治系统以外以及自治系统内其他区域的路由信息。该区域的ABR除了不允许发送Type 5类型的LSA，还不允许发送type 3类型的LSA；  
NSSA：非纯末梢区域，类似与末梢区域，允许接收TYPE 7类型的LSA，并将其转换成TYPE 5类型；  

cost：计算方式 = 100Mbps / 接口带宽，最小是1，也可以人为配置；

路由器类型  
区域边界路由器ABR：用来连接AREA0和其他区域的路由器，ABR和骨干区域之间可以是物理连接，也可以是虚拟连接；  
内部路由器：每个区域内部的路由器；  
骨干路由器：至少有一个接口连接到骨干区域的路由器，所有的ABR都是骨干路由器；  
自治边界路由器ASBR：用来连接ospf的AS(自治系统)与不属于ospf区域的路由器；  

## 数据包类型
1. Hello包  
用于发现和维持邻居关系，选举DR和BDR；  
用组播的形式发送hello报文，目的地址224.0.0.5。对于NBMA网络，不能发送组播，就只能通过人工配置的方式指定邻居；  
a. network mask：hello包发送接口的掩码，用作校验，点对点网络不需要检验该掩码；  
b. hello interval：hello包发送间隔，通常是10s；  
c. route dead interval：失效时间，即时间内没有收到hello包则认为邻居失效，通常是40s；  
d. neighbor：邻居的route id列表；  
e. route priority：优先级，默认位1，用于选举DR/BDR，如果为0则表示不参与DR/BDR选举；  
f. Designated Router：DR路由器地址；  
g. Back Designated Route：BDR路由器地址；  

2、DBD包
描述本地lsdb的摘要信息，即本地有哪些LSA，接收方可以根据该信息判断自己需要哪些LSA；  
DBD包还用来确定主从关系；  
两个邻居发送的第一个DBD包都没有摘要信息，会携带一个seq和route id。比如A给B发的DBD seq = x，B给A发的DBD seq = y；  
B的route id大为主，则A给B回的DBD包seq = y，表示收到了B的DBD报文，同时会携带摘要信息；B接下来发的DBD报文seq = y + 1，也会携带摘要信息；  

3、LSR包  
链路状态请求包，路由器收到包含新内容的DBD包之后，请求更详细的信息；  

4、LSU包  
链路状态更新包，收到LSR包之后发送LSA，通告链路状态。一个LSU可能包含几个LSA。可以给其他邻居路由器发送，也可以自己以组播的方式洪泛本地LSA；  
收到LSA之后需要通过ack确认，对于没有收到的LSA重传，重传是点对点发送的；  

5、LSA包  
链路状态确认包，确认收到LSU；  
type1 路由器LSA 每个路由器都会产生，在所属的区域内传播。描述该路由器到本区域内部的链路状态和cost。边界路由器可以产生多个不同的type1 LSA；  
type2 网络LSA 由区域内DR发出，描述区域内所有链路状态信息；  
type3 网络汇总LSA 由ABR产生描述区域内所有网段的路由，通告给其他区域；  
type4 ASBR汇总LSAE 由ARB产生描述到ASBR的路由，通告给除ASBR所在区域内的其他区域；  
type5 AS外部LSA 由ASBR产生描述到AS外部的路由，通告除stub区域和NSSA区域外的所有区域；  
type7 NSSA外部LSA NSSA区域内ASBR发出，用于通告本区域连接的外部路由；  

## 三个步骤三张表
1. 建立邻居关系 -- 邻居表；
2. 形成邻接关系 -- LSDB表，保存所有的LSA信息；
3. 计算路由 -- OSPF路由表；  

## 邻居状态机
DOWN、INIT、2-WAY状态  
路由器通过hello包建立双向通信的过程。
DOWN：路由器起始状态，没有收到任何邻居的hello报文，自己会不断组播发送hello报文；  
INIT：收到了邻居的hello包，但没有自己的routeID；  
2-WAY：收到了邻居的hello包，且有自己的routeID。双向通信已建立，但还未建立邻接关系。选举DR和BDR在这个阶段完成；  

EXSTART、EXCHANGE、LOADING、FULL状态  
路由器建立完全邻接关系的过程。  
EXSTART：发送自己的空DBD报文和收到邻居的空DBD报文，确定主从关系。routeID大的路由器为主。主路由器决定DBD报文的序号；  
EXCHAGE：互相发送DBD报文同步自己的链路状态数据库；  
LOADING：发送LSR包请求链路状态的详细信息，收到邻居回复的LSU报文，发送LSAck确认收到了LSU；  
FULL：区域内所有的路由器链路状态信息达成一致；  

## 选举DR/BDR
1. 如果是点对点网络不需要选举DR/BDR，广播网络需要选举，网络类型是接口的一个属性；  
2. 选举的目的是避免区域内的路由器重复交换LSA信息；  
3. 选举之后区域内其他路由只需要与DR保持邻接关系，交换路由信息；  
4. 通过hello报文的优先级字段，优先级高的成为DR，优先级相同则route id大的称为DR；  
5. DR和BDR也是接口属性；  

