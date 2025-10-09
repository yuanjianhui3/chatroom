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
#define USER_FILE "user_data.txt" // 20250929新增：用户数据存储文件（当前目录）20251008

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
    MSG_GROUP_CHAT,         // 20250929新增群聊消息    
    MSG_LOGOUT            // 20250928新增退出登录
} MsgType;

// 20251009新增：强制1字节对齐（与客户端保持一致）
#pragma pack(1)

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

// 20251009新增：恢复默认对齐方式
#pragma pack()

// 20251009新增修改函数声明
static void Get_Online_User_Str(char *buf, int buf_len, ClientInfo *client);

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
        printf("用户数据文件[%s]不存在，初始化空列表\n", USER_FILE);
        return;
    }
    // 20251008新增修改：读取注册用户数
    size_t read_cnt = fread(&reg_user_count, sizeof(int), 1, fp);
    if (read_cnt != 1) {
        printf("读取用户数失败，初始化空列表\n");
        fclose(fp);
        return;
    }

    if (reg_user_count > MAX_USER) reg_user_count = MAX_USER; // 防止溢出
    
    // 20251008新增修改：读取用户数据
    read_cnt = fread(reg_users, sizeof(RegUser), reg_user_count, fp);
    if (read_cnt != reg_user_count) {
        printf("读取数据不完整（预期%d，实际%zu）\n", reg_user_count, read_cnt);
        reg_user_count = read_cnt;
    }

    fclose(fp);
    printf("加载成功：%d个注册用户\n", reg_user_count);
}

// 保存用户数据（修改后调用）
static void Save_Reg_Users() {
    FILE *fp = fopen(USER_FILE, "wb");
    if (fp == NULL) {
        perror("保存用户数据失败（fopen）");
        return;
    }

    // 20251008新增修改：写入用户数+用户数据
    if (fwrite(&reg_user_count, sizeof(int), 1, fp) != 1) {
        perror("写入用户数失败");
        fclose(fp);
        return;
    }
    if (fwrite(reg_users, sizeof(RegUser), reg_user_count, fp) != reg_user_count) {
        perror("写入数据不完整");
        fclose(fp);
        return;
    }

    fclose(fp);
    printf("保存成功：%d个注册用户\n", reg_user_count);
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
static void Get_Online_User_Str(char *buf, int buf_len, ClientInfo *client) {
    buf[0] = '\0';
    for(int i=0; i<client_count; i++) {
        if(clients[i].user.online) {
             // 新增：添加在线状态标识（账号:昵称:签名:头像:状态（20251008新增头像字段））
            char temp[256];
            // 20251008新增修改：根据实际在线状态显示
            const char *status = clients[i].user.online ? "在线" : "离线";
            const char *avatar = strlen(clients[i].user.avatar) ? clients[i].user.avatar : "S:/8080icon_img.jpg"; //20251008新增
            snprintf(temp, 256, "%s:%s:%s:%s:%s|", clients[i].user.account, clients[i].user.nickname, clients[i].user.signature,avatar, status);

            strncat(buf, temp, buf_len - strlen(buf) - 1);
        }
    }

    // 20251009新增：若无其他在线用户，仅返回当前请求用户（避免客户端列表为空）
    if(strlen(buf) == 0 && client != NULL) {
        char temp[256];
        const char *status = "在线";
        const char *avatar = strlen(client->user.avatar) ? client->user.avatar : "S:/8080icon_img.jpg";
        snprintf(temp, 256, "%s:%s:%s:%s:%s", 
                 client->user.account, 
                 client->user.nickname, 
                 client->user.signature, 
                 avatar, 
                 status);
        strncat(buf, temp, buf_len - strlen(buf) - 1);

    // 移除最后一个'|'
    } else if(strlen(buf) > 0) {
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

    // 20251009新增：显式清空content并复制（避免内存残留）
    memset(ack.content, 0, sizeof(ack.content));
    if(type != NULL) {
        strncpy(ack.content, type, sizeof(ack.content)-1); // 留1字节存'\0'
    }

    ack.user.port = result; // 1成功，0失败

    // 20250929新增：复制用户信息（账号/昵称/签名/头像）
    if (user != NULL) {
        strncpy(ack.user.account, user->account, sizeof(ack.user.account)-1);
        strncpy(ack.user.nickname, user->nickname, sizeof(ack.user.nickname)-1);
        strncpy(ack.user.signature, user->signature, sizeof(ack.user.signature)-1);
        strncpy(ack.user.avatar, user->avatar, sizeof(ack.user.avatar)-1);
    }

    send(sockfd, &ack, sizeof(ack), 0);
    printf("服务器发送ACK：type=%d, content=%s, result=%d\n", ack.type, ack.content, result);//20251009新增
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

        Save_Reg_Users(); // 20250929新增：保存注册用户
        free(new_user); // 20250927新增:释放堆内存

        Send_ACK(client->sockfd, "register", 1, NULL); // 成功ACK
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
        printf("登录失败：账号%s不存在\n", msg->user.account); // 20250930新增日志
        return;
    }
    // 验证密码
    if(strcmp(reg_user->password, msg->user.password) != 0) {
        Send_ACK(client->sockfd, "login", 0, reg_user);// 20250930新增：传reg_user，确保账号回传
        printf("登录失败：账号%s密码错误\n", msg->user.account); // 20250930新增日志
        return;
    }
    // 标记在线（关键：确保online=1）
    client->user = *reg_user;
    client->user.online = 1;// 强制设为1，避免未初始化
    Send_ACK(client->sockfd, "login", 1, reg_user); // 传递reg_user，返回完整信息。成功ACK
    printf("用户%s登录成功，返回ACK=1\n", msg->user.account);
}

static void Handle_Get_Online_User(ClientInfo *client) {
    NetMsg user_msg;
    memset(&user_msg, 0, sizeof(user_msg));
    user_msg.type = MSG_USER_LIST;
    Get_Online_User_Str(user_msg.content, 256, client); // 20251009新增：传入client

    //验证在线用户列表请求是否成功 // 20250928新增log，打印返回的用户列表
    printf("Handle_Get_Online_User：客户端%s请求在线用户列表，列表：%s\n",client->user.account, user_msg.content);
    
    if(send(client->sockfd, &user_msg, sizeof(user_msg), 0) <= 0)
    {printf("Handle_Get_Online_User：发送用户列表失败\n");};
}

static void Handle_Add_Friend(NetMsg *msg, ClientInfo *client) {
    // 查找目标用户
    RegUser *target = Find_Reg_User(msg->content);
    if(!target) {
        Send_ACK(client->sockfd, "add_friend", 0, NULL);// 不存在返回0
        printf("添加好友失败：目标账号%s不存在\n", msg->content);   //20250930新增
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

static void Handle_Set_Signature(NetMsg *msg, ClientInfo *client) {
    // 20250928修改更新签名
    snprintf(client->user.signature, sizeof(client->user.signature), "%s", msg->user.signature);

    // 更新注册用户
    RegUser *reg_user = Find_Reg_User(client->user.account);

    if (reg_user != NULL) {
        strncpy(reg_user->signature, client->user.signature, sizeof(reg_user->signature)-1);
        Save_Reg_Users(); // 20250929新增修改：保存个性签名修改
    }

    printf("更新签名：%s→%s\n", client->user.account, msg->user.signature);

    // 20250929新增：返回ACK给客户端
    Send_ACK(client->sockfd, "set_signature", 1, reg_user);
}

// --------20250929新增：服务器端头像设置函数------------------------
static void Handle_Set_Avatar(NetMsg *msg, ClientInfo *client) {
    // 更新客户端头像
    snprintf(client->user.avatar, sizeof(client->user.avatar), "%s", msg->user.avatar);
    // 更新注册用户
    RegUser *reg_user = Find_Reg_User(client->user.account);
    if (reg_user != NULL) {
        strncpy(reg_user->avatar, client->user.avatar, sizeof(reg_user->avatar)-1);
        Save_Reg_Users(); // 保存修改
    }
    printf("更新头像：%s→%s\n", client->user.account, msg->user.avatar);
    Send_ACK(client->sockfd, "set_avatar", 1, reg_user); // 返回ACK
}

// 20250929新增：群聊消息处理（广播给所有在线客户端）
static void Handle_Group_Chat(NetMsg *msg, ClientInfo *client) {
    // 解析群ID（此处简化为“default”默认群）
    char group_id[32], msg_content[224];    //20251008修改后：支持含空格消息，限制长度避免溢出
    if (sscanf(msg->content, "%31[^:]:%223[^\n]", group_id, msg_content) != 2) {
        printf("群聊消息格式错误：%s\n", msg->content);
        return;
    }
    // 构造群聊消息（携带发送者昵称+消息）
    NetMsg group_msg;
    memset(&group_msg, 0, sizeof(group_msg));
    group_msg.type = MSG_GROUP_CHAT;
    strncpy(group_msg.user.nickname, client->user.nickname, sizeof(group_msg.user.nickname)-1); // 发送者昵称
    snprintf(group_msg.content, sizeof(group_msg.content), "%s", msg_content);

    // 广播给所有在线客户端（排除发送者自己）
    pthread_mutex_lock(&data_mutex);
    Broadcast_Msg(&group_msg, client->sockfd);  // 20251008新增修改：调用已定义的广播函数
    pthread_mutex_unlock(&data_mutex);
    printf("群聊广播：%s（%s）→%s\n", client->user.nickname, group_id, msg_content);
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
            case MSG_GROUP_CHAT: Handle_Group_Chat(msg, client); break;         // 新增群聊处理
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