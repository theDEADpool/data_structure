# socket
## api
1. int socket(int family, int type, int protocol);  
family：协议族，AF_INET(ipv4)、AF_INET6(ipv6)、AF_LOCAL  
type：SOCK_STREAM(TCP)、SOCK_DGRAM(UDP)、SOCK_RAW(ping)  
protocol：IPPROTO_TCP、IPPROTO_UDP等  
函数成功时，返回非负的fd，错误返回-1。  
创建socket的时候type字段设置SOCK_NONBLOCK，就可以创建非阻塞的socket。  
除了创建和下面accept阶段设置非阻塞，还可以通过fcntl和ioctl来设置socket为非阻塞模式。  

2. int bind(int fd, const struct sockaddr* address, socklen_t address_len);  
address：包含要绑定的地址和端口号。  
address_len：address结构体的大小。  
服务器需要调用bind，客户端可以调用bind也可以不调用。  
不用bind的时候系统会自动分配一个未占用的端口。  
函数成功返回0，错误返回-1。  

3、int listen(int fd, int backlog);
backlog：由于TCP连接是需要三次握手的，那么在服务器上有可能存在还在握手过程中的连接，或者是刚刚握手完成但应用程序还没有来得及处理的连接。
这两种类型的连接内核会分别用两个队列来进行保存。backlog参数则是指定了这两个队列的长度之和。
函数成功返回0，错误返回-1。

4、int accept(int fd, struct socketaddr* cli_addr, socklen_t* len);
cli_addr：客户端的地址。
服务器调用，函数成功时，返回非负的fd，错误返回-1。
int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
flags设置为SOCK_NONBLOCK可以使accept得到的socket是非阻塞的。

5、int connect(int fd, const struct sockaddr *servaddr, socklen_t addrlen);
servaddr：需要连接的服务器ip和port。
函数成功返回0，错误返回-1。

6、ssize_t send(int fd, const void *buf, size_t nbytes, int flags);
flags的取值：
MSG_DONTROUTE：作用于send和sendto，本次发送绕过路由表查找。也可以对整个socket设置SO_DONTROUTE选项，绕过路由表查找。
MSG_DONTWAIT：仅本操作非阻塞。
MSG_OOB：发送或接收带外数据。TCP连接上只有一个字节可以作为带外数据发送。对于recv，本标志指明即将读入的是带外数据。
MSG_PEEK：窥看外来消息。作用于recv和recvfrom，允许我们查看已读取的数据，而且系统不再recv或recvfrom返回后丢弃这些数据。
MSG_WAITALL：等待所有数据。作用于recv和recvfrom，告诉内核不要在尚未读入请求数目的字节之前，让一个读操作返回。
函数成功则为发送的字节数，出错返回-1。返回值为0，表示对端关闭了socket。
send的用法就是for循环不断调用send，然后累计send每次返回的已发送字节数。调用者根据实际业务判断数据是否已经发送完成，然后结束for循环。

7、ssize_t recv(int fd, void *buf, size_t nbytes, int flags);
函数成功则为接收的字节数，出错返回-1。返回值为0，表示对端关闭了socket。

8、ssize_t sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *to , socklen_t tolen);
用于发送UDP报文。
to：用来指定目的ip和port。
成功则返回实际发送的字符数，失败返回－1，错误原因存于errno中。

9、ssize_t recvfrom(int fd, void *buf, size_t len, int flags, const struct sockaddr *from, socklen_t fromlen);
用于接收TCP报文。
成功则返回实际接收的字符数，失败返回－1，错误原因存于errno中。

注意：sendto和recvfrom也可以用来发送和接收TCP报文。

10、ssize_t recvmsg(int fd, struct msghdr *msg, int flags);
sszie_t sendmsg(int fd, struct msghdr *msg, int flags);
函数成功则为发送或接收的字节数，若出错为-1。
这两个接口是对其他发送和接收接口的整体封装。

## 选项
1.SO_REUSEADDR  
a. 设置之后，除非绑定完全相同的ip和port，其他情况多个socket都可以绑定成功。
比如，一般情况下如果socket1绑定了0.0.0.0:80，那么socket2是无法绑定1.1.1.1:80。但如果设置了该选项，socket2就可以绑定成功。
b. 设置该选项之后，TIME_WAIT状态socket绑定的ip和port可以被其他socket绑定成功。

2. SO_REUSEPORT    
设置之后，允许任意多的socket绑定完全相同的ip和port。但必须是每个socket都设置了该选项。  
如果socket1没有设置该选项绑定了1.1.1.1:80，那么后续其他socket即使设置了该选项也无法绑定1.1.1.1:80。  

3. SO_RCVBUF和SO_SNDBUF   
用来设置socket接收缓冲区和发送缓冲区大小的。  

4. SO_SNDLOWAT和SO_RCVLOWAT    
设置socket发送缓冲区和接收缓冲区的低水位。以接收缓冲区为例，缓冲区内数据大小没有超过低水位时，不通知上层应用来读取数据。只有超过低水位之后，才通知上层应用。  

5. SO_KEEPALIVE  
对于TCP的socket，如果2小时内双方都没有任何的数据传输，TCP就自动给对端发送一个存活探测报文，对端必须相应。
1)如果对端响应，那么就继续等待，如果再过2个小时还是没有数据传输，则再发一个存活探测报文。
2)如果对端响应RST，表示对端程序崩溃之后再次启动，socket被关闭。
3)如果没有收到对端响应，则每隔75s再发送一个存活探测报文，一共发送8个，如果都没有得到相应就关闭socket。
TCP_KEEPIDLE设置多久没有数据传输就开始存活探测。
TCP_KEEPINTVL设置两次探测之间的间隔。
TCP_KEEPCNT设置探测次数。
keepalive如果超时，会触发epoll事件可读，recv返回-1。错误码为ECONNRESET和ENETRESET。

6. SO_LINGER  
有两个参数l_onoff和l_linger。
l_onoff为0则表示该选项关闭。
l_onoff为1，l_linger为0，当关闭socket的时候，socket直接丢弃发送缓冲区内所有数据，并发送一个RST给对端。不走正常的四次挥手断链，避免了TIME_WAIT状态。
l_onoff为1，l_linger为1，如果socket发送缓冲区内有数据，则等待所有数据都发送完成并收到对端确认。或者就是等待超时，才关闭socket。

7、TCP_NODELAY
关闭Nagle算法。

8、TCP_DEFER_ACCEPT
对于建连完成的socket，直到收到第一个数据之后才通知应用程序处理该连接。

9、TCP_CORK
启动CORK算法。

## socket操作常见错误errno
EAGAIN，对于非阻塞socket不算错误，可以再次调用socket接口。
在ET触发模式下，出现EAGAIN就是读数据结束了，不能继续调用recv接口。在LT触发模式下可以继续调用send和recv。
EINTER，被系统调用中断，connect在这个错误码之后不能再次调用connect。只需要重新设置epoll事件就可以了。其他的socket接口可以再次调用。
connect不能重复调用的原因是因为connect已经将请求发送给对方，正在等对方的回答。如果重新调用connect而对方又接收了上一次的请求，这一次的connect就会返回失败。
EINPROGRESS，连接进行中，可以通过epoll事件来判断是否完成。
ETIMEOUT，发生了读超时或者写超时。
ECONNREFUSED，连接拒绝，一般发生在建连过程。
EINVAL，提供的参数非法。或者socket状态不对，比如没有listen就accept。

## TSO\GSO\GRO\LRO
1. TSO  
TCP大包分片，将tcp层的分片卸载到网卡执行，需要网卡支持；  
2. GSO  
通用包分片，不需要网卡支持，将分片动作延迟到网卡驱动执行；  
3. GRO  
通用组包，不需要网卡支持，将组包动作交给网卡驱动执行；  
4. LRO
网卡组包，需要网卡支持，网卡将收到的包合并成大包之后再传递给协议栈；  