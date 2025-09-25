//@file chat_adapter.c

#include "chat_adapter.h"
#include "../project2/chat_room/chat_room.h"
#include <string.h>

static lv_obj_t *chat_room_btn = NULL;
static struct Ui_Ctrl *g_uc = NULL;

/**
 * 聊天室按钮点击回调函数
 */
static void Chat_Room_Btn_Click(lv_event_t *e)
{
    Chat_Show_Main_UI();
}

/**
 * 初始化聊天室适配器
 */
int Chat_Adapter_Init(struct Ui_Ctrl *UC_P)
{
    if (!UC_P || !UC_P->start_ui_p || !UC_P->start_ui_p->start_ui) {
        return -1;
    }
    
    g_uc = UC_P;
    
    // 在首页添加聊天室按钮
    chat_room_btn = lv_btn_create(UC_P->start_ui_p->start_ui);
    if (!chat_room_btn) return -1;
    
    lv_obj_set_size(chat_room_btn, 100, 100);
    // 位置设置在合适的地方，避免与原有按钮冲突
    lv_obj_set_pos(chat_room_btn, 350, 220);
    lv_obj_set_style_bg_color(chat_room_btn, lv_color_hex(0x3a7bd5), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(chat_room_btn, 10, LV_STATE_DEFAULT);
    
    // 按钮上的图标（假设已有聊天室图标）
    lv_obj_t *chat_img = lv_img_create(chat_room_btn);
    if (chat_img) {
        lv_img_set_src(chat_img, "S:/images/chat_icon.png");
        lv_obj_center(chat_img);
    }
    
    // 按钮文字标签
    lv_obj_t *chat_label = lv_label_create(chat_room_btn);
    if (chat_label) {
        lv_label_set_text(chat_label, "聊天室");
        lv_obj_align(chat_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    }
    
    // 绑定点击事件
    lv_obj_add_event_cb(chat_room_btn, Chat_Room_Btn_Click, LV_EVENT_CLICKED, NULL);
    
    // 初始化聊天室
    return Chat_Room_Init(UC_P->start_ui_p->start_ui);
}

/**
 * 获取首页屏幕对象
 */
lv_obj_t *Dir_Look_Get_Main_Screen(void)
{
    if (g_uc && g_uc->start_ui_p) {
        return g_uc->start_ui_p->start_ui;
    }
    return NULL;
}
