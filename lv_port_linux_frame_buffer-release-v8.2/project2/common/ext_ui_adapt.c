//@file ext_ui_adapt.c   扩展UI适配实现

#include "ext_ui_adapt.h"

// 通用界面切换动画（上滑进入/下滑返回，贴合原有交互逻辑）
void Ext_Scr_Switch(lv_obj_t *old_scr, lv_obj_t *new_scr, lv_dir_t dir)
{
    if(old_scr == NULL || new_scr == NULL) return;
    lv_coord_t scr_h = lv_disp_get_ver_res(NULL);

    // 新屏幕初始位置（从底部进入/顶部退出）
    lv_obj_set_y(new_scr, (dir == LV_DIR_BOTTOM) ? scr_h : -scr_h);
    lv_scr_load_anim(new_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    // 旧屏幕退出动画
    lv_anim_t anim_out;
    lv_anim_init(&anim_out);
    lv_anim_set_var(&anim_out, old_scr);
    lv_anim_set_values(&anim_out, 0, (dir == LV_DIR_BOTTOM) ? -scr_h : scr_h);
    lv_anim_set_exec_cb(&anim_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&anim_out, 400);

    // 新屏幕进入动画
    lv_anim_t anim_in;
    lv_anim_init(&anim_in);
    lv_anim_set_var(&anim_in, new_scr);
    lv_anim_set_values(&anim_in, (dir == LV_DIR_BOTTOM) ? scr_h : -scr_h, 0);
    lv_anim_set_exec_cb(&anim_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&anim_in, 400);

    lv_anim_start(&anim_out);
    lv_anim_start(&anim_in);
}

// 聊天室按钮回调（首页→聊天室登录界面）
static void Chat_Room_Enter(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    if(ext_uc->chat_ui == NULL) {
        Chat_Room_Init(ext_uc); // 初始化聊天室UI
    }
    Ext_Scr_Switch(ext_uc->base_uc->start_ui_p->start_ui, ext_uc->chat_ui->login_scr, LV_DIR_BOTTOM);
}

// 天气预报按钮回调（首页→天气预报界面）
static void Weather_Enter(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    if(ext_uc->weather_ui == NULL) {
        Weather_Init(ext_uc);   // 初始化天气预报UI
        Weather_Get_RealTime(ext_uc); // 立即获取天气数据
    }
    Ext_Scr_Switch(ext_uc->base_uc->start_ui_p->start_ui, ext_uc->weather_ui->weather_scr, LV_DIR_BOTTOM);
}

// 智能家居按钮回调（首页→智能家居界面）
static void Smart_Home_Enter(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    if(ext_uc->home_ui == NULL) {
        Smart_Home_Init(ext_uc); // 初始化智能家居UI
    }
    Ext_Scr_Switch(ext_uc->base_uc->start_ui_p->start_ui, ext_uc->home_ui->home_scr, LV_DIR_BOTTOM);
}

// 首页添加扩展按钮（严格遵循原有UI布局风格，不修改原有代码）
int Ext_Start_Ui_Append(EXT_UI_CTRL_P ext_uc)
{
    struct Start_Ui *start_ui = ext_uc->base_uc->start_ui_p;
    LV_FONT_DECLARE(lv_font_simsun_20); // 复用原有字库

    // 1. 聊天室按钮（首页左侧，配图标+文字）
    start_ui->chat_btn = lv_btn_create(start_ui->start_ui);
    lv_obj_set_size(start_ui->chat_btn, 70, 70);
    lv_obj_set_pos(start_ui->chat_btn, 150, 180);
    lv_obj_set_style_bg_opa(start_ui->chat_btn, 0, LV_STATE_DEFAULT); // 透明背景

    lv_obj_t *chat_img = lv_img_create(start_ui->chat_btn);
    lv_img_set_src(chat_img, "S:/chat_icon.png"); // 提前放入开发板S盘
    lv_obj_center(chat_img);

    lv_obj_t *chat_lab = lv_label_create(start_ui->start_ui);
    lv_obj_align_to(chat_lab, start_ui->chat_btn, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_set_style_text_font(chat_lab, &lv_font_simsun_20, LV_STATE_DEFAULT);
    lv_label_set_text(chat_lab, "聊天室");
    lv_obj_add_event_cb(start_ui->chat_btn, Chat_Room_Enter, LV_EVENT_SHORT_CLICKED, ext_uc);

    // 2. 天气预报按钮（首页右侧）
    start_ui->weather_btn = lv_btn_create(start_ui->start_ui);
    lv_obj_set_size(start_ui->weather_btn, 70, 70);
    lv_obj_set_pos(start_ui->weather_btn, 580, 180);
    lv_obj_set_style_bg_opa(start_ui->weather_btn, 0, LV_STATE_DEFAULT);

    lv_obj_t *weather_img = lv_img_create(start_ui->weather_btn);
    lv_img_set_src(weather_img, "S:/weather_icon.png");
    lv_obj_center(weather_img);

    lv_obj_t *weather_lab = lv_label_create(start_ui->start_ui);
    lv_obj_align_to(weather_lab, start_ui->weather_btn, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_set_style_text_font(weather_lab, &lv_font_simsun_20, LV_STATE_DEFAULT);
    lv_label_set_text(weather_lab, "天气预报");
    lv_obj_add_event_cb(start_ui->weather_btn, Weather_Enter, LV_EVENT_SHORT_CLICKED, ext_uc);

    // 3. 智能家居按钮（首页底部）
    start_ui->home_btn = lv_btn_create(start_ui->start_ui);
    lv_obj_set_size(start_ui->home_btn, 70, 70);
    lv_obj_align(start_ui->home_btn, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_set_style_bg_opa(start_ui->home_btn, 0, LV_STATE_DEFAULT);

    lv_obj_t *home_img = lv_img_create(start_ui->home_btn);
    lv_img_set_src(home_img, "S:/home_icon.png");
    lv_obj_center(home_img);

    lv_obj_t *home_lab = lv_label_create(start_ui->start_ui);
    lv_obj_align_to(home_lab, start_ui->home_btn, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_set_style_text_font(home_lab, &lv_font_simsun_20, LV_STATE_DEFAULT);
    lv_label_set_text(home_lab, "智能家居");
    lv_obj_add_event_cb(start_ui->home_btn, Smart_Home_Enter, LV_EVENT_SHORT_CLICKED, ext_uc);

    return 0;
}

// 跨模块状态更新（线程安全）
void Ext_State_Update(EXT_UI_CTRL_P ext_uc, const char *key, int value)
{
    pthread_mutex_lock(&ext_uc->state_mutex);
    if(strcmp(key, "temp") == 0) {
        ext_uc->curr_temp = value;
    } else if(strcmp(key, "smoke") == 0) {
        ext_uc->curr_smoke = value;
    }
    pthread_mutex_unlock(&ext_uc->state_mutex);
}

// 跨模块状态获取（线程安全）
int Ext_State_Get(EXT_UI_CTRL_P ext_uc, const char *key)
{
    int value = 0;
    pthread_mutex_lock(&ext_uc->state_mutex);
    if(strcmp(key, "temp") == 0) {
        value = ext_uc->curr_temp;
    } else if(strcmp(key, "smoke") == 0) {
        value = ext_uc->curr_smoke;
    }
    pthread_mutex_unlock(&ext_uc->state_mutex);
    return value;
}

// 初始化扩展UI（遵循原框架函数命名风格：首字母大写+下划线）
int Ext_Ui_Init(EXT_UI_CTRL_P ext_uc, struct Ui_Ctrl *base_uc)
{
    memset(ext_uc, 0, sizeof(EXT_UI_CTRL));
    ext_uc->base_uc = base_uc;
    ext_uc->mqtt_running = true;
    pthread_mutex_init(&ext_uc->state_mutex, NULL);

    // 启动华为云MQTT通信线程
    if(pthread_create(&ext_uc->mqtt_thread, NULL, Iot_Mqtt_Run, ext_uc) != 0) {
        printf("MQTT线程创建失败\n");
        return -1;
    }
    pthread_detach(ext_uc->mqtt_thread);

    // 首页添加扩展按钮（核心适配点，无侵入修改原有代码）
    return Ext_Start_Ui_Append(ext_uc);
}

// 释放扩展UI资源
void Ext_Ui_Free(EXT_UI_CTRL_P ext_uc)
{
    ext_uc->mqtt_running = false;
    sleep(1); // 等待MQTT线程退出

    if(ext_uc->chat_ui != NULL) Chat_Room_Free(ext_uc);
    if(ext_uc->weather_ui != NULL) Weather_Free(ext_uc);
    if(ext_uc->home_ui != NULL) Smart_Home_Free(ext_uc);

    pthread_mutex_destroy(&ext_uc->state_mutex);
}