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

#include <time.h> // 20251010新增：用于时间函数

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

    // 20251010新增：离线消息（最多20条）
    char offline_msgs[20][512]; 
    int offline_msg_cnt; 

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
static void Load_Reg_Users() 
{
    FILE *fp = fopen(USER_FILE, "r");// 文本模式

    //20251009新增大改：文本化加载用户数据
    //功能说明：user_data.txt改为文本格式，每行对应一个用户，字段用逗号分隔，示例如下：
    // test1,123456,测试1,默认签名,S:/avatar/1.png,1,1,test2
    // test2,654321,测试2,无签名,S:/avatar/2.png,0,1,test1

    if (!fp) {
        printf("用户数据文件[%s]不存在，初始化空列表\n", USER_FILE);
        return;
    }

    // 读取用户总数
    if (fscanf(fp, "%d\n", &reg_user_count) != 1) {
        printf("读取用户数失败，初始化空列表\n");
        fclose(fp);
        reg_user_count = 0;
        return;
    }
    if (reg_user_count > MAX_USER) reg_user_count = MAX_USER;
    memset(reg_users, 0, sizeof(reg_users)); // 清空原有数据

    // 20251009新增大改1：忽略文件第一行的用户数，手动统计有效用户（避免文件数据错误）
    char first_line[32] = {0};
    fgets(first_line, 32, fp); // 读取第一行（丢弃，不用文件中的用户数）
    reg_user_count = 0; // 重新计数有效用户
    memset(reg_users, 0, sizeof(reg_users)); // 清空原有数据

    // 修复2：循环读取所有行，只加载有效用户（字段完整+账号非空）
    char line[1024] = {0};
    while (fgets(line, 1024, fp) != NULL && reg_user_count < MAX_USER) 
    {
        RegUser *u = &reg_users[reg_user_count];
        line[strcspn(line, "\n")] = '\0';   // 20251009新增：去除换行符（仅1次读取）

        // 跳过空行或纯逗号行
        if (strlen(line) == 0 || strspn(line, ",") == strlen(line) || (reg_user_count == 0 && isdigit(line[0]))) {
            printf("Load_Reg_Users：跳过无效行（空行/纯逗号/用户数行）\n");
            continue;
        }

        // 解析字段（严格判断每个token是否存在，避免访问NULL），确保账号/密码/昵称非空
        char *token = strtok(line, ",");
        // 字段1：账号（必须非空，否则跳过）
        if (!token || strlen(token) == 0) {
            printf("Load_Reg_Users：用户%d无有效账号，跳过\n", reg_user_count);
            continue;
        }
        strncpy(u->account, token, sizeof(u->account)-1);

        // 字段2：密码（为空则设默认）
        token = strtok(NULL, ",");
        if (token) strncpy(u->password, token, sizeof(u->password)-1);
        else strcpy(u->password, ""); // 避免未初始化

        // 字段3：昵称（为空则用账号）
        token = strtok(NULL, ",");
        if (token && strlen(token) > 0) strncpy(u->nickname, token, sizeof(u->nickname)-1);
        else strncpy(u->nickname, u->account, sizeof(u->nickname)-1);

        // 字段4：签名（为空则设默认）
        token = strtok(NULL, ",");
        if (token) strncpy(u->signature, token, sizeof(u->signature)-1);
        else strcpy(u->signature, "默认签名");

        // 字段5：头像（为空则用默认路径）
        token = strtok(NULL, ",");
        if (token && strlen(token) > 0) strncpy(u->avatar, token, sizeof(u->avatar)-1);
        else strcpy(u->avatar, "S:/8080icon_img.jpg");

        // 字段6：在线状态（为空则设0）
        token = strtok(NULL, ",");
        u->online = token ? atoi(token) : 0;

        // 字段7：好友数（为空则设0）
        token = strtok(NULL, ",");
        u->friend_cnt = token ? atoi(token) : 0;
        // 限制好友数不超过20（避免数组越界）
        if (u->friend_cnt > 20) u->friend_cnt = 20;

        // 字段8~n：解析好友列表（最多20个）（仅账号）
        for (int j=0; j<u->friend_cnt; j++) {
            token = strtok(NULL, ",");
            if (token) strncpy(u->friends[j], token, sizeof(u->friends[j])-1);
            else break; // 字段不足时停止
        }

        // 有效用户：计数+1并打印日志
        reg_user_count++;
        printf("Load_Reg_Users：加载有效用户%d → 账号=%s，密码=%s，昵称=%s\n", 
               reg_user_count-1, u->account, u->password, u->nickname);
    }

    fclose(fp);
    printf("加载用户数据成功：%d个用户，文件：%s\n\n", reg_user_count, USER_FILE);
}

// 保存用户数据（修改后调用）
static void Save_Reg_Users() {
    FILE *fp = fopen(USER_FILE, "w");// 文本模式（覆盖写入）

    //20251009新增大改：文本化存储用户数据
    if (!fp) {
        perror("Save_Reg_Users fopen failed");
        return;
    }
    // 第一行：注册用户总数
    fprintf(fp, "%d\n", reg_user_count);
    // 后续每行：一个用户的信息（字段用逗号分隔）
    for (int i=0; i<reg_user_count; i++) {
        RegUser *u = &reg_users[i];
        // 格式：账号,密码,昵称,签名,头像,在线状态,好友数,好友1,好友2,...
        fprintf(fp, "%s,%s,%s,%s,%s,%d,%d", 
                u->account, u->password, u->nickname, u->signature, u->avatar,
                u->online, u->friend_cnt);
        // 追加好友列表
        for (int j=0; j<u->friend_cnt; j++) {
            fprintf(fp, ",%s", u->friends[j]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    printf("保存用户数据成功：%d个用户，文件：%s\n\n", reg_user_count, USER_FILE);
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
static void Get_Online_User_Str(char *buf, int buf_len, ClientInfo *client) 
{
    buf[0] = '\0';

    if (!client) return; // 避免空指针
    // 1. 先获取当前用户的所有好友（含离线）
    RegUser *cur_reg_user = Find_Reg_User(client->user.account);
    if (cur_reg_user) {
        for (int j=0; j<cur_reg_user->friend_cnt; j++) {
            const char *friend_acc = cur_reg_user->friends[j];
            // 2. 检查好友是否在线
            ClientInfo *friend_client = Find_Online_Client(friend_acc);
            const char *status = friend_client ? "在线" : "离线";
            const char *nickname = friend_client ? friend_client->user.nickname : friend_acc; // 离线时用账号当昵称
            const char *signature = friend_client ? friend_client->user.signature : "离线用户";
            const char *avatar = friend_client ? (strlen(friend_client->user.avatar) ? friend_client->user.avatar : "S:/8080icon_img.jpg") : "S:/8080icon_img.jpg";
            
            // 3. 拼接好友信息
            char temp[256];
            snprintf(temp, 256, "%s:%s:%s:%s:%s|", friend_acc, nickname, signature, avatar, status);
            if (strlen(buf) + strlen(temp) < buf_len - 1) {
                strncat(buf, temp, buf_len - strlen(buf) - 1);
            }
        }
    }
    // 4. 最后添加当前用户
    char self_temp[256];
    const char *self_status = client->user.online ? "在线" : "离线";
    const char *self_avatar = strlen(client->user.avatar) ? client->user.avatar : "S:/8080icon_img.jpg";
    // 生成当前用户信息（格式：账号:昵称:签名:头像:状态）20251010修改：添加“(当前用户)”标识
    snprintf(self_temp, 256, "%s:%s(当前用户):%s:%s:%s", client->user.account, client->user.nickname, client->user.signature, self_avatar, self_status);
    if (strlen(buf) == 0) 
    {
         // 空缓冲区直接拼接，保留原长度校验（避免溢出）
        strncat(buf, self_temp, buf_len - 1);
    } else {
        //用strcat拼接分隔符“|”（固定单字符，且buf有足够空间）
        strcat(buf, "|");
        //strncat长度校验，确保不超出buf总长度
        strncat(buf, self_temp, buf_len - strlen(buf) - 1);
    }
    // 移除最后一个'|'（若存在，确保客户端解析格式正确）
    if (strlen(buf) > 0 && buf[strlen(buf)-1] == '|') 
    {
        buf[strlen(buf)-1] = '\0';
    }
}

// 广播消息给所有在线客户端（排除发送者自己）
static void Broadcast_Msg(NetMsg *msg, int exclude_fd) 
{
    pthread_mutex_lock(&data_mutex);

    int broadcast_cnt = 0;
    for(int i=0; i<client_count; i++)
    {
        if(clients[i].sockfd != exclude_fd && clients[i].user.online) 
        {
           if(send(clients[i].sockfd, msg, sizeof(NetMsg), 0) > 0) 
            {
                broadcast_cnt++;
            }
        }
    }
    printf("群聊广播完成：共发送给%d个在线用户\n", broadcast_cnt);
    pthread_mutex_unlock(&data_mutex);
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

        // 20251009新增大改：增加最大用户数判断，避免数组溢出，确保注册数据实时持久化，不超出最大用户数
        if (reg_user_count < MAX_USER) 
        {   // 校验注册信息完整性（账号、密码、昵称非空）
            if (strlen(new_user->account) == 0 || strlen(new_user->password) == 0 || strlen(new_user->nickname) == 0) {
                free(new_user);
                Send_ACK(client->sockfd, "register", 0, NULL);
                printf("用户注册失败：信息不完整\n");
                return;
            }
            reg_users[reg_user_count++] = *new_user;
            Save_Reg_Users(); // 立即保存到文件
            printf("用户[%s]注册并持久化成功（昵称：%s）\n", new_user->account, new_user->nickname);
            Send_ACK(client->sockfd, "register", 1, NULL);
        } else {
            free(new_user);
            Send_ACK(client->sockfd, "register", 0, NULL);
            printf("用户注册失败：达到最大用户数（%d）\n", MAX_USER);//20251009新增
            return;
        }

        Save_Reg_Users(); // 20250929新增：保存注册用户
        free(new_user); // 20250927新增:释放堆内存

        Send_ACK(client->sockfd, "register", 1, NULL); // 成功ACK
        printf("注册成功：账号=%s, 昵称=%s, IP=%s, 端口=%d\n\n",
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
        printf("登录失败：账号%s不存在\n\n", msg->user.account); // 20250930新增日志
        return;
    }
    // 20251009新增大改：验证密码。（增加reg_user非空校验，避免极端情况）
    if (!reg_user) {
        Send_ACK(client->sockfd, "login", 0, NULL);
        printf("登录失败：内部错误（reg_user为NULL）\n\n");
        return;
    }
    if(strcmp(reg_user->password, msg->user.password) != 0) {
        // 密码错误：仅返回账号（避免传递多余信息）
        Send_ACK(client->sockfd, "login", 0, NULL); 
        printf("登录失败：账号%s密码错误（输入：%s，正确：%s）\n\n",
             msg->user.account, msg->user.password, reg_user->password);//20251010修改
        return;
    }

    // 标记在线（关键：确保online=1）
    client->user = *reg_user;
    client->user.online = 1;// 强制设为1在线状态，避免未初始化
    Send_ACK(client->sockfd, "login", 1, reg_user); // 传递reg_user，返回完整信息（账号、昵称、头像）。成功ACK
    printf("用户%s（昵称：%s）登录成功，返回ACK=1\n\n", msg->user.account, reg_user->nickname);

    // 20251010新增：推送离线消息
    if (reg_user && reg_user->offline_msg_cnt > 0) {
        printf("向[%s]推送离线消息（共%d条）\n", reg_user->account, reg_user->offline_msg_cnt);
        for (int i=0; i<reg_user->offline_msg_cnt; i++) {
            // 解析离线消息：发送者昵称|内容|时间
            char sender[32], content[256], time[32];
            sscanf(reg_user->offline_msgs[i], "%[^|]|%[^|]|%s", sender, content, time);
            // 构造单聊消息
            NetMsg offline_msg;
            memset(&offline_msg, 0, sizeof(offline_msg));
            offline_msg.type = MSG_SEND_MSG;
            strncpy(offline_msg.user.nickname, sender, sizeof(offline_msg.user.nickname)-1);
            strncpy(offline_msg.content, content, sizeof(offline_msg.content)-1);
            // 发送给客户端
            send(client->sockfd, &offline_msg, sizeof(offline_msg), 0);
        }
        // 清空离线消息
        reg_user->offline_msg_cnt = 0;
        Save_Reg_Users();
    }
}

static void Handle_Get_Online_User(ClientInfo *client) {
    NetMsg user_msg;
    memset(&user_msg, 0, sizeof(user_msg));
    user_msg.type = MSG_USER_LIST;
    Get_Online_User_Str(user_msg.content, 256, client); // 20251009新增：传入client

    //验证在线用户列表请求是否成功 // 20250928新增log，打印返回的用户列表
    printf("Handle_Get_Online_User：客户端%s请求在线用户列表，列表：%s\n\n",client->user.account, user_msg.content);
    
    if(send(client->sockfd, &user_msg, sizeof(user_msg), 0) <= 0)
    {printf("Handle_Get_Online_User：发送用户列表失败\n");};
}

static void Handle_Add_Friend(NetMsg *msg, ClientInfo *client) 
{
    // 查找目标用户
    RegUser *target = Find_Reg_User(msg->content);
    if(!target) {
        Send_ACK(client->sockfd, "add_friend", 0, NULL);// 不存在返回0
        printf("添加好友失败：目标账号%s不存在\n\n", msg->content);   //20250930新增
        return;
    }
    // 检查是否已为好友
    for(int i=0; i<client->user.friend_cnt; i++) {
        if(strcmp(client->user.friends[i], target->account) == 0) {
            Send_ACK(client->sockfd, "add_friend", 0, NULL);
            return;
        }
    }

    // 20251009新增：添加好友（校验好友数量不超过上限）
    if (client->user.friend_cnt >= 20) { // 好友数组最大20个
        Send_ACK(client->sockfd, "add_friend", 0, NULL);
        printf("添加好友失败：%s好友数量已达上限（20个）\n", client->user.account);
        return;
    }

    // 20250928修改：先复制，friend_cnt 稍后再增加。给添加方添加好友
    snprintf(client->user.friends[client->user.friend_cnt], sizeof(client->user.friends[0]), "%s", target->account);
    client->user.friend_cnt++; // 复制成功后，再增加计数

    // 20251009新增：给被添加方同步添加好友（新增核心逻辑）
    snprintf(target->friends[target->friend_cnt], sizeof(target->friends[0]), "%s", client->user.account);
    target->friend_cnt++;

    // 20251009新增修改：更新注册用户的好友列表并保存（关键：确保持久化）
    RegUser *reg_user = Find_Reg_User(client->user.account);// 添加方
    RegUser *target_reg = Find_Reg_User(target->account);   // 20251009新增：被添加方

    if (reg_user && target_reg) 
    {
        *reg_user = client->user;       // 更新添加方
        *target_reg = *target;      // 更新被添加方
        Save_Reg_Users();               // 持久化到user_data.txt
        Send_ACK(client->sockfd, "add_friend", 1, reg_user); // 返回更新后的用户信息
        printf("添加好友成功：%s→%s，双方列表已同步，当前好友数：%d\n", client->user.account, target->account, client->user.friend_cnt);
    }
    else {
        Send_ACK(client->sockfd, "add_friend", 0, NULL);
        printf("添加好友失败：未找到用户%s\n", client->user.account);
    }
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

    printf("更新签名：%s→%s\n\n", client->user.account, msg->user.signature);

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
    printf("更新头像：%s→%s\n\n", client->user.account, msg->user.avatar);
    Send_ACK(client->sockfd, "set_avatar", 1, reg_user); // 返回ACK
}

// 20250929新增：群聊消息处理（广播给所有在线客户端）
static void Handle_Group_Chat(NetMsg *msg, ClientInfo *client) 
{
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

    //20251010新增修改：广播给所有在线客户端（排除发送者）
    Broadcast_Msg(&group_msg, client->sockfd);//20251008新增修改：调用已定义的广播函数
    printf("群聊广播：%s（群ID：%s）→ 所有在线用户\n", client->user.nickname, group_id);

    // 20251009新增大改：获取当前时间并格式化---------------
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[32];
    strftime(time_str, 32, "[%Y-%m-%d %H:%M:%S]", t);
    
    // 20251010新增：解析群聊消息格式（增加调试）
    if (sscanf(msg->content, "%31[^:]:%223[^\n]", group_id, msg_content) != 2) {
        printf("群聊消息格式错误：%s → 正确格式：群ID:消息内容\n", msg->content);
        return;
    }
    // 20251010新增：广播后打印日志（原逻辑保留，增加解析成功提示）
    printf("%s 群聊消息解析成功：\n", time_str);
    printf("  原始消息：%s\n  解析后群ID：%s，消息内容：%s\n", msg->content, group_id, msg_content);

    // 打印带时间戳的群聊日志（含发送者账号+群ID）
    printf("%s 群聊广播成功：\n", time_str);
    printf("  发送者：%s（账号：%s）\n  群ID：%s\n  消息内容：%s\n  接收人数：%d（排除发送者）\n\n",
        client->user.nickname, client->user.account,
        group_id, msg_content,
        client_count - 1); // 客户端总数-1（排除发送者）
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

    printf("新客户连接: %s:%d\n\n", 
           inet_ntoa(client->addr.sin_addr), ntohs(client->addr.sin_port));//新客户连接

    while(1) {
        memset(msg, 0, sizeof(NetMsg));   //初始化堆上的结构体。栈memset(&msg, 0, sizeof(msg));
        ret = recv(client->sockfd, msg, sizeof(NetMsg), 0); // 注意这里传指针
        if(ret <= 0) {
            printf("客户断开连接: %s:%d\n\n", 
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
            case MSG_SEND_MSG: 
            {
                // 20250929新增修改：解析「接收者账号:消息内容」（单聊转发）
                char recv_account[32] = {0}, msg_content[224] = {0};

                int parse_ret = sscanf(msg->content, "%31[^:]:%223[^\n]", recv_account, msg_content);

                // 修复1：细化格式错误场景，打印具体原因
                if (parse_ret != 2) {
                    const char *err_msg = (parse_ret == 0) ? "未解析到账号和消息" : "未解析到消息内容";
                    printf("[%s] 单聊消息格式错误（正确格式：账号:消息）：%s → 错误：%s\n",
                           client->user.account, msg->content, err_msg);
                    Send_ACK(client->sockfd, "send_msg", 0, NULL);
                    break;
                }

                // 修复2：校验账号和消息非空
                if (strlen(recv_account) == 0) {
                    printf("[%s] 单聊转发失败：接收者账号为空\n", client->user.account);
                    Send_ACK(client->sockfd, "send_msg", 0, NULL);
                    break;
                }
                if (strlen(msg_content) == 0) {
                    printf("[%s] 单聊转发失败：消息内容为空\n", client->user.account);
                    Send_ACK(client->sockfd, "send_msg", 0, NULL);
                    break;
                }

                // 修复3：区分“接收者不存在”和“接收者离线”
                ClientInfo *recv_client = Find_Online_Client(recv_account);
                RegUser *recv_reg = Find_Reg_User(recv_account);
                if (recv_client == NULL) 
                {   
                    // 20251010新增：定义time_str并获取当前时间（格式：2024-06-19 15:30:00）
                    char time_str[32];
                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);
                    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

                    //20251010新增大改：存储离线消息
                    if (recv_reg != NULL) 
                    { // 接收者存在但离线→存储离线消息
                        if (recv_reg->offline_msg_cnt < 20) { // 限制20条离线消息
                            // 格式：发送者昵称|消息内容|时间
                            snprintf(recv_reg->offline_msgs[recv_reg->offline_msg_cnt], 512, 
                                     "%s|%s|%s", client->user.nickname, msg_content, time_str);
                            recv_reg->offline_msg_cnt++;
                            Save_Reg_Users(); // 持久化离线消息.立即保存，防止服务器重启丢失
                            printf("[%s] 接收者[%s]离线，存储离线消息（共%d条）\n\n", 
                                   time_str, recv_account, recv_reg->offline_msg_cnt);
                        }
                        Send_ACK(client->sockfd, "send_msg", 1, NULL); // 提示发送成功
                    } else { // 接收者不存在
                        Send_ACK(client->sockfd, "send_msg", 0, NULL);// 告知客户端发送失败
                        printf("单聊转发失败：接收者[%s]不存在，离线消息已满（20条）消息丢弃\n\n", recv_account);
                    }
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

                // 20251009新增大改：确保发送者昵称非空（用账号兜底）（确保日志显示聊天内容）
                const char *sender_nick = strlen(client->user.nickname) > 0 ? client->user.nickname : client->user.account;
                strncpy(send_user->nickname, sender_nick, sizeof(send_user->nickname)-1);
                // 补充：打印消息解析日志（新手可验证格式）
                printf("Handle_Send_Msg：解析消息 → 接收者=%s，内容=%s\n", recv_account, msg_content);

                strncpy(send_msg.content, msg_content, sizeof(send_msg.content)-1);

                // 转发给接收者
                // 20251009新增大改：获取当前时间并格式化。打印完整日志（含时间戳、IP、消息内容）
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char time_str[32];
                strftime(time_str, 32, "[%Y-%m-%d %H:%M:%S]", t);
                ssize_t send_ret = send(recv_client->sockfd, &send_msg, sizeof(send_msg), 0); //20251009新增

                if (send_ret > 0) 
                {   // 打印完整聊天信息
                    printf("[%s] 单聊转发成功：\n", time_str);
                    printf("  发送者：昵称=%s，账号=%s，IP=%s\n",
                           client->user.nickname, client->user.account, inet_ntoa(client->addr.sin_addr));
                    printf("  接收者：昵称=%s，账号=%s，IP=%s\n",
                           recv_client->user.nickname, recv_client->user.account, inet_ntoa(recv_client->addr.sin_addr));
                    printf("  消息内容：%s\n  转发字节数：%zd\n\n", msg_content, send_ret);
                    Send_ACK(client->sockfd, "send_msg", 1, NULL);
                }
                else {
                    printf("%s 单聊转发失败：发送字节数=%zd，错误码=%d\n",
                           time_str, send_ret, errno);
                    Send_ACK(client->sockfd, "send_msg", 0, NULL);
                    break;
                }
                break;
            }

            case MSG_ADD_FRIEND: Handle_Add_Friend(msg, client); break;        //调用添加好友 消息处理函数
            case MSG_SET_SIGNATURE: Handle_Set_Signature(msg, client); break;  //调用个性签名 消息处理函数

            case MSG_LOGOUT: 
            {      //20250928新增，退出消息处理
                client->user.online = 0;    // 更新在线状态
                RegUser *reg_user = Find_Reg_User(msg->user.account);
                *reg_user = client->user;
                printf("用户%s退出登录，更新离线状态\n\n", msg->user.account);
                break;
            }
            default:
                printf("Unknown message type: %d\n\n", msg->type);                 //未知消息类型
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
    printf("服务器启动，监听端口 %d...\n", SERVER_PORT);   //服务器启动，监听端口

    pthread_mutex_init(&data_mutex, NULL);    // 初始化互斥锁
    Load_Reg_Users();               // 20250929新增：启动时加载历史用户数据
    // 循环接收客户端连接
    while(1) {
        if(client_count >= MAX_CLIENT) {
            printf("达到最大连接数，等待...\n"); //达到最大连接数，等待...
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