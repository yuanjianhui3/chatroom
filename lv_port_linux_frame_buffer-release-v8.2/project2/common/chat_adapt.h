//@file chat_adapt.h

#ifndef CHAT_ADAPT_H
#define CHAT_ADAPT_H

#include "../../dir_look/dir_look.h"

LV_FONT_DECLARE(lv_myfont_kai_20); //楷体字体

// 声明聊天室按钮添加函数
void Dir_Look_Append_ChatBtn(struct Ui_Ctrl *UC_P, lv_obj_t *scr_home);
static void Chat_Btn_Click(lv_event_t *e);

// 20250928新增；绑定输入框与触摸屏键盘（中英文输入）供 chat_room.c 调用
void Dir_Look_Bind_Textarea_Keyboard(lv_obj_t *textarea, lv_obj_t *parent_scr);

static void Hide_Keyboard_Task(lv_event_t *e);


#endif