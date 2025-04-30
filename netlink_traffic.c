#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <netlink/netlink.h>
#include <netlink/msg.h>
#include <netlink/route/link.h>

// 通过NetLink做网卡流量统计
#define INTERVAL_SEC 1  // 统计间隔（秒）

typedef struct {
    char ifname[IF_NAMESIZE];    // 网卡名称
    uint64_t last_rx_bytes;      // 上次接收字节数
    uint64_t last_tx_bytes;      // 上次发送字节数
    time_t last_update;          // 上次更新时间戳
} TrafficContext;

static volatile sig_atomic_t keep_running = 1;

// 信号处理：优雅退出
void sigint_handler(int sig) {
    keep_running = 0;
}

// 解析网卡统计信息（含流量速率计算）
static int parse_link_stats(struct nl_msg *msg, void *arg) {
    TrafficContext *ctx = (TrafficContext *)arg;
    struct nlmsghdr *nlh = nlmsg_hdr(msg);
    struct ifinfomsg *ifinfo = NLMSG_DATA(nlh);
    struct nlattr *attrs[IFLA_MAX + 1];
    
    nlmsg_parse(nlh, sizeof(struct ifinfomsg), attrs, IFLA_MAX, NULL);

    if (!attrs[IFLA_IFNAME] || !attrs[IFLA_STATS64]) 
        return NL_SKIP;

    // 匹配目标网卡
    char *current_ifname = nla_get_string(attrs[IFLA_IFNAME]);
    if (strcmp(current_ifname, ctx->ifname) != 0)
        return NL_SKIP;

    // 获取当前统计值
    struct rtnl_link_stats64 *stats = nla_data(attrs[IFLA_STATS64]);
    time_t now = time(NULL);
    
    // 计算时间差（处理首次运行）
    double time_diff = (ctx->last_update == 0) ? 1.0 : 
                      difftime(now, ctx->last_update);

    // 计算速率（B/s）
    if (ctx->last_update != 0) {
        uint64_t rx_diff = stats->rx_bytes - ctx->last_rx_bytes;
        uint64_t tx_diff = stats->tx_bytes - ctx->last_tx_bytes;
        
        printf("[%ld] %s:\n", now, ctx->ifname);
        printf("  RX: %lu B (%.2f KB/s)\n", stats->rx_bytes, rx_diff/time_diff/1024);
        printf("  TX: %lu B (%.2f KB/s)\n", stats->tx_bytes, tx_diff/time_diff/1024);
        printf("--------------------------------\n");
    }

    // 更新上下文
    ctx->last_rx_bytes = stats->rx_bytes;
    ctx->last_tx_bytes = stats->tx_bytes;
    ctx->last_update = now;

    return NL_OK;
}

// 周期性获取统计数据
void fetch_stats(struct nl_sock *sock, TrafficContext *ctx) {
    struct nl_msg *msg = nlmsg_alloc_simple(RTM_GETLINK, NLM_F_REQUEST);
    if (!msg) {
        fprintf(stderr, "Failed to allocate message\n");
        return;
    }

    // 构造请求消息
    struct ifinfomsg ifinfo = {
        .ifi_family = AF_UNSPEC,
        .ifi_index = if_nametoindex(ctx->ifname)  // 直接指定目标网卡
    };
    nlmsg_append(msg, &ifinfo, sizeof(ifinfo), NLMSG_ALIGNTO);

    // 发送请求并处理响应
    nl_send_auto(sock, msg);
    nl_socket_modify_cb(sock, NL_CB_MSG_IN, NL_CB_CUSTOM, parse_link_stats, ctx);
    nl_recvmsgs_default(sock);

    nlmsg_free(msg);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <interface>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // 初始化上下文
    TrafficContext ctx = {
        .last_rx_bytes = 0,
        .last_tx_bytes = 0,
        .last_update = 0
    };
    strncpy(ctx.ifname, argv[1], IF_NAMESIZE-1);

    // 创建 Netlink 套接字
    struct nl_sock *sock = nl_socket_alloc();
    if (!sock || nl_connect(sock, NETLINK_ROUTE) < 0) {
        fprintf(stderr, "Failed to initialize socket\n");
        nl_socket_free(sock);
        return EXIT_FAILURE;
    }

    signal(SIGINT, sigint_handler);

    // 主循环
    while (keep_running) {
        fetch_stats(sock, &ctx);
        sleep(INTERVAL_SEC);
    }

    // 清理资源
    nl_socket_free(sock);
    printf("\nMonitoring stopped.\n");
    return EXIT_SUCCESS;
}