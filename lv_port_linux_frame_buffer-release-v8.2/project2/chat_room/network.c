//@file network.c 网络通信实现 

#include "network.h"
#include <errno.h>

static int g_tcp_fd = -1;
static int g_udp_fd = -1;
static struct sockaddr_in g_server_addr;

/**
 * 初始化TCP连接
 */
static int Network_Init_TCP(void)
{
    // 创建TCP套接字
    g_tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_tcp_fd == -1) {
        perror("socket error");
        return -1;
    }

    // 设置服务器地址
    memset(&g_server_addr, 0, sizeof(g_server_addr));
    g_server_addr.sin_family = AF_INET;
    g_server_addr.sin_port = htons(HUAWEI_CLOUD_SERVER_PORT);
    
    // 转换IP地址
    if (inet_pton(AF_INET, HUAWEI_CLOUD_SERVER_IP, &g_server_addr.sin_addr) <= 0) {
        perror("inet_pton error");
        close(g_tcp_fd);
        g_tcp_fd = -1;
        return -1;
    }

    // 连接服务器
    if (connect(g_tcp_fd, (struct sockaddr*)&g_server_addr, sizeof(g_server_addr)) == -1) {
        perror("connect error");
        close(g_tcp_fd);
        g_tcp_fd = -1;
        return -1;
    }

    return 0;
}

/**
 * 初始化UDP
 */
static int Network_Init_UDP(void)
{
    // 创建UDP套接字
    g_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_udp_fd == -1) {
        perror("udp socket error");
        return -1;
    }

    // 设置本地地址（随机端口）
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(0);  // 系统自动分配端口

    if (bind(g_udp_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) == -1) {
        perror("udp bind error");
        close(g_udp_fd);
        g_udp_fd = -1;
        return -1;
    }

    return 0;
}

/**
 * 初始化网络连接
 */
int Network_Init(void)
{
    // 先关闭可能存在的连接
    Network_Close();

    // 初始化TCP
    if (Network_Init_TCP() != 0) {
        return -1;
    }

    // 初始化UDP
    if (Network_Init_UDP() != 0) {
        Network_Close();
        return -1;
    }

    return 0;
}

/**
 * 发送数据
 */
int Network_Send_Data(ChatData_t *data)
{
    if (g_tcp_fd == -1 || !data) return -1;
    
    int ret = send(g_tcp_fd, data, sizeof(ChatData_t), 0);
    if (ret <= 0) {
        perror("send error");
        return -1;
    }
    return ret;
}

/**
 * 接收数据
 */
int Network_Recv_Data(ChatData_t *data)
{
    if (g_tcp_fd == -1 || !data) return -1;
    
    int ret = recv(g_tcp_fd, data, sizeof(ChatData_t), 0);
    if (ret <= 0) {
        // 如果是连接被重置，返回-1
        if (errno == ECONNRESET) {
            return -1;
        }
        // 其他错误也返回-1
        return -1;
    }
    return ret;
}

/**
 * 发送UDP数据
 */
int Network_Send_UDP(ChatData_t *data, struct sockaddr_in *to)
{
    if (g_udp_fd == -1 || !data || !to) return -1;
    
    int ret = sendto(g_udp_fd, data, sizeof(ChatData_t), 0, 
                    (struct sockaddr*)to, sizeof(struct sockaddr_in));
    if (ret <= 0) {
        perror("udp send error");
        return -1;
    }
    return ret;
}

/**
 * 接收UDP数据
 */
int Network_Recv_UDP(ChatData_t *data, struct sockaddr_in *from)
{
    if (g_udp_fd == -1 || !data || !from) return -1;
    
    socklen_t len = sizeof(struct sockaddr_in);
    int ret = recvfrom(g_udp_fd, data, sizeof(ChatData_t), 0,
                      (struct sockaddr*)from, &len);
    if (ret <= 0) {
        perror("udp recv error");
        return -1;
    }
    return ret;
}

/**
 * 关闭网络连接
 */
void Network_Close(void)
{
    if (g_tcp_fd != -1) {
        close(g_tcp_fd);
        g_tcp_fd = -1;
    }
    if (g_udp_fd != -1) {
        close(g_udp_fd);
        g_udp_fd = -1;
    }
}
