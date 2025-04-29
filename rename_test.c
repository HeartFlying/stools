#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// 针对已经存在的文件内容进行原子操作修订，避免修订过程中异常导致服务无法正常启动
#define TARGET_FILE "./interfaces"
#define TEMPLATE    "./interfaces.XXXXXX"

int main() {
    // 创建临时文件路径并生成唯一文件名
    char temp_path[sizeof(TEMPLATE)];
    strcpy(temp_path, TEMPLATE);
    int fd = mkstemp(temp_path);
    if (fd == -1) {
        perror("mkstemp failed");
        exit(EXIT_FAILURE);
    }

    // 获取原文件属性以保留权限
    struct stat orig_st;
    if (stat(TARGET_FILE, &orig_st) != 0) {
        perror("stat failed - target file may not exist");
        close(fd);
        unlink(temp_path);
        exit(EXIT_FAILURE);
    }

    // 设置临时文件权限与原文件一致
    if (fchmod(fd, orig_st.st_mode) == -1) {
        perror("fchmod failed");
        close(fd);
        unlink(temp_path);
        exit(EXIT_FAILURE);
    }

    // 示例配置内容（实际应用中可替换为动态生成的内容）
    const char *config_content = 
        "auto eth0\n"
        "iface eth0 inet static\n"
        "address 192.168.1.100\n"
        "netmask 255.255.255.0\n";

    // 循环写入确保所有数据写入临时文件
    const char *p = config_content;
    size_t remaining = strlen(config_content);
    while (remaining > 0) {
        ssize_t bytes_written = write(fd, p, remaining);
        if (bytes_written == -1) {
            if (errno == EINTR) continue; // 被信号中断则重试
            perror("write failed");
            close(fd);
            unlink(temp_path);
            exit(EXIT_FAILURE);
        }
        p += bytes_written;
        remaining -= bytes_written;
    }

    // 确保数据刷入磁盘
    if (fsync(fd) == -1) {
        perror("fsync failed");
        close(fd);
        unlink(temp_path);
        exit(EXIT_FAILURE);
    }
    close(fd); // 关闭文件描述符以便后续重命名操作

    // 原子替换操作
    if (rename(temp_path, TARGET_FILE) == -1) {
        perror("rename failed");
        unlink(temp_path);
        exit(EXIT_FAILURE);
    }

    printf("Network configuration updated atomically.\n");
    return EXIT_SUCCESS;
}

