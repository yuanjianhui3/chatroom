//@file main.c

#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include <unistd.h>
#include <pthread.h>    // 线程头文件
#include <time.h>
#include <sys/time.h>
#include "./dir_look/dir_look.h"

#include "./project2/common/chat_adapt.h" // 20250926新增头文件引用

// -------20250903新增：线程函数声明（必须在main函数前声明）------ 09.32
// 线程函数：周期性获取开发板时间，更新第二个界面的时间标签
void *sys_time_update_thread(void *arg);
// -------------------------------------------------------------------

// 20250904 19.00新增：custom_tick_get函数前置声明（适配lv_conf.h中的LV_TICK_CUSTOM_SYS_TIME_EXPR）
uint32_t custom_tick_get(void);

#define DISP_BUF_SIZE (600 * 1024)

int main(void)
{
    /*LittlevGL init*/
    lv_init();

    /*Linux frame buffer device init*/
    fbdev_init();

    /*A small buffer for LittlevGL to draw the screen's content*/
    static lv_color_t buf[DISP_BUF_SIZE];

    /*Initialize a descriptor for the buffer*/
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);

    /*Initialize and register a display driver*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf   = &disp_buf;
    disp_drv.flush_cb   = fbdev_flush;
    disp_drv.hor_res    = 800;
    disp_drv.ver_res    = 480;
    lv_disp_drv_register(&disp_drv);

    evdev_init();
    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1); /*Basic initialization*/
    indev_drv_1.type = LV_INDEV_TYPE_POINTER;

    /*This function will be called periodically (by the library) to get the mouse position and state*/
    indev_drv_1.read_cb = evdev_read;
    lv_indev_t *mouse_indev = lv_indev_drv_register(&indev_drv_1);


    /*Set a cursor for the mouse*/
    LV_IMG_DECLARE(mouse_cursor_icon)
    lv_obj_t * cursor_obj = lv_img_create(lv_scr_act()); /*Create an image object for the cursor */
    lv_img_set_src(cursor_obj, &mouse_cursor_icon);           /*Set the image source*/
    lv_indev_set_cursor(mouse_indev, cursor_obj);             /*Connect the image  object to the driver*/


   /*********************************************************** */

    // 目录浏览器 Running ---- 显示第一个界面
    // 定义一个 界面管理的结构体变量
    struct Ui_Ctrl UC = {NULL,NULL,NULL,1,NULL};

    if(Dir_Look_Running(&UC) == -1)
    {
        printf("例程启动失败！\n");
        return -1;
    }

// ----------20250903新增：创建时间更新线程 ------ 09.34
    pthread_t time_thread;
    // 传入UC结构体指针，线程中通过该指针访问sys_time_lab标签
    int thread_create_ret = pthread_create(&time_thread, NULL, sys_time_update_thread, &UC);
    if(thread_create_ret != 0)
    {
        printf("创建时间更新线程失败！\n");
        return -1;
    }
    // 设置线程分离（无需主线程等待，退出时自动释放资源）
    pthread_detach(time_thread);
    // -------------------------------------------------------------------


   /*********************************************************** */

    /*Handle LitlevGL tasks (tickless mode)*/
    while(1) {
        lv_timer_handler();     //遍历哈希表中的每一个控件是否触发中断
        usleep(5000);
    }

    return 0;
}

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}

// ----- 20250903新增：时间更新线程函数实现 ------ 08.10
void *sys_time_update_thread(void *arg)
{
    // 从参数获取Ui_Ctrl结构体指针（线程创建时传入的&UC）
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)arg;
    if(UC_P == NULL || UC_P->dir_ui_p == NULL)
    {
        printf("线程参数无效：无法更新时间\n");
        pthread_exit(NULL);
    }

    // 时间格式化缓冲区（存储“2025-09-03 15:30:00”格式字符串）
    char time_buf[32] = {0};
    struct tm *local_time;
    struct timeval tv;

    // 周期性更新：每秒获取一次时间
    while(1)
    {
        // 1. 获取开发板当前时间（Linux系统调用）
        gettimeofday(&tv, NULL);
        // 转换为本地时间（年/月/日/时/分/秒）
        local_time = localtime(&tv.tv_sec);
        if(local_time == NULL)
        {
            printf("获取本地时间失败！\n");
            sleep(1);
            continue;
        }

        // 2. 格式化时间字符串（年-月-日 时:分:秒）
        snprintf(time_buf, sizeof(time_buf), 
                 "%04d-%02d-%02d %02d:%02d:%02d",
                 local_time->tm_year + 1900, // tm_year是从1900年开始的差值，需+1900
                 local_time->tm_mon + 1,    // tm_mon是0~11，需+1
                 local_time->tm_mday,      // 日（1~31）
                 local_time->tm_hour,      // 时（0~23）
                 local_time->tm_min,       // 分（0~59）
                 local_time->tm_sec);      // 秒（0~59）

        // 3. 更新第二个界面的时间标签（判断标签是否已创建）
        if(UC_P->dir_ui_p->sys_time_lab != NULL)
        {
            lv_label_set_text(UC_P->dir_ui_p->sys_time_lab, time_buf);
        }

        // 4. 每秒更新一次（线程休眠1秒）
        sleep(1);
    }

    pthread_exit(NULL);
}