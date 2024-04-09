# Vegas
在CA_OPEN下vegas enable，其他状态disable，vegas disable之后按照reno算法进行拥塞避免；  
## RTT采样
1. baseRTT，流传输过程中采样到的最小rtt，每次收到ack会更新；  
2. minRTT，每一个发包窗口内采集到的最小rtt；   
3. diff = cwnd * （minRTT - baseRTT）/ baseRTT，可以认为diff是实际发包量与网络理想能承载包量之间的差值；  

## 慢启动
没有经过一轮完整的发包周期，按照tcp常规slow start的方式增窗：cwnd += ack；  
每经过一轮发包周期，即上一窗的包对端已经全部ack：  
如果diff <= gamma(1)，按照tcp常规slow start的方式增窗：cwnd += ack；  
如果diff > gamma，说明增长过快，通过target_cwnd来限制cwnd的增长，target_cwnd = cwnd * baseRTT / minRTT，此时也会更新vegas ssthresh；  

## 拥塞避免
每经过一轮发包周期，即上一窗的包对端已经全部ack：
如果diff > beta(4)，认为cwnd过大导致发送太快，cwnd -= 1；  
如果diff < alpha(2)，cwnd += 1；  
