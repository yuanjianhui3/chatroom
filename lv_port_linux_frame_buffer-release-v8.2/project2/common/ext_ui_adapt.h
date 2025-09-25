//@file ext_ui_adapt.h   扩展UI适配头文件

#ifndef _EXT_UI_ADAPT_H_
#define _EXT_UI_ADAPT_H_

#include <pthread.h>
#include <string.h>
#include "../../dir_look/dir_look.h"
#include "../chat_room/chat_room.h"
#include "../weather/weather.h"
#include "../smart_home/smart_home.h"

// 扩展UI控制结构体（关联原有框架与新模块，遵循原框架命名风格）
typedef struct Ext_Ui_Ctrl {
    struct Ui_Ctrl *base_uc;        // 原有框架UI指针（复用资源）
    CHAT_ROOM_UI_P chat_ui;         // 聊天室UI句柄
    WEATHER_UI_P weather_ui;        // 天气预报UI句柄
    SMART_HOME_UI_P home_ui;        // 智能家居UI句柄
    pthread_t mqtt_thread;          // 华为云MQTT通信线程
    bool mqtt_running;              // MQTT运行标志
    // 跨模块共享状态（温度/烟雾浓度，用于联动控制）
    int curr_temp;                  // 当前温度（天气预报→智能家居联动）
    int curr_smoke;                 // 当前烟雾浓度（智能家居→报警联动）
    pthread_mutex_t state_mutex;    // 状态共享互斥锁
}EXT_UI_CTRL, *EXT_UI_CTRL_P;

// 初始化扩展UI（关联原有Ui_Ctrl，无侵入修改）
int  Ext_Ui_Init(EXT_UI_CTRL_P ext_uc, struct Ui_Ctrl *base_uc);

// 首页添加扩展功能按钮（聊天室/天气预报/智能家居）
int  Ext_Start_Ui_Append(EXT_UI_CTRL_P ext_uc);

// 释放扩展UI资源
void Ext_Ui_Free(EXT_UI_CTRL_P ext_uc);

// 通用界面切换动画（复用原有框架视觉风格）
void Ext_Scr_Switch(lv_obj_t *old_scr, lv_obj_t *new_scr, lv_dir_t dir);

// 跨模块状态更新（如温度、烟雾浓度）
void Ext_State_Update(EXT_UI_CTRL_P ext_uc, const char *key, int value);

// 获取跨模块状态
int  Ext_State_Get(EXT_UI_CTRL_P ext_uc, const char *key);

#endif