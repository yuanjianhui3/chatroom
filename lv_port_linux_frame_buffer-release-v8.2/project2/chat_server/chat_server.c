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

#include <errno.h>

#define SERVER_PORT 8888    // 服务器端口（需与客户端一致）
#define MAX_CLIENT 10       // 最大连接数
#define MAX_USER 100        // 最大注册用户数

// ========== 类型定义 ==========
// 协议类型（与客户端一致，区分不同请求/响应）
typedef enum {
    MSG_REGISTER = 1,    // 注册请求
    MSG_LOGIN,           // 登录请求
    MSG_ACK,             // 服务器应答（1成功/0失败）
    MSG_GET_ONLINE_USER, // 获取在线用户列表
    MSG_USER_LIST,       // 在线用户列表数据
    MSG_SEND_MSG,        // 发送聊天消息
    MSG_ADD_FRIEND,      // 添加好友
    MSG_SET_SIGNATURE    // 设置个性签名
} MsgType;

// 用户信息结构体（注册/登录/在线用户共用）
typedef struct {
    char account[32];    // 账号（唯一）
    char password[32];   // 密码
    char nickname[32];   // 昵称
    char ip[16];         // IP地址
    int port;            // 端口号
    char signature[64];  // 个性签名
    char avatar[64];     // 头像路径（开发板本地路径）

    int online;          // 20250927新增：是否在线（1=在线，0=离线）
    char friends[20][32];// 好友账号列表
    int friend_cnt;      // 好友数量

} UserInfo;

// 网络消息结构体（统一传输格式）
typedef struct {
    MsgType type;        // 消息类型
    UserInfo user;       // 用户信息
    char content[256];   // 附加内容（消息/提示）
} NetMsg;
// ========== 类型定义结束 ==========

// 用户信息存储（注册用户）
typedef struct {
    char account[32];    // 账号（唯一）
    char password[32];   // 密码
    char nickname[32];   // 昵称
    char signature[64];  // 个性签名
    char avatar[64];     // 头像路径
    int online; // 是否在线（1=在线，0=离线）

    char friends[20][32];// 20250927新增：好友账号列表
    int friend_cnt;      // 好友数量
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
static RegUser *Find_Reg_User(const char *account) {
    for(int i=0; i<reg_user_count; i++) {
        if(strcmp(reg_users[i].account, account) == 0) {
            return &reg_users[i];
        }
    }
    return NULL;
}

// 查找在线客户端（通过账号）
static ClientInfo *Find_Online_Client(const char *account) {
    for(int i=0; i<client_count; i++) {
        if(strcmp(clients[i].user.account, account) == 0) {
            return &clients[i];
        }
    }
    return NULL;
}

// 生成在线用户列表字符串（格式：账号:昵称:签名|...）
static void Get_Online_User_Str(char *buf, int buf_len) {
    buf[0] = '\0';
    for(int i=0; i<client_count; i++) {
        if(clients[i].user.online) {
            char temp[128];
            snprintf(temp, 128, "%s:%s:%s|", clients[i].user.account, clients[i].user.nickname,clients[i].user.signature);
            strncat(buf, temp, buf_len - strlen(buf) - 1);
        }
    }
    // 移除最后一个'|'
    if(strlen(buf) > 0) {
        buf[strlen(buf)-1] = '\0';
    }
}

// 广播消息给所有在线客户端
static void Broadcast_Msg(NetMsg *msg, int exclude_fd) {
    for(int i=0; i<client_count; i++) {
        if(clients[i].sockfd != exclude_fd && clients[i].user.online) {
            send(clients[i].sockfd, msg, sizeof(NetMsg), 0);
        }
    }
}

// -----------20250927新增：发送ACK应答---------------------------
static void Send_ACK(int sockfd, const char *type, int result) {
    NetMsg ack;
    memset(&ack, 0, sizeof(ack));
    ack.type = MSG_ACK;
    strncpy(ack.content, type, 255);
    ack.user.port = result; // 1成功，0失败
    send(sockfd, &ack, sizeof(ack), 0);
}

// -------------------------- 20250927新增：消息处理函数 -------------
static void Handle_Register(NetMsg *msg, ClientInfo *client) {
    // 检查账号是否已存在
    if(Find_Reg_User(msg->user.account) != NULL) {
        Send_ACK(client->sockfd, "register", 0); // 失败ACK
        return;
    }
    // 保存注册用户
    if(reg_user_count < MAX_USER) {
        RegUser new_user;
        memset(&new_user, 0, sizeof(new_user));
        strncpy(new_user.account, msg->user.account, 31);
        strncpy(new_user.password, msg->user.password, 31);
        strncpy(new_user.nickname, msg->user.nickname, 31);
        strncpy(new_user.signature, msg->user.signature, 63);
        strncpy(new_user.avatar, msg->user.avatar, 63);
        new_user.online = 0;
        new_user.friend_cnt = 0;
        reg_users[reg_user_count++] = new_user;
        Send_ACK(client->sockfd, "register", 1); // 成功ACK
        printf("注册成功：%s\n", msg->user.account);
    } else {
        Send_ACK(client->sockfd, "register", 0);
    }
}

static void Handle_Login(NetMsg *msg, ClientInfo *client) {
    // 查找注册用户
    RegUser *reg_user = Find_Reg_User(msg->user.account);
    if(!reg_user) {
        Send_ACK(client->sockfd, "login", 0);
        return;
    }
    // 验证密码
    if(strcmp(reg_user->password, msg->user.password) != 0) {
        Send_ACK(client->sockfd, "login", 0);
        return;
    }
    // 标记在线
    client->user = *reg_user;
    client->user.online = 1;
    Send_ACK(client->sockfd, "login", 1); // 成功ACK
    printf("登录成功：%s\n", msg->user.account);
}

static void Handle_Get_Online_User(ClientInfo *client) {
    NetMsg user_msg;
    memset(&user_msg, 0, sizeof(user_msg));
    user_msg.type = MSG_USER_LIST;
    Get_Online_User_Str(user_msg.content, 256);
    send(client->sockfd, &user_msg, sizeof(user_msg), 0);
}

static void Handle_Add_Friend(NetMsg *msg, ClientInfo *client) {
    // 查找目标用户
    RegUser *target = Find_Reg_User(msg->content);
    if(!target) {
        Send_ACK(client->sockfd, "add_friend", 0);
        return;
    }
    // 检查是否已为好友
    for(int i=0; i<client->user.friend_cnt; i++) {
        if(strcmp(client->user.friends[i], target->account) == 0) {
            Send_ACK(client->sockfd, "add_friend", 0);
            return;
        }
    }
    // 添加好友
    strncpy(client->user.friends[client->user.friend_cnt++], target->account, 31);
    // 更新注册用户的好友列表
    RegUser *reg_user = Find_Reg_User(client->user.account);
    *reg_user = client->user;
    Send_ACK(client->sockfd, "add_friend", 1);
    printf("添加好友：%s→%s\n", client->user.account, target->account);
}

static void Handle_Set_Signature(NetMsg *msg, ClientInfo *client) {
    // 更新签名
    strncpy(client->user.signature, msg->user.signature, 63);
    // 更新注册用户
    RegUser *reg_user = Find_Reg_User(client->user.account);
    *reg_user = client->user;
    printf("更新签名：%s→%s\n", client->user.account, msg->user.signature);
}
// -------------------------------------------------

// -------------------------- 客户端处理线程 --------------------------
static void *Handle_Client(void *arg) {
    ClientInfo *client = (ClientInfo *)arg;
    NetMsg msg;
    int ret;

    printf("new client connected: %s:%d\n", 
           inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));//新客户连接

    while(1) {
        memset(&msg, 0, sizeof(msg));
        ret = recv(client->sockfd, &msg, sizeof(NetMsg), 0);
        if(ret <= 0) {
            printf("client disconnected: %s:%d\n", 
                   inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));//客户断开连接
            break;
        }

        pthread_mutex_lock(&data_mutex);
        switch(msg.type) {
            case MSG_REGISTER: Handle_Register(&msg, client); break;            //调用注册 消息处理函数
            case MSG_LOGIN: Handle_Login(&msg, client); break;                  //调用登录 消息处理函数
            case MSG_GET_ONLINE_USER: Handle_Get_Online_User(client); break;    //调用在线用户 消息处理函数
            case MSG_SEND_MSG: Broadcast_Msg(&msg, client->sockfd);break;       //调用广播聊天 消息处理函数
            case MSG_ADD_FRIEND: Handle_Add_Friend(&msg, client); break;        //调用添加好友 消息处理函数
            case MSG_SET_SIGNATURE: Handle_Set_Signature(&msg, client); break;  //调用个性签名 消息处理函数
            default:
                printf("Unknown message type: %d\n", msg.type);                 //未知消息类型
                break;
        }
        pthread_mutex_unlock(&data_mutex);
    }

    // 客户端断开，标记离线
    pthread_mutex_lock(&data_mutex);
    client->user.online = 0;
    pthread_mutex_unlock(&data_mutex);

    // 关闭socket，清理资源
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
    printf("server start, listen port %d...\n", SERVER_PORT);   //服务器启动，监听端口

    pthread_mutex_init(&data_mutex, NULL);    // 初始化互斥锁

    // 循环接收客户端连接
    while(1) {
        if(client_count >= MAX_CLIENT) {
            printf("max client reached, wait...\n"); //达到最大连接数，等待...
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
        pthread_create(&tid, NULL, Handle_Client, client);
        pthread_detach(tid); // 分离线程，自动释放资源
        client_count++;
    }

    // 清理（实际不会执行到）
    pthread_mutex_destroy(&data_mutex);
    close(listen_fd);
    return 0;
}