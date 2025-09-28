//@file chat_adapt.c

#include "chat_adapt.h"
#include "../../lvgl/lvgl.h"
#include "../../dir_look/dir_look.h"
#include "../chat_room/chat_room.h"  // 20250929新增以包含 Chat_Room_Init 的声明

// 聊天室按钮点击回调函数
static void Chat_Btn_Click(lv_event_t *e) 
{
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)lv_event_get_user_data(e);
    // 直接进入聊天室登录界面，不立即连接服务器
    Chat_Room_Init(UC_P, UC_P->start_ui_p->start_ui, false);
}

// 完全复用相册按钮的设置方式创建聊天室按钮
void Dir_Look_Append_ChatBtn(struct Ui_Ctrl *UC_P, lv_obj_t *scr_home) 
{
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
    
    // 4. 计算位置：相册按钮正下方，间距30px
    lv_obj_set_pos(chat_btn, album_x, album_y + album_h + 30);
    
    // 5. 创建聊天室按钮图片（复用相册图片设置方式）
    lv_obj_t *chat_img = lv_img_create(chat_btn);
    lv_img_set_src(chat_img, "S:/8080chat.png");  // 聊天室图标
    lv_obj_set_style_img_opa(chat_img, 180, 0);        // 相同透明度
    lv_obj_set_size(chat_img, album_w, album_h);        // 相同大小
    lv_obj_center(chat_img);
    
    // 6. 创建聊天室按钮标签（复用相册标签设置方式）
    lv_obj_t *chat_lab = lv_label_create(scr_home);
    
    // 计算标签位置：聊天室按钮下方30px
    lv_coord_t chat_lab_x = album_x + 4;
    lv_coord_t chat_lab_y = album_y + album_h + album_h + 30;
    
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
    lv_obj_add_event_cb(chat_btn, Chat_Btn_Click, LV_EVENT_CLICKED, UC_P);
}

// 20250928新增【静态辅助函数】创建全局唯一的触摸屏键盘（避免重复创建）

// 20250929新增：静态回调函数
static void Hide_Keyboard_Task(lv_event_t *e) {
    lv_obj_t *parent_scr = lv_event_get_current_target(e);
    lv_obj_t *keyboard = (lv_obj_t *)lv_obj_get_user_data(parent_scr); // 获取之前设置的键盘指针
    if (keyboard != NULL) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN); // 隐藏键盘
    }
}

static lv_obj_t *Create_Touch_Keyboard(lv_obj_t *parent_scr)
{
    static lv_obj_t *keyboard = NULL;
    if (keyboard == NULL) {
        // 1. 创建键盘实例（父容器为当前界面，确保随界面切换显示）
        keyboard = lv_keyboard_create(parent_scr);
        lv_obj_set_size(keyboard, lv_obj_get_width(parent_scr), 200); // 键盘高度200px，适配800*480屏幕
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0); // 键盘固定在屏幕底部
        
        // 2. 设置键盘样式（复用项目绿豆沙背景色，适配原有UI风格）
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(keyboard, &lv_myfont_kai_20, LV_STATE_DEFAULT); // 中英文字体统一
        
        // 3. 启用中英文切换（LVGL8.2默认支持，通过键盘左下角"ABC"按钮切换）
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER); // 默认文本模式（支持英文）
        
        // 4. 点击键盘外区域隐藏键盘
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN); // 默认隐藏
        lv_obj_add_event_cb(parent_scr,Hide_Keyboard_Task, LV_EVENT_CLICKED, keyboard);
        lv_obj_set_user_data(parent_scr, keyboard);     //20250929修改
    }
    return keyboard;
}

// 【静态辅助函数】输入框点击事件：弹出键盘
static void Textarea_Btn_Task(lv_event_t *e) {
    lv_obj_t *textarea = lv_event_get_current_target(e);
    lv_obj_t *parent_scr = lv_obj_get_parent(textarea);
    lv_obj_t *keyboard = Create_Touch_Keyboard(parent_scr);
    
    // 绑定输入框与键盘（输入内容自动同步到输入框）
    lv_keyboard_set_textarea(keyboard, textarea);
    // 显示键盘
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

// 【对外函数】绑定输入框与触摸屏键盘（供chat_room.c调用）
void Dir_Look_Bind_Textarea_Keyboard(lv_obj_t *textarea, lv_obj_t *parent_scr) {
    if (textarea == NULL || parent_scr == NULL) return;
    
    // 1. 为输入框添加点击事件（点击弹出键盘）
    lv_obj_add_event_cb(textarea, Textarea_Btn_Task, LV_EVENT_CLICKED, NULL);
    
    // 2. 优化输入框样式（确保触摸区域足够大，便于点击）
    lv_obj_set_style_pad_all(textarea, 10, LV_STATE_DEFAULT); // 增加内边距，扩大触摸区域
}

