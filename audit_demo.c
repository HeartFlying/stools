#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/audit.h>

//#define NETLINK_AUDIT 21  // 或通过 sys/socket.h 中的定义
//通过NetLink做日志审计
// Netlink 消息缓冲区大小
#define BUFFER_SIZE 4096

int main() {
    struct sockaddr_nl src_addr, dest_addr;
    struct nlmsghdr *nlh = NULL;
    int sock_fd;
    char buffer[BUFFER_SIZE];

    // 1. 创建 Netlink 套接字
    sock_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_AUDIT);
    if (sock_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. 绑定地址
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();  // 用户空间进程的 PID
    src_addr.nl_groups = 1;  // 订阅审计日志组

    if (bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) == -1) {
        perror("bind");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Listening for audit events...\n");

    // 3. 循环接收审计日志
    while (1) {
        int len;
        // 接收 Netlink 消息
        len = recv(sock_fd, buffer, BUFFER_SIZE, 0);
        if (len == -1) {
            perror("recv");
            break;
        }

        // 解析消息头
        nlh = (struct nlmsghdr *)buffer;

        // 检查消息是否完整
        if (!NLMSG_OK(nlh, len)) {
            fprintf(stderr, "Invalid Netlink message\n");
            break;
        }

        // 打印审计日志内容
        if (nlh->nlmsg_type == AUDIT_EOE || nlh->nlmsg_type == AUDIT_PATH) {
            // 从消息体中提取日志字符串
            char *log = NLMSG_DATA(nlh);
            printf("Audit Event: %s\n", log);
        }
    }

    // 4. 关闭套接字
    close(sock_fd);
    return 0;
}
