#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <netlink/route/route.h>
#include <arpa/inet.h>
#include <net/if.h>  // 新增关键头文件

/*
* source： https://github.com/thom311/libnl
* https://www.netfilter.org/projects/libmnl/
* libnetfilter (Netfilter 子项目库)
https://github.com/linux-audit/audit-userspace
* 1. 设置环境变量 NLCB=debug ./myprogram 将启用调试消息处理程序
* 2. NLDBG=[0..4] ./myprogram
*/
// 打印接口的IP地址信息（保持不变）
// 打印接口的IP地址信息
void print_addresses(struct rtnl_link *link) {
    struct nl_sock *sock = nl_socket_alloc();
    nl_connect(sock, NETLINK_ROUTE);
    
    struct nl_cache *addr_cache;
    rtnl_addr_alloc_cache(sock, &addr_cache);

    printf("IP Addresses:\n");
    
    // 遍历所有地址缓存
    struct rtnl_addr *addr = (struct rtnl_addr*) nl_cache_get_first(addr_cache);
    for (; addr; addr = (struct rtnl_addr*) nl_cache_get_next(addr)) {
        if (rtnl_addr_get_ifindex(addr) == rtnl_link_get_ifindex(link)) {
            struct nl_addr *local = rtnl_addr_get_local(addr);
            char buf[INET6_ADDRSTRLEN];
            
            if (nl_addr_get_family(local) == AF_INET) {
                inet_ntop(AF_INET, nl_addr_get_binary_addr(local), 
                         buf, sizeof(buf));
                printf("  IPv4: %s/%d\n", buf, nl_addr_get_prefixlen(local));
            } else if (nl_addr_get_family(local) == AF_INET6) {
                inet_ntop(AF_INET6, nl_addr_get_binary_addr(local), 
                         buf, sizeof(buf));
                printf("  IPv6: %s/%d\n", buf, nl_addr_get_prefixlen(local));
            }
        }
    }
    
    nl_cache_free(addr_cache);
    nl_socket_free(sock);
}

int main() {
    struct nl_sock *sock = nl_socket_alloc();
    nl_connect(sock, NETLINK_ROUTE);
    
    struct nl_cache *link_cache;
    rtnl_link_alloc_cache(sock, AF_UNSPEC, &link_cache);

    printf("%-10s %-17s %-6s %-18s %s\n", 
          "Interface", "MAC Address", "MTU", "State", "Flags");

    struct rtnl_link *link = (struct rtnl_link*) nl_cache_get_first(link_cache);
    for (; link; link = (struct rtnl_link*) nl_cache_get_next(link)) {
        const char *name = rtnl_link_get_name(link);
        int mtu = rtnl_link_get_mtu(link);
        unsigned flags = rtnl_link_get_flags(link);

        // 修正后的MAC地址处理
        struct nl_addr *mac = rtnl_link_get_addr(link);
        char mac_buf[18];
        const char *mac_str;
        if (mac) {
            nl_addr2str(mac, mac_buf, sizeof(mac_buf));
            mac_str = mac_buf;
        } else {
            mac_str = "N/A";
        }

        // 修正后的状态判断
        const char *state = (flags & IFF_UP) ? "UP" : "DOWN";
        
        printf("%-10s %-17s %-6d %-18s 0x%x\n", 
              name, mac_str, mtu, state, flags);

        print_addresses(link);
        printf("\n");
    }

    nl_cache_free(link_cache);
    nl_socket_free(sock);
    return 0;
}