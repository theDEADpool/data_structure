# TCP
## 数据接收流程
数据包 - 网卡 - DMA将数据映射到网卡的RX ring buffer，触发硬终端通知CPU - CPU调用对应的中断函数，最终调用网卡驱动 -
驱动关闭硬终端，保证数据包来了之后不需要再通知CPU，而是直接写入内存 - 驱动触发一个软中断，调用netrx_action来收包；  
(链路层)netif_rx -> netif_rcv_skb -> (网络层)ip_rcv -> ip_local_deliver -> (传输层)tcp_v4_rcv -> tcp_v4_do_rcv -> tcp_recv_established -> tcp_rcvmsg -> sock_recvmsg -> recv/read；  
## 数据发送流程
用户态 - 系统调用 - 数据copy到内核协议栈的缓冲区 - 每层协议栈封装头部之后 - 网卡驱动TX ring buffer中；  
send/write -> sock_sendmsg -> tcp_sendmsg -> tcp_push -> tcp_write_xmit -> tcp_transmit_skb -> (网络层)ip_queue_xmit -> (链路层)dev_queue_xmit；  

## 建联
### 状态机
服务端：listen->收到syn并发送syn ack-syn_recv->收到ack-establilshed状态；  
客户端：connect发送syn-syn_sent->收到syn ack发送ack-established状态；  

### 半连接队列
accept操作会把sk从queue中删除，数据结构上是一个链表icsk_accept_queue；  
inet_csk_reqsk_queue_add; 调用点tcp_get_cookie_sock, tcp_conn_request;  
inet_csk_complete_hashdance;
inet_csk_resk_queue_drop;
inet_csk_req_queue_is_full，调用点tcp_conn_request, tcp_get_syncookie_mss;  
旧版本半连接队列最大长度有单独变量控制，5.x版本的内核已经改为sk_max_ack_backlog，与全连接队列一致；  

### 全连接队列
并不是一个具体的链表或数组，而是一个计数值。几个相关函数：
sk_acceptq_added;调用点inet_csk_reqsk_queue_add；  
sk_acceptq_removed;调用点reqsk_queue_remove；  
sk_acceptq_is_full;调用点tcp_conn_request, tcp_v4_syn_recv_sock, tcp_get_syncookie_mss；  
记录队列长度是sk_ack_backlog，队列最大长度是sk_max_ack_backlog，该值是listen接口的backlog参数指定的，最大不能超过sysctl_somaxconn设置的值，默认是4096；  

### Defer Accept
三次握手完成之后并不会立刻通过accept告知上层应用，而是等到有数据来之后再告知上层应用；
TCP_DEFER_ACCEPT选项设置一个单位为s的时间，内核将其转换成一个重传次数syn_ack_recalc。这种socket在收到最后一个ack之后并不会进入establish状态，而是标记为acked。此时还是会重传syn ack报文，超过上面的次数之后，则socket标记为epxired；  

### Syn Cookie
Syn flood攻击：通过大量伪造syn报文，导致服务端创建大量半链接socket填满半连接队列之后无法处理正常的建联请求；  
syn cookie是服务端可以在不分配资源保存半连接socket的情况下，完成握手；  
利用syn报文seq随机性的特点来构造syn ack的seq：  
1. t是一个缓慢增长的时间戳，默认每64s增长一次；m为客户端发送syn报文的mss选项，s = hash(四元组，t)；  
2. syn ack的初始seq是，高5位t mod 32，接下来3位是m的编码值，低24位为s；  
3. 客户端收到syn ack之后回复ack其seq = syn ack的seq + 1，服务端就可以根据ack的seq还原出syn ack的seq并进行校验；
4. 高5位的t与当前时间对比看是否可以接受，根据四元组和t重新hash与低24位对比，校验通过则建连完成；  
缺点：mss只有3位，只能支持8种mss值；没法保存syn报文中需要协商的wscale和sack等选项；存在hash预算消耗；  

### Fast Open
对于已经正常完成过三次握手的客户端在后续建立连接的时候能够在syn包里携带数据。分为两个阶段：
1. Fast open cookie  
syn包头部包含fast open cookie选型且内容为空，表示客户端请求fast open cookie；  
服务端会生产一个cookie值通过syn ack通告给客户端，客户端缓存服务端ip和cookie；  
2. Fast open
客户端下次建连的时候发送syn数据包，包含真实数据和之前的cookie；  
服务端检验收到的TFO cookie和传输的数据是否合法，如果合法就返回syn ack包进行确认并将数据包传递给应用层。如果不合法就丢弃数据包，回复syn ack走正常三次握手流程；  
服务端的cookie每隔一段时间就会过期需要重新生产cookie；客户端在缓存cookie的时候也需要缓存服务端的mss，这样才好确定后续携带数据的syn报文大小；  
fast open也需要中间交换机or路由设备的支持，实用性比较差；

## 滑动窗口
TCP协议传输中有四类报文：
1、已发送且对端已确认的报文
2、已发送且对端没有确认的报文
3、未发送且对端允许发送的报文
4、未发送且对端不允许发送的报文
其中2、3就是存在于滑动窗口内的报文。
滑动窗口的大小就是对端socket接受缓冲区的大小。TCP三次握手过程中通信双方会通知对方窗口的大小。
通信过程中则会实时通知对方自己当前可用的窗口大小。
TCP报文头部用来记录窗口大小的字段长度为16位，最大能表示64K。同时TCP头部的option选项中可以配置放大倍数，这样就能够表示更大的窗口大小。放大倍数是2的幂，比如3，就是放大2^3 = 8倍。

### 零窗探测
当接收方通告的窗口大小是0，发送方就不能继续发数据，直到对端通告新的窗口。但如果通告窗口大小的包被丢弃了，那么发送方就会一直无法发送数据，零窗探测就是发送方主动探测接收方的窗口是否变化；  
1. 启动定时器
a. 收到ack时，packet_out == 0则会启动tcp_ack_probe，如果当前窗口已经无法容纳下一个要发送的数据包，会启动零窗探测定时器。如果足够容纳下一个包，则清除该定时器；
b. __tcp_push_pending_frames时，如果发送出去数据均已acked，但写队列仍有数据却无法发送，调用tcp_check_probe_timer设置零窗探测定时器；  
c. 如果要设置retrasn、early_retrans、tlp、rto定时器也会清除零窗探测定时器；  
2. 超时时间
a. 复用了RTO的超时时间，受退避机制影响，范围tcp_rto_min(200ms) ~ tcp_rto_max(120s)内；  
b. 如果应用设置了超时时间user_timeout。会根据探测次数和基准超时时间计算一个间隔elapse。如果时间间隔超过了该值则产生一个写错误etimedout，连接终止；  
3. 超时处理
a. sysctl_tcp_retries2作为最大探测次数max_probes，如果探测次数>= max_probes，生产一个写错误。探测次数是probe0和keepalive两者共用的；  
b. 调用tcp_send_probe0发送探测包：如果写队列有数据且对端接收窗口还有空间，那么能发多少数据就发多少数据。最小1字节，最大不超过1个mss；如果没有数据可发or没有剩余窗口，就看是否有紧急数据没有被ack，有则生成一个snd_una的ack，否则发送一个snd_una-1的ack；  
c. 如果网络上有数据在传输or写队列没有数据要发送，则清空退避次数、探测此时、时间等。
d. 退避次数能超过tcp_retires2的值，如果探测包发送成功，则根据rto时间和退避次数计算下一次发送时间。如果发送失败则按照resouce_probe_interval(500ms)作为定时器超时时间；  

## 丢包判定
### Delay Ack
延迟发送ack，规则是：
1. 如果有数据要发送，ack会随着数据一起发送；  
2. 如果没有数据发送，ack会有一个延迟。TCP会启动一个定时器每隔200ms就会检查一次是否要发送ack。如果200ms没有发送ack且有数据尚未被应答，那么定时器超时后就会发送ack；  
3. 如果在延迟ack期间，第二个数据又到了，那么就要立马发送ack；  

### SACK
sysctl_tcp_sack = 1，本端开启sack功能，需要双方协商；  
tcp_syn_options中判断如果sysctl_tcp_sacked == 1，则携带OPTION_SACK_ADVERTISE选项，在tcp_option_write时候写入TCPOPT_SACK_PERM选项；  
对端收到syn，解析到TCPOPT_SACK_PERM，且本端也开启了sack功能，则设置sack_ok，在synack的option中通过OPTION_SACK_ADVERTISE选项告知对端；  
基于空间序的丢包判定，如果ca状态 < Recovery， 则收到dupack数量 >= reordering会判定丢包；如果ca状态 == Recovery，每次确认新的数据都会把重传丢列头部的skb标记为lost；  

### RACK
基于时间序的丢包判定机制，当收到一个sack，说明存在乱序，就根据最近被确认数据包的发送时间来推测该数据包之前发送的数据包是否已经丢包，并启动快速重传；  
tcp_identify_packet_loss处理了两种情况：
1. 没有开启sack，则tcp_is_reno；  
2. 开启了rack，通过tcp_rack_mark_lost判定丢包。如果rack.advaned = 1表示需要重新进行rack判定，两种情况需要重新判定：一是收到了新的sack数据段，二是收到了ack且ack不是虚假重传；  
计算乱序窗口tcp_rack_reo_wnd，乱序窗口在收到dsack时候回扩大，在丢包恢复try_undo_recovery会缩小；  
判定是否丢包，计算一个时间差 = rtt*reo_wnd - (当前时间 - skb发送时间)。如果时间差 < 0 则判定为丢包。时间差>0但又没有被ack or sack的数据，则计算一个reo乱序超时时间。
乱序超时定时器的超时时间最大不能超过rto，超时之后会再次调用tcp_rack_detect_loss进行丢包判定。
tsorted_sent_queue是按时间顺序记录发送skb队列，而tcp_rtx_queue是以空间顺序记录发送skb队列。一个包如果发送多次，会改变其在tsorted_sent_queue中的位置，但不会改变其在tcp_rtx_queue中的位置。

### DSACK
开启sack的情况下如果配置sysctl_tcp_dsack = 1，启动duplicate sack校验；  
作用：
1. 让发送方知道是原始包丢了还是返回的ack丢了；  
2. 网伦乱序是否导致了无效重传；  
3. 数据包是否被中转设备复制并转发了；  
4. RTO太小导致出现错误重传；  
如果收到多个重复数据包，DSACK只会包含其中第一个。如何判断是否收到DSACK？
1. sack的start seq小于ack seq；  
2. 如果有多个sack option，sack[0]的seq范围在sack[1]之内，判定为dsack；  
判定dsack后会有什么影响？
会减少undo_retrans的值，该值是记录进入recovery或loss状态时，已重传但尚未被确认的包个数，最终控制了reo_wnd还原回正常值的时间，减少无效重传；  
同时tcp_undo_cwnd_reduction会取消因为重传导致的cwnd减少；  

## 重传
### Fast Retrans
发包不受cwnd和snd_wnd的限制；  

### Early Retrans
没有足够的dupack来触发FR，通过ER机制避免进入RTO。该功能在新版本已经被删除，仅剩下sysctl_tcp_early_retran来控制TLP开关；  
之前ER是有个单独的定时器，等待1/4个RTT之后触发重传；  

### TLP
解决哪些问题：
1. 尾部丢包，没有足够的dup ack、sack来触发FR；  
2. 整窗数据 or ack都丢了，也无法触发FR；  
3. Dup acks不够，无法触发FR；  
4. 超长rtt导致rto比ack先触发；  
tcp_schedule_loss_probe
tcp_send_loss_probe
定时器2 * srtt超时时间，如果当前packet_out == 1，则多加TCP_RTO_MIN来规避delayed ack；  
超时后如果有新数据则发送新数据，没有新数据则发送seq最大的数据包来触发对端回ack/sack；  

### RTO/FRTO
srtt = 7/8 * srtt + 1/8 * 本次rtt；  
超时时间 = 1/8 * srtt + rttvar;  

FRTO是判断RTO是不是真的，如果不是真的就不进LOSS状态；  
如何判定FRTO，同时满足以下几个条件：
1. frto功能开启；  
2. 当前不能是recovery状态 或 当前是LOSS状态且不是第一次进入，即已经RTO重传过至少一次；  
判定了FRTO之后，在tcp_process_loss里会做特别处理：
1. 如果ack/sack确认了并没有重传过的数据，说明超时之前发送的数据被对端收到，本次超时重传是虚假的，直接退出loss状态；  
2. 如果确认了部分数据，发送队列还有数据尚未发送，且窗口允许，发送新数据，即进入slow start；  
3. 如果收到的ack、sack并未确认新数据，说明超时重传是真实的；  

## 断连
### 状态转移
established-发送FIN->FIN_WAIT1->收到ack->FIN_WAIT2-收到FIN发送ack->TIME_WAIT->等待2MSL->CLOSED；  
established-收到FIN发送ack->CLOSE_WAIT->发送FIN->LAST_ACK->收到ack->CLOSED； 
timewait优化，可以增加一个配置项来控制timewait状态的等待时间。默认60s，最小1s，最大60s；   

### Close与Shutdown的区别
1. shoudown只能用于socket fd，close可以用于所有文件fd；  
2. 调用shutdown不会释放socket，即使SHUT_RDWR关闭双向读写，还是需要close才能释放socket；  
3. 如果缓冲区内还有数据未读，调用close则向对端发送RST；  
4. 多进程使用同一个socket，close每调用一次，则引用计数-1，当计数为0则socket释放。在引用计数为0之前，一个进程调用close不影响其他进程使用该socket。但一个进程调用shutdown回影响其他进程使用socket读写；  

### Shutdown
1. 对于LISTEN状态的socket设置RCV_SHUTDOWN，SYN_SENT状态设置任意SHUTDOWN都会触发tcp_disconnect断开连接；  
2. 对于TCP_CLOSE状态设置任意SHUTDOWN，返回-ENOTCONN错误；  
3. 对于其他状态，调用tcp_shutdown进行处理；  
4. tcp_shutdown只处理设置RCV_SHUTDOWN，对于处于ESTABLISHED、SYN_RECV、CLOSE_WAIT这三种状态的socket，通过tcp_send_fin主动发送fin；  
5. 此时如果写队列还有数据，则fin标记设置在写队列最后一个skb；如果写队列没有数据，则新建一个带fin标记的skb加入写队列，同时关闭nagle算法将该skb发送出去；  
6. 如果设置了SEND_SHUTDOWN，则在do_tcp_sendpages判断，返回错误。当收到FIN，就可以调用close关闭socket；  

### Close
1. 对LISTEN状态的socket，将其从监听队列中移除并关闭socket；  
2. 如果socket队列中还有数据没有交给应用，会清除并发送RST报文；  
3. 对于repair模式的socket，调用tcp_disconnect关闭socket；  
4. 对于ESTABLISHED、TCP_SYN_RECV、TCP_CLOSE_WAIT关闭socket会主动发送fin报文；  
5. 如果已经发送过fin，等待对端发送fin状态TCP_FIN_WAIT2，如果TCP_LINGER选项小于0，直接发送RST；如果TCP_LINGER2 >= 0，就看fin超时时间是否大于TIME_WAIT超时时间，大于就启动FIN_WAIT2定时器否则就进入TIME_WAIT状态；  
6. 如果orphen socket数量超过系统上仙，主动发送RST并设置socket为TCP_CLOSE，这种情况socket不会成为orphan socket；  
7. SO_LIGNER对close的影响如下；  

### SO_LINGER
作用是控制close关闭socket时的具体行为，传入一个开关和关闭时间，有三种情况：  
1. 开关关闭，不生效。close走默认逻辑，如果发送队列没有数据则立即关闭，如果有数据，会等到所有数据被发送完成之后关闭。这里只会发数据，不会等待数据的ack。这里关闭是4次挥手过程；  
2. 开关开，时间为0。不管是否有数据，都直接关闭，此时发RST；  
3. 开关开，时间不为0。如果有数据，等待数据发送并被完整ack之后关闭。如果数据发送完成之前超时，则返回EWOULDBLOCK错误，数据会丢失；

### TCP_LINGER2
设置orphan socket在FIN_WAIT2状态的生存时间，单位s。优先级高于sysctl_tcp_fin_timeout配置；  
该配置可以是负数，即socket不会进入FIN_WAIT2状态直接进入tcp_done，并发送RST;  0则配置不生效；  

## 其他
### CA状态机
1. TCP_CA_OPEN：正常状态；  
2. TCP_CA_Disorder：乱序状态，重传队列首包已经重传过，且还存在尚未恢复的乱序包；  
3. TCP_CA_CWR：拥塞窗口减少，tcp_enter_cwr，进入条件：收到了携带ecn-ece标记的ack；  
4. TCP_CA_Recovery：快速回复，正在重传丢失的pkt；  
5. TCP_CA_Loss：超时重传，触发RTO进入，synack超时重传也会进入；  

### Keepalive
收到syn ack后，或者收到三次握手最后一个ack，会判断是否设置了SOCK_KEEPOPEN这个标记，来启动keepalive定时器；  
定时器超时时间看用户有没有通过setsockopt的TCP_KEEPIDLE来设置时间间隔，默认是sysctl的tcp_keepalive_time，120分钟；  
定时器超时之后：
1. 如果当前socket正在被用户使用，则等50ms再来执行keepalive操作；  
2. 如果socket处于listen状态，就什么都不做（5.x版本），旧版本会调用tcp_synack_timer;  
3. 在fin_wait2状态下且没有设置linger选项，则直接发送RST，关闭socket；  
4. 如果packet_out >0，有数据还在传输的话就重置定时器；  
5. 上述几个条件都不满足，就计算出一个时间差elapsed = min(当前时间 - 最近一次收到数据的时间， 当前时间 - 最近一次收到ack的时间)；  
6. 如果elapsed > keepalive定时器，有两种可能：
a. 应用程序通过TCP_USR_TIMEOUT设置了一个时间，elapsed超过了这个时间，同时也发送过keepalive探测包；或应用未设置时间，但keepalive探测次数超过了限制（默认是9），即没有收到ack的情况下发送了多少个包，如果收到了ack计数会被清0。这两种则发送rst，产生一个错误ETIMEOUT；
b. 正常是发送keepalive探测包，如果探测包发送不成功则重置定时器，把超时时间改为500ms；  
探测包优先发送write_queue头部的包，大小不能超过对端接收窗口的剩余空间，包会被打上PSH标记，且不受cwnd控制。如果write queue没有报，那就构造一个长度为0的包作为探测包；  
相关配置
SO_KEEPALIVE：如果设置了SO_KEEPOPEN，那么建连成功会自动启动keepalive定时器。如果没有设置SO_OPEN，且传入的值为1，启动keepalive定时器。传入值为0就停止keepalive定时器。
TCP_KEEPIDLE：定时器超时时间；
TCP_KEEPINTVL：探测包发送间隔，不会退避；  
TCP_KEEPCNT，设置keepalive探测包的发送次数；
sysctl_tcp_keepalive_intvl默认75s；   
sysctl_tcp_keepalive_probes，默认9次；  
sysctl_tcp_keepalive_time，默认7200s；    

### Orphan Socket
orphan sockets是没有与任何文件描述符关联的socket，应用程序已经不能与此socket进行交互了，但是由于内核中TCP未完成，仍然占用TCP的内存暂时不能释放；  
TCP_FIN_WAIT1、TCP_LAST_ACK、TCP_CLOSING状态都可归类计数于orphan socket，但当通过TCP_LINGER2或sysctl_tcp_fin_timeout设置的超时时间大于60秒时的TCP_FIN_WAIT2的连接也归类计数于orphan socket；小于60秒的TCP_FIN_WAIT2状态的连接则归类计数于TIME_WAIT，从代码可以看出TCP_TIME_WAIT状态是不计入orphan socket；TCP_CLOSE_WAIT 状态的连接既不计入orphan socket 也不计入TIME_WAIT；  
相关配置  
tcp_orphan_retries，默认是3，控制orphan socket的重传次数，超过这个数量会丢弃socket；  
tcp_max_orphans，控制orphan socket的最大个数，超过这个数量会告警；  

### 快路径&慢路径
快路径：没有乱序包 && 对端接收窗口不为0 && 本端有空余接收缓存 && 没有紧急数据；  
避免一些信息的处理，比如校验和。针对预期内的信息处理，比如回复ack，记录时间戳等；  
慢路径：非预期的数据，比如乱序、紧急数据、0窗探测；  

### Silly window
由于数据收发两端的速度不对等，导致网络中存在大量小包的现象；  
解决办法：  
1. Nagle算法，尽量避免发送小包数据，与delay ack不能共存；  
a. 可用窗口 > MSS && 待发数据 > MSS，立即发送数据；  
b. 不满足上述条件，如果有数据尚未被ack，则缓存当前数据等待ack；如果没有数据尚未被ack，则立刻发送数据；  
2. Delay Ack，减少ack频率来降低小窗口通告的可能性，避免发送小包数据；  

### NAGLE算法
TCP连接建立的时候双方会确认该连接的MSS（TCP数据包每次能够传输最大的数据分段）。
该算法的基本是任意时刻，最多只能有一个未被确认的“小段”（小于MSS的数据块）。
1、如果数据包长度达到MSS，则允许发送。
2、如果数据包包含FIN，则允许发送。
3、设置了TCP_NODELAY（禁用NAGLE算法），则允许发送。
4、未设置TCP_CORK选项，所有发出去的小段均被确认，则允许发送。
5、上述条件都未满足，但发生了ACK超时，则立即发送。

### TCP_CORK
该选项的作用是让内核将小数据包拼接成一个大数据包再发送出去。或者在一定时间内，仍未拼成一个大数据包，也发送。
这个选项对于那种发送小包，且发送间隔比较长的应用程序无效，反而会降低发送的实时性。