//@file chat_room.c  聊天室核心功能

#include "chat_room.h"
#include "network.h"
#include "ui.h"
#include "../common/chat_adapter.h"
#include <time.h>

static ChatRoom_t g_chat_room;

/**
 * 初始化聊天室全局变量
 */
static void Chat_Room_Init_Global(lv_obj_t *parent)
{
    memset(&g_chat_room, 0, sizeof(ChatRoom_t));
    g_chat_room.main_screen = parent;
    g_chat_room.logged_in = 0;
    g_chat_room.friend_count = 0;
    g_chat_room.msg_count = 0;
    pthread_mutex_init(&g_chat_room.mutex, NULL);
}

/**
 * 接收消息线程函数
 */
static void *Chat_Recv_Thread(void *arg)
{
    ChatData_t data;
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    while (g_chat_room.logged_in) {
        memset(&data, 0, sizeof(ChatData_t));
        
        // 接收TCP消息
        int ret = Network_Recv_Data(&data);
        if (ret > 0) {
            pthread_mutex_lock(&g_chat_room.mutex);
            
            switch (data.cmd) {
                case CMD_REPLY_OK:
                    printf("操作成功: %s\n", data.msg);
                    break;
                    
                case CMD_REPLY_FAIL:
                    printf("操作失败: %s\n", data.msg);
                    break;
                    
                case CMD_GET_ONLINE_USERS:
                    // 更新在线用户列表
                    g_chat_room.friend_count = data.user_count;
                    memcpy(g_chat_room.friends, data.online_users, 
                           sizeof(UserInfo_t) * data.user_count);
                    // 刷新好友列表UI
                    Chat_Show_Friend_List();
                    break;
                    
                case CMD_SEND_MSG:
                    // 收到消息，添加到消息记录
                    if (g_chat_room.msg_count < 100) {
                        strncpy(g_chat_room.msg_records[g_chat_room.msg_count].account, 
                                data.user.account, 63);
                        strncpy(g_chat_room.msg_records[g_chat_room.msg_count].nickname, 
                                data.user.nickname, 63);
                        strncpy(g_chat_room.msg_records[g_chat_room.msg_count].msg, 
                                data.msg, 1023);
                        g_chat_room.msg_records[g_chat_room.msg_count].is_self = 0;
                        g_chat_room.msg_records[g_chat_room.msg_count].time = time(NULL);
                        g_chat_room.msg_count++;
                    }
                    
                    // 如果是当前聊天对象的消息，刷新聊天窗口
                    if (strcmp(data.user.account, g_chat_room.current_chat_account) == 0) {
                        Chat_Show_Chat_Window(data.user.account);
                    } else {
                        // 否则显示消息提示
                        // TODO: 实现消息提示功能
                    }
                    break;
            }
            
            pthread_mutex_unlock(&g_chat_room.mutex);
        } else if (ret <= 0) {
            // 连接断开
            printf("与服务器连接断开\n");
            g_chat_room.logged_in = 0;
            break;
        }
        
        usleep(100000); // 100ms休眠，降低CPU占用
    }
    
    return NULL;
}

/**
 * 初始化聊天室
 */
int Chat_Room_Init(lv_obj_t *parent)
{
    // 初始化全局变量
    Chat_Room_Init_Global(parent);
    
    // 初始化网络
    if (Network_Init() != 0) {
        printf("网络初始化失败\n");
        return -1;
    }
    
    // 初始化UI
    Chat_Init_UI();
    
    return 0;
}

/**
 * 用户注册
 */
int Chat_Register(const char *account, const char *passwd, const char *nickname)
{
    if (!account || !passwd || !nickname) return -1;
    
    ChatData_t data;
    memset(&data, 0, sizeof(ChatData_t));
    
    data.cmd = CMD_REGISTER;
    strncpy(data.user.account, account, sizeof(data.user.account)-1);
    strncpy(data.user.passwd, passwd, sizeof(data.user.passwd)-1);
    strncpy(data.user.nickname, nickname, sizeof(data.user.nickname)-1);
    
    // 获取本机地址信息
    struct sockaddr_in local_addr;
    socklen_t len = sizeof(local_addr);
    getsockname(g_chat_room.tcp_fd, (struct sockaddr*)&local_addr, &len);
    data.user.addr = local_addr;
    
    // 发送注册信息
    if (Network_Send_Data(&data) <= 0) {
        return -1;
    }
    
    // 等待服务器回复
    if (Network_Recv_Data(&data) <= 0) {
        return -1;
    }
    
    return (data.cmd == CMD_REPLY_OK) ? 0 : -1;
}

/**
 * 用户登录
 */
int Chat_Login(const char *account, const char *passwd)
{
    if (!account || !passwd) return -1;
    
    ChatData_t data;
    memset(&data, 0, sizeof(ChatData_t));
    
    data.cmd = CMD_LOGIN;
    strncpy(data.user.account, account, sizeof(data.user.account)-1);
    strncpy(data.user.passwd, passwd, sizeof(data.user.passwd)-1);
    
    // 发送登录信息
    if (Network_Send_Data(&data) <= 0) {
        return -1;
    }
    
    // 等待服务器回复
    if (Network_Recv_Data(&data) <= 0) {
        return -1;
    }
    
    if (data.cmd == CMD_REPLY_OK) {
        // 登录成功，保存用户信息
        pthread_mutex_lock(&g_chat_room.mutex);
        
        strncpy(g_chat_room.self.account, account, sizeof(g_chat_room.self.account)-1);
        strncpy(g_chat_room.self.nickname, data.user.nickname, sizeof(g_chat_room.self.nickname)-1);
        strncpy(g_chat_room.self.signature, data.user.signature, sizeof(g_chat_room.self.signature)-1);
        strncpy(g_chat_room.self.avatar, data.user.avatar, sizeof(g_chat_room.self.avatar)-1);
        g_chat_room.self.online = 1;
        g_chat_room.logged_in = 1;
        
        pthread_mutex_unlock(&g_chat_room.mutex);
        
        // 启动接收线程
        pthread_create(&g_chat_room.recv_thread, NULL, Chat_Recv_Thread, NULL);
        
        // 获取在线用户列表
        Chat_Get_Online_Users();
        
        return 0;
    }
    
    return -1;
}

/**
 * 获取在线用户列表
 */
int Chat_Get_Online_Users(void)
{
    ChatData_t data;
    memset(&data, 0, sizeof(ChatData_t));
    data.cmd = CMD_GET_ONLINE_USERS;
    
    return Network_Send_Data(&data);
}

/**
 * 发送消息
 */
int Chat_Send_Msg(const char *target, const char *msg)
{
    if (!target || !msg || !g_chat_room.logged_in) return -1;
    
    ChatData_t data;
    memset(&data, 0, sizeof(ChatData_t));
    
    data.cmd = CMD_SEND_MSG;
    memcpy(&data.user, &g_chat_room.self, sizeof(UserInfo_t));
    strncpy(data.target_account, target, sizeof(data.target_account)-1);
    strncpy(data.msg, msg, sizeof(data.msg)-1);
    
    // 发送消息
    if (Network_Send_Data(&data) <= 0) {
        return -1;
    }
    
    // 将发送的消息添加到本地记录
    pthread_mutex_lock(&g_chat_room.mutex);
    
    if (g_chat_room.msg_count < 100) {
        strncpy(g_chat_room.msg_records[g_chat_room.msg_count].account, 
                g_chat_room.self.account, 63);
        strncpy(g_chat_room.msg_records[g_chat_room.msg_count].nickname, 
                g_chat_room.self.nickname, 63);
        strncpy(g_chat_room.msg_records[g_chat_room.msg_count].msg, 
                msg, 1023);
        g_chat_room.msg_records[g_chat_room.msg_count].is_self = 1;
        g_chat_room.msg_records[g_chat_room.msg_count].time = time(NULL);
        g_chat_room.msg_count++;
    }
    
    pthread_mutex_unlock(&g_chat_room.mutex);
    
    return 0;
}

/**
 * 添加好友
 */
int Chat_Add_Friend(const char *friend_account)
{
    if (!friend_account || !g_chat_room.logged_in) return -1;
    
    ChatData_t data;
    memset(&data, 0, sizeof(ChatData_t));
    
    data.cmd = CMD_ADD_FRIEND;
    memcpy(&data.user, &g_chat_room.self, sizeof(UserInfo_t));
    strncpy(data.target_account, friend_account, sizeof(data.target_account)-1);
    
    if (Network_Send_Data(&data) <= 0) {
        return -1;
    }
    
    // 等待回复
    if (Network_Recv_Data(&data) <= 0) {
        return -1;
    }
    
    return (data.cmd == CMD_REPLY_OK) ? 0 : -1;
}

/**
 * 设置个性签名
 */
int Chat_Set_Signature(const char *signature)
{
    if (!signature || !g_chat_room.logged_in) return -1;
    
    ChatData_t data;
    memset(&data, 0, sizeof(ChatData_t));
    
    data.cmd = CMD_SET_SIGNATURE;
    memcpy(&data.user, &g_chat_room.self, sizeof(UserInfo_t));
    strncpy(data.msg, signature, sizeof(data.msg)-1);
    
    if (Network_Send_Data(&data) <= 0) {
        return -1;
    }
    
    // 等待回复
    if (Network_Recv_Data(&data) <= 0) {
        return -1;
    }
    
    if (data.cmd == CMD_REPLY_OK) {
        pthread_mutex_lock(&g_chat_room.mutex);
        strncpy(g_chat_room.self.signature, signature, sizeof(g_chat_room.self.signature)-1);
        pthread_mutex_unlock(&g_chat_room.mutex);
        return 0;
    }
    
    return -1;
}

/**
 * 设置头像
 */
int Chat_Set_Avatar(const char *avatar)
{
    if (!avatar || !g_chat_room.logged_in) return -1;
    
    ChatData_t data;
    memset(&data, 0, sizeof(ChatData_t));
    
    data.cmd = CMD_SET_AVATAR;
    memcpy(&data.user, &g_chat_room.self, sizeof(UserInfo_t));
    strncpy(data.msg, avatar, sizeof(data.msg)-1);
    
    if (Network_Send_Data(&data) <= 0) {
        return -1;
    }
    
    // 等待回复
    if (Network_Recv_Data(&data) <= 0) {
        return -1;
    }
    
    if (data.cmd == CMD_REPLY_OK) {
        pthread_mutex_lock(&g_chat_room.mutex);
        strncpy(g_chat_room.self.avatar, avatar, sizeof(g_chat_room.self.avatar)-1);
        pthread_mutex_unlock(&g_chat_room.mutex);
        return 0;
    }
    
    return -1;
}

/**
 * 返回首页
 */
void Chat_Back_To_Home(lv_event_t *e)
{
    // 隐藏聊天室相关界面
    if (g_chat_room.login_screen) {
        lv_obj_add_flag(g_chat_room.login_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_chat_room.register_screen) {
        lv_obj_add_flag(g_chat_room.register_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_chat_room.friend_screen) {
        lv_obj_add_flag(g_chat_room.friend_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_chat_room.chat_screen) {
        lv_obj_add_flag(g_chat_room.chat_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 显示首页
    lv_obj_t *main_screen = Dir_Look_Get_Main_Screen();
    if (main_screen) {
        lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

// 全局变量访问函数，供UI模块使用
ChatRoom_t *Chat_Get_Room_Handle(void)
{
    return &g_chat_room;
}
