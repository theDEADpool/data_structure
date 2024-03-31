# EBPF
分为用户空间和内核空间，两者交互有2种方式：
1. BPF map；  
a.BPF程序和用户态程序交互，将结果存储到map中，用户态可以通过fd访问；  
b.BPF程序和内核态程序交互，也可以使用map作为媒介；  
c.BPF程序之间交互，BPF本身不允许使用全局变量，可以用map来充当全局变量；  
d.BPF tail call尾调用，通过BPF_MAP_TYPE_ARRAY类型的map来感知多个BPF程序的指针，再通过tail call的方式实现多个BPF程序之间的跳转；  
通用的map有：  
BPF_MAP_TYPE_HASH、BPF_MAP_TYPE_ARRAY、BPF_MAP_TYPE_PERCPU_HASH、BPF_MAP_TYPE_PERCPU_ARRAY、
BPF_MAP_TYPE_LRU_HASH、BFP_MAP_TYPELRU_PERCPU_HASH、BPF_MAP_TYPE_LPM_TRIE；  
非通用的map有：  
BPF_MAP_TYPE_PROG_ARRAY，用于hold其他BPF程序；  
BPF_MAP_TYPE_PERF_EVENT_ARRAY，用于检查skb中的cgroup2成员信息；  
BPF_MAP_TYPE_STACK_TRACE，用于存储栈跟踪的MAP；  
BPF_MAP_TYPE_ARRAY_OF_MAPS、BPF_MAP_TYPE_HASH_OF_MAPS，持有其他map的指针，map在运行时可以原子替换；  

尾调用机制：  
一个BPF程序可以调用另一个BPF程序，且调用完成后不会返回原来的BPF程序；通过这个机制，可以将一个大型的逻辑复杂的BPF程序拆分成多个子BPF程序，通过尾调用的方式来实现大程序的功能；

2. perf event；

工作逻辑：
1. BPF程序通过LLVM/Clang翻译成eBPF定义的字节码prog.bpf；  
2. 经过验证器检验字节码的安全性；  
3. 将字节码加载到内核模块执行，相关程序类型可以是kprobe/uprobe/tracepoint/perf_events中一个或多个；
4. 内核将执行结果通过BPF map或perf_event通知给用户空间；

## BPF系统调用
int bpf(int cdm, union bpf_attr *attr, unsigned int size);  
cmd:指定了bpf系统调用执行的命令类型；attr是执行cmd传入的参数，size表示了attr对象的大小；
cmd一般分为ebpf map的操作和ebpf程序两种类型：
1.BPF_MAP_CREATE：创建一个ebpf map返回一个fd；  
2.BPF_MAP_LOOKUP_ELEM：某个MAP中根据key查找元素返回value；   
3.BPF_MAP_UPDATE_ELEM：某个MAP中新增或更新一对key/value；  
4.BPF_MAP_DELETE_ELEM：某个MAP中删除一个key；  
5.BPF_MAP_GET_NEXT_KEY：某个MAP中根据key查找下一个成员的key；  
6.BPF_PROG_LOAD：加载ebpf程序并返回与之关联的fd；  

## BPF程序类型
BPF_PROG_LOAD加载程序类型规定了：  
1. 程序加载在什么位置；  
2. 验证器允许调用内核中的哪些辅助函数；  
3. 网络包的数据是否可以直接访问；  
4. 作为第一个参数传递给程序的对象类型；  
bpf支持的程序类型有：
BPF_PROG_TYPE_SOCKET_FILTER，一种网络数据包过滤器；  
BPF_PROG_TYPE_KPROBE，确定kprobe是否应该触发；  
BPF_PROG_TYPE_SCHED_CLS
BPF_PROG_TYPE_SCHED_ACT
BPF_PROG_TYPE_TRACEPOINT
BPF_PROG_TYPE_XDP，从设备驱动收路径运行网络数据包过滤器；  
网卡收到数据包，尚未生成skb_buff数据结构之前的一个hook点。  
BPF_PROG_TYPE_PREF_EVENT
BPF_PROG_TYPE_CGROUP_SKB
BPF_PROG_TYPE_CGROUP_SOCK
BPF_PROG_TYPE_LWT_*
BPF_PROG_TYPE_SOCK_OPS
BPF_PROG_TYPE_SK_SKB，一种套接字之间转发网络数据包的过滤器；  
BPF_PROG_CGROUP_DEVICE


## 限制
1. ebpf栈大小限制在MAX_BPF_STACK = 512；
2. ebpf指令码大小到最新5.8版本内核扩大为100w；
3. epbf不能随意调用内核参数，只限于内核模块中列出的BPF Helper函数；
4. epbf对循环的次数和时间有限制，防止在kprobes中插入恶意循环。

## 工具链
bcc
bpftrace

一个ebpf程序如何加载到内核
1. 内核程序编译生产.o文件；  
2. 用户态bpf_prog_load打开的.o文件，获取到bpf_object指针和prog_fd；  
3. 再通过setsockopt的方式把prog_fd写入到对应的sock；  
