# BBR2
## 想要解决的问题
1. BBR每次收到ack都会把算法完整执行一遍，比cubic产生了大概2%~5%的性能开销，v2里面引入了fastpath；  
2. BBR的pacing单纯基于bw计算，如果bw小，pacing小；  

## 新增的概念
1. inflight hi  
在probe_bw阶段，丢包率超过2%时，记录此时的inflight数量，在probe_up阶段且没有出现丢包的情况下，inflight_hi会继续增大；  

2. inflight_lo
在非probe_bw阶段出现丢包的时候，根据cwnd * 0.75得到的。通常是小于max_inflight和inflight_hi；  

3. inflight headroom  
是一个系数0.15， inflight_headroom = inflight_hi * (1 - headroom)，用于cruise阶段，cwnd不能超过inflight_headroom;  
在probe_down阶段如果inflight > inflight_hi_with_headroom，会停留在probe_down；  
headroom的存在就会让BBR2在运行时能留出一部分带宽空间给其他的流使用；  

4. idle restart
如果发数据的时候inflight == 0 && is_app_limited则认为是idle restart，当收到ack之后，就结束idle restart；  
进入idle restart，如果是probe_bw会使用之前记录的max_bw计算pacing rate， gain = 1。使用该pacing进行数据发送；  
如果是probe_rtt会使用进入probe_rtt之前的cwnd作为当前cwnd发送数据；  
退出idle restart状态是不会进入probe_rtt的，因为该状态相当于队列已经排空过了；  

5. fast path  
主要是减少不必要的代码流程；  
1. try_fast_path = 1，当cwnd达到target_cwnd时，try_fast_path每次ack来都会被重置；  
2. 当前is_app_limited && sample bw < max_bw && 当前round无丢包，同时满足这些条件，判断是否退出drain阶段，跟新probe_bw的cycle，更新min_rtt；   
3. 再加上状态没变化 && min rtt没变化 && probe_bw的cycle没变化，那本次就是fast path流程，不会计算cwnd和pacing rate;   

## 状态机的变化
1. probe_refill  
填充管道，又不引起排队；  
pacing_gain  = 1.0, cwnd_gain = 2.0；  
a. probe_down、 probe_cruise都可以进入probe_refill，条件是这两个阶段持续时间超过了probe_wait（2~3s）设定的值；  
b. probe_down还有一种情况也能进入probe_refill，即前一次的probe_up的inflight > inflight_hi && 触发了2%的丢包。如果在round_start时还满足上面的状态就进入probe_up；  

2. probe_cruise  
pacing_gain = 1.0， cwnd_gain = 2.0;  
a. probe_rtt结束之后，如果状态机不处于startup阶段，就进入probe_cruise;   
b. probe_down结束之后先判断是否进入probe_bw，如果不满足条件，则判断是否进入probe_cruise，两个条件满足其中之一：
1. inflight < 根据bw算出的理论inflight；  
2. probe_down状态持续超过了一个min_rtt;  
在probe_cruise阶段cwnd是不能超过inflight_headroom，退出probe_cruise阶段之后，会进入probe_refill；  

3. probe_down
pacing_gain = 0.75, cwnd_gain = 2.0；  
a. 退出probe_up状态进入probe_down；  
b. 退出drain状态进入probe_bw首先进入probe_down；  
退出probe_down是一下两种情况：  
a. 优先判断是否可以进入probe_refill，当前phase持续时间超过了probe_wait_time则进入probe_refill；  
b. 再判断是否可以进入probe_cruise，当前phase持续时间超过了min_rtt或inflight < bdp；  

