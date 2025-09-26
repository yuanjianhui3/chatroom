//@file chat_server.c 云服务器端

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

#define SERVER_PORT 8888    // 服务器端口（需与客户端一致）
#define MAX_CLIENT 10       // 最大连接数
#define MAX_USER 100        // 最大注册用户数

// 用户信息存储（注册用户）
typedef struct {
    char account[32];
    char password[32];
    char nickname[32];
    char signature[64];
    char avatar[64];
    int online; // 是否在线（1=在线，0=离线）
} RegUser;

// 客户端连接信息
typedef struct {
    int sockfd;
    struct sockaddr_in addr;
    RegUser user; // 对应的注册用户
} ClientInfo;

// 全局数据（需互斥锁保护）
ClientInfo clients[MAX_CLIENT];  // 客户端列表
RegUser reg_users[MAX_USER];     // 注册用户列表
int client_count = 0;            // 当前连接数
int reg_user_count = 0;          // 注册用户数
pthread_mutex_t data_mutex;      // 数据保护互斥锁

// -------------------------- 工具函数 --------------------------
// 查找注册用户（通过账号）
static RegUser *find_reg_user(const char *account) {
    for(int i=0; i<reg_user_count; i++) {
        if(strcmp(reg_users[i].account, account) == 0) {
            return &reg_users[i];
        }
    }
    return NULL;
}

// 查找在线客户端（通过账号）
static ClientInfo *find_online_client(const char *account) {
    for(int i=0; i<client_count; i++) {
        if(strcmp(clients[i].user.account, account) == 0) {
            return &clients[i];
        }
    }
    return NULL;
}

// 生成在线用户列表字符串（格式：账号:昵称|账号:昵称|...）
static void get_online_user_str(char *buf, int buf_len) {
    buf[0] = '\0';
    for(int i=0; i<client_count; i++) {
        if(clients[i].user.online) {
            char temp[64];
            snprintf(temp, 64, "%s:%s|", clients[i].user.account, clients[i].user.nickname);
            strncat(buf, temp, buf_len - strlen(buf) - 1);
        }
    }
    // 移除最后一个'|'
    if(strlen(buf) > 0) {
        buf[strlen(buf)-1] = '\0';
    }
}

// 广播消息给所有在线客户端
static void broadcast_msg(NetMsg *msg, int exclude_fd) {
    for(int i=0; i<client_count; i++) {
        if(clients[i].sockfd != exclude_fd && clients[i].user.online) {
            send(clients[i].sockfd, msg, sizeof(NetMsg), 0);
        }
    }
}

// -------------------------- 客户端处理线程 --------------------------
static void *handle_client(void *arg) {
    ClientInfo *client = (ClientInfo *)arg;
    NetMsg msg;
    int ret;

    printf("new client connected: %s:%d\n", 
           inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));

    while(1) {
        memset(&msg, 0, sizeof(msg));
        ret = recv(client->sockfd, &msg, sizeof(NetMsg), 0);
        if(ret <= 0) {
            printf("client disconnected: %s:%d\n", 
                   inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));
            break;
        }

        pthread_mutex_lock(&data_mutex);
        switch(msg.type) {
            case MSG_REGISTER: {
                // 处理注册请求
                NetMsg ack_msg = {.type = MSG_ACK, .user.port = 0}; // 默认失败
                strncpy(ack_msg.content, "register", 15);

                // 检查账号是否已存在
                if(find_reg_user(msg.user.account) != NULL) {
                    send(client->sockfd, &ack_msg, sizeof(NetMsg), 0);
                    pthread_mutex_unlock(&data_mutex);
                    break;
                }

                // 保存注册用户
                if(reg_user_count < MAX_USER) {
                    RegUser new_user;
                    memset(&new_user, 0, sizeof(new_user));
                    strncpy(new_user.account, msg.user.account, 31);
                    strncpy(new_user.password, msg.user.password, 31);
                    strncpy(new_user.nickname, msg.user.nickname, 31);
                    strncpy(new_user.signature, "默认签名", 15); // 默认签名
                    strncpy(new_user.avatar, "default_avatar.png", 20); // 默认头像
                    new_user.online = 0;
                    reg_users[reg_user_count++] = new_user;
                    ack_msg.user.port = 1; // 注册成功
                }

                send(client->sockfd, &ack_msg, sizeof(NetMsg), 0);
                break;
            }
            case MSG_LOGIN: {
                // 处理登录请求
                NetMsg ack_msg = {.type = MSG_ACK, .user.port = 0}; // 默认失败
                strncpy(ack_msg.content, "login", 15);

                // 查找注册用户
                RegUser *reg_user = find_reg_user(msg.user.account);
                if(!reg_user) {
                    send(client->sockfd, &ack_msg, sizeof(NetMsg), 0);
                    pthread_mutex_unlock(&data_mutex);
                    break;
                }

                // 验证密码
                if(strcmp(reg_user->password, msg.user.password) != 0) {
                    send(client->sockfd, &ack_msg, sizeof(NetMsg), 0);
                    pthread_mutex_unlock(&data_mutex);
                    break;
                }

                // 标记用户在线
                client->user = *reg_user;
                client->user.online = 1;
                ack_msg.user.port = 1; // 登录成功
                strncpy(ack_msg.user.account, reg_user->account, 31);

                send(client->sockfd, &ack_msg, sizeof(NetMsg), 0);
                break;
            }
            case MSG_GET_ONLINE_USER: {
                // 发送在线用户列表
                NetMsg user_msg = {.type = MSG_USER_LIST};
                get_online_user_str(user_msg.content, 256);
                send(client->sockfd, &user_msg, sizeof(NetMsg), 0);
                break;
            }
            case MSG_SEND_MSG: {
                // 广播聊天消息
                broadcast_msg(&msg, client->sockfd);
                break;
            }
        }
        pthread_mutex_unlock(&data_mutex);
    }

    // 客户端断开，标记离线
    pthread_mutex_lock(&data_mutex);
    client->user.online = 0;
    pthread_mutex_unlock(&data_mutex);

    // 关闭socket
    close(client->sockfd);
    free(client);
    client_count--;
    return NULL;
}

// -------------------------- 服务器初始化与运行 --------------------------
int main() {
    // 创建socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0) {
        perror("socket create failed");
        exit(1);
    }

    // 设置端口复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有网卡
    server_addr.sin_port = htons(SERVER_PORT);
    if(bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(listen_fd);
        exit(1);
    }

    // 监听
    if(listen(listen_fd, 5) < 0) {
        perror("listen failed");
        close(listen_fd);
        exit(1);
    }
    printf("server start, listen port %d...\n", SERVER_PORT);

    // 初始化互斥锁
    pthread_mutex_init(&data_mutex, NULL);

    // 循环接收客户端连接
    while(1) {
        if(client_count >= MAX_CLIENT) {
            printf("max client reached, wait...\n");
            sleep(1);
            continue;
        }

        // 接受连接
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if(conn_fd < 0) {
            perror("accept failed");
            continue;
        }

        // 分配客户端信息
        ClientInfo *client = (ClientInfo *)malloc(sizeof(ClientInfo));
        memset(client, 0, sizeof(ClientInfo));
        client->sockfd = conn_fd;
        client->addr = client_addr;

        // 创建线程处理客户端
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client);
        pthread_detach(tid); // 分离线程，自动释放资源
        client_count++;
    }

    // 清理（实际不会执行到）
    pthread_mutex_destroy(&data_mutex);
    close(listen_fd);
    return 0;
}