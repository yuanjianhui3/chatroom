//@file chat_adapter.h

#ifndef CHAT_ADAPTER_H
#define CHAT_ADAPTER_H

#include "lvgl/lvgl.h"
#include "../dir_look/dir_look.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化聊天室适配器，在原有界面添加聊天室按钮
 * @param UC_P 原有UI控制结构体
 * @return 0:成功 -1:失败
 */
int Chat_Adapter_Init(struct Ui_Ctrl *UC_P);

/**
 * 获取首页屏幕对象
 * @return 首页lv_obj_t对象
 */
lv_obj_t *Dir_Look_Get_Main_Screen(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CHAT_ADAPTER_H */
