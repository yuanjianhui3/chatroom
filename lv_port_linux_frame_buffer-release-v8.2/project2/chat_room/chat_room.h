//@file chat_room.h 协议与数据结构

#ifndef CHAT_ROOM_H
#define CHAT_ROOM_H

#include "../../lvgl/lvgl.h"
#include "../../dir_look/dir_look.h"  // 引用原有UI控制结构体

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
    MSG_GROUP_CHAT,         // 20250929新增群聊消息（发送）
    MSG_SINGLE_CHAT,     // 单聊消息（发送）
    MSG_SINGLE_CHAT_RECV,// 单聊消息（接收）
    MSG_GROUP_CHAT_RECV, // 群聊消息（接收）

    MSG_LOGOUT,          // 20250928新增：退出登录
    MSG_GET_FRIEND_LIST  // 获取好友列表

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
    char cur_nickname[32];  // 20250929新增：当前用户昵称
    char cur_avatar[64];    // 20250929新增：当前用户80*80头像路径

    char cur_chat_friend[32];  // 20250929新增：当前聊天好友账号
    lv_obj_t *chat_log_cont;   // 20250929新增：聊天记录容器
    lv_obj_t *chat_input_ta;   // 20250929新增：消息输入框
    int is_group_chat;          // 20250929新增：1=群聊，0=单聊

    lv_obj_t *avatar_preview_btn;   // 20250929新增：头像预览按钮
    char temp_avatar_path[64];      // 20250929新增：暂存选择的头像路径

} CHAT_CTRL, *CHAT_CTRL_P;

#endif

// 外部可调用函数
void Chat_Room_Init(struct Ui_Ctrl *uc, lv_obj_t *scr_home, bool connect_now); // 初始化聊天室模块
void Chat_Room_Exit(void); // 退出聊天室并释放资源