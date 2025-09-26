//@file chat_room.h 数据结构定义

#ifndef CHAT_ROOM_H
#define CHAT_ROOM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// 服务器配置 - 请替换为实际的华为云服务器IP和端口
#define HUAWEI_CLOUD_SERVER_IP "121.36.XXX.XXX"  // 华为云服务器公网IP
#define HUAWEI_CLOUD_SERVER_PORT 8888            // 服务器端口

// 命令定义
enum ChatCmd {
    CMD_REGISTER = 1,        // 注册
    CMD_LOGIN,               // 登录
    CMD_GET_ONLINE_USERS,    // 获取在线用户
    CMD_SEND_MSG,            // 发送消息
    CMD_ADD_FRIEND,          // 添加好友
    CMD_SET_SIGNATURE,       // 设置个性签名
    CMD_SET_AVATAR,          // 设置头像
    CMD_REPLY_OK,            // 操作成功应答
    CMD_REPLY_FAIL           // 操作失败应答
};

// 用户信息结构体
typedef struct {
    char account[64];          // 账号
    char passwd[64];           // 密码
    char nickname[64];         // 昵称
    char signature[256];       // 个性签名
    char avatar[256];          // 头像路径
    struct sockaddr_in addr;   // IP地址和端口
    int online;                // 在线状态 1:在线 0:离线
} UserInfo_t;

// 消息结构体
typedef struct {
    int cmd;                   // 命令
    UserInfo_t user;           // 发送者信息
    char msg[1024];            // 消息内容
    char target_account[64];   // 目标账号
    UserInfo_t online_users[50];// 在线用户列表
    int user_count;            // 用户数量
} ChatData_t;

// 聊天消息记录
typedef struct {
    char account[64];          // 发送者账号
    char nickname[64];         // 发送者昵称
    char msg[1024];            // 消息内容
    int is_self;               // 是否是自己发送的 1:是 0:否
    time_t time;               // 发送时间
} ChatMsgRecord_t;

// 聊天室全局状态
typedef struct {
    int tcp_fd;                // TCP套接字
    int udp_fd;                // UDP套接字
    UserInfo_t self;           // 自身信息
    UserInfo_t friends[50];    // 好友列表
    int friend_count;          // 好友数量
    int logged_in;             // 登录状态 1:已登录 0:未登录
    pthread_t recv_thread;     // 接收线程
    pthread_mutex_t mutex;     // 互斥锁
    lv_obj_t *main_screen;     // 主界面
    lv_obj_t *login_screen;    // 登录界面
    lv_obj_t *register_screen; // 注册界面
    lv_obj_t *friend_screen;   // 好友列表界面
    lv_obj_t *chat_screen;     // 聊天窗口界面
    char current_chat_account[64]; // 当前聊天对象账号
    ChatMsgRecord_t msg_records[100]; // 消息记录
    int msg_count;             // 消息数量
} ChatRoom_t;

ChatRoom_t *Chat_Get_Room_Handle(void);

/**
 * 初始化聊天室
 * @param parent 父界面对象
 * @return 0:成功 -1:失败
 */
int Chat_Room_Init(lv_obj_t *parent);

/**
 * 用户注册
 * @param account 账号
 * @param passwd 密码
 * @param nickname 昵称
 * @return 0:成功 -1:失败
 */
int Chat_Register(const char *account, const char *passwd, const char *nickname);

/**
 * 用户登录
 * @param account 账号
 * @param passwd 密码
 * @return 0:成功 -1:失败
 */
int Chat_Login(const char *account, const char *passwd);

/**
 * 获取在线用户列表
 * @return 0:成功 -1:失败
 */
int Chat_Get_Online_Users(void);

/**
 * 发送消息
 * @param target 目标账号
 * @param msg 消息内容
 * @return 0:成功 -1:失败
 */
int Chat_Send_Msg(const char *target, const char *msg);

/**
 * 添加好友
 * @param friend_account 好友账号
 * @return 0:成功 -1:失败
 */
int Chat_Add_Friend(const char *friend_account);

/**
 * 设置个性签名
 * @param signature 个性签名内容
 * @return 0:成功 -1:失败
 */
int Chat_Set_Signature(const char *signature);

/**
 * 设置头像
 * @param avatar 头像路径
 * @return 0:成功 -1:失败
 */
int Chat_Set_Avatar(const char *avatar);

/**
 * 显示聊天室主界面
 */
void Chat_Show_Main_UI(void);

/**
 * 显示好友列表
 */
void Chat_Show_Friend_List(void);

/**
 * 显示聊天窗口
 * @param friend_account 好友账号
 */
void Chat_Show_Chat_Window(const char *friend_account);

/**
 * 返回首页
 */
void Chat_Back_To_Home(lv_event_t *e);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CHAT_ROOM_H */
