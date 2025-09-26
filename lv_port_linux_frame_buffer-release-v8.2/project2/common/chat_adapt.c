//@file chat_adapt.c

#include "chat_adapt.h"
#include "../chat_room/chat_room.h"

// 聊天室按钮点击回调
static void chat_btn_click(lv_event_t *e) {
    struct Ui_Ctrl *uc = (struct Ui_Ctrl *)lv_event_get_user_data(e);
    lv_obj_t *scr_home = lv_event_get_current_target(e)->parent;
    
    // 初始化并进入聊天室登录界面
    Chat_Room_Init(uc, scr_home);
}

// 向原有首页添加聊天室按钮（不修改dir_look.c）
void Dir_Look_Append_ChatBtn(struct Ui_Ctrl *UC_P, lv_obj_t *scr_home) {
    // 创建按钮（位置：首页底部，与原有按钮横向排列）
    lv_obj_t *chat_btn = lv_btn_create(scr_home);
    lv_obj_set_size(chat_btn, 120, 50);
    lv_obj_align_to(chat_btn, UC_P->back_btn, LV_ALIGN_OUT_RIGHT_MID, 20, 0); // 基于原有返回按钮定位
    
    // 设置按钮文本
    lv_obj_t *chat_label = lv_label_create(chat_btn);
    lv_label_set_text(chat_label, "聊天室");
    lv_obj_center(chat_label);
    
    // 绑定点击事件（传递UI控制指针）
    lv_obj_add_event_cb(chat_btn, chat_btn_click, LV_EVENT_CLICKED, UC_P);
}
