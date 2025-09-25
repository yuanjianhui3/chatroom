//@file chat_room.h  聊天室模块头文件

#ifndef _CHAT_ROOM_H_
#define _CHAT_ROOM_H_

#include "../common/ext_ui_adapt.h"

// 聊天室UI结构体（对应注册/登录、好友列表、聊天窗口三界面）
typedef struct Chat_Room_UI {
    // 注册登录界面
    lv_obj_t *login_scr;
    lv_obj_t *acc_ta;    // 账号输入框
    lv_obj_t *pwd_ta;    // 密码输入框
    lv_obj_t *reg_btn;   // 注册按钮
    lv_obj_t *login_btn; // 登录按钮

    // 好友列表界面（按方案要求：含返回首页+设置按钮）
    lv_obj_t *friend_scr;
    lv_obj_t *friend_list;      // 在线好友列表
    lv_obj_t *back_home_btn;    // 返回首页按钮
    lv_obj_t *setting_btn;      // 设置按钮
    lv_obj_t *add_friend_btn;   // 添加好友按钮

    // 聊天窗口界面（按方案要求：含返回好友列表+发送按钮）
    lv_obj_t *chat_scr;
    lv_obj_t *msg_area;         // 消息显示区
    lv_obj_t *msg_ta;           // 消息输入框
    lv_obj_t *send_btn;         // 发送按钮
    lv_obj_t *back_friend_btn;  // 返回好友列表按钮

    // 用户数据
    char user_acc[64];    // 当前登录账号
    char user_nick[64];   // 昵称
    char user_sign[128];  // 个性签名（扩展功能）
    bool is_login;        // 登录状态
}CHAT_ROOM_UI, *CHAT_ROOM_UI_P;

// 初始化聊天室（关联扩展UI控制）
int  Chat_Room_Init(EXT_UI_CTRL_P ext_uc);

// 释放聊天室资源
void Chat_Room_Free(EXT_UI_CTRL_P ext_uc);

// 注册请求（按方案要求：发送IP/端口/账号/密码/昵称到服务器）
int  Chat_Reg(EXT_UI_CTRL_P ext_uc, const char *acc, const char *pwd, const char *nick);

// 登录请求（发送账号密码，等待ACK应答）
int  Chat_Login(EXT_UI_CTRL_P ext_uc, const char *acc, const char *pwd);

// 获取在线好友列表（登录成功后请求）
int  Chat_Get_Friends(EXT_UI_CTRL_P ext_uc);

// 发送消息
int  Chat_Send_Msg(EXT_UI_CTRL_P ext_uc, const char *dst_acc, const char *msg);

#endif