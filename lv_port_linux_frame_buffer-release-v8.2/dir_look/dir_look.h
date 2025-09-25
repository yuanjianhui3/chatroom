//@file dir_look.h

#ifndef _DIR_LOOK_H_
#define _DIR_LOOK_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include "../lvgl/lvgl.h"
#include "../lvgl/demos/lv_demos.h"
#include "../lv_drivers/display/fbdev.h"
#include "../lv_drivers/indev/evdev.h"
#define  DEFAULT_SHOW_DIR "/"
// ------20250902 18.16
#include <fcntl.h>   // 文件操作相关函数
#include <unistd.h>  // 系统调用（lseek/read/close）
#include <sys/types.h> // off_t类型定义
// ----------------------------

#include <time.h>  // 20250905新增：用于time()函数

struct Ui_Ctrl ;

//第一个界面的控件管理结构体
typedef struct Statr_UI
{
    lv_obj_t * start_ui;//容器控件 800*480
    lv_obj_t * background;//背景图片控件
    lv_obj_t * tittle_lab;//主题标签
    lv_obj_t * enter_btn;//进入相册按钮
    lv_obj_t * enter_lab;//进入相册按钮标签
// -----------------------------------
    lv_obj_t * start_lab;//首页文本标签
    lv_obj_t * enter_img;//进入按钮图片控件

// -------------20250903新增 10.16
    lv_obj_t * emergency_btn;//进入紧急呼叫按钮
    lv_obj_t * emergency_lab;//进入紧急呼叫按钮标签

//------------20250903新增：设备信息按钮相关控件  13.52
    lv_obj_t * device_info_btn;//设备信息按钮
    lv_obj_t * device_info_img;//设备信息按钮图片
    lv_obj_t * device_info_lab;//设备信息按钮标签

    // 20250903新增：2048游戏按钮相关控件（相册按钮的左边按钮）
    lv_obj_t * game2048_btn;   // 2048游戏按钮
    lv_obj_t * game2048_img;   // 游戏按钮图片
    lv_obj_t * game2048_lab;   // 游戏按钮标签（“2048游戏”）

}START_UI,*START_UI_P;

//第二个界面的控件管理结构体
typedef struct Dir_UI
{
    lv_obj_t * dir_ui;//容器控件 800*480
    lv_obj_t * dir_list;//目录列表控件 300 X 450
    lv_obj_t * file_ui;//小容器   470 X 450
    lv_obj_t * exit_btn; // 跳到 第三个界面的按钮  exit
    lv_obj_t * img_ui;//第二个界面衍生出来的图片界面

// -----------------------------------20250902 17.19
    lv_obj_t * close_btn; // 关闭按钮
    lv_obj_t * close_lab; // 关闭按钮标签

    lv_obj_t * return_btn; // 第二个界面中的返回按钮
    lv_obj_t * return_lab; // 第二个界面中的返回按钮标签

    // ---------------------- 新增：图片切换核心变量 ------20250902 20.10
    char **img_file_list;   // 存储当前目录下所有图片路径（.jpg/.png/.gif）
    int current_img_idx;    // 当前显示图片的索引（从0开始）
    int img_file_count;     // 当前目录下图片总数
    lv_obj_t *current_img_obj; // 当前显示的图片控件（gif/img）
    // -------------------------------------------------------------------

    // ---------------------- 新增：当前目录路径标签（UI优化核心）------20250903 05.22
    lv_obj_t * current_dir_lab; // 显示当前所在目录路径（如“/home/pi/images”）
    
    // ---------------------- 新增：系统时间标签（显示开发板时间）------20250903 09.26
    lv_obj_t * sys_time_lab; // 显示开发板当前时间（如“2025-09-03 15:30:00”）
    // -------------------------------------------------------------------


}DIR_UI,*DIR_UI_P;


//第三个界面的控件管理结构体
typedef struct End_UI
{
   lv_obj_t * end_ui;//容器控件 800*480

   lv_obj_t * end_img; //结束界面图片
}END_UI,*END_UI_P;

// --------20250903新增：紧急呼叫界面控件管理结构体 ------ 10.00
typedef struct Emergency_UI
{
    lv_obj_t * emergency_ui;    // 紧急呼叫界面根容器（800*480）
    lv_obj_t * call_label;      // 显示“紧急呼叫中...”标签
    lv_obj_t * hangup_btn;      // 挂断按钮
    lv_obj_t * hangup_lab;      // 挂断按钮标签（“挂断”）
}EMERGENCY_UI,*EMERGENCY_UI_P;
// -------------------------------------------------------------------

// --------20250903新增：设备信息界面控件管理结构体 ------ 13.41
typedef struct Device_Info_UI
{
    lv_obj_t * device_info_ui;  // 设备信息界面根容器（800*480）
    lv_obj_t * return_btn;      // 返回首页按钮（同第二个界面return_btn样式）
    lv_obj_t * return_lab;      // 返回按钮标签（“return”）
    lv_obj_t * exit_btn;        // 退出到结束界面按钮（同第二个界面exit_btn样式）
    lv_obj_t * exit_lab;        // 退出按钮标签（“exit”）
    lv_obj_t * info_lab1;       // 设备信息标签1（设备型号）
    lv_obj_t * info_lab2;       // 设备信息标签2（LVGL版本）
    lv_obj_t * info_lab3;       // 设备信息标签3（屏幕分辨率）
    lv_obj_t * info_lab4;       // 设备信息标签4（系统版本）
}DEVICE_INFO_UI,*DEVICE_INFO_UI_P;
// -------------------------------------------------------------------

//存放目录按钮的名字 一个结构体存放一个
typedef struct dir_btn_inf
{
    struct Ui_Ctrl * UC_P;
    char new_dir_name[256*2]; //新的目录路径，即将读取他的所有文件夹名字更新到list

    struct dir_btn_inf * next;
    struct dir_btn_inf * prev;

}DBI,*DBI_P;

// --------20250905新增：2048游戏数据结构体------
typedef struct Game2048_Data
{
    lv_obj_t *game_root;       // 游戏界面根容器（800*480）
    lv_obj_t *btnm;            // 4x4按钮矩阵（显示数字）
    lv_obj_t *score_label;     // 分数显示标签
    lv_obj_t *game_over_label; // 游戏结束提示标签
    lv_obj_t *restart_btn;     // 重新开始按钮
    lv_obj_t *btn_labels[4][4];// 修复：保存每个按钮的标签（关键！）
    uint16_t matrix[4][4];     // 4x4游戏矩阵（1=2,2=4...11=2048）
    uint32_t score;            // 当前分数
    bool game_over;            // 游戏结束标志
} GAME_2048_DATA, *GAME_2048_DATA_P;

//界面管理结构体
struct Ui_Ctrl
{
    START_UI_P start_ui_p;//第一个界面控件指针
    DIR_UI_P   dir_ui_p;  //第二个界面控件指针
    END_UI_P   end_ui_p;  //第三个界面控件指针
    int        exit_mask; //退出标志位，初始化为1
    DBI_P      dir_btn_list_head;//目录按钮链表头节点
// ----------------------------20250903新增 10.01
    EMERGENCY_UI_P emergency_ui_p; // 新增：紧急呼叫界面指针

    DEVICE_INFO_UI_P device_info_ui_p; // 20250903新增：设备信息界面指针

    GAME_2048_DATA_P game2048_data; // 20250905新增：重构2048游戏数据
};

DBI_P Create_Node();//创建目录按钮头节点
int  Dir_Look_Running(struct Ui_Ctrl * UC_P); //目录浏览器例程总接口
int  Show_Start_Ui(struct Ui_Ctrl * UC_P); //显示第一个界面的函数
int  Show_Dir_Ui(struct Ui_Ctrl * UC_P);   //显示第二个界面的函数
int  Show_End_Ui(struct Ui_Ctrl * UC_P);   //显示第三个界面的函数
void Dir_Look_Free(struct Ui_Ctrl * UC_P);  //程序结束释放函数
void Enter_Btn_Task(lv_event_t * e); //第一个界面进入按钮的中断函数
void Exit_Btn_Task(lv_event_t * e);//第二个界面退出按钮的中断函数
void Dir_Btn_Task(lv_event_t * e); //目录按钮中断函数
void File_Btn_Task(lv_event_t * e);//文件按钮中断函数
void close_img_win(lv_event_t * e);//关闭图片窗口的按钮中断函数
bool Show_Dir_List(const char * obj_dir_path,struct Ui_Ctrl * UC_P);// 显示指定目录下的文件夹，第一次默认显示根目录下
bool  Head_Add_Node(DBI_P head_node,DBI_P new_node);//创建目录按钮头节点
bool  Destory_Dir_Btn_List(DBI_P head_node);

// -------------------------------------20250902 17.35
void Return_Btn_Task(lv_event_t * e);//第二个界面返回按钮的中断函数

// ------------------------20250902 20.11
// ---------------------- 新增：图片切换相关函数声明 ----------------------
// 扫描目录下所有支持的图片文件，填充图片列表
void Scan_Img_Files(const char *dir_path, struct Ui_Ctrl *UC_P);
// 上一张图片事件回调
void Prev_Img_Task(lv_event_t * e);
// 下一张图片事件回调
void Next_Img_Task(lv_event_t * e);
// ----------20250908新增：删除图片事件回调声明 ------04.27
void Delete_Img_Task(lv_event_t * e);


// ---------------------- 新增：紧急呼叫相关函数声明 ------20250903 10.03
    // 显示紧急呼叫界面
    int Show_Emergency_Ui(struct Ui_Ctrl * UC_P);

    // 首页紧急呼叫按钮回调（切换到紧急界面）
    void Emergency_Call_Task(lv_event_t * e);

    // 紧急界面挂断按钮回调（返回首页）
    void Hangup_Call_Task(lv_event_t * e);
// -------------------------------------------------------------------

// -----------20250903新增：设备信息界面相关函数声明 ------ 13.43
// 1. 创建设备信息界面
int Show_Device_Info_Ui(struct Ui_Ctrl * UC_P);
// 首页→设备信息界面（进入回调）
void Device_Info_Enter_Task(lv_event_t * e);
// 设备信息界面→首页（返回回调）
void Device_Info_Return_Task(lv_event_t * e);
// 设备信息界面→结束界面（退出回调）
void Device_Info_Exit_Task(lv_event_t * e);
// -------------------------------------------------------------------

// 界面回调函数
void Game2048_Enter_Task(lv_event_t *e);  // 首页→游戏
void Game2048_Return_Task(lv_event_t *e); // 游戏→首页
void Game2048_Exit_Task(lv_event_t *e);   // 游戏→结束界面

#endif