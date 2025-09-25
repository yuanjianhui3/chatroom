//@file chat_server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8888
#define MAX_USERS 50
#define MAX_EVENTS 10

// 用户信息结构体
typedef struct {
    char account[64];          // 账号
    char passwd[64];           // 密码
    char nickname[64];         // 昵称
    char signature[256];       // 个性签名
    char avatar[256];          // 头像路径
    struct sockaddr_in addr;   // IP地址和端口
    int fd;                    // 套接字句柄
    int online;                // 在线状态 1:在线 0:离线
    time_t last_active;        // 最后活动时间
} UserInfo_t;

// 消息结构体
typedef struct {
    int cmd;                   // 命令
    UserInfo_t user;           // 发送者信息
    char msg[1024];            // 消息内容
    char target_account[64];   // 目标账号
    UserInfo_t online_users[MAX_USERS];// 在线用户列表
    int user_count;            // 用户数量
} ChatData_t;

// 全局用户列表
static UserInfo_t g_users[MAX_USERS];
static int g_user_count = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * 初始化用户列表（从文件加载）
 */
static void Init_Users(void)
{
    // 实际应用中应该从文件或数据库加载用户信息
    // 这里只是初始化一些测试用户
    pthread_mutex_lock(&g_mutex);
    
    // 测试用户1
    strcpy(g_users[g_user_count].account, "user1");
    strcpy(g_users[g_user_count].passwd, "123456");
    strcpy(g_users[g_user_count].nickname, "用户1");
    strcpy(g_users[g_user_count].signature, "我是用户1");
    strcpy(g_users[g_user_count].avatar, "default.png");
    g_users[g_user_count].online = 0;
    g_user_count++;
    
    // 测试用户2
    strcpy(g_users[g_user_count].account, "user2");
    strcpy(g_users[g_user_count].passwd, "123456");
    strcpy(g_users[g_user_count].nickname, "用户2");
    strcpy(g_users[g_user_count].signature, "我是用户2");
    strcpy(g_users[g_user_count].avatar, "default.png");
    g_users[g_user_count].online = 0;
    g_user_count++;
    
    pthread_mutex_unlock(&g_mutex);
}

/**
 * 保存用户列表到文件
 */
static void Save_Users(void)
{
    // 实际应用中应该将用户信息保存到文件或数据库
    FILE *fp = fopen("users.dat", "wb");
    if (fp) {
        pthread_mutex_lock(&g_mutex);
        fwrite(&g_user_count, sizeof(int), 1, fp);
        fwrite(g_users, sizeof(UserInfo_t), g_user_count, fp);
        pthread_mutex_unlock(&g_mutex);
        fclose(fp);
    }
}

/**
 * 查找用户
 * @param account 账号
 * @return 用户索引，-1表示未找到
 */
static int Find_User(const char *account)
{
    if (!account) return -1;
    
    pthread_mutex_lock(&g_mutex);
    for (int i = 0; i < g_user_count; i++) {
        if (strcmp(g_users[i].account, account) == 0) {
            pthread_mutex_unlock(&g_mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&g_mutex);
    return -1;
}

/**
 * 注册新用户
 */
static int Handle_Register(ChatData_t *recv_data, ChatData_t *send_data)
{
    if (!recv_data || !send_data) return -1;
    
    // 检查用户是否已存在
    if (Find_User(recv_data->user.account) != -1) {
        send_data->cmd = CMD_REPLY_FAIL;
        strcpy(send_data->msg, "账号已存在");
        return 0;
    }
    
    // 检查用户数量是否已满
    pthread_mutex_lock(&g_mutex);
    if (g_user_count >= MAX_USERS) {
        pthread_mutex_unlock(&g_mutex);
        send_data->cmd = CMD_REPLY_FAIL;
        strcpy(send_data->msg, "用户数量已达上限");
        return 0;
    }
    
    // 添加新用户
    strcpy(g_users[g_user_count].account, recv_data->user.account);
    strcpy(g_users[g_user_count].passwd, recv_data->user.passwd);
    strcpy(g_users[g_user_count].nickname, recv_data->user.nickname);
    strcpy(g_users[g_user_count].signature, "");
    strcpy(g_users[g_user_count].avatar, "default.png");
    g_users[g_user_count].online = 0;
    g_user_count++;
    
    pthread_mutex_unlock(&g_mutex);
    
    // 保存用户信息
    Save_Users();
    
    send_data->cmd = CMD_REPLY_OK;
    strcpy(send_data->msg, "注册成功");
    return 0;
}

/**
 * 处理登录
 */
static int Handle_Login(int client_fd, struct sockaddr_in *client_addr, 
                       ChatData_t *recv_data, ChatData_t *send_data)
{
    if (!recv_data || !send_data || !client_addr) return -1;
    
    // 查找用户
    int user_idx = Find_User(recv_data->user.account);
    if (user_idx == -1) {
        send_data->cmd = CMD_REPLY_FAIL;
        strcpy(send_data->msg, "账号不存在");
        return 0;
    }
    
    // 检查密码
    pthread_mutex_lock(&g_mutex);
    if (strcmp(g_users[user_idx].passwd, recv_data->user.passwd) != 0) {
        pthread_mutex_unlock(&g_mutex);
        send_data->cmd = CMD_REPLY_FAIL;
        strcpy(send_data->msg, "密码错误");
        return 0;
    }
    
    // 标记用户为在线
    g_users[user_idx].online = 1;
    g_users[user_idx].fd = client_fd;
    g_users[user_idx].addr = *client_addr;
    g_users[user_idx].last_active = time(NULL);
    
    // 复制用户信息到回复中
    memcpy(&send_data->user, &g_users[user_idx], sizeof(UserInfo_t));
    
    pthread_mutex_unlock(&g_mutex);
    
    send_data->cmd = CMD_REPLY_OK;
    strcpy(send_data->msg, "登录成功");
    return 0;
}

/**
 * 处理获取在线用户列表
 */
static int Handle_Get_Online_Users(ChatData_t *recv_data, ChatData_t *send_data)
{
    if (!recv_data || !send_data) return -1;
    
    send_data->cmd = CMD_GET_ONLINE_USERS;
    send_data->user_count = 0;
    
    pthread_mutex_lock(&g_mutex);
    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i].online) {
            memcpy(&send_data->online_users[send_data->user_count], 
                   &g_users[i], sizeof(UserInfo_t));
            send_data->user_count++;
        }
    }
    pthread_mutex_unlock(&g_mutex);
    
    return 0;
}

/**
 * 处理发送消息
 */
static int Handle_Send_Msg(ChatData_t *recv_data, ChatData_t *send_data)
{
    if (!recv_data || !send_data) return -1;
    
    // 查找目标用户
    int target_idx = Find_User(recv_data->target_account);
    if (target_idx == -1) {
        send_data->cmd = CMD_REPLY_FAIL;
        strcpy(send_data->msg, "目标用户不存在");
        return 0;
    }
    
    pthread_mutex_lock(&g_mutex);
    // 检查目标用户是否在线
    if (!g_users[target_idx].online) {
        pthread_mutex_unlock(&g_mutex);
        send_data->cmd = CMD_REPLY_FAIL;
        strcpy(send_data->msg, "目标用户不在线");
        return 0;
    }
    
    // 转发消息给目标用户
    ChatData_t forward_data;
    memcpy(&forward_data, recv_data, sizeof(ChatData_t));
    
    if (send(g_users[target_idx].fd, &forward_data, sizeof(ChatData_t), 0) <= 0) {
        perror("send to target error");
    }
    
    pthread_mutex_unlock(&g_mutex);
    
    send_data->cmd = CMD_REPLY_OK;
    strcpy(send_data->msg, "消息发送成功");
    return 0;
}

/**
 * 处理添加好友
 */
static int Handle_Add_Friend(ChatData_t *recv_data, ChatData_t *send_data)
{
    if (!recv_data || !send_data) return -1;
    
    // 查找好友账号
    int friend_idx = Find_User(recv_data->target_account);
    if (friend_idx == -1) {
        send_data->cmd = CMD_REPLY_FAIL;
        strcpy(send_data->msg, "好友账号不存在");
        return 0;
    }
    
    // TODO: 实际应用中应该实现好友关系的存储和验证
    
    send_data->cmd = CMD_REPLY_OK;
    strcpy(send_data->msg, "添加好友请求已发送");
    return 0;
}

/**
 * 处理设置个性签名
 */
static int Handle_Set_Signature(ChatData_t *recv_data, ChatData_t *send_data)
{
    if (!recv_data || !send_data) return -1;
    
    // 查找用户
    int user_idx = Find_User(recv_data->user.account);
    if (user_idx == -1) {
        send_data->cmd = CMD_REPLY_FAIL;
        strcpy(send_data->msg, "用户不存在");
        return 0;
    }
    
    // 更新个性签名
    pthread_mutex_lock(&g_mutex);
    strcpy(g_users[user_idx].signature, recv_data->msg);
    pthread_mutex_unlock(&g_mutex);
    
    // 保存用户信息
    Save_Users();
    
    send_data->cmd = CMD_REPLY_OK;
    strcpy(send_data->msg, "个性签名设置成功");
    return 0;
}

/**
 * 处理设置头像
 */
static int Handle_Set_Avatar(ChatData_t *recv_data, ChatData_t *send_data)
{
    if (!recv_data || !send_data) return -1;
    
    // 查找用户
    int user_idx = Find_User(recv_data->user.account);
    if (user_idx == -1) {
        send_data->cmd = CMD_REPLY_FAIL;
        strcpy(send_data->msg, "用户不存在");
        return 0;
    }
    
    // 更新头像
    pthread_mutex_lock(&g_mutex);
    strcpy(g_users[user_idx].avatar, recv_data->msg);
    pthread_mutex_unlock(&g_mutex);
    
    // 保存用户信息
    Save_Users();
    
    send_data->cmd = CMD_REPLY_OK;
    strcpy(send_data->msg, "头像设置成功");
    return 0;
}

/**
 * 处理客户端断开连接
 */
static void Handle_Disconnect(int client_fd)
{
    pthread_mutex_lock(&g_mutex);
    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i].fd == client_fd) {
            g_users[i].online = 0;
            g_users[i].fd = -1;
            printf("用户 %s 已下线\n", g_users[i].account);
            break;
        }
    }
    pthread_mutex_unlock(&g_mutex);
}

/**
 * 处理客户端数据
 */
static void Handle_Client_Data(int client_fd, struct sockaddr_in *client_addr)
{
    ChatData_t recv_data, send_data;
    memset(&recv_data, 0, sizeof(ChatData_t));
    memset(&send_data, 0, sizeof(ChatData_t));
    
    // 接收数据
    int ret = recv(client_fd, &recv_data, sizeof(ChatData_t), 0);
    if (ret <= 0) {
        if (ret < 0) perror("recv error");
        Handle_Disconnect(client_fd);
        close(client_fd);
        return;
    }
    
    // 更新用户最后活动时间
    int user_idx = Find_User(recv_data.user.account);
    if (user_idx != -1) {
        pthread_mutex_lock(&g_mutex);
        g_users[user_idx].last_active = time(NULL);
        pthread_mutex_unlock(&g_mutex);
    }
    
    // 根据命令类型处理
    switch (recv_data.cmd) {
        case CMD_REGISTER:
            printf("处理注册请求: %s\n", recv_data.user.account);
            Handle_Register(&recv_data, &send_data);
            break;
            
        case CMD_LOGIN:
            printf("处理登录请求: %s\n", recv_data.user.account);
            Handle_Login(client_fd, client_addr, &recv_data, &send_data);
            break;
            
        case CMD_GET_ONLINE_USERS:
            printf("处理获取在线用户请求: %s\n", recv_data.user.account);
            Handle_Get_Online_Users(&recv_data, &send_data);
            break;
            
        case CMD_SEND_MSG:
            printf("处理消息发送: %s -> %s\n", 
                   recv_data.user.account, recv_data.target_account);
            Handle_Send_Msg(&recv_data, &send_data);
            break;
            
        case CMD_ADD_FRIEND:
            printf("处理添加好友: %s -> %s\n", 
                   recv_data.user.account, recv_data.target_account);
            Handle_Add_Friend(&recv_data, &send_data);
            break;
            
        case CMD_SET_SIGNATURE:
            printf("处理设置个性签名: %s\n", recv_data.user.account);
            Handle_Set_Signature(&recv_data, &send_data);
            break;
            
        case CMD_SET_AVATAR:
            printf("处理设置头像: %s\n", recv_data.user.account);
            Handle_Set_Avatar(&recv_data, &send_data);
            break;
            
        default:
            send_data.cmd = CMD_REPLY_FAIL;
            strcpy(send_data.msg, "未知命令");
            break;
    }
    
    // 发送回复
    if (send(client_fd, &send_data, sizeof(ChatData_t), 0) <= 0) {
        perror("send reply error");
        Handle_Disconnect(client_fd);
        close(client_fd);
    }
}

/**
 * 服务器主函数
 */
int main()
{
    int server_fd, epoll_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct epoll_event ev, events[MAX_EVENTS];
    
    // 初始化用户列表
    Init_Users();
    printf("初始化用户数量: %d\n", g_user_count);
    
    // 创建TCP套接字
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // 设置套接字选项，允许端口重用
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    // 绑定地址和端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    // 监听连接
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("聊天室服务器启动，端口: %d\n", PORT);
    
    // 创建epoll实例
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }
    
    // 添加服务器套接字到epoll
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl: server_fd");
        exit(EXIT_FAILURE);
    }
    
    // 事件循环
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // 新的连接请求
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd == -1) {
                    perror("accept");
                    continue;
                }
                
                // 设置客户端套接字为非阻塞
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                
                // 添加客户端套接字到epoll
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    perror("epoll_ctl: client_fd");
                    close(client_fd);
                    continue;
                }
                
                printf("新连接: %s:%d\n", 
                       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            } else {
                // 处理客户端数据
                Handle_Client_Data(events[i].data.fd, &client_addr);
            }
        }
    }
    
    // 清理
    close(server_fd);
    close(epoll_fd);
    return 0;
}
