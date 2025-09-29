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
#define USER_FILE "./iot_chat_users.dat" // 20250929新增：用户数据存储文件（当前目录）

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
    MSG_SET_SIGNATURE,    // 设置个性签名
    MSG_SET_AVATAR,       // 20250929新增：设置头像
    MSG_GROUP_CHAT,         // 20250929新增群聊消息（发送）
    MSG_SINGLE_CHAT,     // 单聊消息（发送）
    MSG_SINGLE_CHAT_RECV,// 单聊消息（接收）
    MSG_GROUP_CHAT_RECV, // 群聊消息（接收）

    MSG_LOGOUT            // 20250928新增退出登录

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

// 20250929新增：用户数据持久化---------加载用户数据（启动时调用）
static void Load_Reg_Users() {
    FILE *fp = fopen(USER_FILE, "rb");
    if (fp == NULL) {
        printf("[log] 首次启动，无历史用户数据\n");
        return;
    }
    // 读取注册用户数
    fread(&reg_user_count, sizeof(int), 1, fp);
    if (reg_user_count > MAX_USER) reg_user_count = MAX_USER; // 防止溢出
    // 读取用户数据
    fread(reg_users, sizeof(RegUser), reg_user_count, fp);
    fclose(fp);
    printf("[log] 加载成功：%d个注册用户\n", reg_user_count);
}

// 保存用户数据（注册/修改后调用）
static void Save_Reg_Users() {
    FILE *fp = fopen(USER_FILE, "wb");
    if (fp == NULL) {
        perror("[log] 保存用户数据失败");
        return;
    }
    // 写入用户数+用户数据
    fwrite(&reg_user_count, sizeof(int), 1, fp);
    fwrite(reg_users, sizeof(RegUser), reg_user_count, fp);
    fclose(fp);
    printf("[log] 保存成功：%d个注册用户\n", reg_user_count);
}   //-----------------------------

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
            char temp[256];
            snprintf(temp, 256, "%s:%s:%s|", clients[i].user.account, clients[i].user.nickname,clients[i].user.signature);
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
// 20250929修改：传递RegUser信息，支持返回账号/昵称/签名/头像
static void Send_ACK(int sockfd, const char *type, int result, RegUser *user) {
    NetMsg ack;
    memset(&ack, 0, sizeof(ack));
    ack.type = MSG_ACK;
    strncpy(ack.content, type, 255);
    ack.user.port = result; // 1成功，0失败（复用online字段传结果）

    // 20250929新增：复制用户信息（账号/昵称/签名/头像）
    if (user != NULL) {
        strncpy(ack.user.account, user->account, sizeof(ack.user.account)-1);
        strncpy(ack.user.nickname, user->nickname, sizeof(ack.user.nickname)-1);
        strncpy(ack.user.signature, user->signature, sizeof(ack.user.signature)-1);
        strncpy(ack.user.avatar, user->avatar, sizeof(ack.user.avatar)-1);// 头像路径（80*80）
        ack.user.online = user->online;
    }

    send(sockfd, &ack, sizeof(ack), 0);
}

// -------------------------- 20250927新增：消息处理函数 -------------
static void Handle_Register(NetMsg *msg, ClientInfo *client) {
    // 检查账号是否已存在
    if(Find_Reg_User(msg->user.account) != NULL) {
        Send_ACK(client->sockfd, "register", 0, NULL); // 失败ACK
        return;
    }
    // 保存注册用户
    if(reg_user_count < MAX_USER) 
    {
        // 20250927新增：改为堆分配。栈RegUser new_user;
        RegUser *new_user = malloc(sizeof(RegUser));
        if (new_user == NULL) {
            perror("malloc failed in Handle_Register");
            Send_ACK(client->sockfd, "register", 0, NULL);
            return;
        }
        memset(new_user, 0, sizeof(RegUser));

        snprintf(new_user->account, sizeof(new_user->account), "%s", msg->user.account);  //20250928修改
        snprintf(new_user->password, sizeof(new_user->password), "%s", msg->user.password);
        snprintf(new_user->nickname, sizeof(new_user->nickname), "%s", msg->user.nickname);
        snprintf(new_user->signature, sizeof(new_user->signature), "%s", msg->user.signature);
        snprintf(new_user->avatar, sizeof(new_user->avatar), "%s", msg->user.avatar);

        new_user->online = 0;
        new_user->friend_cnt = 0;
        reg_users[reg_user_count++] = *new_user;   // 复制到注册用户列表加*

        Save_Reg_Users(); // 20250929新增：注册成功后保存注册用户
        free(new_user); // 20250927新增:释放堆内存

        Send_ACK(client->sockfd, "register", 1, new_user); // 成功ACK。20250929修改：传递new_user（完整注册信息）
        printf("注册成功：账号=%s, 昵称=%s, IP=%s, 端口=%d\n",
            msg->user.account, msg->user.nickname, msg->user.ip, msg->user.port);//20250928修改
    } else {
        Send_ACK(client->sockfd, "register", 0, NULL);// 20250929新增修改（补传NULL作为user参数）
    }
}

static void Handle_Login(NetMsg *msg, ClientInfo *client) {
    // 查找注册用户
    RegUser *reg_user = Find_Reg_User(msg->user.account);
    if(!reg_user) {
        Send_ACK(client->sockfd, "login", 0, NULL);
        return;
    }
    // 验证密码
    if(strcmp(reg_user->password, msg->user.password) != 0) {
        Send_ACK(client->sockfd, "login", 0, NULL);
        return;
    }
    // 标记在线
    client->user = *reg_user;
    client->user.online = 1;
    Send_ACK(client->sockfd, "login", 1, reg_user); // 传递reg_user，返回完整注册用户信息。成功ACK
    printf("登录成功：%s\n", msg->user.account);
}

static void Handle_Get_Online_User(ClientInfo *client) {
    NetMsg online_msg;
    memset(&online_msg, 0, sizeof(online_msg));
    online_msg.type = MSG_USER_LIST;

    online_msg.content[0] = '\0'; //----202509929新增--------

    // 拼接在线用户信息（格式：账号:昵称:头像:签名:在线状态|...）
    for (int i = 0; i < reg_user_count; i++) {
        int is_online = 0;
        // 检查用户是否在线
        for (int j = 0; j < MAX_CLIENT; j++) {
            if (strcmp(clients[j].user.account, reg_users[i].account) == 0) {
                is_online = 1;
                break;
            }
        }
        // 拼接信息（含80*80头像路径）
        char user_info[256];
        snprintf(user_info, 256, "%s:%s:%s:%s:%d|", 
                 reg_users[i].account,
                 reg_users[i].nickname,
                 reg_users[i].avatar, // 80*80头像路径
                 reg_users[i].signature,
                 is_online);
        strncat(online_msg.content, user_info, sizeof(online_msg.content)-1);
    }
    // 发送给客户端----------------------------
    
    send(client->sockfd, &online_msg, sizeof(online_msg), 0);

    printf("[log] 客户端%s请求在线用户列表，列表：%s\n",
        client->user.account, online_msg.content); // 20250928新增log

}

static void Handle_Add_Friend(NetMsg *msg, ClientInfo *client) {
    // 查找目标用户
    RegUser *target = Find_Reg_User(msg->content);
    if(!target) {
        Send_ACK(client->sockfd, "add_friend", 0, NULL);
        return;
    }
    // 检查是否已为好友
    for(int i=0; i<client->user.friend_cnt; i++) {
        if(strcmp(client->user.friends[i], target->account) == 0) {
            Send_ACK(client->sockfd, "add_friend", 0, NULL);
            return;
        }
    }
    // 添加好友

    // 20250928修改：先复制，friend_cnt 稍后再增加
    snprintf(client->user.friends[client->user.friend_cnt], sizeof(client->user.friends[0]), "%s", target->account);
    client->user.friend_cnt++; // 复制成功后，再增加计数

    // 更新注册用户的好友列表
    RegUser *reg_user = Find_Reg_User(client->user.account);
    *reg_user = client->user;
    Send_ACK(client->sockfd, "add_friend", 1, NULL);
    printf("添加好友：%s→%s\n", client->user.account, target->account);
    Save_Reg_Users(); // 20250929新增：保存好友列表
}

static void Handle_Set_Signature(NetMsg *msg, ClientInfo *client) 
{
    strncpy(client->user.signature, msg->user.signature, 63);
    RegUser *reg_user = Find_Reg_User(msg->user.account);
    if (reg_user) {
        strncpy(reg_user->signature, msg->user.signature, 63);
        Save_Reg_Users(); // 持久化
    }
    Send_ACK(client->sockfd, "set_signature", 1, reg_user);
    printf("[log] %s更新签名：%s\n", msg->user.account, msg->user.signature);
}

// --------20250929新增：服务器端头像设置函数------------------------
static void Handle_Set_Avatar(NetMsg *msg, ClientInfo *client) {

    // 1. 更新客户端当前头像
    strncpy(client->user.avatar, msg->user.avatar, 63);

    // 2. 更新注册用户数组（持久化）
    RegUser *reg_user = Find_Reg_User(msg->user.account);
    if (reg_user) {
        strncpy(reg_user->avatar, msg->user.avatar, 63);
        Save_Reg_Users(); // 方案要求：持久化保存
    }
    
    // 3. 返回成功ACK
    Send_ACK(client->sockfd, "set_avatar", 1, reg_user);
    printf("[log] %s更新头像：%s\n", msg->user.account, msg->user.avatar);
}

// -------------------------- 客户端处理线程 --------------------------
static void *Handle_Client(void *arg) {
    ClientInfo *client = (ClientInfo *)arg;

    //20250927新增-------------------
    NetMsg *msg = malloc(sizeof(NetMsg));  // 堆分配。栈NetMsg msg;
    int ret;
    if (msg == NULL) {
        perror("malloc failed");
        close(client->sockfd);
        free(client);
        client_count--;
        return NULL;
    }
    //------------------------------

    printf("new client connected: %s:%d\n", 
           inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));//新客户连接

    while(1) {
        memset(msg, 0, sizeof(NetMsg));   //初始化堆上的结构体。栈memset(&msg, 0, sizeof(msg));
        ret = recv(client->sockfd, msg, sizeof(NetMsg), 0); // 注意这里传指针
        if(ret <= 0) {
            printf("client disconnected: %s:%d\n", 
                   inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));//客户断开连接
            break;
        }

        pthread_mutex_lock(&data_mutex);
        switch(msg->type) // 访问时用->
        { 
            case MSG_REGISTER: Handle_Register(msg, client); break;             //调用注册 消息处理函数。传指针
            case MSG_LOGIN: Handle_Login(msg, client); break;                  //调用登录 消息处理函数
            case MSG_GET_ONLINE_USER: Handle_Get_Online_User(client); break;    //调用在线用户 消息处理函数
            case MSG_SET_AVATAR: Handle_Set_Avatar(msg, client); break;          // 20250929新增头像处理
            case MSG_GROUP_CHAT:
            {
                // 1. 20250929新增修改：构造群聊广播消息（携带发送者昵称+头像）
                NetMsg group_msg;
                memset(&group_msg, 0, sizeof(group_msg));
                group_msg.type = MSG_GROUP_CHAT_RECV;
                strncpy(group_msg.user.nickname, msg->user.nickname, 31);
                strncpy(group_msg.user.avatar, msg->user.avatar, 63);
                strncpy(group_msg.content, msg->content, 191);

                // 2. 广播给所有在线客户端（排除发送者自己）
                pthread_mutex_lock(&data_mutex); // 加锁防并发
                for (int i = 0; i < MAX_CLIENT; i++) {
                    if (clients[i].user.online && clients[i].sockfd != client->sockfd) {
                        send(clients[i].sockfd, &group_msg, sizeof(group_msg), 0);
                    }
                }
                pthread_mutex_unlock(&data_mutex);

                // 3. 向发送者返回成功ACK
                Send_ACK(client->sockfd, "group_chat", 1, NULL);
                printf("[log] 群聊广播：%s发送消息：%s\n", client->user.account, msg->content);
                break;
            }

            case MSG_SEND_MSG: {
                // 20250929新增修改：解析「接收者账号:消息内容」（单聊转发）
                char recv_account[32], msg_content[224];
                if (sscanf(msg->content, "%[^:]:%s", recv_account, msg_content) != 2) {
                    printf("消息格式错误：%s\n", msg->content);
                    break;
                }
                // 查找接收者客户端
                ClientInfo *recv_client = Find_Online_Client(recv_account);
                if (recv_client == NULL) {
                    Send_ACK(client->sockfd, "send_msg", 0, NULL); // 接收者离线
                    break;
                }
                // 构造转发消息
                NetMsg send_msg;
                memset(&send_msg, 0, sizeof(send_msg));
                send_msg.type = MSG_SEND_MSG;

                // 20250929新增修改（逐字段复制，匹配UserInfo结构）：发送者信息
                UserInfo *send_user = &send_msg.user;
                // 复制RegUser中与UserInfo对应的字段
                strncpy(send_user->account, client->user.account, sizeof(send_user->account)-1);
                strncpy(send_user->nickname, client->user.nickname, sizeof(send_user->nickname)-1);
                strncpy(send_user->signature, client->user.signature, sizeof(send_user->signature)-1);
                strncpy(send_user->avatar, client->user.avatar, sizeof(send_user->avatar)-1);
                send_user->port = client->user.online; // 复用port字段传递在线状态（不影响核心逻辑）
                
                strncpy(send_msg.content, msg_content, sizeof(send_msg.content)-1);
                // 转发给接收者
                send(recv_client->sockfd, &send_msg, sizeof(send_msg), 0);
                Send_ACK(client->sockfd, "send_msg", 1, NULL); // 发送成功
                break;
            }

            case MSG_SINGLE_CHAT: {
                // 20250929新增：解析消息：好友账号:消息内容
                char friend_account[32], msg_text[192];
                sscanf(msg->content, "%[^:]:%s", friend_account, msg_text);

                // 1. 查找接收方客户端（在线）
                ClientInfo *recv_client = NULL;
                for (int i = 0; i < MAX_CLIENT; i++) {
                    if (clients[i].user.online && strcmp(clients[i].user.account, friend_account) == 0) {
                        recv_client = &clients[i];
                        break;
                    }
                }
                if (!recv_client) {
                    // 接收方离线（方案未要求离线存储，仅提示发送方）
                    Send_ACK(client->sockfd, "single_chat", 0, NULL);
                    printf("[log] 单聊转发失败：%s离线\n", friend_account);
                    break;
                }

                // 2. 构造转发消息（携带发送者昵称+头像）
                NetMsg forward_msg;
                memset(&forward_msg, 0, sizeof(forward_msg));
                forward_msg.type = MSG_SINGLE_CHAT_RECV;
                strncpy(forward_msg.user.nickname, msg->user.nickname, 31);
                strncpy(forward_msg.user.avatar, msg->user.avatar, 63);
                strncpy(forward_msg.content, msg_text, 191);

                // 3. 转发给接收方
                send(recv_client->sockfd, &forward_msg, sizeof(forward_msg), 0);
                // 4. 向发送方返回成功ACK
                Send_ACK(client->sockfd, "single_chat", 1, NULL);
                printf("[log] 单聊转发：%s→%s，消息：%s\n", 
                       client->user.account, friend_account, msg_text);
                break;
            }

            case MSG_ADD_FRIEND: Handle_Add_Friend(msg, client); break;        //调用添加好友 消息处理函数
            case MSG_SET_SIGNATURE: Handle_Set_Signature(msg, client); break;  //调用个性签名 消息处理函数

            case MSG_LOGOUT: 
            {      //20250928新增，退出消息处理
                client->user.online = 0;    // 更新在线状态
                RegUser *reg_user = Find_Reg_User(msg->user.account);
                *reg_user = client->user;
                printf("用户%s退出登录，更新离线状态\n", msg->user.account);
                break;
            }
            default:
                printf("Unknown message type: %d\n", msg->type);                 //未知消息类型
                break;
        }
        pthread_mutex_unlock(&data_mutex);
    }

    // 20250927新增：释放堆内存
    free(msg);

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
    Load_Reg_Users();               // 20250929新增：启动时加载历史用户数据
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