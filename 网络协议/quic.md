# QUIC
以quic-go为代码版本
## 建连
### iquic握手
1. TLS握手需要的信息是封装在quic initial packet的crypto frame中发给对端的；  
2. 最初的encLevel都是initial，通过qtls的回调，会改变为handshake，此时发给对端的packet就变为handshake packet。握手完成之后encLevel会变成applicationData，此时发送的是1RTT packet；  
3. Retry packet，服务端发客户端受，必须是第一个包，否则被丢弃。retry packet会携带服务端生产的token。客户端后续必须使用该token；  
4. 握手过程传递的参数：quic version、connection id长度、握手超时时间、空闲超时时间、初始连接接收窗口、最大连接接收窗口、initial scid、retry scid、origin dcid等；  
5. connection id变化过程：
a. 客户端连接建立之初随机生产一个4字节的scid（如果已经创建了一个udp conn，这里的scid为空），也随机生成一个8~20字节的dcid。客户端发出的第一个initial packet其scid是空；  
b. 服务端收到initial pkt，得到token，但一般情况下token不对，accept token失败之后服务端会返回retry包。retry包的scid是服务端随机生产的，dcid由于客户端发来的initial pkt的scid为空，所以也为空；  
c. 客户端收到retry pkt之后会再次发送initial pkt，此时scid仍是空，dcid为retry包的scid；  
d. 服务端再次收到initial pkt，携带了有效的token，此时服务端会创建new session。new session有5个关于cid的参数：
1. origin des cid是第一个initial pkt客户端随机生产的dcid；  
2. retrySrcConnID是服务端发送的retry pkt生产的cid；  
3. hdr.DestConnectionID是本次收到的包中dst cid，这个和retrySrcConnID是一样的；  
4. hdr.SrcConnectionId是本次收到的包中src cid，这个为空，到目前位置客户端都没有生产cid；  
5. connID是本次随机生成的cid，这个cid就是客户端后续发包会用到的dcid了；  
f. 客户端发出的不论是long header还是short header pkt，scid都是空；  

### 0RTT
省略掉双方进行身份验证的1个RTT 在交换握手密钥的同时携带应用数据；  
客户端行为：  
1. 发送clientHello exetnsions必须携带early data和pre_shared_key两种扩展；  
2. early data表示握手阶段要发送的应用数据，pre_shared_key用于0RTT密钥材料提供给服务端，进行客户端身份验证；  
服务端行为：
1. 如果接受客户端的0RTT数据，会在EncryptedExtension中同样携带early data的扩展；  
需要注意的点：
1. 统一设置服务端的SessionTicketKey提高0RTT的成功率；  
在1RTT握手完成后，如果开启SessionTicket机制，服务端会在ApplicationEncryptionLevel即1RTT包号空间发送包含NewSessionTicket的crypto帧。其中保存了SessionTicketKey加密过的SessionTicket，并计算PSK保存到Ticket中。客户端收到SessionTicket后会同样计算PSK，双方的PSK都是基于前期交换的握手密钥，因此可以保证双方的PSK一定相等。客户端把PSK和SessionTicket绑定保存到ClientSessionCache中；  
客户端0RTT握手时会去除ClientSessionCache，并根据ClientHello和SessionTicket生产pre_shared_key扩展需要的pskIdentity和binderEntry，SessionTicket会原封不动的发送给服务端。随后客户端根据PSK派生出EarlyEncryptionLevel的密钥，即加密0RTT数据的密钥。服务端通过SessionTicketKey解密SessionTicket取出PSK，采用相同的方式派生出解密0RTT数据密钥，这样就完成了0RTT数据加解密Key的交换，0RTT正常工作；  
如果不设置统一的SessionTicketKey，那么TLS库会自动生成随机数作为密钥。假设客户端0RTT握手包被路由到另外的服务器，由于加密解密SessionTicket不一致，因此0RTT会失败，服务端重启也会导致相同的结果；  
2. 走0RTT的请求必须是幂等，因为0RTT很容易被重放攻击；  

## ACK机制
发送Ack的时机：每次处理完收到的包就会进入sendPacket的逻辑，会将所有需要的ack包通过ack frame发送出去，没有为ack单独设置定时器；  
Ack range的增加与删除：
1. 收到包就增加ack range，最多32个，超过了就会删除旧range；  
2. 发送ack不会删除ack range；  
3. IgnoreBelow机制会删除ack range；  

## PTO
1. 三种PTO，Initial、Handshake、AppData对应三种加密类型。握手没有完成只能发送Initial和Handshake包；  
2. PTO超时时间 = srtt + 4 * rtt_dev(最小1ms) + maxAckDelay；  
3. 超时时间 = 当前加密级别内上一个AckEliciting pkt发送时间 + PTO超时时间（会退避）；  
4. 握手没完成且没有out pkt（未被ack的包），超时时间 = 当前时间 + PTO超时时（会退避）；  
5. 重传包数量：Inflight == 0 且 没有完成地址校验，则每次重传1个包；其他情况每次重传2个包；  
6. 重传包内容：如果有out pkt，每次取out pkt第一个加入到retrans Queue中，并标记为lost进行重传；如果没有out pkt，构造一个ping frame加入到retrans Queue中进行重传；  

## Keepalive
1. interval = min(dileTimeout / 2, max_keepalive_interval(20s))，IdleTimeout默认30s，可配；  
2. 定时器超时时间 = last pkt接收时间 + interval；  
3. 定时器超时触发后，会向control frame queue中加入一个ping frame。下次发包的时候会携带这个ping frame。在收到对端回包之间，定时器不会再次触发；  
4. 另外连续发送19个以上的unack elicting pkt就会在其中加一个ping frame；  
5. rfc规定收到ping frame要ack，如果没有ack，那么发送端就进入Idle timeout的逻辑；  

## 流量控制
1. session级rcv wnd初始值是1.5 * 512KB，最大15MB；stream级rcv wnd初始值512KB，最大6MB；  
2. rcv wnd = bytesRead + 更新之后的receiveWindowSize，实际就是更新之后，对端能够发送的最大offset。recieveWindow表示本端接收窗口的大小；  
3. 接收窗口扩大条件：周期内接收的字节数 > 接收窗口 / 2；距离上次窗口扩大时间间隔 < 4 * (周期内接收字节数 / 接收窗口大小) * rtt；接收窗口每次扩大2倍，但不会超过最大值；  
4. 发送MaxDataFrame条件：剩余窗口大小 < 接收窗口大小 * 0.75， 满足这个条件，会生成一个MaxDataFrame加入到controlFrames的队列里；  
5. 发送MaxStreamDataFrame条件同上；  
6. 发送DataBlockedFrame，满足连接发送窗口 - bytesSent == 0， 且发送窗口 != 上次block记录的发送窗口，则判定为blocked，每次发包的时候都会做这个判断；  
7. 发送StreamDataBlockedFrame条件同上；  

## 各类帧
### new_connecetion_id
1. 在applyTransportParameters时候会根据MaxIssuedConnectionIDs（默认6个）来生产new connection id；  
2. 在issueNewConnID与此同时也会生成对应数量的new_connection_id frame。每个connection id会有一个seq num相当于其编号；  

### 地址校验
1. 服务端在完成地址校验之前发送的数据量不能超过接收数据量的三倍；  
2. 服务端在加密等级变为EncryptionHandshake时会设置peerAddressValidated = true，此时上面限制会失效；客户端是在加密等级变为EncryptionHandshake或Encryption1RTT时候设置peerAddressValidated = true；  
3. 为了防止地址校验完成之前发包限制导致死锁，客户端在地址校验完成之前PTO会尝试发送initial pkt或handshake pkt；  

## 多路径
设计原则：
1. 最大限度的复用quic的机制和header format；  
2. cc和rtt计算都是per path；  
3. 要求双端都是非0 CID；  
4. 每个path是由4元组定义，因此每个4元组至多对应一个path和CID；  
5. 如果四元组发生变化但CID没变，则认为是连接迁移；  
每个path使用独立的包号空间，需要改变AEAD加密方式；  

### 握手与参数协商
新增enable_multipath，是否使用多路径；  
如果enable_multipath = 1，但收到0CID的pkt，则触发TRANSPORT_PARAMETER_ERROR关闭连接；  
rfc指出启用MP之后，如果服务端配置了perferred address，客户端也不应该假设这些地址可以被用于多路径；  

### 路径管理
1. 创建路径
不同的path使用不同CID，如果active_connection_id_limite为N，则服务端提供N个CID，客户端使用N个CID，如果达到上限还想创建新path，则需要退出一条旧path；  
双端通过PATH_CHALLENGE和PATH_RESPONSE frame来完成新path的地址验证，验证成功后客户端在该path上发送1RTT pkt，服务端收到1RTT pkt后不进行路径迁移而是标记该路径为可用路径；  
xquic实现
a.xqc_conn_create_path需要传入一个scid，创建的path归属于scid对应的连接；  
b.xqc_conn_check_unused_cids判定双端可用的cid数量，任意一方为0都无法创建path；  
c.xqc_path_init，客户端会生产path_challegen_frame并发送，path_challenge的数据部分是8字节随机数；  
服务端收到path_response frame之后会对比其携带的data和之前写入path_challenge的data是否一致，如一致则创建的path状态变为可用；  
新创建path的cid是怎么来的？  
xqc_get_unused_cid，pkt中scid和dcid分别从conn的scid_set和dcid_set只能够获取的，如果获取不到，则无法创建新path；  

unused_cid列表又是如何生产的？
客户端创建连接时，会生产scid和dcid，这两个cid分别加入scid_set和dcid_set。双端成功处理initial pkt之后，pkt的scid也会加入本端的dcid_set中；  
双端握手完成之后，如果scid_set中可用的cid数量为0，且已使用的cid数量 < active_connection_id_limit，则本端生产新的scid加入到scid_set中xqc_write_new_conn_id_frame_to_packet。同时会通过new_connection_id frame将新的cid通告给对端；  
对端收到new_connection_id_frame之后也会将其携带的cid加入本端的dcid_set中；  

path和cid如何对应？  
收到一个pkt，是根据pkt的dcid，也就是本端scid来找到对应的path；  

2. 路径状态  
双端通过path_status frame来通告路径状态，有两种状态：
可用available，对端可以在该path上发送数据；  
备用standby，如果有其他path，不建议在该path上发送数据；  
状态机：
发送或接收path_challenge -> validating -> 收到path_response -> active -> 发送或接收path_abandon -> closing -> 超过3个pto -> closed；  

3. 关闭路径  
有以下几种关闭方式：  
a. 双端通过path_abandon_frame来关闭一个path，该frame的发送和接收方都不应该立即释放资源，而是等待至少3个PTO。该frame需要再最后一个数据包之后且path所对应的CID retire_connection_id frame之前发送。path_abandon frame可以再任意一个可用的path上发送，而并非只能在需要关闭的path上发送。如果conenction仅有一个path上收到的path_abandon frame，则本端应该发送一个conenction_close frame关闭当前连接；   
b. 拒绝新path的创建，不发送path_response来响应path_challenge;  
c. 空闲超时，某个path在规定时间内没有发送非探测数据，则取消该path的可用资格；  