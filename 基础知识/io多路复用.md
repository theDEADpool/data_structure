# epoll
LT:水平触发，当被监控的文件描述符上有可读写事件时，epoll_wait()会通知应用程序。
如果没有把数据读写完，下次调用epoll_wait()还会继续通知应用程序。
水平触发支持阻塞模式或者非阻塞模式。
1. 对于读操作，只要缓冲内容不为空，LT模式返回读就绪。
2. 对于写操作，只要缓冲区还不满，LT模式会返回写就绪。

ET:边缘触发，当被监控的文件描述符上有可读写事件时，epoll_wait()会通知应用程序。
但不管有没有读写完，下次调用epoll_wait()都不会再通知，直到再次出现可读写事件。
边缘触发只支持非阻塞模式。
1. 对于读操作
（1）当缓冲区由不可读变为可读的时候，即缓冲区由空变为不空的时候。
（2）当有新数据到达时，即缓冲区中的待读数据变多的时候。
（3）当缓冲区有数据可读，且应用进程对相应的描述符进行EPOLL_CTL_MOD修改EPOLLIN事件时。
2. 对于写操作
（1）当缓冲区由不可写变为可写时。
（2）当有旧数据被发送走，即缓冲区中的内容变少的时候。
（3）当缓冲区有空间可写，且应用进程对相应的描述符进行EPOLL_CTL_MOD修改EPOLLOUT事件时。

使用边缘触发时，以读数据为例，如果recv函数返回的字节数和用于接受数据的缓存大小相同时，
则有可能缓冲区内还有数据没有读完。此时需要再次调用recv函数。
ET触发，如果是accept，需要多次调用。因为每次调用accept只会处理一个建立的连接。但新建连接可能不止一个。不循环调用accept会导致连接没有被处理。

select、poll、epoll
相同点:
1）三者都需要在fd上注册用户关心的事件； 
2）三者都要一个timeout参数指定超时时间；

int select(int maxfdp1,fd_set *readset,fd_set *writeset,fd_set *exceptset,const struct timeval *timeout)
返回值为就绪的描述符数目，超时返回0，出错返回-1。

int poll (struct pollfd * fds, unsigned int nfds, int timeout);
struct pollfd {
int fd;         /* 文件描述符 */
short events;   /* 等待的事件 */
short revents;  /* 实际发生了的事件 */
};
返回值>0为fds中准备号读写或出错的描述符总数。超时返回0。函数调用出错返回-1。

不同点:
1）select： 
a）select指定三个文件描述符集，分别是可读、可写和异常事件，所以不能更加细致地区分所有可能发生的事件； 
b）select如果检测到就绪事件，会在原来的文件描述符上改动，以告知应用程序，文件描述符上发生了什么事件，所以再次调用select时，必须先重置文件描述符； 
c）select采用对所有注册的文件描述符集轮询的方式，会返回整个用户注册的事件集合，所以应用程序索引就绪文件的时间复杂度为O(n)； 
d）select允许监听的最大文件描述符个数通常有限制，一般是1024，如果大于1024，select的性能会急剧下降； 
e）只能工作在LT模式。
f）select每次调用都要将描述符集在用户空间和内核空间进行拷贝，fd数量多的时候，开销很大。

2）poll： 
a）从上面的结构体可以看出poll把文件描述符和事件结合在一起，每个文件描述符的事件可以单独指定，而且可以是多个事件的按位或，这样更加细化了事件的注册，而且poll单独采用一个元素用来保存就绪返回时的结果，这样在下次调用poll时，就不用重置之前注册的事件； 
b）poll采用对所有注册的文件描述符集轮询的方式，会返回整个用户注册的事件集合，所以应用程序索引就绪文件的时间复杂度为O(n)。 
c）poll用nfds参数指定最多监听多少个文件描述符和事件，这个数能达到系统允许打开的最大文件描述符数目，即65535。 
d）只能工作在LT模式。

3）epoll： 
a）epoll把用户注册的文件描述符和事件放到内核当中的事件表中，提供了一个独立的系统调用epoll_ctl来管理用户的事件，而且epoll采用回调的方式，一旦有注册的文件描述符就绪，
将触发回调函数，该回调函数将就绪的文件描述符和事件拷贝到用户空间events所管理的内存，这样应用程序索引就绪文件的时间复杂度达到O(1)。 
b）epoll_wait使用maxevents来制定最多监听多少个文件描述符和事件，这个数能达到系统允许打开的最大文件描述符数目，即65535； 
c）不仅能工作在LT模式，而且还支持ET高效模式（即EPOLLONESHOT事件，读者可以自己查一下这个事件类型，对于epoll的线程安全有很好的帮助）。

epoll的内部实现
epoll_create会在内核中创建一个eventpoll结构体，
该结构体中有红黑树来存储需要监控的事件。有双向链表存放需要通过epoll_wait返回给用户的满足条件的事件。
struct eventpoll{
    ....
    /*红黑树的根节点，这颗树中存储着所有添加到epoll中的需要监控的事件*/
    struct rb_root  rbr;
    /*双链表中则存放着将要通过epoll_wait返回给用户的满足条件的事件*/
    struct list_head rdlist;
    ....
};
epoll_ctl则用来向epoll对象中添加事件。每个添加的事件都会与网卡驱动程序建立回调关系，当有事件发生时回调这个方法。
这个回调方法在内核中叫ep_poll_callback,它会将发生的事件添加到rdlist双链表中。
在epoll中，对于每一个事件，都会建立一个epitem结构体。
struct epitem{
    struct rb_node  rbn;//红黑树节点
    struct list_head    rdllink;//双向链表节点
    struct epoll_filefd  ffd;  //事件句柄信息
    struct eventpoll *ep;    //指向其所属的eventpoll对象
    struct epoll_event event; //期待发生的事件类型
}
当调用epoll_wait检查是否有事件发生时，只需要检查eventpoll对象中的rdlist双链表中是否有epitem元素即可。如果rdlist不为空，则把发生的事件复制到用户态，同时将事件数量返回给用户。
typedef union epoll_data {
	void *ptr;
	int fd;
	__uint32_t u32;
	__uint64_t u64;
} epoll_data_t;
struct epoll_event {
	__uint32_t events;
	epoll_data_t data;
}
epoll支持的events事件类型：
EPOLLIN：触发该事件，表示对应的文件描述符上有可读数据。(包括对端SOCKET正常关闭)； 
EPOLLOUT：触发该事件，表示对应的文件描述符上可以写数据； 
EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）； 
EPOLLERR：表示对应的文件描述符发生错误； 
EPOLLHUP： 表示对应的文件描述符被挂断； 
EPOLLET：将EPOLL设为边缘触发(EdgeTriggered)模式，这是相对于水平触发(Level Triggered)来说的。 
EPOLLONESHOT： 只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里。

int epoll_create(int size) size表示要处理的大致事件数目，新版本的linux内核这个参数没有意义。
nginx在调用epoll_create传入的size是connection_n / 2。是在epoll模块init的阶段调用的epoll_create。一个进程一个epollfd。
项目是一个线程创建一个epollfd，比如tcp的accept线程，读写线程，udp线程等。

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev)
epfd就是epoll_create返回的fd。
fd是待检测的socket的fd。
op有三种EPOLL_CTL_ADD、EPOLL_CTL_MOD、EPOLL_CTL_DEL。
ev就是上面说的支持的事件类型。
nginx在使用的时候传入的ev就是一个局部变量，设置好本次操作的事件类型即可。ev中的data保存的就是ngx_connection_t结构的地址。

int epoll_wait(int epfd, struct epoll_event *ev, int maxevent, int timeout)
返回值表示当前发生的事件个数。返回-1表示出现错误需要看errno返回码。
timeout表示最多等待多少毫秒后返回。
maxevent表示本次可以返回的最大事件数目。
ev是一个事件数组用来存储触发的事件。
nginx使用的时候传入的ev是一个全局变量，数组的长度是通过配置决定的，默认值是512。
nginx在epoll_wait触发返回所有事件的数组，数组成员是epoll_event，其中data字段存放的就是ngx_connection_t数据结构，里面就保存了连接的读写处理函数。
项目的用法是，在epoll_event数据结构的data字段保存待处理的socket的fd。epoll_wait在事件触发后返回所有触发事件的数组。线程遍历触发事件的数组。
从中取出待处理的fd，然后根据fd找到对应的数据结构。数据结构中保存了fd的读写处理函数。根据触发的epoll事件类型调用对应的处理函数。

