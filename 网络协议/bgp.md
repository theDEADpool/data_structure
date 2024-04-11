# BGP
不计算路由，只是传递路由信息；路由信息的来源可以是静态配置、可以是动态计算得到比如ospf。
用于大型网络，可以通告自治区域内的路由信息，也可以通告不同自治区域之间的路由信息。

TCP 179端口

建立邻居（对等体peer）
1.先tcp三次握手；  
2.open报文通告参数，比如自身AS号，keepalive报文发送时间、route id；  
3.双方周期性互发keepalive；  
4.对于IBGP建立邻居，一般使用loopback地址为路由更新的源地址；在同一网络中的设备，可能存在多个可联通的接口，只要其中一个接口是可联通的，loopback就是联通的。使用loopback会有更好的稳定性；  
5.对于EBGP建立邻居，则使用直连接口IP地址为源地址；

可以手动指定哪些路由需要发送给邻居
## 参考资料
https://cshihong.github.io/2018/01/23/BGP%E5%9F%BA%E7%A1%80%E7%9F%A5%E8%AF%86/

## BGP报文格式
### BGP header
1.Marker(16Byte):用于明确BGP报文边界，所有bit均为1；  
2.Length(2Byte):BGP报文总长度，包括报文头，单位是字节；  
3.Type(1Byte):BGP报文类型，取值1~5，分别对应Open、Update、Notification、Keepalive、Route-refresh；  
### Open报文
1.Version(1Byte):BGP版本，现在是4；  
2.My AS(2Byte):本地AS号，协商的双端通过AS号判断是否与自己属于同一个AS。  
3.Hold Time(2Byte):保持时间，双端会协商一致的Hold Time，如果在这个时间周期内没有收到Keepalive或Update报文，则认为BGP连接中断；  
4.BGP Identifier(4Byte):BGP标识符，以IP地址的形式表示，用来识别BGP路由器；  
5.Opt Parm Len(1Byte):表示接下来Opt参数的长度；  
### Update报文
发送路由更新，也包括删除路由；  
1.Unfeasible routes length(2Byte)
2.Withdrawn routes(NByte):撤销路由列表，当一条路由失效，就通过这个字段通告给邻居；  
3.Total path attribute length(2Byte)：Path attributes列表的长度；  
4.Path attributes(NByte):路由属性，包括下一跳信息、路由经过了哪些AS（如果路由属于当前AS内部，则为空）；  
5.NLRI(NByte):更新的路由信息（网络号+掩码）；    
NLRI字段用来描述路由信息，包括路由网络号和掩码；  
Path Attributes字段描述路径属性；  
### Notification报文
终止邻居关系，报告具体错误原因；    
1.Error code(1Byte):错误码；  
2.Error subcode(1Byte):错误子码；  
3.Data(可变长度):详细的错误内容；  
### Keepalive报文
标志邻居关系建立，维持邻居关系；  
只包含BGP Header，没有其他内容；  
### Route-refresh报文
改变路由策略后请求邻居重新发送路由信息，只有支持路由刷新能力的BGP才会发送和响应该报文；  
1. AFI(2Byte):地址族标识，IPv4、IPv6、vpn等；  
2. Res(1Byte):保留，8个bit全为0；  
3. SAFI(1Byte):子地址族标识，比如IPv4地址族下面还存在单播、组播、vpn等；  

## 状态机
1. Idle  
最开始的状态，如果无法正常开始三次握手，则停留在该状态；  
2. Connect  
正在进行TCP连接，等待建联完成。如果TCP连接建立失败，则进入Active状态，尝试重建TCP连接；  
3. Active  
TCP建联失败，重试建联；  
4. OpenSent  
TCP建联成功，发送Open包；  
5. OpenConfirm  
收到邻居的Open包，参数协商完成，自己发出Keepalive，等待邻居的Keepalive；  
6. Established  
已经收到对方的Keepalive报文，进入该状态。双方协商完成，开始使用Update报文通告路由信息； 

1. Idle状态是BGP初始状态。在Idle状态下，BGP拒绝邻居发送的连接请求。只有在收到本设备的Start事件后，BGP才开始尝试和其它BGP对等体进行TCP连接，并转至Connect状态。
2. 在Connect状态下，BGP启动连接重传定时器（Connect Retry），等待TCP完成连接。
如果TCP连接成功，那么BGP向对等体发送Open报文，并转至OpenSent状态。
如果TCP连接失败，那么BGP转至Active状态。
如果连接重传定时器超时，BGP仍没有收到BGP对等体的响应，那么BGP继续尝试和其它BGP对等体进行TCP连接，停留在Connect状态。
3. 在Active状态下，BGP总是在试图建立TCP连接。
如果TCP连接成功，那么BGP向对等体发送Open报文，关闭连接重传定时器，并转至OpenSent状态。
如果TCP连接失败，那么BGP停留在Active状态。
如果连接重传定时器超时，BGP仍没有收到BGP对等体的响应，那么BGP转至Connect状态。
4. 在OpenSent状态下，BGP等待对等体的Open报文，并对收到的Open报文中的AS号、版本号、认证码等进行检查。
如果收到的Open报文正确，那么BGP发送Keepalive报文，并转至OpenConfirm状态。
如果发现收到的Open报文有错误，那么BGP发送Notification报文给对等体，并转至Idle状态。
5. 在OpenConfirm状态下，BGP等待Keepalive或Notification报文。如果收到Keepalive报文，则转至Established状态，如果收到Notification报文，则转至Idle状态。
6. 在Established状态下，BGP可以和对等体交换Update、Keepalive、Route-refresh报文和Notification报文。
如果收到正确的Update或Keepalive报文，那么BGP就认为对端处于正常运行状态，将保持BGP连接。
如果收到错误的Update或Keepalive报文，那么BGP发送Notification报文通知对端，并转至Idle状态。
Route-refresh报文不会改变BGP状态。
如果收到Notification报文，那么BGP转至Idle状态。
如果收到TCP拆链通知，那么BGP断开连接，转至Idle状态。

## 邻居信息
1. Peer:邻居地址；  
2. V(Version):BGP版本号；  
3. AS:邻居所属的AS号；  
4. Up/Down:邻居Up/Down的持续时间；  
5. State:邻居BGP状态机状态；  
6. PrefRecv:从该邻居获取到的路由前缀数目；  

## 路由表信息
1. Network:路由目的网络地址和掩码；  
2. NextHop:下一跳地址；  
3. MED
4. LocPrf
5. PrefVal
6. Path/Ogn:Origin表示路由来源方式，incomplete表示路由是import route导入的；

加入路由的方式  
1. Network:逐条添加路由；  
2. Import route:将直连路由、静态路由、其他路由协议生产的路由导入BGP；  

BGP支持根据现有路由生成聚合路由Aggregation；

## 路由通告原则
1. 发布最优&有效路由；所谓有效，指某条路由的下一跳是可达的；所谓最优，指去到相同目的地经过AS数量越少就越优；  
2. 从EBGP邻居获得的路由会发给所有邻居(包括IBGP邻居和其他EBGP邻居)；
3. 从IBGP邻居获得的路由不会再发布给IBGP的邻居，该原则也被称为IBGP水平分割。水平分割防止AS内出现路由环路；
4. 当一台路由器从自己的IBGP邻居学习到一条BGP路由时，它不能使用该路由或将该路由发布给EBGP的邻居，除非又从其他途径（Ospf、静态路由等）学习到这条路由。该原则也被称为BGP同步原则，该原则是为了解决路由黑洞；
5. 为了解决原则3导致的IBGP路由无法再IBGP内部通告，IBGP内部无法互通的情况。可以让相同AS内的路由器实现全互联。但这种方式会带来AS内部负载开销变大；

## 路由属性（Update报文中Path attributes）
### 必须要携带
1. Origin:表示该路由的来源（IGP[network注入]、EGP[EGP学习到]、Incomplete[import route注入]，优先级从前到后）；
2. AS_path:表示该路由经过了哪些AS及顺序；
3. Next_hop:路由器学习到BGP路由后，会对下一跳信息进行检查，如果不可达则该条路由无效；  
a. 如果路由器是将路由通告给EBGP的邻居，此时路由的下一跳会设置为自己IP；  
b. 路由收到EBGP邻居通告的BGP路由之后，通告给IBGP邻居时，下一跳不变；  
c. 如果BGP路由的下一跳和EBGP邻居（通告对象）属于同一个网段，则该路由下一跳保持不变；  
d. 如果AS_path中包含自己所在的AS则该路由不会被接收；
### 必须要支持
1. Local_preference  
本地优先级，数值越大优先级越高。即当前AS的路由器会优先选择Lp数值大的路由作为去往其他AS的路由；该属性只能在IBGP邻居之间传递（即AS内部），不会传递给EBGP邻居；  
但AS边界路由器在收到EBGP的路由之后，可以通过命令给改路由赋予一个Lp属性；
2. Atomic_aggregate
### 可选过渡(如果BGP设备不识别此类属性，但会传递给其他邻居)
1. Aggregator
2. Community  
可以给一组相同用途的路由打上一个标记，后续路由器可以根据该标记批量选择一组路由做不同的策略；公认的community值如下：  
a.Internet(0):是community的缺省值；  
b.No_Advertise(0xFFFFFF02):该属性的路由不会向任何BGP通告；  
c.No_Export(0xFFFFFF01):该属性的路由不会向AS外发送，既不会发给EBGP邻居；  
d.No_Export_Subconfed(0xFFFFFF03):该属性路由不会向AS外发送，也不会发送AS内其他子AS发布；  
### 可选非过渡(如果BGP设备不识别此类属性，则不会传递给其他邻居)
1. MED(多出口鉴别器)
用于向EBGP邻居通告进入本AS优先选择使用哪个入口。数值越小优先级越高。主要影响AS之间的路由选择，当路由被传递给EBGP邻居时，其在本AS传递路由时会携带该值。当路由被再次传递给EBGP邻居时，不会携带该值；
MED只影响去到同一个AS不同路由的优先级，不影响去往不同AS路由的优先级；  
Cluster-List
Originator-ID

## 路由选择策略
当到达同一目的地存在多条路由时，BGP依次对比下列属性来选择路由：  
1. 优选协议首选值（PrefVal）最高的路由。  
协议首选值（PrefVal）是华为设备的特有属性，该属性仅在本地有效。    
2. 优选本地优先级（Local_Pref）最高的路由。  
如果路由没有本地优先级，BGP选路时将该路由按缺省的本地优先级100来处理。  
3. 依次优选手动聚合路由、自动聚合路由、network命令引入的路由、import-route命令引入的路由、从对等体学习的路由。  
4. 优选AS路径（AS_Path）最短的路由。  
5. 依次优选Origin类型为IGP、EGP、Incomplete的路由。  
6. 对于来自同一AS的路由，优选MED值最低的路由。  
7. 依次优选EBGP路由、IBGP路由、LocalCross路由、RemoteCross路由。  
PE上某个VPN实例的VPNv4路由的ERT匹配其他VPN实例的IRT后复制到该VPN实例，称为LocalCross；从远端PE学习到的VPNv4路由的ERT匹配某个VPN实例的IRT后复制到该VPN实例，称为RemoteCross。  
8. 优选到BGP下一跳IGP度量值（metric）最小的路由。  
9. 优选Cluster_List最短的路由。  
10. 优选Router ID最小的设备发布的路由。  
11. 优选从具有最小IP Address的对等体学来的路由。  

## 路由反射器
为了减少区域内路由器之间的邻居关系，只需要与RR（路由反射器）建立邻居关系，通过RR完成区域内路由信息的交换；  
### 基本概念
路由反射器RR（Route Reflector）：允许把从IBGP对等体学到的路由反射到其他IBGP对等体的BGP设备，类似OSPF网络中的DR；  
客户机（Client）：与RR形成反射邻居关系的IBGP设备。在AS内部客户机只需要与RR直连；  
非客户机（Non-Client）：既不是RR也不是客户机的IBGP设备。在AS内部非客户机与RR之间，以及所有的非客户机之间仍然必须建立全连接关系；  
始发者（Originator）：在AS内部始发路由的设备。Originator_ID属性用于防止集群内产生路由环路；  
集群（Cluster）：路由反射器及其客户机的集合。Cluster_List属性用于防止集群间产生路由环路；  

1. 从非客户机学到的路由，发布给所有客户机；  
2. 从客户机学到的路由，发布给所有非客户机和客户机（发起此路由的客户机除外）；  
3. 从EBGP对等体学到的路由，发布给所有的非客户机和客户机； 

### Cluster list
路由反射器和它的客户机组成一个集群（Cluster），使用AS内唯一的Cluster ID作为标识。为了防止集群间产生路由环路，路由反射器使用Cluster_List属性，记录路由经过的所有集群的Cluster ID；  

当一条路由第一次被RR反射的时候，RR会把本地Cluster ID添加到Cluster List的前面。如果没有Cluster_List属性，RR就创建一个；  
当RR接收到一条更新路由时，RR会检查Cluster List。如果Cluster List中已经有本地Cluster ID，丢弃该路由；如果没有本地Cluster ID，将其加入Cluster List，然后反射该更新路由；  

## BGP 联盟（Confederation）
解决AS内部的IBGP网络连接激增问题，除了使用路由反射器之外，还可以使用联盟（Confederation）。联盟将一个AS划分为若干个子AS。每个子AS内部建立IBGP全连接关系，子AS之间建立联盟EBGP连接关系，但联盟外部AS仍认为联盟是一个AS。配置联盟后，原AS号将作为每个路由器的联盟ID;  
这样有两个好处：  
1. 可以保留原有的IBGP属性，包括Local Preference属性、MED属性和NEXT_HOP属性等；
2. 联盟相关的属性在传出联盟时会自动被删除，即管理员无需在联盟的出口处配置过滤子AS号等信息的操作;  

## 路由聚合
在大规模的网络中，BGP路由表十分庞大，给设备造成了很大的负担，同时使发生路由振荡的几率也大大增加，影响网络的稳定性；  
路由聚合是将多条路由合并的机制，它通过只向对等体发送聚合后的路由而不发送所有的具体路由的方法，减小路由表的规模。并且被聚合的路由如果发生路由振荡，也不再对网络造成影响，从而提高了网络的稳定性；   
BGP在IPv4网络中支持自动聚合和手动聚合两种方式，而IPv6网络中仅支持手动聚合方式：  
1. 自动聚合：对BGP引入的路由进行聚合。配置自动聚合后，BGP将按照自然网段聚合路由（例如非自然网段A类地址10.1.1.1/24和10.2.1.1/24将聚合为自然网段A类地址10.0.0.0/8），并且BGP向对等体只发送聚合后的路由；  
2. 手动聚合：对BGP本地路由表中存在的路由进行聚合。手动聚合可以控制聚合路由的属性，以及决定是否发布具体路由。
为了避免路由聚合可能引起的路由环路，BGP设计了AS_Set属性。AS_Set属性是一种无序的AS_Path属性，标明聚合路由所经过的AS号。当聚合路由重新进入AS_Set属性中列出的任何一个AS时，BGP将会检测到自己的AS号在聚合路由的AS_Set属性中，于是会丢弃该聚合路由，从而避免了路由环路的形成；  

## 路由衰减
路由振荡指路由表中添加一条路由后，该路由又被撤销的过程。当发生路由振荡时，设备就会向邻居发布路由更新，收到更新报文的设备需要重新计算路由并修改路由表。所以频繁的路由振荡会消耗大量的带宽资源和CPU资源，严重时会影响到网络的正常工作；  
路由衰减使用惩罚值（Penalty value）来衡量一条路由的稳定性，惩罚值越高说明路由越不稳定。如上图所示，路由每发生一次振荡，BGP便会给此路由增加1000的惩罚值，其余时间惩罚值会慢慢下降。当惩罚值超过抑制阈值（suppress value）时，此路由被抑制，不加入到路由表中，也不再向其他BGP对等体发布更新报文。被抑制的路由每经过一段时间，惩罚值便会减少一半，这个时间称为半衰期（half-life）。当惩罚值降到再使用阈值（reuse value）时，此路由变为可用并被加入到路由表中，同时向其他BGP对等体发布更新报文。从路由被抑制到路由恢复可用的时间称为抑制时间（suppress time）；  
路由衰减只对EBGP路由起作用，对IBGP路由不起作用。这是因为IBGP路由可能含有本AS的路由，而IGP网络要求AS内部路由表尽可能一致。如果路由衰减对IBGP路由起作用，那么当不同设备的衰减参数不一致时，将会导致路由表不一致；  

## BGP-LS（link state）
收集网络拓扑的一种新方式，之前是由IGP协议（ospf、is-is）收集网络拓扑信息，存在几个问题：
1. 要求上层控制器也支持IGP协议，计算能力要求高；  
2. 涉及到跨区域路由，上层控制器就看不到具体拓扑，无法计算端到端最优路径；  
3. 如果不同区域采用不同IGP协议来上报拓扑信息，控制器的处理就很复杂；  
BGP-LS可以将IGP协议的网络拓扑信息汇总上报，解决上述缺点。通过BGP-LS路由来携带网络拓扑信息，一共有三种类型的BGP-LS路由：
1. 节点路由，记录拓扑的节点信息，包括：  
a. AS：BGP-LS的区域号；  
b. IGP route id：IGP协议生产的route id；  
2. 链路路由，记录节点间的链路信息，包括：  
a. remote：远端地址；  
b. if-address：接口地址；  
c. peer-address：对端接口地址；  
3. 前缀路由，记录节点可达的网段信息，包括：  
a. prefix：IGP路由地址前缀；  
b. ospf-route-type：ospf的路由类型，区域内、区域间、NSSA等；  
