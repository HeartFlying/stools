#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define UEVENT_BUFFER_SIZE 2048  // 足够大的缓冲区存储uevent消息
static volatile sig_atomic_t running = 1;

// 利用Netlink处理linux下的硬件设备热插拔等
// 信号处理：优雅退出
void sigint_handler(int sig) {
    running = 0;
}

// 解析uevent消息并打印关键信息
void parse_uevent(const char *buf, ssize_t len) {
    const char *p = buf;
    const char *end = buf + len;

    char action[32] = {0};   // 设备动作：add/remove/bind/unbind等
    char subsystem[32] = {0};// 设备子系统：usb/block/platform等
    char devpath[256] = {0}; // 设备路径

    // 逐行解析键值对（以'\0'分隔）
    while (p < end) {
        if (strncmp(p, "ACTION=", 7) == 0) {
            sscanf(p + 7, "%31[^ ]", action);
        } else if (strncmp(p, "SUBSYSTEM=", 10) == 0) {
            sscanf(p + 10, "%31[^ ]", subsystem);
        } else if (strncmp(p, "DEVPATH=", 8) == 0) {
            sscanf(p + 8, "%255[^ ]", devpath);
        }
        p += strlen(p) + 1; // 移动到下一个字段
    }

    // 只打印有效事件
    if (action[0] && subsystem[0]) {
        printf("[EVENT] Action: %-8s Subsystem: %-12s Path: %s\n",
               action, subsystem, devpath);
    }
}

int main() {
    // 创建Netlink Socket
    int sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (sock == -1) {
        perror("Failed to create socket");
        return EXIT_FAILURE;
    }

    // 绑定到内核的uevent组
    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();      // 使用当前进程ID
    addr.nl_groups = 1;          // 加入多播组1（uevent默认组）

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("Failed to bind socket");
        close(sock);
        return EXIT_FAILURE;
    }

    // 设置信号处理
    signal(SIGINT, sigint_handler);
    printf("Monitoring uevents. Press Ctrl+C to exit...\n");

    // 接收消息循环
    char buf[UEVENT_BUFFER_SIZE];
    while (running) {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len == -1) {
            if (running) perror("Error receiving message");
            continue;
        }

        parse_uevent(buf, len);  // 解析并打印事件
    }

    close(sock);
    printf("\nExiting...\n");
    return EXIT_SUCCESS;
}
