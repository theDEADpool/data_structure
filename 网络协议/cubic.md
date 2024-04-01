# CUBIC
## 慢启动slow start
在该阶段是不需要做拥塞避免的，cwnd每次增长大小 = ack确认的数据包量，如果cwnd + ack > ssthresh，那么多出来的包是需要参与拥塞避免计算的；  

## 拥塞避免
即没有发生拥塞（无乱序、丢包）的情况下，cubic的处理逻辑；  
### 触发条件
并非每次ack都会触发拥塞避免逻辑，需要满足以下条件之一：  
1. 如果乱序度 > sysctl_tcp_reordering， 有效ack和sack都会触发拥塞避免；  
2. 如果乱序度 <= sysctl_tcp_reordering， 只有有效ack才能触发拥塞避免；  
有效ack，即对端确认收到了新数据，sack原则上不算是ack的，sack确认的数据是不会从retrans queue中删除；   

### 三次方程
w_cubic(t) = C*(t-K)^3 + w_max；  
w_cubic并不是tcp的cwnd，而是target cwnd；  
C是一个常数，w_max是上次出现拥塞之前的窗口大小，t是从当前到拥塞避免开始的时间间隔；  
1. 凹区域   
cwnd < Wmax，此时cwnd会不断向Wmax靠近，两者差距越小，接近速度就越慢，尽可能达到理想cwnd同时又要避免拥塞；  
2. 凸区域
cwnd > Wmax，此时如果还没有出现拥塞，说明网络上有更多的可用带宽，先慢慢的上探，只要不出现拥塞，就不断增加上探的速度，这样才能更快的找到网络的瓶颈cwnd；   
3. t的计算，与epoch_start和delay_min有关；  
a. epoch_start在出现超时重传，状态机进入TCP_CA_LOSS状态时，会设置为0；  
b. 在拥塞避免过程中，如果epoch_start = 0，则更新为当前时间；   
c. 如果是CA_EVENT_TX_START状态，即inflight == 0时发送数据，此时如果epoch_start != 0时，epoch_start会更新为当前时间（epoch_start != 0表示之前有收到过ack，也就意味着协议栈第一次发包是不会触发epoch_start更新的）；  
d. delay_min = 0的条件与epoch_start相同，每次收到ack，会根据单次采样的rtt来计算delay_min，最小是1ms，delay_min也就是过去一段时间内最小的rtt；  
4. 代码实现  
    ```
    //这里只是更新了cnt大小，cnt决定了当前需要ack多少个数据包，cwnd才能上升  
    //入参w就是ca->cnt，snd_cwnd_cnt < ca->cnt时候，cwnd不会发生变化  
    //ca->cnt在bictcp_update中计算，决定了cwnd是线性增长还是指数增长   
    void tcp_cong_avoid_ai(struct tcp_sock *tp, u32 w, u32 acked)
    {
        /* If credits accumulated at a higher w, apply them gently now. */
        if (tp->snd_cwnd_cnt >= w) {
            tp->snd_cwnd_cnt = 0;
            tp->snd_cwnd++;
        }

        tp->snd_cwnd_cnt += acked;
        if (tp->snd_cwnd_cnt >= w) {
            u32 delta = tp->snd_cwnd_cnt / w;

            tp->snd_cwnd_cnt -= delta * w;
            tp->snd_cwnd += delta;
        }
        tp->snd_cwnd = min(tp->snd_cwnd, tp->snd_cwnd_clamp);
    }
    EXPORT_SYMBOL_GPL(tcp_cong_avoid_ai);
    ```
所以三次方程决定了cwnd的变化趋势和速度，而变化大小依然是由ack数据包的多少来决定的；  

## 拥塞控制
即出现了丢包、乱序等拥塞事件时，cubic的处理逻辑；  
### 触发条件
1. RTO进入loss状态；  
2. 进入Recovery状态或CWR状态；  
### 计算方法
1. 保存当前窗口Wmax = cwnd；  
2. 记录新的slow start阈值sstreh = cwnd * beta_cubic，最少是2个包；  
3. 减窗cwnd = cwnd * beta_cubic；  
ssthresh作为退出慢启动的阈值，同时还能控制pacing的大小，cwnd>=ssthresh/2，会用一个相对较小的系数来计算pacing;  
同时，TCP处于CWR或Recovery状态，每次ack都会根据ssthresh和packet inflight来计算cwnd的大小；  

## 辅助逻辑
### 友好区间
有模块参数开关决定是否开启友好区间功能，开启后，可以加快snd_cwnd的上涨速度。主要是对应于一些小rtt或小带宽网络；  
### 快速收敛
所谓快速收敛，即网络中现有流已经占用了所有的带宽，当新流加入，需要现有流让出一部分带宽，让网络重新收敛到公平状态；  
模块参数fast_convergence开启快速收敛功能，启用之后，会在出现拥塞，记录last_max_cwnd时候会加权一个系数，让last_max_cwnd变得更小，从而降低cwnd的上涨速度和幅度；  
### 混合慢启动
目的是避免传统慢启动阶段每次RTT后cwnd加倍，导致可能出现的大量丢包，有两个指标ack train lenght和delay increase来决定何时退出慢启动，进入拥塞避免；  
cubic模块参数hystart控制开启该功能，hystart_detect用来控制使用哪一种方式作为混合慢启动的判断标准HYSTART_ACK_TRAIN|HYSTART_DELAY，最终都是通过调整ssthresh来控制何时退出慢启动；   
1. round_start，记录混合慢启动统计周期开始，当RTO或snd_una > end_seq时重置；   
2. end_seq，记录每次混合慢启动周期开始时的snd_nxt；  
3. curr_rtt，混合慢启动周期内的min_rtt；   
4. sample_cnt， 每个混合慢启动周期内的采样次数，每个周期最多采样7次；  
5. delay_min，拥塞避免阶段的min rtt;   
HYSTART_ACK_TRAIN模式，当混合慢启动采样持续时间超过了delay_min >> 4，则更新ssthresh的值；  
HYSTART_DELAY模式，curr_rtt > delay_min + delay_min >> 3，则更新ssthresh的值；  