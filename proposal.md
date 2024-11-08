<h3 align="center">
EEP 569/ EE 595 Final Project Proposal
</div>


<div align="center">
Shuting Shao, Hao Wang, Fengrui Cheng
</div>

#### **Introduction**

The primary goal of this project is to validate the Enhanced Distributed Channel Access (EDCA) protocol by comparing ns-3 simulation results for latency and throughput with an analytical model. This research is significant because it investigates how EDCA prioritizes traffic to enhance Quality of Service (QoS) for different Access Categories (ACs) in Wi-Fi networks. By understanding how EDCA impacts network performance, we can optimize Wi-Fi network efficiency, especially where QoS differentiation is critical.

#### **Background and Related Work**

1.Dolińska, I. (2018). The EDCA Implementation in NS-3 Network Simulator. *Zeszyty Naukowe*, 59(2), 19-29.

2.Obaidat, I., Alsmirat, M., & Jararweh, Y. (2016). Completing IEEE 802.11e Implementation in NS-3. In *Proceedings of the International Conference on Information and Communication Systems* (pp. 190-195). IEEE.

3.ns3::Txop Class Reference, [ns-3: ns3::Txop Class Reference](https://www.nsnam.org/doxygen/de/dca/classns3_1_1_txop.html)

#### **Experiment Plan**

##### 1.Access Categories (ACs) Variation Experiment

Objective: analyze the impact of different Access Categories (ACs) on Wi-Fi network performance (delay and throughput).

Metrics: delay, throughput, packet loss rate.

##### 2.Network Load Variation Experiment

Objective: analyze the performance of the EDCA protocol as the network load changes from low to high.

Metrics: delay, throughput, packet loss rate.

##### 3.Impact of Traffic Types Experiment

Objective: analyze the performance differences of different types of traffic under EDCA.

Metrics: delay, throughput, packet loss rate.

##### 4.Priority Handling Experiment

Objective: verify how EDCA manages traffic when high-priority and low-priority traffic coexist.

Metrics: delay, throughput, packet loss rate.

##### 5.Comparison with Analytical Model

Objective: compare simulation results with the analytical model to verify the accuracy of the simulation.

Metrics: delay, throughput, error analysis.