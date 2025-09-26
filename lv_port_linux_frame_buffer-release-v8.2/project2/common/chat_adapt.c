//@file chat_adapt.c

//@file chat_adapt.c

#include "chat_adapt.h"
#include "../../lvgl/lvgl.h"
#include "../../dir_look/dir_look.h"
#include "../chat_room/chat_room.h"  // 添加这行以包含 Chat_Room_Init 的声明

// 聊天室按钮点击回调函数
static void chat_btn_click(lv_event_t *e) {
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)lv_event_get_user_data(e);
    // 直接进入聊天室登录界面，不立即连接服务器
    Chat_Room_Init(UC_P, UC_P->start_ui_p->start_ui, false);
}

// 完全复用相册按钮的设置方式创建聊天室按钮
void Dir_Look_Append_ChatBtn(struct Ui_Ctrl *UC_P, lv_obj_t *scr_home) {
    // 1. 获取相册按钮
    lv_obj_t *album_btn = UC_P->start_ui_p->enter_btn;
    
    if(album_btn == NULL) {
        printf("未找到相册按钮，聊天室按钮适配失败\n");
        return;
    }

    // 2. 获取相册按钮的位置和大小
    lv_coord_t album_x = lv_obj_get_x(album_btn);
    lv_coord_t album_y = lv_obj_get_y(album_btn);
    lv_coord_t album_w = lv_obj_get_width(album_btn);
    lv_coord_t album_h = lv_obj_get_height(album_btn);
    
    // 3. 创建聊天室按钮（完全复用相册按钮的设置方式）
    lv_obj_t *chat_btn = lv_btn_create(scr_home);
    lv_obj_set_size(chat_btn, album_w, album_h);  // 相同大小
    lv_obj_set_style_bg_opa(chat_btn, 0, 0);     // 相同透明度
    
    // 4. 计算位置：相册按钮正下方，间距30px（与相册标签相同）
    lv_obj_set_pos(chat_btn, album_x, album_y + album_h + 30);
    
    // 5. 创建聊天室按钮图片（复用相册图片设置方式）
    lv_obj_t *chat_img = lv_img_create(chat_btn);
    lv_img_set_src(chat_img, "S:/8080chat.png");  // 聊天室图标
    lv_obj_set_style_img_opa(chat_img, 180, 0);        // 相同透明度
    lv_obj_set_size(chat_img, album_w, album_h);        // 相同大小
    lv_obj_center(chat_img);
    
    // 6. 创建聊天室按钮标签（复用相册标签设置方式）
    lv_obj_t *chat_lab = lv_label_create(scr_home);
    
    // 计算标签位置：聊天室按钮下方30px（与相册标签相同）
    lv_coord_t chat_lab_x = album_x;
    lv_coord_t chat_lab_y = album_y + album_h + 30 + album_h + 30;
    
    lv_obj_set_pos(chat_lab, chat_lab_x, chat_lab_y);
    
    // 复用相册标签的字体和颜色设置
    LV_FONT_DECLARE(lv_myfont_kai_20);
    lv_obj_set_style_text_font(chat_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(chat_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(chat_lab, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_set_width(chat_lab, album_w);
    lv_label_set_long_mode(chat_lab, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_lab, "聊天室");
    
    // 7. 绑定点击事件
    lv_obj_add_event_cb(chat_btn, chat_btn_click, LV_EVENT_CLICKED, UC_P);
}