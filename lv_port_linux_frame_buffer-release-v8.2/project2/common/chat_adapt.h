//@file chat_adapt.h 

#ifndef CHAT_ADAPT_H
#define CHAT_ADAPT_H

#include "../../dir_look/dir_look.h"

// 为dir_look模块添加聊天室按钮（无侵入式扩展）
void Dir_Look_Append_ChatBtn(struct Ui_Ctrl *UC_P, lv_obj_t *scr_home);

#endif