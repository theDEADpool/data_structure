MPLS分两大协议LDP和RSVP-TE
LDP标签分发协议，本身没有路径计算能力，是基于IGP协议计算路径的结果，存在两个问题：
1. 设备之间需要大量的交互来维持状态；  
2. 需要同步IGP的状态否则会产生流量黑洞；  
网络中所有的设备都需要建立IGP和LDP的邻接关系，LDP依赖IGP路由信息在网络中建立MPLS的标签交换路径LSP；  
路径没有完全建立，会转发失败；路径中某个设备故障了，但LDP没有及时收敛，转发也依然会失败；  

RSVP-TE的问题
1. 配置复杂，网络中需要通过大量的交互去得知网络中其他设备的状态；  
2. 本质上是一个分布式架构，及时网络设备只知道自己的状态，想要知道网络拓扑就需要发送大量的RSVP报文来维持邻居关系，传递信息；  

SR协议
1. 基于现有协议进行扩展，集中控制和分布式转发的平衡；
2. 业务定义转发规则，数据按需转发；将网络中的段和节点，分配sid，sid有序排列就形成了一条路径；  
sid有三种：
1. node sid，为节点lo地址分配的，用来标识1个节点的，指示按照设备进行转发；  
2. adjacency sid，邻接sid，为邻接关系分配的sid，指示设备按照出接口进行转发；  
3. prefix sid，为一个网络前缀分配sid；  

如何通告sid？   
使用IGP协议通告进行拓扑信息，前缀信息，locator、sid信息。需要对报文的TLV字段进行扩展；比如ISIS；  
对于多个AS之间的通告，BGP协议有针对SRv6的扩展；  

sid可以通过IGP网络进行泛洪，网络中节点可以知道所有的sid，通过sid指导设备进行转发称为SR-BE，也可以通过多个sid的组合知道转发， 做一些流量控制，SR-TE；  
SR-BE，比如希望从a转发到d，a就给报文加上d的sid，但报文怎么去往d没有强制路径，而是根据igp计算的最短路由转发，代替LDP+IGP的方案；  
SR-TE，比如从a到d，要求经过1,2,3节点，那么就在报文前加上一个sid的list，指定1,2,3,d，报文就会按照指定好的路线进行转发；  
SR-TE里的sid可以是node sid，邻接sid，prefix sid，也可以三者混用；  
SR-TE的路径，可以是手工配置，可以是头结点根据cspf算法计算，也可以是控制器计算路径；  

SR-BE支持三种引流策略：
1. 隧道策略，根据优先级选择隧道；  
2. 配置静态路由进入隧道；  
3. 公网BGP路由通过ip进入隧道；  

SR-TE支持四种引流策略：
1. 隧道策略；  
2. 静态路由；  
3. 自动路由，把隧道当做普通路由参与IGP计算；  
4. 策略路由，将策略路由apply语句的出接口指定为隧道；  

如果转发要跨越两个SR-TE区域，SR-TE1和SR-TE2之间没有node id，需要用到BGP SR技术在两个区域之间分配sid，可以是邻接sid，也可以是邻居sid，也可以是为一组邻居分配sid；但如果转发路径很长，sid的列表也很长，浪费空间大；就可以通过bind sid解决，binding sid可以直接分配个SR-TE2这个区域，这样标签就缩短为SR-TE1内部node标签 + BGP ISD + binding SID;  

运营商骨干网路由器P
运营商边缘路由器PE
用户边缘路由器CE

# SRv6   
区别于MPLS-RS是在ip报文之外再加上标签，SRV6是复用了ipv6的扩展头部，在里面加上了SR header信息，这样除了TOR路由器，其他设备只要支持IPv6都可以进行转发；  
SR header
两个关键信息segment list + segment left， segment left是一个id指向segment list中当前活跃的成员；  
整个数据包的dst ipv6 = segment list[segment left];   
segment list是128位的ipv6地址，第一个可编程特性就体现在segment list的顺序上，可以自定segment list的顺序，按照不同的路径进行传输；  
每个segment list成员包括locator、function和argument 三个部分，locator携带位置信息，function指定报文转发的指令，argument可选；  
第二个可编程特性体现在function的内容上，function根据不同的sid类型，可以指定不同的动作：  
1. End，代表一个node，对应的动作是处理SHR更新ipv6地址，然后查找路由表进行转发；  
2. End.X 代表网络中的一个邻接，对应指定是处理SRH更新ipv6地址，从SID制定的出接口转发；  
3. End.DX2：在用L2VPN或VPWS，对应解封装报文，取出SRH和IPv6头部，按照SID指定的出接口转发报文；  
4. End.DT4：PE类型的SID，主要用在IPv4 L3VPN场景，对应指令是解封装报文，取出SRH和IPv6头部，根据宝文理的目的地址，查找ipv4 VPN实例进行转发；  
5. End.DT6：主要用在IPv6 L3VPN场景，作用和上面类似；  

## 转发流程
host1 - A - B - C（无srv6能力） - D - E - host2
1. 源节点A将SRv6路径信息封装在SRH中，指定B-C，C-D链路的SID，另外封装E点发布的SID A5::10（此SID对应于节点E的一个IPv4 VPN），共3个SID，按照逆序形式压入SID序列。此时SL（Segment Left）=2，将Segment List[2]值复制到目的地址DA字段，按照最长匹配原则查找IPv6路由表，将其转发到节点B;  
2. 报文到达节点B，B节点查找本地SID表（存储本节点生成的SRv6 SID信息）,命中自身的SID（End.X SID），执行SID对应的指令动作。SL值减1，并将Segment List[1]值复制到DA字段，同时将报文从SID绑定的链路（B-C）发送出去;  
3. 报文到达节点C，C无SRv6能力，无法识别SRH，按照正常IPv6报文处理流程，按照最长匹配原则查找IPv6路由表，将其转发到当前目的地址所代表的节点D;  
4. 节点D收报文后根据目的地址A4::45查找本地SID表，命中自身的SID（End.X SID）。同节点B，SL值减1，将A5::10作为DA，并将报文发送出去;  
5. 节点E收到报文后根据A5::10查找本地SID表，命中自身SID（End.DT4 SID），执行对应的指令动作，解封装报文，去除IPv6报文头，并将内层IPv4报文在SID绑定的VPN实例的IPv4路由表中进程查表转发，最终将报文发送给主机2;   

## 工作模式
### SRv6 BE
基于IGP最短路径算法计算得到最优SRv6路径，仅使用一个业务SID来指引报文在链路中的转发。即不能控制报文在网络中具体的转发路径，没有流量工程能力；  

### SRv6 TE policy
#### 层级结构
1. 第一级三个要素：
a. 头端headend：在某个节点配置SRv6 TE policy，该节点就属于headend；  
b. 尾端endpoint：相当于SRv6 TE policy的目的地址；  
c. color：标识SRv6 TE policy的特点，比如低时延，比如大带宽；路由也可以配置color属性，在使用中，路由的color需要与SRv6 TE policy相一致；  
2. 第二级candidate path，一个SRv6 TE policy可以拥有多个candidate path，只有优先级最高的才能成为主路径；
3. 第三级segment list，每个candidate path包含segment list形成真正的转发路径。segment list可以通过携带weight（权重）来实现负载分担；  
每个policy可以配置一个binding SID，提供给其他policy使用，形成一个组合路径，这种在跨域场景很常用到；  

利用Segment Routing的源路由机制，通过在头节点封装一个有序的指令列表（路径信息）来指导报文穿越网络；  
控制器通过PE路由器上报的网络拓扑信息，计算路径信息并下发给网络头结点；  
头节点根据经信息生产SRv6 policy，指定路径每一跳的SID和对应的action并以此转发报文；  
路径上每个节点匹配SID后执行对应的action，最终完成报文转发；  

#### 路径计算
支持三种路径计算方法：
1. 根据亲和属性动态计算路径  
a. 根据亲和属性规则过滤出可以使用的链路；  
a1. 包括亲和规则、亲和属性和链路属性，采用位运算方式；  
a2. 亲和规则为include-any，即链路属性包含亲和属性中的任意一种即认为该链路可用；  
a3. 亲和规则为include-all，即链路属性需要包含所有亲和属性才认为可用；  
a4. 亲和规则为exclude-any，即链路属性包含亲和属性中任意一个则认为该路径不可用；  
b. 根据度量来计算路径，支持的度量方式有以下几种：  
b1. 以跳数作为度量值，选择跳数最少的链路；  
b2. 以IGP链路开销值作为度量值，选择IGP链路开销值最低的链路；  
b3. 以接口平均时延作为度量值，选择接口平均时延最低的链路；  
b4. 以MPLS TE度量值作为度量值，选择TE度量值最低的链路；   

2. 根据Flex-Algo算法动态计算路径   

3. 使用PCE动态计算路径   
SRv6节点可以作为路径计算的客户端PCC，通过路径计算单元PCE来进行路径计算；  
PCE分为无状态PCE和有状态PCE，无状态PCE仅提供路径计算服务，有状态PCE会掌握所有PCC维护的路径信息来计算和优化路径；  

#### 控制器下发SRv6 TE policy
1. 控制器通过BGP-LS收集网络拓扑和SID信息；  
2. 控制器与源节点之间建立BGP IPv6 SR Policy地址族的BGP会话；  
3. 控制器计算SRv6 TE Policy候选路径后，通过BGP会话将SRv6 TE Policy的Color属性、Endpoint地址、BSID、候选路径和SID列表下发给源节点。源节点设备生成SRv6 TE Policy；  

为了支持SRv6 TE Policy，MP-BGP定义了新的子地址族BGP SRv6 TE Policy地址族，并新增了SRv6 TE Policy NLRI（Network Layer Reachability Information，网络层可达性信息），即SRv6 TE Policy路由（也称为BGP SRv6 TE Policy路由）。SRv6 TE Policy路由中包含SRv6 TE Policy的相关配置，包括BSID、Color、Endpoint、候选路径优先级、SID列表和SID列表的权重等；  
SRv6 TE Policy NLRI包含的字段如下：  
1. NLRI length：1字节，NLRI的长度；  
2. Distinguisher：4字节，唯一标识一个SRv6 TE Policy；  
3. policy color：4字节，color属性标识；  
4. EndPoint：16字节，目的节点地址；  

携带SRv6 TE Policy NLRI的BGP Update消息中，需要同时携带Tunnel Encapsulaton Attribute，包含字段如下：  
1. Preference Sub-TLV：通告候选路径的优先级；  
2. SRv6 Binding SID Sub-TLV：通告候选路径的BSID；   
3. Segment List Sub-TLV：通告Segment List；  
4. Weight Sub-TLV：通告Segment List的权重；  
5. Policy Candidate Path Name Sub-TLV：通告候选路径的名称；  

#### 引流策略
1. color引流  
2. 静态路由引流  
3. 策略路由引流  
4. QoS引流  

#### 保护策略
1. TI-LFA方案  
比如路径是A-B-C-D-E，当B和C之间的链路故障，此时B可以选择一条其他的路径比如B-H-C，绕过故障链路进行转发；  
如果是C节点故障，那TI-LFA就会失效无法绕行；  
TI-LFA FRR方案
基本计算过程：  
a. 计算扩展P空间，以保护链路源端所有邻居为根节点建立SPF树，是所有从根节点不经过保护链路可达的节点集合；  
b. 计算Q空间，以保护链路目的端为根节点建立SPF树，是所有从根节点不经过保护链路可达的节点集合；  
c. 寻找PQ空间的交集节点，得到最短路径；    
d. 计算备份出接口，如果PQ空间没有交际，也没有直连邻居，备份出接口为收敛之后下一跳出接口；  
e. 计算repair list，由P节点标签 + PQ路径上的邻接标签组成；  
2. 中间节点保护  
某个EndPoint节点故障，TI-LFA无法生效的情况下，可以由前序节点跳过该故障点，要执行的转发行为包括SL减1，将故障点下个节点的SID拷贝到外层IPv6头；  
3. Hot-standby保护  
通过备份的candidate path保护主candidate path，需要配合SBFD和单臂BFD的监测；  
4. VPN-FRR保护  
通过一个policy保护另一个policy；  
5. SRv6-BE逃生保护  
即从SRv6-TE切换到SRv6 BE模式，使用IGP计算的最短路由进行转发，但无法保证业务的SLA需求；  
4. 防微环  
微环的产生，是当网络中出现故障，各节点路径收敛顺序不一致出现的一种暂时性环路；  
a. 正切微防环：感知到故障的节点维持TI-LFA计算的备份路径转发一段时间，等待网络上其他节点完成新的路径收敛之后，其自身再更新路径；  
b. 回切微防环：故障路径恢复后，还是会因为收敛时间不一致导致出现的微环，而回切不会进入TI-LFA的过程，因此也无法通过延迟来解决；  
比如b c d e f，如果cd之间故障，b c之间的路径不受影响且无环，d e f之间的路径也不受影响且无环，因此故障恢复之后，只需要在指定cd之间的邻接sid，就可以保证整条链路无环；  
c. 远端正切微防环：

policy两种下发策略
1. 控制器下发  
需要控制器与转发器之间建立BGP-LS邻居，控制器收集带宽、时延等网络信息，完成路径计算后下发给头节点，头节点通过BGP update报文生成SRv6 TE policy；  
2. 静态配置  

#### 参考资料
https://www.h3c.com/cn/Service/Document_Software/Document_Center/Home/Routers/00-Public/Learn_Technologies/White_Paper/SRv6_TE_Policy-6930/#_Toc118215814  

### 路由协议对SRv6的扩展
#### BGP for SRv6
SRv6场景BGP Update消息一个比较明显的变化是增加了与SRv6相关的BGP Prefix-SID属性。扩展了两个BGP Prefix-SID属性TLV，用于携带业务相关的SRv6 SID：  
1. SRv6 L3 Service TLV：用于携带三层业务的SRv6 SID信息，支持携带End.DX4，End.DT4，End.DX6，End.DT6等SID；  
2. SRv6 L2 Service TLV：用于携带二层业务的SRv6 SID信息，支持携带End.DX2，End.DX2V，End.DT2U，End.DT2M等SID的信息；  
BGP Prefix-SID属性是专门为SR定义的BGP路径属性，这个属性是可选和可传递的，类型号为40，其Value字段为实现各种业务功能的一个或多个TLV。每个TLV都是type、length、value的组合；  
#### ISIS for SRv6
#### OSPFv3 for SRv6
OSPFv3 SRv6扩展也有两个功能：发布Locator信息和SID信息。Locator信息用于帮助其他节点定位到发布SID的节点；SID信息用于完整描述SID的功能，比如SID绑定的Function信息；  
为发布Locator的路由信息，OSPFv3需要发布两个LSA（Link State Advertisement，链路状态通告）:SRv6 Locator LSA和Prefix LSA；  
SRv6 Locator LSA包含SRv6 Locator TLV,TLV中包括前缀和掩码，以及OSPFv3路由类型。网络中的其他节点可以通过该LSA学习到Locator的路由。SRv6 Locator TLV除了携带用于指导路由的信息外，还会携带不需要关联OSPFv3邻居节点的SRv6 SID，例如End SID；  
通过Prefix LSA可以发布Locator前缀。这些Prefix LSA是OSPFv3协议已有的LSA，普通IPv6节点（不支持SRv6的节点）也能够通过学习这些LSA，生成Locator路由（指导报文转发到发布Locator的节点的路由），进而支持与SRv6节点共同组网；  

#### 参考资料
https://support.huawei.com/enterprise/zh/doc/EDOC1100174722/2d89ee77

尾节点保护

微防环



