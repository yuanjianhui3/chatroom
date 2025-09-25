//@file chat_room.c 聊天室模块

#include "chat_room.h"
#include "../common/iot_mqtt.h"
#include "../cJSON/cJSON.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

// 注册按钮回调
static void Chat_Reg_Cb(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    CHAT_ROOM_UI_P ui = ext_uc->chat_ui;

    // 获取输入框内容
    const char *acc = lv_textarea_get_text(ui->acc_ta);
    const char *pwd = lv_textarea_get_text(ui->pwd_ta);
    const char *nick = "默认昵称"; // 可扩展为输入框（方案扩展功能）

    // 注册请求（按方案要求上报用户全量信息）
    if(Chat_Reg(ext_uc, acc, pwd, nick) == 0) {
        lv_msgbox_create(NULL, "提示", "注册成功！", NULL, true);
    } else {
        lv_msgbox_create(NULL, "提示", "注册失败！", NULL, true);
    }
}

// 登录按钮回调
static void Chat_Login_Cb(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    CHAT_ROOM_UI_P ui = ext_uc->chat_ui;

    const char *acc = lv_textarea_get_text(ui->acc_ta);
    const char *pwd = lv_textarea_get_text(ui->pwd_ta);

    // 登录请求（等待华为云返回ACK=1，方案要求）
    if(Chat_Login(ext_uc, acc, pwd) == 1) {
        strncpy(ui->user_acc, acc, 63);
        ui->is_login = true;
        Chat_Get_Friends(ext_uc); // 获取在线好友
        Ext_Scr_Switch(ui->login_scr, ui->friend_scr, LV_DIR_BOTTOM);
    } else {
        lv_msgbox_create(NULL, "提示", "账号/密码错误！", NULL, true);
    }
}

// 好友列表项点击（进入聊天窗口）
static void Friend_Item_Cb(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    CHAT_ROOM_UI_P ui = ext_uc->chat_ui;

    // 获取选中好友账号（简化处理，实际需关联用户数据）
    lv_obj_t *item = lv_event_get_target(e);
    const char *friend_nick = lv_label_get_text(lv_obj_get_child(item, 1));

    // 切换到聊天窗口
    Ext_Scr_Switch(ui->friend_scr, ui->chat_scr, LV_DIR_BOTTOM);
}

// 发送消息回调
static void Chat_Send_Cb(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    CHAT_ROOM_UI_P ui = ext_uc->chat_ui;

    const char *msg = lv_textarea_get_text(ui->msg_ta);
    if(strlen(msg) == 0) return;

    // 群发（可扩展为选中好友单发，方案扩展功能）
    Chat_Send_Msg(ext_uc, "all", msg);

    // 本地显示自己的消息
    char show_msg[256];
    sprintf(show_msg, "我(%s): %s", ui->user_nick, msg);
    lv_label_t *msg_lab = lv_label_create(ui->msg_area);
    lv_label_set_text(msg_lab, show_msg);
    lv_obj_set_style_text_align(msg_lab, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);

    // 清空输入框
    lv_textarea_set_text(ui->msg_ta, "");
}

// 返回首页回调（好友列表→首页，方案要求）
static void Back_Home_Cb(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    CHAT_ROOM_UI_P ui = ext_uc->chat_ui;
    Ext_Scr_Switch(ui->friend_scr, ext_uc->base_uc->start_ui_p->start_ui, LV_DIR_TOP);
}

// 返回好友列表回调（聊天窗口→好友列表，方案要求）
static void Back_Friend_Cb(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    CHAT_ROOM_UI_P ui = ext_uc->chat_ui;
    Ext_Scr_Switch(ui->chat_scr, ui->friend_scr, LV_DIR_TOP);
}

// 构建注册登录界面
static void Chat_Build_LoginScr(EXT_UI_CTRL_P ext_uc)
{
    CHAT_ROOM_UI_P ui = ext_uc->chat_ui;
    LV_FONT_DECLARE(lv_font_simsun_20);

    // 根屏幕
    ui->login_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui->login_scr, lv_color_hex(0x2C3E50), LV_STATE_DEFAULT);

    // 账号输入框
    ui->acc_ta = lv_textarea_create(ui->login_scr);
    lv_obj_set_size(ui->acc_ta, 300, 40);
    lv_obj_set_pos(ui->acc_ta, 250, 150);
    lv_textarea_set_placeholder_text(ui->acc_ta, "请输入账号");
    lv_obj_set_style_text_font(ui->acc_ta, &lv_font_simsun_20, LV_STATE_DEFAULT);

    // 密码输入框（隐藏显示）
    ui->pwd_ta = lv_textarea_create(ui->login_scr);
    lv_obj_set_size(ui->pwd_ta, 300, 40);
    lv_obj_set_pos(ui->pwd_ta, 250, 220);
    lv_textarea_set_placeholder_text(ui->pwd_ta, "请输入密码");
    lv_textarea_set_password_mode(ui->pwd_ta, true);
    lv_obj_set_style_text_font(ui->pwd_ta, &lv_font_simsun_20, LV_STATE_DEFAULT);

    // 注册按钮
    ui->reg_btn = lv_btn_create(ui->login_scr);
    lv_obj_set_size(ui->reg_btn, 120, 35);
    lv_obj_set_pos(ui->reg_btn, 250, 300);
    lv_obj_t *reg_lab = lv_label_create(ui->reg_btn);
    lv_label_set_text(reg_lab, "注册");
    lv_obj_center(reg_lab);
    lv_obj_add_event_cb(ui->reg_btn, Chat_Reg_Cb, LV_EVENT_SHORT_CLICKED, ext_uc);

    // 登录按钮
    ui->login_btn = lv_btn_create(ui->login_scr);
    lv_obj_set_size(ui->login_btn, 120, 35);
    lv_obj_set_pos(ui->login_btn, 430, 300);
    lv_obj_t *login_lab = lv_label_create(ui->login_btn);
    lv_label_set_text(login_lab, "登录");
    lv_obj_center(login_lab);
    lv_obj_add_event_cb(ui->login_btn, Chat_Login_Cb, LV_EVENT_SHORT_CLICKED, ext_uc);
}

// 构建好友列表界面（按方案要求：返回首页+设置按钮）
static void Chat_Build_FriendScr(EXT_UI_CTRL_P ext_uc)
{
    CHAT_ROOM_UI_P ui = ext_uc->chat_ui;
    LV_FONT_DECLARE(lv_font_simsun_20);

    ui->friend_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui->friend_scr, lv_color_hex(0x2C3E50), LV_STATE_DEFAULT);

    // 好友列表（带滚动条）
    ui->friend_list = lv_list_create(ui->friend_scr);
    lv_obj_set_size(ui->friend_list, 400, 320);
    lv_obj_set_pos(ui->friend_list, 200, 60);
    lv_obj_set_style_bg_color(ui->friend_list, lv_color_hex(0xECF0F1), LV_STATE_DEFAULT);

    // 返回首页按钮（方案要求）
    ui->back_home_btn = lv_btn_create(ui->friend_scr);
    lv_obj_set_size(ui->back_home_btn, 80, 30);
    lv_obj_set_pos(ui->back_home_btn,30, 420);
    lv_obj_t *back_lab = lv_label_create(ui->back_home_btn);
    lv_label_set_text(back_lab, "返回首页");
    lv_obj_center(back_lab);
    lv_obj_add_event_cb(ui->back_home_btn, Back_Home_Cb, LV_EVENT_SHORT_CLICKED, ext_uc);

    // 设置按钮（方案扩展功能入口）
    ui->setting_btn = lv_btn_create(ui->friend_scr);
    lv_obj_set_size(ui->setting_btn, 80, 30);
    lv_obj_set_pos(ui->setting_btn, 690, 420);
    lv_obj_t *set_lab = lv_label_create(ui->setting_btn);
    lv_label_set_text(set_lab, "设置");
    lv_obj_center(set_lab);

    // 添加好友按钮（方案扩展功能）
    ui->add_friend_btn = lv_btn_create(ui->friend_scr);
    lv_obj_set_size(ui->add_friend_btn, 80, 30);
    lv_obj_set_pos(ui->add_friend_btn,690, 380);
    lv_obj_t *add_lab = lv_label_create(ui->add_friend_btn);
    lv_label_set_text(add_lab, "添加好友");
    lv_obj_center(add_lab);
}

// 构建聊天窗口界面（按方案要求：返回好友列表+发送按钮）
static void Chat_Build_ChatScr(EXT_UI_CTRL_P ext_uc)
{
    CHAT_ROOM_UI_P ui = ext_uc->chat_ui;
    LV_FONT_DECLARE(lv_font_simsun_20);

    ui->chat_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui->chat_scr, lv_color_hex(0x2C3E50), LV_STATE_DEFAULT);

    // 消息显示区（带滚动）
    ui->msg_area = lv_obj_create(ui->chat_scr);
    lv_obj_set_size(ui->msg_area, 600, 300);
    lv_obj_set_pos(ui->msg_area, 100, 50);
    lv_obj_set_style_bg_color(ui->msg_area, lv_color_hex(0xECF0F1), LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(ui->msg_area, LV_SCROLLBAR_MODE_AUTO);

    // 消息输入框
    ui->msg_ta = lv_textarea_create(ui->chat_scr);
    lv_obj_set_size(ui->msg_ta, 500, 40);
    lv_obj_set_pos(ui->msg_ta, 100, 380);
    lv_textarea_set_placeholder_text(ui->msg_ta, "请输入消息...");
    lv_obj_set_style_text_font(ui->msg_ta, &lv_font_simsun_20, LV_STATE_DEFAULT);

    // 发送按钮（方案要求）
    ui->send_btn = lv_btn_create(ui->chat_scr);
    lv_obj_set_size(ui->send_btn, 80, 40);
    lv_obj_set_pos(ui->send_btn, 620, 380);
    lv_obj_t *send_lab = lv_label_create(ui->send_btn);
    lv_label_set_text(send_lab, "发送");
    lv_obj_center(send_lab);
    lv_obj_add_event_cb(ui->send_btn, Chat_Send_Cb, LV_EVENT_SHORT_CLICKED, ext_uc);

    // 返回好友列表按钮（方案要求）
    ui->back_friend_btn = lv_btn_create(ui->chat_scr);
    lv_obj_set_size(ui->back_friend_btn, 80, 30);
    lv_obj_set_pos(ui->back_friend_btn,30, 420);
    lv_obj_t *back_friend_lab = lv_label_create(ui->back_friend_btn);
    lv_label_set_text(back_friend_lab, "返回好友");
    lv_obj_center(back_friend_lab);
    lv_obj_add_event_cb(ui->back_friend_btn, Back_Friend_Cb, LV_EVENT_SHORT_CLICKED, ext_uc);
}

// 初始化聊天室（遵循原框架命名风格）
int Chat_Room_Init(EXT_UI_CTRL_P ext_uc)
{
    ext_uc->chat_ui = (CHAT_ROOM_UI_P)malloc(sizeof(CHAT_ROOM_UI));
    memset(ext_uc->chat_ui, 0, sizeof(CHAT_ROOM_UI));
    ext_uc->chat_ui->is_login = false;

    // 构建三界面
    Chat_Build_LoginScr(ext_uc);
    Chat_Build_FriendScr(ext_uc);
    Chat_Build_ChatScr(ext_uc);

    return 0;
}

// 注册请求（按方案要求：发送IP/端口/账号/密码/昵称到华为云）
int Chat_Reg(EXT_UI_CTRL_P ext_uc, const char *acc, const char *pwd, const char *nick)
{
    // 获取设备IP（简化：从网卡获取）
    char ip[32] = {0};
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    getifaddrs(&ifap);
    for(ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr->sa_family == AF_INET && strcmp(ifa->ifa_name, "eth0") == 0) {
            sa = (struct sockaddr_in *)ifa->ifa_addr;
            strcpy(ip, inet_ntoa(sa->sin_addr));
            break;
        }
    }
    freeifaddrs(ifap);

    // 构建JSON数据（方案要求的全量字段）
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "cmd", 1);          // 1=注册
    cJSON_AddStringToObject(root, "acc", acc);        // 账号
    cJSON_AddStringToObject(root, "pwd", pwd);        // 密码
    cJSON_AddStringToObject(root, "nick", nick);      // 昵称
    cJSON_AddStringToObject(root, "ip", ip);          // IP地址
    cJSON_AddNumberToObject(root, "port", 8888);      // 端口（固定）
    char *json_str = cJSON_PrintUnformatted(root);

    // 发送到华为云MQTT上报主题
    int ret = Iot_Mqtt_Publish(IOT_MQTT_TOPIC_UP, json_str, strlen(json_str));

    cJSON_Delete(root);
    free(json_str);
    return ret;
}

// 登录请求（等待ACK应答，方案要求）
int Chat_Login(EXT_UI_CTRL_P ext_uc, const char *acc, const char *pwd)
{
    // 构建登录JSON
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "cmd", 2);          // 2=登录
    cJSON_AddStringToObject(root, "acc", acc);
    cJSON_AddStringToObject(root, "pwd", pwd);
    char *json_str = cJSON_PrintUnformatted(root);

    // 发送登录请求
    Iot_Mqtt_Publish(IOT_MQTT_TOPIC_UP, json_str, strlen(json_str));
    sleep(1); // 等待云端应答

    // 获取ACK（从MQTT接收缓存）
    int ack = Iot_Mqtt_Get_Ack();

    cJSON_Delete(root);
    free(json_str);
    return ack; // 1=成功，0=失败（方案要求）
}

// 获取在线好友列表（方案要求）
int Chat_Get_Friends(EXT_UI_CTRL_P ext_uc)
{
    // 发送获取好友请求
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "cmd", 3);          // 3=获取好友
    char *json_str = cJSON_PrintUnformatted(root);
    Iot_Mqtt_Publish(IOT_MQTT_TOPIC_UP, json_str, strlen(json_str));
    sleep(1);
    cJSON_Delete(root);
    free(json_str);

    // 从MQTT缓存获取好友列表（格式："acc1,nick1;acc2,nick2"）
    char *friends_str = Iot_Mqtt_Get_Friends();
    if(friends_str == NULL) return -1;

    // 解析并添加到LVGL列表
    char *token = strtok(friends_str, ";");
    while(token != NULL) {
        char *acc = strtok(token, ",");
        char *nick = strtok(NULL, ",");
        if(acc && nick) {
            // 列表项：图标+昵称（带账号提示）
            lv_obj_t *item = lv_list_add_btn(ext_uc->chat_ui->friend_list, LV_SYMBOL_USER, nick);
            lv_obj_set_user_data(item, acc);
            lv_obj_add_event_cb(item, Friend_Item_Cb, LV_EVENT_SHORT_CLICKED, ext_uc);
        }
        token = strtok(NULL, ";");
    }
    return 0;
}

// 发送消息
int Chat_Send_Msg(EXT_UI_CTRL_P ext_uc, const char *dst_acc, const char *msg)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "cmd", 4);          // 4=发送消息
    cJSON_AddStringToObject(root, "src_acc", ext_uc->chat_ui->user_acc); // 发送者
    cJSON_AddStringToObject(root, "dst_acc", dst_acc); // 接收者
    cJSON_AddStringToObject(root, "msg", msg);        // 消息内容
    char *json_str = cJSON_PrintUnformatted(root);

    int ret = Iot_Mqtt_Publish(IOT_MQTT_TOPIC_UP, json_str, strlen(json_str));

    cJSON_Delete(root);
    free(json_str);
    return ret;
}

// 释放资源
void Chat_Room_Free(EXT_UI_CTRL_P ext_uc)
{
    lv_obj_del(ext_uc->chat_ui->login_scr);
    lv_obj_del(ext_uc->chat_ui->friend_scr);
    lv_obj_del(ext_uc->chat_ui->chat_scr);
    free(ext_uc->chat_ui);
    ext_uc->chat_ui = NULL;
}