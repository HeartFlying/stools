#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <netlink/route/route.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

// 依赖第三方工具 apt install -y libnl-3-dev libnl-route-3-dev libnl-genl-3-dev
static struct nl_cache *link_cache = NULL;

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <netlink/route/route.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * 添加IP地址、子网掩码、默认网关并配置DNS
 * @param sk         Netlink套接字
 * @param ifname     接口名（如eth0）
 * @param ip_cidr    IP地址/CIDR（如192.168.1.10/24）
 * @param gateway    默认网关（如192.168.1.1，NULL表示不配置）
 * @param dns_servers DNS服务器数组（如{"8.8.8.8", "1.1.1.1"}）
 * @param dns_count  DNS服务器数量
 * @return 成功返回0，失败返回-1
 */
static int configure_network(
    struct nl_sock *sk, 
    const char *ifname,
    const char *ip_cidr,
    const char *gateway,
    const char **dns_servers,
    int dns_count
) {
    // 获取接口对象
    struct rtnl_link *link = rtnl_link_get_by_name(link_cache, ifname);
    if (!link) {
        fprintf(stderr, "接口 %s 不存在\n", ifname);
        return -1;
    }

    //======== 1. 配置IP地址和子网掩码 ========
    struct rtnl_addr *addr = rtnl_addr_alloc();
    struct nl_addr *local_addr;

    // 解析IP/CIDR（自动提取子网掩码）
    int ret = nl_addr_parse(ip_cidr, AF_INET, &local_addr);
    if (ret < 0) {
        fprintf(stderr, "无效的IP地址格式: %s\n", ip_cidr);
        goto cleanup_link;
    }

    // 设置地址属性
    rtnl_addr_set_local(addr, local_addr);
    rtnl_addr_set_link(addr, link);
    rtnl_addr_set_prefixlen(addr, nl_addr_get_prefixlen(local_addr)); // 显式设置子网掩码

    // 提交到内核
    if ((ret = rtnl_addr_add(sk, addr, 0)) < 0) {
        fprintf(stderr, "添加IP地址失败: %s\n", nl_geterror(ret));
        goto cleanup_addr;
    }

    //======== 2. 配置默认网关 ========
    if (gateway) {
        struct rtnl_route *route = rtnl_route_alloc();
        struct nl_addr *dst_addr, *gw_addr;

        // 目标网络：0.0.0.0/0
        nl_addr_parse("0.0.0.0/0", AF_INET, &dst_addr);
        rtnl_route_set_dst(route, dst_addr);

        // 下一跳网关
        nl_addr_parse(gateway, AF_INET, &gw_addr);
        struct rtnl_nexthop *nh = rtnl_route_nh_alloc();
        rtnl_route_nh_set_gateway(nh, gw_addr);
        rtnl_route_nh_set_ifindex(nh, rtnl_link_get_ifindex(link)); // 绑定出口接口
        rtnl_route_add_nexthop(route, nh);

        // 提交路由
        if ((ret = rtnl_route_add(sk, route, 0)) < 0) {
            fprintf(stderr, "添加默认网关失败: %s\n", nl_geterror(ret));
            goto cleanup_route;
        }

        // 释放资源
cleanup_route:
        rtnl_route_put(route);
        nl_addr_put(dst_addr);
        nl_addr_put(gw_addr);
        rtnl_route_nh_free(nh);
        if (ret < 0) goto cleanup_addr;
    }

    //======== 3. 配置DNS服务器 ========
    if (dns_servers && dns_count > 0) {
        FILE *fp = fopen("/etc/resolv.conf", "a"); // 追加模式
        if (!fp) {
            perror("无法配置DNS（需要root权限）");
            ret = -1;
            goto cleanup_addr;
        }

        // 写入DNS服务器
        for (int i = 0; i < dns_count; i++) {
            fprintf(fp, "nameserver %s\n", dns_servers[i]);
        }

        fclose(fp);
    }

    //======== 清理资源 ========
cleanup_addr:
    nl_addr_put(local_addr);
    rtnl_addr_put(addr);
cleanup_link:
    rtnl_link_put(link);
    return ret;
}

// 添加IP地址
static int add_ip_address(struct nl_sock *sk, const char *ifname, 
                         const char *ip_cidr) {
    struct rtnl_link *link = rtnl_link_get_by_name(link_cache, ifname);
    if (!link) {
        fprintf(stderr, "接口 %s 不存在\n", ifname);
        return -1;
    }

    struct rtnl_addr *addr = rtnl_addr_alloc();
    struct nl_addr *local_addr;
    
    // 解析IP/CIDR
    int ret = nl_addr_parse(ip_cidr, AF_UNSPEC, &local_addr);
    if (ret < 0) {
        fprintf(stderr, "无效的IP地址格式: %s\n", ip_cidr);
        return -1;
    }
    
    rtnl_addr_set_local(addr, local_addr);
    rtnl_addr_set_link(addr, link);
    
    ret = rtnl_addr_add(sk, addr, 0);
    nl_addr_put(local_addr);
    rtnl_link_put(link);
    
    return ret;
}

// 删除IP地址
static int del_ip_address(struct nl_sock *sk, const char *ifname, 
                         const char *ip) {
    struct rtnl_addr *addr = rtnl_addr_alloc();
    struct nl_addr *local_addr;
    
    nl_addr_parse(ip, AF_UNSPEC, &local_addr);
    rtnl_addr_set_local(addr, local_addr);
    
    struct rtnl_link *link = rtnl_link_get_by_name(link_cache, ifname);
    rtnl_addr_set_link(addr, link);
    
    int ret = rtnl_addr_delete(sk, addr, 0);
    nl_addr_put(local_addr);
    rtnl_link_put(link);
    
    return ret;
}

// 正确设置网关的代码
// static int set_default_gateway(struct nl_sock *sk, const char *gw, const char *ifname) {
//     struct rtnl_route *route = rtnl_route_alloc();
//     struct rtnl_nexthop *nh = rtnl_route_nexthop_alloc();
    
//     // 设置路由属性
//     rtnl_route_set_family(route, AF_INET);
//     rtnl_route_set_type(route, RTN_UNICAST);
//     rtnl_route_set_scope(route, RT_SCOPE_UNIVERSE);
//     rtnl_route_set_table(route, RT_TABLE_MAIN);
    
//     // 设置网关地址
//     struct nl_addr *gateway;
//     nl_addr_parse(gw, AF_INET, &gateway);
//     rtnl_route_nexthop_set_gateway(nh, gateway);
    
//     // 设置出口设备
//     struct rtnl_link *link = rtnl_link_get_by_name(link_cache, ifname);
//     rtnl_route_nexthop_set_ifindex(nh, rtnl_link_get_ifindex(link));
    
//     // 添加下一跳
//     rtnl_route_add_nexthop(route, nh);
    
//     int ret = rtnl_route_add(sk, route, 0);
    
//     // 释放资源
//     rtnl_link_put(link);
//     nl_addr_put(gateway);
//     rtnl_route_nexthop_put(nh);
//     rtnl_route_put(route);
    
//     return ret;
// }


// 命令行帮助
static void print_help() {
    puts("可用命令:");
    puts("  show [ifname]            - 显示接口信息");
    puts("  add ip <ifname> <ip/cidr> - 添加IP地址");
    puts("  del ip <ifname> <ip>      - 删除IP地址");
    //puts("  route add default via <gw> dev <ifname> - 添加默认路由");
    puts("  exit                     - 退出程序");
}

// 主交互循环
int main() {
    // 初始化netlink socket
    struct nl_sock *sk = nl_socket_alloc();
    // 连接到路由套接字
    nl_connect(sk, NETLINK_ROUTE);
    // 获取接口缓存
    rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache);

    char cmd[256];
    while (1) {
        printf("netcfg> ");
        fgets(cmd, sizeof(cmd), stdin);
        
        // 解析命令
        char *argv[6];
        int argc = 0;
        char *token = strtok(cmd, " \n");
        while (token && argc < 5) {
            argv[argc++] = token;
            token = strtok(NULL, " \n");
        }
        
        if (argc == 0) continue;
        
        if (strcmp(argv[0], "exit") == 0) {
            break;
        } else if (strcmp(argv[0], "show") == 0) {
            // 显示接口信息的实现（参考之前demo）
        } else if (strcmp(argv[0], "add") == 0 && argc >=4) {
            if (strcmp(argv[1], "ip") == 0) {
                if (add_ip_address(sk, argv[2], argv[3]) == 0) {
                    printf("成功添加地址: %s\n", argv[3]);
                }
            }
        } else if (strcmp(argv[0], "del") == 0 && argc >=4) {
            if (strcmp(argv[1], "ip") == 0) {
                if (del_ip_address(sk, argv[2], argv[3]) == 0) {
                    printf("成功删除地址: %s\n", argv[3]);
                }
            }
        // } else if (strcmp(argv[0], "route") == 0 && argc >=6) {
        //     if (strcmp(argv[1], "add") == 0 && 
        //         strcmp(argv[2], "default") == 0 &&
        //         strcmp(argv[3], "via") == 0 &&
        //         strcmp(argv[5], "dev") == 0) {
        //         if (set_default_gateway(sk, argv[4], argv[6]) == 0) {
        //             printf("默认网关已设置: %s\n", argv[4]);
        //         }
        //     }
        } else {
            print_help();
        }
    }

    nl_cache_free(link_cache);
    nl_socket_free(sk);
    return 0;
}
