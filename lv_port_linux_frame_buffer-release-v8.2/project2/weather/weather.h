//@file weather.h 天气模块头文件

#ifndef _WEATHER_H_
#define _WEATHER_H_

#include "../common/ext_ui_adapt.h"

// 天气预报UI结构体
typedef struct Weather_UI {
    lv_obj_t *weather_scr;      // 天气预报根屏幕
    lv_obj_t *city_lab;         // 城市显示
    lv_obj_t *temp_lab;         // 温度显示
    lv_obj_t *cond_lab;         // 天气状况显示
    lv_obj_t *back_btn;         // 返回首页按钮
    lv_timer_t *temp_check_timer; // 温度检测定时器（联动LED/蜂鸣器）
}WEATHER_UI, *WEATHER_UI_P;

// 初始化天气预报
int  Weather_Init(EXT_UI_CTRL_P ext_uc);

// 释放资源
void Weather_Free(EXT_UI_CTRL_P ext_uc);

// 获取实时天气（HTTP协议，方案要求）
int  Weather_Get_RealTime(EXT_UI_CTRL_P ext_uc);

// 温度联动控制（温度升高→LED闪烁频繁，达标→蜂鸣器响，方案要求）
void Weather_Temp_Linkage(EXT_UI_CTRL_P ext_uc, int temp);

#endif