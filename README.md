# stools
this project record some smart tools for work

# linux下内核与应用通讯方式
|机制|性能|‌灵活性|‌安全性|‌适用场景‌|
|:------:|:------:|:------:|:------:|:------:|
|系统调用|	中|	低|	高|	标准功能调用|
|Netlink|	高|	高|	高|	网络配置、热插拔事件|
|ioctl|	中|	极高|	中|	设备驱动控制|
|共享内存|	极高|	中|	低|	大数据零拷贝传输|
|eBPF|	高|	高|	高|	动态内核扩展、监控|
|Sysfs/Procfs|	低|	低|	高|	系统状态查看|
|Debugfs|	低|	中|	低	|内核调试|
|信号	|高	|低	|中	|简单事件通知|

### ‌选择建议‌
- 对于需要高性能、灵活性和安全性的场景，优先考虑使用Netlink或eBPF。
- 如果涉及到设备驱动的控制和配置，可以考虑使用ioctl。
- 对于简单的功能调用，系统调用是一个不错的选择。
- 共享内存适用于大数据量的零拷贝传输。
- Sysfs/Procfs适合用于查看系统的状态信息。
- Debugfs主要用于内核调试。
- 信号机制适用于简单的事件通知。


----

# projects list
+ rename_test.c 支持文件原子操作
+ nl3_startDemo.c nl3库使用示例
+ netcfg.c 网络配置工具，依赖libnl3工具包
+ netlink_traffic.c 网卡流量采集
+ audit_demo.c linux的安全日志审计
+ uevent_monitor.c linux下设备热插拔
