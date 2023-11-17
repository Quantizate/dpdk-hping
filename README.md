# dpdk-hping

## Name
hping3 - send ICMP packets that can calculate minimum/maximum and average RTTs for the number of packets sent. 

## Synopsis
dpdk-hping [ -p port id ] [ -c client mode] [ -s server mode ] [ -l number of packets ] [ -W timeout ] [ -a server ip address ] [ -b client ip address ]

## Description
dpdk-hping is a network tool able to send ICMP packets, packets containing only ethernet header address, and displays the statistics of the received packets from clients. It is able to calculate RTT between server and client hosts. 

## Base Options

-p: Port Id: <br>
    The index of the port to be used by the DPDK driver for server or client. <br>
    <br>
-c: client mode: <br>
    Run the host in client mode and give the MAC address of host as argument.<br>
    <br>
-s: server mode:<br>
    Run the host in server mode. <br>
    <br>
-l: number of packets:<br>
    Number of packets to be sent. Default = flood.<br>
    <br>
-W: timeout:<br>
    Change timeout using this setting. Default = 2 secs<br>
    <br>
-a: server IP address:<br>
    Pass the server IP address as argument.<br>
    <br>
-b: client IP address:<br>
    Pass the client IP address as argument. <br>
    <br>
## How to setup?
Configure DPDK.
Bind your machine with DPDK-compatible driver. If using the same machine, bind two network interfaces to each machine. 
Run the following commands on each host: 
```
 sudo ./dpdk-hping --file-prefix server -a <network_interface> -- -s -p 0
 sudo ./dpdk-hping --file-prefix client -a <network_interface> -- -c <server MAC address>  -p 0
```
Few other examples: 
```
sudo ./dpdk-hping --file-prefix client -a <network_interface> -- -c <server MAC address>  -p 0 -l 150
sudo ./dpdk-hping --file-prefix client -a <network_interface> -- -c <server MAC address>  -p 0 -W 5
sudo ./dpdk-hping --file-prefix client -a <network_interface> -- -c <server MAC address>  -p 0 -a 10.0.0.1
```
## Authors
Dhruv Gupta, Hirva Patel, Srujan Shetty, Dhruv Khandelwal

## References
https://github.com/JiakunYan/dpu-pingpong
