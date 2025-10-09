//@file chat_room.h 协议与数据结构

#ifndef CHAT_ROOM_H
#define CHAT_ROOM_H

#include "../../lvgl/lvgl.h"
#include "../../dir_look/dir_look.h"  // 引用原有UI控制结构体

// 20251008 新增：解决 errno 和 EINTR 未声明问题
#include <errno.h>

// 协议类型（区分不同请求/响应）
typedef enum {
    MSG_REGISTER = 1,    // 注册请求
    MSG_LOGIN,           // 登录请求
    MSG_ACK,             // 服务器应答（1成功/0失败）
    MSG_GET_ONLINE_USER, // 获取在线用户列表
    MSG_USER_LIST,       // 在线用户列表数据
    MSG_SEND_MSG,        // 发送聊天消息
    MSG_ADD_FRIEND,      // 添加好友
    MSG_SET_SIGNATURE,    // 设置个性签名
    MSG_SET_AVATAR,         // 20250929新增设置头像
    MSG_GROUP_CHAT,         // 20250929新增群聊消息
    MSG_GET_FRIEND_LIST,  // 获取好友列表
    MSG_LOGOUT          // 20250928新增：退出登录

} MsgType;

// 20251009新增：强制1字节对齐（解决网络传输结构体解析错误）
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
    int online;          // 20251009新增：客户端暂不使用，仅为对齐服务器结构体
    char friends[20][32];// 客户端暂不使用，仅为对齐服务器结构体
    int friend_cnt;      // 客户端暂不使用，仅为对齐服务器结构体

} UserInfo;

// 网络消息结构体（统一传输格式）
typedef struct {
    MsgType type;        // 消息类型
    UserInfo user;       // 用户信息
    char content[256];   // 附加内容（消息/提示）
} NetMsg;

// 聊天室全局控制结构体
typedef struct {
    struct Ui_Ctrl *uc;  // 原有UI控制指针
    int sockfd;          // 服务器连接socket
    lv_obj_t *scr_home;  // 首页（用于返回）
    lv_obj_t *scr_login; // 登录界面
    lv_obj_t *scr_register; // 注册界面
    lv_obj_t *scr_friend; // 好友列表界面
    lv_obj_t *scr_chat;  // 聊天窗口界面
    lv_obj_t *friend_list; // 好友列表控件
    char cur_account[32];// 当前登录账号

    lv_obj_t *scr_setting; // 设置界面（个性签名/头像）
    bool exiting;   // 20250928新增退出标志

    char chat_friend_account[32]; // 20250929新增：当前聊天好友账号

    lv_obj_t *chat_content_ta; // 20250930新增：聊天内容文本框（单聊/群聊共用）
    lv_obj_t *chat_avatar_btn; // 新增：聊天窗口头像按钮
    lv_obj_t *group_chat_title; // 新增：群聊标题（避免重叠）
    lv_obj_t *chat_title; // 20251008新增：单聊标题（避免重叠）

} CHAT_CTRL, *CHAT_CTRL_P;

// 恢复默认对齐方式
#pragma pack()

#endif

// 外部可调用函数
void Chat_Room_Init(struct Ui_Ctrl *uc, lv_obj_t *scr_home, bool connect_now); // 初始化聊天室模块
void Chat_Room_Exit(void); // 退出聊天室并释放资源