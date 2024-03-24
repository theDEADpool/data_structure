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
Path Attributes字段描述路径属性

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

路由反射器
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
