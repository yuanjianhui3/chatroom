//@file ui.h

#ifndef CHAT_UI_H
#define CHAT_UI_H

#include "lvgl/lvgl.h"
#include "chat_room.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化聊天室UI
 */
void Chat_Init_UI(void);

/**
 * 显示登录界面
 */
void Chat_Show_Login_UI(void);

/**
 * 显示注册界面
 */
void Chat_Show_Register_UI(void);

/**
 * 刷新好友列表UI
 */
void Chat_Refresh_Friend_List(void);

/**
 * 刷新聊天窗口UI
 */
void Chat_Refresh_Chat_Window(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CHAT_UI_H */
