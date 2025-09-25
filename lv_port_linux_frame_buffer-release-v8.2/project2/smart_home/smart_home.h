//@file smart_home.h  智能家居模块头文件

#ifndef _SMART_HOME_H_
#define _SMART_HOME_H_

//网络编程头文件
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../common/ext_ui_adapt.h"

// 智能家居UI结构体
typedef struct Smart_Home_UI {
    lv_obj_t *home_scr;         // 智能家居根屏幕
    // LED控制（方案要求：按钮控制亮灭）
    lv_obj_t *led_btn;          // LED开关按钮
    lv_obj_t *led_state_lab;    // LED状态显示
    bool led_on;                // LED状态
    // 烟雾报警（方案要求：模拟数据+蜂鸣器+短信）
    lv_obj_t *smoke_lab;        // 烟雾浓度显示
    lv_obj_t *alarm_state_lab;  // 报警状态显示
    pthread_t smoke_thread;     // 烟雾检测线程
    // 远程呼叫（方案要求：HTTP发送短信）
    lv_obj_t *call_btn;         // 呼叫按钮
    // 公共
    lv_obj_t *back_btn;         // 返回首页按钮
    // 设备文件描述符（参考led_drv.c、gec6818_beep.c）
    int led_fd;                 // LED驱动FD
    int beep_fd;                // 蜂鸣器驱动FD
}SMART_HOME_UI, *SMART_HOME_UI_P;

// 初始化智能家居
int  Smart_Home_Init(EXT_UI_CTRL_P ext_uc);

// 释放资源
void Smart_Home_Free(EXT_UI_CTRL_P ext_uc);

// 模拟烟雾浓度检测（线程，方案要求）
void *Smart_Home_Smoke_Detect(void *arg);

// 远程呼叫（按方案要求：HTTP发送短信通知）
int  Smart_Home_Remote_Call(EXT_UI_CTRL_P ext_uc);

// LED控制（按方案要求：按钮控制亮灭）
void Smart_Home_LED_Ctrl(EXT_UI_CTRL_P ext_uc, bool on);

#endif