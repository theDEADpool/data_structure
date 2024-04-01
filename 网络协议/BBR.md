# 基本原理
## 状态机
1. startup，cwnd_gain = pacing_gain = 2.89；  
进入drain条件：达到full_bw，即连续3个rtt round，max_bw增长幅度小于25%；  
max_bw连续3次增长幅度小于25%，则full_bw_reached = 1；   

2. drain，cwnd_gain = 2.89，pacing_gain = 1/2.89；  
进入probe_bw条件：inflight <= BDP；  

3. probe_bw，cwnd_gain = 2，pacing_gain = {1.25, 0.75, 1, 1, 1, 1, 1, 1};  
从probe_up phase进入next_phase需要同时满足两个条件：
a.最近一次收到有确认数据ack或sack的时间距离当前cycle开始时间超过min_rtt；  
b.出现丢包或实际网络中报文数量 >= BDP；
其他phase进入到next_phase只需要满足上面两个条件之一；  

4. probe_rtt， cwnd_gain = pacing_gain = 1；  
任何状态都可以进入probe_rtt阶段，且条件都是min_rtt过期；  
probe_rtt阶段结束且full_bw，则进入probe_bw，否则进入startup；  
首先会做排空，cwnd = 4，当inflight <= probe_rtt_cwnd时候，维持这个状态最多200ms，以此来尝试获取min_rtt；  

## 计算公式
带宽bw = rs->delivered * BW_UNIT / rs->interval_us；   
bdp = max_bw * min_rtt；  
pacing_rate = max_bw(lt_bw) * mss * pacing_gain；  
target_cwnd = max_bw * min_rtt_us * cwnd_gain;  
target_cwnd += ack聚合需要补偿的cwnd；  
snd_cwnd = min(snd_cwnd + acked, taret_cwnd)，逐渐靠近target_cwnd；  

## 包守恒
1. 如果ack有判定丢包，cwnd = min(cwnd - loses, 1);  
2. 本次从其他状态进入recovery状态，启动包守恒，cwnd = max(cwnd, inflight + acked);  
3. 如果本次从recovery或loss恢复成其他状态，退出包守恒状态。snd_cwnd = max(cwnd, 进入recovery状态时记录的snd_cwnd)；  
4. 包守恒状态最多持续1个rtt周期，新的rtt周期开始也会退出包守恒状态；  

## Ack聚合
通过计算一定时间窗口内，对端理论应该确认的数据包个数与对端实际确认的数据包个数进行比较，如果实际确认的数据包个数 > 理论应该确认的数据包个数，则说明出现ack聚合；  
理论应该确认的数据包个数 = bbr带宽 * 时间窗口；  
实际确认的数据包个数 = 时间窗口内ack确认数据包个数累加；  
每次收到ack、sack只要确认了数据包就会进行上面的判断，对于不满足ack聚合条件的，更新时间窗口；  
如果出现ack聚合，会补偿target_cwnd的大小。补偿cwnd的大小 = min(根据时间计算的补偿值， 根据确认包数量计算的补偿值)；  
根据时间计算的补偿值 = max_bw * 100ms；  
根据确认包量计算的补偿值 = 额外ack数量 * extra_ack_gain(= 1)；  
额外ack数量 = 时间窗口内实际确认的包个数 - 理论确认的包个数；   

## long term机制
网络上令牌筒机制的路由器和交换机，当令牌耗尽时就会出现丢包。long term机制就是针对这个情况对bw进行修正；   
1. lt采样开始的条件：本次ack判定出现了丢包；  
2. lt采样结束的条件：  
a. 当前probe_bw阶段 && round_start == 1 && lt采样经过的rtt轮次 >= lt_bw_max_rtts（默认48）。这种情况会停止lt采样，重置lt采样rtt轮次，并更新probe_bw所使用的增益值；  
b. lt采样经过的rtt轮次 > 4 * lt_intvl_min_rtt（默认是4）,会停止lt采样，重置lt采样rtt轮次；   
3. lt采样最小间隔：采样过程中，并不是每一个ack都会采样， 而是要求经过的rtt轮次不小于lt_intvl_min_rtts（默认是4）才会采样；  
4. lt采样计算   
a. 计算采样间隔内丢包个数lost和对端确认数据个数delivered。满足delivered != 0 && lost > lt_loss_thresh（默认50） * delivered，进入下一步；  
b. 当前距离上一次采样的时间间隔t，计算lt_bw = delivered / t；  
c. 如果lt_bw与正常计算的bw之间差距过小，则对lt_bw进行矫正 = （bw + lt_bw）/ 2，并限制pacing_gain  = 1；  
5. 什么时候会使用lt_bw，上面说到的'差距过小'时会开始使用lt_bw，直到lt_bw采样结束；   

用过哪些优化：
1. expect_bw，达到这个带宽之后，probe bw阶段会采用相对收敛的gain = {1.1, 0.9, 1, ...}；  
2. max_expect_bw，限制每次ack计算的bw上限；  
3. 拥塞避免补偿cwnd，当达到了full bw && 没有进入包守恒，使用beyond_target_cwnd来补偿target_cwnd；当cwnd = target_cwnd + beyond_target_cwnd，且发包受限于cwnd，则扩大beyond_target_cwnd；根据target_cwnd和ai_scale计算一个cwnd阈值，随scale赠大而逐渐减小；阈值越小，则beyond_target_cwnd的值也越大，补偿的越多；  
4. rttvar补偿，bbr->max_rtt保存过去5个round的最大rtt，每次收到ack，得到非0rtt都会更新bbr->max_rtt；设定一个rtt补偿阈值 = 2（startup阶段）、1（probe_bw阶段），根据min_rtt一起加权计算得到一个rtt补偿阈值，如果max_rtt > 该阈值 && 当前srtt > min_rtt，则cwnd补偿值 = （srtt - min_rtt）* bw；  
5. 进入probe rtt的时间随机化，applimit阶段不进入probe rtt，rtt变化不大不进入probe_rtt；还会判断min_rtt与srtt之间大小，如果min_rtt > srtt，则将min_rtt改为srtt；  
6. startup阶段移植bbr2丢包退出startup的逻辑；  
7. drain to target优化
a. 延长了probe down时间，原本最多一个rtt，此处去掉了该限制，要求inflight必须要 <= bbr_inflight；  
b. 正常情况下probe down维持1个min_rtt进入到下个phase的条件是inflight <= bbr_inflight，后者是根据bw计算得到。而bw最少需要1个min_rtt的时间才能更新。该优化是保持probe down，确保drain动作确实已经达到了inflight降低的目标，避免丢包；   
c. 对于点直播类的场景，需要稳住inflight，保证pacing稳定高于码率的同时又不能过多占用带宽，才能使得多条直播流的带宽分配更均衡；  国内cdn百秒卡顿降低5%；  

## 有哪些缺点
1. startup阶段发送过于激进，容易造成长队列；  
2. 带宽探测在多留场景下会高估交付率，导致超过了1.5*bdp的队列，容易出现丢包，且与其他流之间缺乏公平性；  
3. 不同RTT之间的流计算得到的BDP不一样，RTT长的会计算出更高的BDP， 导致RTT短的流获得的带宽更小；  

## 一些问题
1. 为什么BBR计算的cwnd_gain = 2？  
如果cwnd_gain = 1，则满窗口的包刚好填充了整个管道，当第一个包被ack，到这个ack被发送端接收的这rtt / 2时间间隔内，发送端是无法发送任何数据的。这就出现了一个空窗期，且会随着ack丢失、聚合等变大；当gain = 2时，整个发送和接收的过程才是连续的；  

2. 为什么probe_rtt阶段min_cwnd会设置为4？  
TCP ack机制一般是2个包一个ack，那么为了保证能触发对方会ack，发送方就至少要发2个包，算上cwnd_gain = 2，所以min_cwnd = 4;  

3. 为什么drain阶段知识限制了pacing_gain的大小，却没有限制cwnd_gain的大小？  
因为queueing的原因是发的太快而不是发的太多了导致的；  
startup退出的时候cwnd无法继续增长，因此被认为是达到了瓶颈带宽，这种状态是bbr需要保持的，因此不需要改动cwnd的大小；  

4. 为什么probe_bw阶段的gain为1.25？  
是根据实验得到的，bbr论文里的解释是1.25的增益是在蜂窝网络下能够得到基站设备更多的带宽分配，保证发送速率不卡顿；   