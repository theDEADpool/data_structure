# TC流量控制
流量控制的基本概念是队列qdisc，每个网卡都与一个队列qdisc联系。当内核要发送报文，都会将报文添加到该网卡配置的队列中，由队列决定报文的发送顺序。有的队列是先进先出，有的队列会有特别的排队、分类的规则，为了实现这样的功能，需要通过filter将报文分成不同的组class；  
tc一般分为几个步骤：
1. 给网卡配置队列；  
2. 在该队列上建立分类；  
3. 根据需要建立子队列和子分类；  
4. 为每个分类建立过滤器；  

tc命令的一般形式：  
1. 创建1个htb的根   
tc qdisc add dev eth0 root handle 1:0 htb default 1   
2. 创建类别  
tc class [add|change|replace] dev DEV parent [qdisc-id] classid [class-id] qdisc [parameters]   
tc class add dev eth0 parent 1: classid 1:11 htb rate 40mbit ceil 40mbit //创建一个类别享有40mbit带宽；  
3. 创建过滤器   
tc filter [add|change|replace] dev DEV [parent qdisc-id/root] protocol [protocol] pro [priority] filtertype [filtertype specific params] flow [flow id]   
tc filter add dev eth0 protocol ip parent 1:0 prio 1 u32 match ip dport 80 0xffff flowid 1:11

更复杂一点的：  
首先创建两个大类，一个40mbit带宽，一个80mbit带宽   
tc class add dev eth0 parent 1: classid 1:1 htb rate 40mbit ceil 40mbit
tc class add dev eth0 parent 1: classid 1:2 htb rate 80mbit ceil 80mbit
在类别2里再创建两个小类，
tc class add dev eth0 parent 1: classid 1:21 htb rate 30mbit ceil 30mbit
tc class add dev eth0 parent 1:2 classid 1:22 htb rate 50mbit ceil 50mbit

设置延时
tc qdisc add dev eth0 root netem delay 150ms  
tc qdisc add dev eth0 root netem loss 3%
tc qdisc change dev eth0 root netem delay 150ms 50ms