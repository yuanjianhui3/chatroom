//@file ui.c 聊天室 UI 实现 

#include "ui.h"
#include "chat_room.h"
#include <stdio.h>
#include "../common/chat_adapter.h"

#include "../../lvgl/src/core/lv_obj_tree.h" // 显式包含子对象遍历函数的头文件

// 通用删除函数
static void auto_delete_obj(lv_timer_t *t) {
    lv_obj_del(t->user_data);
}

// 输入框对象
static lv_obj_t *account_input = NULL;
static lv_obj_t *passwd_input = NULL;
static lv_obj_t *nickname_input = NULL;
static lv_obj_t *msg_input = NULL;

// 获取聊天室全局对象
static ChatRoom_t *Get_Chat_Room(void)
{
    return Chat_Get_Room_Handle();
}

/**
 * 登录按钮回调函数
 */
static void Login_Btn_Click(lv_event_t *e)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room) return;
    
    const char *account = lv_textarea_get_text(account_input);
    const char *passwd = lv_textarea_get_text(passwd_input);
    
    if (account && passwd && *account != '\0' && *passwd != '\0') {
        int ret = Chat_Login(account, passwd);
        if (ret == 0) {
            // 登录成功，显示好友列表
            Chat_Show_Friend_List();
        } else {
            // 登录失败，显示提示
            lv_obj_t *alert = lv_label_create(chat_room->login_screen);
            lv_label_set_text(alert, "登录失败，账号或密码错误");
            lv_obj_align(alert, LV_ALIGN_CENTER, 0, 150);
            lv_obj_set_style_text_color(alert, lv_color_hex(0xff0000), LV_STATE_DEFAULT);

            lv_timer_create(auto_delete_obj, 2000, alert);  // 与注册按钮的提示删除逻辑保持一致

        }
    }
}

/**
 * 注册按钮回调函数（登录界面）
 */
static void To_Register_Btn_Click(lv_event_t *e)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room) return;
    
    // 隐藏登录界面，显示注册界面
    lv_obj_add_flag(chat_room->login_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(chat_room->register_screen, LV_OBJ_FLAG_HIDDEN);
}

/**
 * 注册提交按钮回调函数
 */
static void Register_Submit_Btn_Click(lv_event_t *e)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room) return;
    
    const char *account = lv_textarea_get_text(account_input);
    const char *passwd = lv_textarea_get_text(passwd_input);
    const char *nickname = lv_textarea_get_text(nickname_input);
    
    if (account && passwd && nickname && 
        *account != '\0' && *passwd != '\0' && *nickname != '\0') {
        
        int ret = Chat_Register(account, passwd, nickname);
        if (ret == 0) {
            // 注册成功，返回登录界面
            lv_obj_t *alert = lv_label_create(chat_room->register_screen);
            lv_label_set_text(alert, "注册成功，请登录");
            lv_obj_align(alert, LV_ALIGN_CENTER, 0, 150);
            lv_obj_set_style_text_color(alert, lv_color_hex(0x00ff00), LV_STATE_DEFAULT);

            lv_timer_create(auto_delete_obj, 2000, alert);  // 2000是延时毫秒数

        } else {
            // 注册失败
            lv_obj_t *alert = lv_label_create(chat_room->register_screen);
            lv_label_set_text(alert, "注册失败，请重试");
            lv_obj_align(alert, LV_ALIGN_CENTER, 0, 150);
            lv_obj_set_style_text_color(alert, lv_color_hex(0xff0000), LV_STATE_DEFAULT);

            lv_timer_create(auto_delete_obj, 2000, alert);  // 2000是延时毫秒数

        }
    }
}

/**
 * 返回登录按钮回调函数（注册界面）
 */
static void Back_To_Login_Btn_Click(lv_event_t *e)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room) return;
    
    // 隐藏注册界面，显示登录界面
    lv_obj_add_flag(chat_room->register_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(chat_room->login_screen, LV_OBJ_FLAG_HIDDEN);
}

/**
 * 好友列表项点击回调函数
 */
static void Friend_Item_Click(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    const char *account = (const char *)lv_obj_get_user_data(obj);
    
    if (account) {
        Chat_Show_Chat_Window(account);
    }
}

/**
 * 发送消息设置按钮回调函数
 */
static void Setting_Btn_Click(lv_event_t *e)
{
    // TODO: 实现设置功能
}

/**
 * 发送消息按钮回调函数
 */
static void Send_Msg_Btn_Click(lv_event_t *e)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room || !chat_room->current_chat_account[0]) return;
    
    const char *msg = lv_textarea_get_text(msg_input);
    if (msg && *msg != '\0') {
        Chat_Send_Msg(chat_room->current_chat_account, msg);
        lv_textarea_set_text(msg_input, "");
        Chat_Refresh_Chat_Window();
    }
}

/**
 * 返回好友列表按钮回调函数
 */
static void Back_To_Friend_Btn_Click(lv_event_t *e)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room) return;
    
    // 隐藏聊天窗口，显示好友列表
    lv_obj_add_flag(chat_room->chat_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(chat_room->friend_screen, LV_OBJ_FLAG_HIDDEN);
}

/**
 * 创建登录界面
 */
static void Create_Login_Screen(void)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room || !chat_room->main_screen) return;
    
    // 创建登录界面
    chat_room->login_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(chat_room->login_screen, lv_obj_get_width(chat_room->main_screen), 
                   lv_obj_get_height(chat_room->main_screen));
    lv_obj_set_style_bg_color(chat_room->login_screen, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);
    lv_obj_add_flag(chat_room->login_screen, LV_OBJ_FLAG_HIDDEN);
    
    // 标题
    lv_obj_t *title = lv_label_create(chat_room->login_screen);
    lv_label_set_text(title, "聊天室登录");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);
    
    // 账号输入
    lv_obj_t *account_label = lv_label_create(chat_room->login_screen);
    lv_label_set_text(account_label, "账号:");
    lv_obj_align(account_label, LV_ALIGN_TOP_LEFT, 100, 120);
    
    account_input = lv_textarea_create(chat_room->login_screen);
    lv_obj_set_size(account_input, 400, 50);
    lv_obj_align_to(account_input, account_label, LV_ALIGN_LEFT_MID, 60, 0);
    lv_textarea_set_placeholder_text(account_input, "请输入账号");
    
    // 密码输入
    lv_obj_t *passwd_label = lv_label_create(chat_room->login_screen);
    lv_label_set_text(passwd_label, "密码:");
    lv_obj_align(passwd_label, LV_ALIGN_TOP_LEFT, 100, 200);
    
    passwd_input = lv_textarea_create(chat_room->login_screen);
    lv_textarea_set_password_mode(passwd_input, true);
    lv_obj_set_size(passwd_input, 400, 50);
    lv_obj_align_to(passwd_input, passwd_label, LV_ALIGN_LEFT_MID, 60, 0);
    lv_textarea_set_placeholder_text(passwd_input, "请输入密码");
    
    // 登录按钮
    lv_obj_t *login_btn = lv_btn_create(chat_room->login_screen);
    lv_obj_set_size(login_btn, 100, 40);
    lv_obj_align(login_btn, LV_ALIGN_TOP_MID, -60, 280);
    lv_obj_add_event_cb(login_btn, Login_Btn_Click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *login_label = lv_label_create(login_btn);
    lv_label_set_text(login_label, "登录");
    lv_obj_center(login_label);
    
    // 注册按钮
    lv_obj_t *reg_btn = lv_btn_create(chat_room->login_screen);
    lv_obj_set_size(reg_btn, 100, 40);
    lv_obj_align(reg_btn, LV_ALIGN_TOP_MID, 60, 280);
    lv_obj_add_event_cb(reg_btn, To_Register_Btn_Click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *reg_label = lv_label_create(reg_btn);
    lv_label_set_text(reg_label, "注册");
    lv_obj_center(reg_label);
    
    // 返回按钮
    lv_obj_t *back_btn = lv_btn_create(chat_room->login_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(back_btn, Chat_Back_To_Home, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回首页");
    lv_obj_center(back_label);
}

/**
 * 创建注册界面
 */
static void Create_Register_Screen(void)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room || !chat_room->main_screen) return;
    
    // 创建注册界面
    chat_room->register_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(chat_room->register_screen, lv_obj_get_width(chat_room->main_screen), 
                   lv_obj_get_height(chat_room->main_screen));
    lv_obj_set_style_bg_color(chat_room->register_screen, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);
    lv_obj_add_flag(chat_room->register_screen, LV_OBJ_FLAG_HIDDEN);
    
    // 标题
    lv_obj_t *title = lv_label_create(chat_room->register_screen);
    lv_label_set_text(title, "用户注册");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    
    // 账号输入
    lv_obj_t *account_label = lv_label_create(chat_room->register_screen);
    lv_label_set_text(account_label, "账号:");
    lv_obj_align(account_label, LV_ALIGN_TOP_LEFT, 100, 100);
    
    // 复用登录界面的账号输入框
    lv_obj_set_parent(account_input, chat_room->register_screen);
    lv_obj_align_to(account_input, account_label, LV_ALIGN_LEFT_MID, 60, 0);
    
    // 密码输入
    lv_obj_t *passwd_label = lv_label_create(chat_room->register_screen);
    lv_label_set_text(passwd_label, "密码:");
    lv_obj_align(passwd_label, LV_ALIGN_TOP_LEFT, 100, 180);
    
    // 复用登录界面的密码输入框
    lv_obj_set_parent(passwd_input, chat_room->register_screen);
    lv_obj_align_to(passwd_input, passwd_label, LV_ALIGN_LEFT_MID, 60, 0);
    
    // 昵称输入
    lv_obj_t *nickname_label = lv_label_create(chat_room->register_screen);
    lv_label_set_text(nickname_label, "昵称:");
    lv_obj_align(nickname_label, LV_ALIGN_TOP_LEFT, 100, 260);
    
    nickname_input = lv_textarea_create(chat_room->register_screen);
    lv_obj_set_size(nickname_input, 400, 50);
    lv_obj_align_to(nickname_input, nickname_label, LV_ALIGN_LEFT_MID, 60, 0);
    lv_textarea_set_placeholder_text(nickname_input, "请输入昵称");
    
    // 注册按钮
    lv_obj_t *reg_btn = lv_btn_create(chat_room->register_screen);
    lv_obj_set_size(reg_btn, 100, 40);
    lv_obj_align(reg_btn, LV_ALIGN_TOP_MID, -60, 340);
    lv_obj_add_event_cb(reg_btn, Register_Submit_Btn_Click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *reg_label = lv_label_create(reg_btn);
    lv_label_set_text(reg_label, "注册");
    lv_obj_center(reg_label);
    
    // 返回按钮
    lv_obj_t *back_btn = lv_btn_create(chat_room->register_screen);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_TOP_MID, 60, 340);
    lv_obj_add_event_cb(back_btn, Back_To_Login_Btn_Click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回登录");
    lv_obj_center(back_label);
    
    // 返回首页按钮
    lv_obj_t *home_btn = lv_btn_create(chat_room->register_screen);
    lv_obj_set_size(home_btn, 80, 30);
    lv_obj_align(home_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(home_btn, Chat_Back_To_Home, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *home_label = lv_label_create(home_btn);
    lv_label_set_text(home_label, "返回首页");
    lv_obj_center(home_label);
}

/**
 * 创建好友列表界面
 */
static void Create_Friend_Screen(void)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room || !chat_room->main_screen) return;
    
    // 创建好友列表界面
    chat_room->friend_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(chat_room->friend_screen, lv_obj_get_width(chat_room->main_screen), 
                   lv_obj_get_height(chat_room->main_screen));
    lv_obj_set_style_bg_color(chat_room->friend_screen, lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    lv_obj_add_flag(chat_room->friend_screen, LV_OBJ_FLAG_HIDDEN);
    
    // 标题
    lv_obj_t *title = lv_label_create(chat_room->friend_screen);
    lv_label_set_text(title, "在线好友");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // 返回首页按钮
    lv_obj_t *back_btn = lv_btn_create(chat_room->friend_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(back_btn, Chat_Back_To_Home, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回首页");
    lv_obj_center(back_label);
    
    // 设置按钮
    lv_obj_t *setting_btn = lv_btn_create(chat_room->friend_screen);
    lv_obj_set_size(setting_btn, 80, 30);
    lv_obj_align(setting_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_event_cb(setting_btn, Setting_Btn_Click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *setting_label = lv_label_create(setting_btn);
    lv_label_set_text(setting_label, "设置");
    lv_obj_center(setting_label);
}

/**
 * 创建聊天窗口界面
 */
static void Create_Chat_Screen(void)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room || !chat_room->main_screen) return;
    
    // 创建聊天窗口界面
    chat_room->chat_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(chat_room->chat_screen, lv_obj_get_width(chat_room->main_screen), 
                   lv_obj_get_height(chat_room->main_screen));
    lv_obj_set_style_bg_color(chat_room->chat_screen, lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    lv_obj_add_flag(chat_room->chat_screen, LV_OBJ_FLAG_HIDDEN);
    
    // 标题栏
    lv_obj_t *title_bar = lv_obj_create(chat_room->chat_screen);
    lv_obj_set_size(title_bar, lv_obj_get_width(chat_room->chat_screen), 50);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x3a7bd5), LV_STATE_DEFAULT);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    // 返回按钮
    lv_obj_t *back_btn = lv_btn_create(title_bar);
    lv_obj_set_size(back_btn, 40, 30);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_add_event_cb(back_btn, Back_To_Friend_Btn_Click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回");
    lv_obj_center(back_label);
    
    // 聊天对象标题
    lv_obj_t *chat_title = lv_label_create(title_bar);
    lv_label_set_text(chat_title, "聊天中...");
    lv_obj_set_style_text_color(chat_title, lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    lv_obj_align(chat_title, LV_ALIGN_CENTER, 0, 0);
    
    // 消息显示区域
    lv_obj_t *msg_area = lv_obj_create(chat_room->chat_screen);
    lv_obj_set_size(msg_area, lv_obj_get_width(chat_room->chat_screen) - 20, 
                   lv_obj_get_height(chat_room->chat_screen) - 120);
    lv_obj_set_style_bg_color(msg_area, lv_color_hex(0xf5f5f5), LV_STATE_DEFAULT);
    lv_obj_align(msg_area, LV_ALIGN_TOP_MID, 0, 60);
    
    // 消息输入框
    msg_input = lv_textarea_create(chat_room->chat_screen);
    lv_obj_set_size(msg_input, lv_obj_get_width(chat_room->chat_screen) - 120, 50);
    lv_obj_align(msg_input, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_textarea_set_placeholder_text(msg_input, "请输入消息...");
    
    // 发送按钮
    lv_obj_t *send_btn = lv_btn_create(chat_room->chat_screen);
    lv_obj_set_size(send_btn, 100, 50);
    lv_obj_align(send_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(send_btn, Send_Msg_Btn_Click, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "发送");
    lv_obj_center(send_label);
}

/**
 * 初始化聊天室UI
 */
void Chat_Init_UI(void)
{
    // 创建各个界面
    Create_Login_Screen();
    Create_Register_Screen();
    Create_Friend_Screen();
    Create_Chat_Screen();
}

/**
 * 显示聊天室主界面
 */
void Chat_Show_Main_UI(void)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room) return;
    
    // 隐藏首页
    lv_obj_t *main_screen = Dir_Look_Get_Main_Screen();
    if (main_screen) {
        lv_obj_add_flag(main_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 根据登录状态显示不同界面
    if (chat_room->logged_in) {
        Chat_Show_Friend_List();
    } else {
        // 显示登录界面
        if (chat_room->login_screen) {
            lv_obj_clear_flag(chat_room->login_screen, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**
 * 显示好友列表
 */
void Chat_Show_Friend_List(void)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room || !chat_room->friend_screen) return;
    
    // 隐藏其他界面
    if (chat_room->login_screen) {
        lv_obj_add_flag(chat_room->login_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (chat_room->register_screen) {
        lv_obj_add_flag(chat_room->register_screen, LV_OBJ_FLAG_HIDDEN);
    }
    if (chat_room->chat_screen) {
        lv_obj_add_flag(chat_room->chat_screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 显示好友列表界面
    lv_obj_clear_flag(chat_room->friend_screen, LV_OBJ_FLAG_HIDDEN);
    
    // 刷新好友列表
    Chat_Refresh_Friend_List();
}

/**
 * 刷新好友列表UI
 */
void Chat_Refresh_Friend_List(void)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room || !chat_room->friend_screen) return;
    
    // 清除现有好友项（保留标题和按钮）
    lv_obj_t *child = lv_obj_get_child(chat_room->friend_screen, 0);  // 获取第一个子对象（索引0）
    while (child) {
        lv_obj_t *next = lv_obj_get_next_sibling(child);  // 获取下一个兄弟对象
        // 保留标题、返回按钮和设置按钮
        if ((lv_obj_get_style_text_font(child, LV_STATE_DEFAULT) != &lv_font_montserrat_24 &&
            lv_obj_get_width(child) != 80) || lv_obj_get_height(child) != 30) {
            lv_obj_del(child);
        }
        child = next;
    }
    
    // 添加好友项
    pthread_mutex_lock(&chat_room->mutex);
    for (int i = 0; i < chat_room->friend_count; i++) {
        // 跳过自己
        if (strcmp(chat_room->friends[i].account, chat_room->self.account) == 0) {
            continue;
        }
        
        lv_obj_t *friend_item = lv_btn_create(chat_room->friend_screen);
        lv_obj_set_size(friend_item, lv_obj_get_width(chat_room->friend_screen) - 40, 60);
        lv_obj_align(friend_item, LV_ALIGN_TOP_MID, 0, 80 + i * 70);
        lv_obj_add_event_cb(friend_item, Friend_Item_Click, LV_EVENT_CLICKED, NULL);
        
        // 保存好友账号
        lv_obj_set_user_data(friend_item, strdup(chat_room->friends[i].account));
        
        // 头像
        lv_obj_t *avatar = lv_obj_create(friend_item);
        lv_obj_set_size(avatar, 40, 40);
        lv_obj_set_style_radius(avatar, 20, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(avatar, lv_color_hex(0xdddddd), LV_STATE_DEFAULT);
        lv_obj_align(avatar, LV_ALIGN_LEFT_MID, 10, 0);
        
        // 在线状态指示
        lv_obj_t *online_indicator = lv_obj_create(avatar);
        lv_obj_set_size(online_indicator, 10, 10);
        lv_obj_set_style_radius(online_indicator, 5, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(online_indicator, 
                                 chat_room->friends[i].online ? lv_color_hex(0x00ff00) : lv_color_hex(0xcccccc), 
                                 LV_STATE_DEFAULT);
        lv_obj_align(online_indicator, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
        
        // 昵称和账号
        char info[128];
        snprintf(info, sizeof(info), "%s (%s)", 
                 chat_room->friends[i].nickname,
                 chat_room->friends[i].account);
        
        lv_obj_t *info_label = lv_label_create(friend_item);
        lv_label_set_text(info_label, info);
        lv_obj_align(info_label, LV_ALIGN_LEFT_MID, 60, 0);
    }
    pthread_mutex_unlock(&chat_room->mutex);
}

/**
 * 显示聊天窗口
 */
void Chat_Show_Chat_Window(const char *friend_account)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room || !friend_account || !chat_room->chat_screen) return;
    
    // 保存当前聊天对象
    strncpy(chat_room->current_chat_account, friend_account, sizeof(chat_room->current_chat_account)-1);
    
    // 隐藏好友列表，显示聊天窗口
    lv_obj_add_flag(chat_room->friend_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(chat_room->chat_screen, LV_OBJ_FLAG_HIDDEN);
    
    // 更新聊天窗口标题
    pthread_mutex_lock(&chat_room->mutex);
    for (int i = 0; i < chat_room->friend_count; i++) {
        if (strcmp(chat_room->friends[i].account, friend_account) == 0)
        {
            lv_obj_t *title = lv_obj_get_child(chat_room->chat_screen, 0);  //用 lv_obj_get_child 获取第一个子对象
            while (title) {
                if (lv_obj_get_width(title) == lv_obj_get_width(chat_room->chat_screen) && lv_obj_get_height(title) == 50){
                    lv_obj_t *title_label = lv_obj_get_child(title, 0);     //获取子对象索引0（第一个子对象）
                    while (title_label) {
                        if (lv_obj_get_style_text_color(title_label, LV_STATE_DEFAULT).full == 0xffffff) {
                            lv_label_set_text(title_label, chat_room->friends[i].nickname);
                            break;
                        }
                        title_label = lv_obj_get_next_sibling(title_label);     //用 lv_obj_get_next_sibling 遍历兄弟对象
                    }
                    break;
                }
                title = lv_obj_get_next_sibling(title);
            }
            break;
        }
    }
    pthread_mutex_unlock(&chat_room->mutex);
    
    // 刷新聊天内容
    Chat_Refresh_Chat_Window();
}

/**
 * 刷新聊天窗口UI
 */
void Chat_Refresh_Chat_Window(void)
{
    ChatRoom_t *chat_room = Get_Chat_Room();
    if (!chat_room || !chat_room->chat_screen) return;
    
    // 找到消息显示区域
    lv_obj_t *msg_area = NULL;
    lv_obj_t *child = lv_obj_get_child(chat_room->chat_screen, 0);  //获取第一个子对象（索引0）
    while (child) {
        if (lv_obj_get_style_bg_color(child, LV_STATE_DEFAULT).full == 0xf5f5f5) {
            msg_area = child;
            break;
        }
        child = lv_obj_get_next_sibling(child);     //用 lv_obj_get_next_sibling 遍历
    }
    
    if (!msg_area) return;
    
    // 清除现有消息
    lv_obj_clean(msg_area);
    
    // 显示消息记录
    pthread_mutex_lock(&chat_room->mutex);
    for (int i = 0; i < chat_room->msg_count; i++) {
        // 只显示与当前聊天对象相关的消息
        if ((strcmp(chat_room->msg_records[i].account, chat_room->current_chat_account) == 0) ||
            (chat_room->msg_records[i].is_self && 
             strcmp(chat_room->current_chat_account, chat_room->self.account) != 0)) {
            
            // 消息容器
            lv_obj_t *msg_cont = lv_obj_create(msg_area);
            lv_obj_set_width(msg_cont, lv_obj_get_width(msg_area) - 40);
            lv_obj_set_style_bg_opa(msg_cont, 0, LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(msg_cont, 0, LV_STATE_DEFAULT);
            
            // 消息气泡
            lv_obj_t *msg_bubble = lv_obj_create(msg_cont);
            lv_obj_set_style_radius(msg_bubble, 10, LV_STATE_DEFAULT);
            
            // 消息文本
            lv_obj_t *msg_label = lv_label_create(msg_bubble);
            lv_label_set_text(msg_label, chat_room->msg_records[i].msg);
            lv_obj_set_width(msg_label, lv_obj_get_width(msg_cont) - 20);
            lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
            lv_obj_align(msg_label, LV_ALIGN_TOP_LEFT, 10, 10);
            
            // 根据是否是自己发送的消息设置不同样式
            if (chat_room->msg_records[i].is_self) {
                // 自己发送的消息，靠右显示
                lv_obj_align(msg_cont, LV_ALIGN_TOP_RIGHT, 0, i * 80);
                lv_obj_set_style_bg_color(msg_bubble, lv_color_hex(0x99ccff), LV_STATE_DEFAULT);
                lv_obj_set_size(msg_bubble, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                lv_obj_align(msg_bubble, LV_ALIGN_RIGHT_MID, 0, 0);
            } else {
                // 收到的消息，靠左显示
                lv_obj_align(msg_cont, LV_ALIGN_TOP_LEFT, 0, i * 80);
                lv_obj_set_style_bg_color(msg_bubble, lv_color_hex(0xffffff), LV_STATE_DEFAULT);
                lv_obj_set_size(msg_bubble, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                lv_obj_align(msg_bubble, LV_ALIGN_LEFT_MID, 0, 0);
                
                // 显示发送者昵称
                lv_obj_t *name_label = lv_label_create(msg_cont);
                lv_label_set_text(name_label, chat_room->msg_records[i].nickname);
                lv_obj_set_style_text_font(name_label, &lv_font_montserrat_12, LV_STATE_DEFAULT);
                lv_obj_set_style_text_color(name_label, lv_color_hex(0x666666), LV_STATE_DEFAULT);
                lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 0, 0);
                lv_obj_align_to(msg_bubble, name_label, LV_ALIGN_BOTTOM_LEFT, 0, 5);
            }
        }
    }
    pthread_mutex_unlock(&chat_room->mutex);
}
