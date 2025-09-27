//@file chat_adapt.h

#ifndef CHAT_ADAPT_H
#define CHAT_ADAPT_H

#include "../../dir_look/dir_look.h"

// 声明聊天室按钮添加函数
void Dir_Look_Append_ChatBtn(struct Ui_Ctrl *UC_P, lv_obj_t *scr_home);
static void Chat_Btn_Click(lv_event_t *e);
void Chat_Room_Init(struct Ui_Ctrl *uc, lv_obj_t *scr_home, bool connect_now);

#endif