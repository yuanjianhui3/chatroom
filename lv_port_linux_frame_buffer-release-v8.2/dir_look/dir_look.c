//@file dir_look.c

#include "dir_look.h"

// 【新增】20250905：2048游戏静态函数声明（仅当前.c文件可见，避免头文件冲突）
static void game2048_init(GAME_2048_DATA_P game_data, struct Ui_Ctrl *UC_P);
static void game2048_add_random(GAME_2048_DATA_P game_data);
static void rotate_matrix(uint16_t matrix[4][4], uint8_t rotate_cnt);
static bool game2048_slide(GAME_2048_DATA_P game_data, lv_dir_t dir);
static void game2048_draw(GAME_2048_DATA_P game_data);
static void game2048_handle_event(lv_event_t *e);
static void game2048_restart_task(lv_event_t *e);
static void game2048_del_container(lv_timer_t *timer);
static void game2048_init_delay(lv_anim_t *a);

// 20250908新增补充Create_Node函数原型声明，消除“no previous prototype”警告
extern DBI_P Create_Node();

int  Dir_Look_Running(struct Ui_Ctrl * UC_P)
{
    printf("这是我的LVGL 目录浏览器！\n");

    //创建目录按钮链表头结点
    if((UC_P->dir_btn_list_head = Create_Node()) == (DBI_P)-1)
    {
        printf("创建目录按钮链表头结点 失败！\n");
        return -1;
    }

    //调用一个函数 显示第一个界面


    if(Show_Start_Ui(UC_P) == -1) //能把第一个界面的容器指针保存到UC里面
    {
        printf("显示初始界面失败！\n");
        return -1;
    }

    if(Show_Dir_Ui(UC_P) == -1) //能把第一个界面的容器指针保存到UC里面
    {
        printf("预加载目录界面失败！\n");
        return -1;
    }

    if(Show_End_Ui(UC_P) == -1) 
    {
        printf("预加结束界面失败！\n");
        return -1;
    }

    // ---------------------- 新增：预加载紧急呼叫界面 ------20250903 10.30
    if(Show_Emergency_Ui(UC_P) == -1) 
    {
        printf("预加载紧急呼叫界面失败！\n");
        return -1;
    }

    // ---------------------- 20250903新增：预加载设备信息界面 ------ 14.01
    if(Show_Device_Info_Ui(UC_P) == -1) 
    {
        printf("预加载设备信息界面失败！\n");
        return -1;
    }
    // -------------------------------------------------------------------

    return 0;
}

void Enter_Btn_Task(lv_event_t * e)
{
    // //设置第一个容器界面隐藏，设置第二个容器界面显示
     struct Ui_Ctrl * UC_P = (struct Ui_Ctrl *)e->user_data;
    
    //从下往上显示过度 500ms 拓展特效
    lv_obj_t * old_screen = UC_P->start_ui_p->start_ui;
    lv_obj_t * new_screen = UC_P->dir_ui_p->dir_ui;

    lv_coord_t screen_height = lv_disp_get_ver_res(NULL);
    
    lv_obj_set_y(new_screen, screen_height);// 新屏幕初始位置：屏幕底部--20250903
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    
    lv_anim_t a_slide_out;    // 旧屏幕动画：向上滑出（Y=0→-screen_height）
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, -screen_height);
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 500);
    
    lv_anim_t a_slide_in;    // 新屏幕动画：从下往上滑入（Y=screen_height→0）
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, screen_height, 0);
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 500);
    
    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);

    //lv_scr_load_anim(UC_P->dir_ui_p->dir_ui,LV_SCR_LOAD_ANIM_FADE_ON,500,0,true);

    return ;
}


//第二个界面退出按钮的中断函数
void Exit_Btn_Task(lv_event_t * e)
{
     // //设置第一个容器界面隐藏，设置第二个容器界面显示
    struct Ui_Ctrl * UC_P = (struct Ui_Ctrl *)e->user_data;
    
    //从下往上显示过度 500ms 拓展特效    20250902 17.55
    lv_obj_t * old_screen = UC_P->dir_ui_p->dir_ui;
    lv_obj_t * new_screen = UC_P->end_ui_p->end_ui;

    lv_coord_t screen_height = lv_disp_get_ver_res(NULL);
    
    lv_obj_set_y(new_screen, screen_height);// 新屏幕初始位置：屏幕底部---20250903
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    
    // 旧屏幕动画：向上滑出（Y=0→-screen_height）---------20250903
    lv_anim_t a_slide_out;
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, -screen_height);// 方向：下→上，要带负号
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 500);
    
    lv_anim_t a_slide_in;// 新屏幕动画：从下往上滑入（Y=screen_height→0）
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, screen_height, 0);
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 500);
    
    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);

    UC_P->exit_mask = 0;
    return ;
}

// --------------------------------------20250902 17.30
//第二个界面返回首页的中断函数
void Return_Btn_Task(lv_event_t * e)
{
     // //设置第一个容器界面隐藏，设置第二个容器界面显示
    struct Ui_Ctrl * UC_P = (struct Ui_Ctrl *)e->user_data;
    
    //从上往下显示过度 500ms 拓展特效    20250902 17.55-----20250903 
    lv_obj_t * old_screen = UC_P->dir_ui_p->dir_ui;// 旧屏幕：目录页
    lv_obj_t * new_screen = UC_P->start_ui_p->start_ui;// 新屏幕：首页

    lv_coord_t screen_height = lv_disp_get_ver_res(NULL);// 获取屏幕高度（如480）
    
    // ----  20250903新增修改：新屏幕初始位置→屏幕顶部（Y=-screen_height，不可见）
    lv_obj_set_y(new_screen, -screen_height);
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    
    lv_anim_t a_slide_out;
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, screen_height);// 方向：上→下，不用带符号
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 500);// 动画时间不变，与进入动画保持500ms

    // -----20250903新增修改：新屏幕动画→从上往下滑入（Y=-screen_height→0，进入屏幕）    
    lv_anim_t a_slide_in;
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, -screen_height, 0);// 方向：上→下，带负号
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 500);
    
    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);

    UC_P->exit_mask = 0;
    return ;
}

// ---------------------------------------

//目录按钮中断函数
void Dir_Btn_Task(lv_event_t * e)
{
    DBI_P btn_inf = (DBI_P)e->user_data;
    printf("即将显示新的目录%s\n",btn_inf->new_dir_name);// / 而不是 /IOT/..


    //更新list控件的目录项
    //清除旧按钮
    lv_obj_clean(btn_inf->UC_P->dir_ui_p->dir_list);

    //清除小容器的按钮
    lv_obj_clean(btn_inf->UC_P->dir_ui_p->file_ui);


    //提前备份按钮节点中数据资源，因为后面要摧毁链表
    char new_dir_name[256*2] = "\0";
    strcpy(new_dir_name,btn_inf->new_dir_name);
    struct Ui_Ctrl * UC_P = btn_inf->UC_P;


    //顺便把旧按钮对应的堆空间free调用 摧毁链表 剩下 头结点
    if(Destory_Dir_Btn_List(btn_inf->UC_P->dir_btn_list_head) == false)
    {
        printf("摧毁链表失败！\n");
        return ;
    }

    //再添加新按钮
    Show_Dir_List(new_dir_name,UC_P);

    // ---------------------- 新增：更新顶部当前目录路径标签 ------20250903 06.40
    if(UC_P->dir_ui_p->current_dir_lab != NULL)
    {
        char dir_text[256] = {0};
        sprintf(dir_text, "当前目录：%s", new_dir_name);
        lv_label_set_text(UC_P->dir_ui_p->current_dir_lab, dir_text);
    }
    // -------------------------------------------------------------------
    return ;
}


//关闭图片窗口的按钮中断函数
void close_img_win(lv_event_t * e)
{
    struct Ui_Ctrl * UC_P = (struct Ui_Ctrl *)e->user_data;
    if(UC_P == NULL || UC_P->dir_ui_p == NULL) return; //20250902 21.04

    //取消第二个界面隐藏
    lv_obj_clear_flag(UC_P->dir_ui_p->dir_ui,LV_OBJ_FLAG_HIDDEN);

    //要不要设置重新加载 第二个界面
    lv_scr_load(UC_P->dir_ui_p->dir_ui);

    //如果删除的是当前显示的容器载体控件，不能先删除，而是先加载新的载体容器在删除----20250908 06.41
    if (UC_P->dir_ui_p->img_ui != NULL && lv_obj_is_valid(UC_P->dir_ui_p->img_ui))
    {
        lv_obj_del(UC_P->dir_ui_p->img_ui);
        UC_P->dir_ui_p->img_ui = NULL; // 新增：立即置空，避免后续重复删除
    }

    // ------------新增：释放图片列表内存 --------20250902 21.05
    if(UC_P->dir_ui_p->img_file_list != NULL)
    {
        for(int i=0; i<UC_P->dir_ui_p->img_file_count; i++)
        {
            if(UC_P->dir_ui_p->img_file_list[i] != NULL)
            {
                free(UC_P->dir_ui_p->img_file_list[i]);
                UC_P->dir_ui_p->img_file_list[i] = NULL;
            }
        }
        free(UC_P->dir_ui_p->img_file_list);
        UC_P->dir_ui_p->img_file_list = NULL;
    }
    // --------------------------
    // 重置图片切换状态
    UC_P->dir_ui_p->img_file_count = 0;
    UC_P->dir_ui_p->current_img_idx = -1;
    UC_P->dir_ui_p->current_img_obj = NULL;// 强制置空，避免野指针
    // ---------------------------------------20250903新增 12.41
    UC_P->dir_ui_p->img_file_list = NULL;    // 二次确认列表指针（防止残留野指针）

    return ;
}

//文件按钮中断函数
void File_Btn_Task(lv_event_t * e)
{
    DBI_P btn_inf = (DBI_P)e->user_data;

    struct Ui_Ctrl *UC_P = btn_inf->UC_P;   //新增20250902 20.17

    //设置第二个界面隐藏
    lv_obj_add_flag(btn_inf->UC_P->dir_ui_p->dir_ui,LV_OBJ_FLAG_HIDDEN);

    //创建一个新的800*480的容器载体
    btn_inf->UC_P->dir_ui_p->img_ui = lv_obj_create(NULL);  //lv_scr_act()

    // ---------------------- 新增：图片原图纯色背景配置 ------20250903 06.18
    lv_obj_t *img_ui = btn_inf->UC_P->dir_ui_p->img_ui;

    // 设置背景色（可替换为 lv_color_hex(0xEEEEEE) 浅灰、青草绿：#E3EDCD、绿豆沙：#C7EDCC、杏仁黄：#FAF9DE
    lv_obj_set_style_bg_color(img_ui, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    // 设置背景不透明度（255=完全不透明，0=透明，根据需求调整）
    lv_obj_set_style_bg_opa(img_ui, 255, LV_STATE_DEFAULT);

    // 设置容器圆角（优化视觉，与按钮圆角风格统一）
    lv_obj_set_style_radius(img_ui, 4, LV_STATE_DEFAULT);
    // -------------------------------------------------------------------

    char * obj_p = strrchr(btn_inf->new_dir_name,'.');
    if(obj_p == NULL) return;
    
    char file_path[256*2] = "\0";//存放有卷标的路径

    lv_obj_t *img_obj = NULL;   //新增20250902 20.17

    // ---------------------- 新增：扫描当前目录图片，构建图片列表 ------20250902 20.19
    char current_dir[256*2] = {0};
    char *last_slash = strrchr(btn_inf->new_dir_name, '/');
    if(last_slash != NULL)
    {
        if(last_slash == btn_inf->new_dir_name) // 根目录文件（如"/test.jpg"）
            strcpy(current_dir, "/");
        else // 子目录文件（如"/dir/test.jpg"）
        {
            strncpy(current_dir, btn_inf->new_dir_name, last_slash - btn_inf->new_dir_name);
            current_dir[last_slash - btn_inf->new_dir_name] = '\0';
        }
    }
    else // 无斜杠（当前目录文件）
        strcpy(current_dir, ".");
    Scan_Img_Files(current_dir, UC_P); // 扫描当前目录图片
    // -------------------------------------------------------------------

    // ------20250902 20.38新增保存当前图片控件指针 -----20250908 06.35修改
    if(strcmp(obj_p, ".gif") == 0) // 动图（指针保存）
    {
        img_obj = lv_gif_create(UC_P->dir_ui_p->img_ui);
        if (img_obj == NULL) // 新增：创建失败处理
        {
            printf("创建动图控件失败，返回目录\n");
            lv_obj_del(UC_P->dir_ui_p->img_ui); // 清理已创建的容器
            lv_obj_clear_flag(UC_P->dir_ui_p->dir_ui, LV_OBJ_FLAG_HIDDEN); // 显示目录界面
            lv_scr_load(UC_P->dir_ui_p->dir_ui);
            return;
        }
        sprintf(file_path, "S:%s", btn_inf->new_dir_name);
        lv_gif_set_src(img_obj, file_path);
        UC_P->dir_ui_p->current_img_obj = img_obj;
    }
    else if((strcmp(obj_p, ".jpg") == 0) || (strcmp(obj_p, ".png") == 0)) // 静图
    {
        img_obj = lv_img_create(UC_P->dir_ui_p->img_ui);
        if (img_obj == NULL) // 新增：创建失败处理
        {
            printf("创建静图控件失败，返回目录\n");
            lv_obj_del(UC_P->dir_ui_p->img_ui);
            lv_obj_clear_flag(UC_P->dir_ui_p->dir_ui, LV_OBJ_FLAG_HIDDEN);
            lv_scr_load(UC_P->dir_ui_p->dir_ui);
            return;
        }
        sprintf(file_path, "S:%s", btn_inf->new_dir_name);
        lv_img_set_src(img_obj, file_path);
        UC_P->dir_ui_p->current_img_obj = img_obj;
    }
    else 
        return;

    //确保img_obj非空后再调用lv_obj_center
    lv_obj_center(img_obj);

    // ----------- 20250903修改：定位当前图片在列表中的索引 ------------13.14
    if(UC_P->dir_ui_p->img_file_list != NULL && UC_P->dir_ui_p->img_file_count > 0)
    {
        for(int i=0; i<UC_P->dir_ui_p->img_file_count; i++)
        {
            // 新增：先检查列表元素非空，避免strcmp访问NULL，预防段错误
            if(UC_P->dir_ui_p->img_file_list[i] == NULL)
                continue;
            if(strcmp(UC_P->dir_ui_p->img_file_list[i], file_path) == 0)
            {
                UC_P->dir_ui_p->current_img_idx = i;
                break;
            }
        }
    }
    // ------------------------------------------------------
    // ---------------------- 新增：创建"上一张"按钮 ----------------------
    lv_obj_t *prev_btn = lv_btn_create(UC_P->dir_ui_p->img_ui);
    lv_obj_set_size(prev_btn, 60, 40);        // 按钮大小
    lv_obj_set_pos(prev_btn, 10, 220);        // 左侧中间位置（适配800*480屏幕）x原值50
    lv_obj_set_style_bg_opa(prev_btn, 100, 0); // 透明度（半透明）
    // 上一张按钮标签
    lv_obj_t *prev_lab = lv_label_create(prev_btn);
    lv_label_set_text(prev_lab, "上一张");
    lv_obj_center(prev_lab);
    // 字体
    LV_FONT_DECLARE(lv_myfont_kai_20);
    lv_obj_set_style_text_font(prev_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    // 绑定上一张事件
    lv_obj_add_event_cb(prev_btn, Prev_Img_Task, LV_EVENT_SHORT_CLICKED, UC_P);
    // -------------------------------------------------------------------

    // ---------------------- 新增：创建"下一张"按钮 ----------------------
    lv_obj_t *next_btn = lv_btn_create(UC_P->dir_ui_p->img_ui);
    lv_obj_set_size(next_btn, 60, 40);        // 按钮大小
    lv_obj_set_pos(next_btn, 730, 220);       // 右侧中间位置（适配800*480屏幕）x原值690
    lv_obj_set_style_bg_opa(next_btn, 100, 0); // 透明度（半透明）
    // 下一张按钮标签
    lv_obj_t *next_lab = lv_label_create(next_btn);
    lv_label_set_text(next_lab, "下一张");
    lv_obj_center(next_lab);
    // 字体
    LV_FONT_DECLARE(lv_myfont_kai_20);    
    lv_obj_set_style_text_font(next_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    // 绑定下一张事件
    lv_obj_add_event_cb(next_btn, Next_Img_Task, LV_EVENT_SHORT_CLICKED, UC_P);
    // -------------------------------------------------------------------
    
    // ---------------------- 20250908新增：创建"删除"按钮 ---------04.30
    lv_obj_t *del_btn = lv_btn_create(UC_P->dir_ui_p->img_ui);
    lv_obj_set_size(del_btn, 60, 40);        // 与上/下一张按钮尺寸一致
    lv_obj_set_pos(del_btn, 730, 280);       // 在下一张按钮左侧（间距10px，适配800*480屏幕）
    lv_obj_set_style_bg_opa(del_btn, 100, 0); // 半透明背景（与其他按钮风格统一）

    // 删除按钮标签
    lv_obj_t *del_lab = lv_label_create(del_btn);
    lv_label_set_text(del_lab, "删除");
    lv_obj_center(del_lab);

    // 字体：复用楷体20号（与其他按钮标签统一）
    LV_FONT_DECLARE(lv_myfont_kai_20);    
    lv_obj_set_style_text_font(del_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);

    // 绑定删除事件（传入UC_P，用于访问图片列表和界面控件）
    lv_obj_add_event_cb(del_btn, Delete_Img_Task, LV_EVENT_SHORT_CLICKED, UC_P);
    // -------------------------------------------------------------------

    //给窗口创建关闭按钮
    lv_obj_t * close_btn = lv_btn_create(btn_inf->UC_P->dir_ui_p->img_ui);
    // -----------------------------------------------
    //设置按钮位置
    lv_obj_set_pos(close_btn,10,450);
    //设置按钮透明度
    lv_obj_set_style_bg_opa(close_btn,178,0);

    //设置关闭按钮大小
    lv_obj_set_size(close_btn,50,30);

    //创建按钮标签基于关闭按钮为父类
    btn_inf->UC_P->dir_ui_p->close_lab = lv_label_create(close_btn);

    //设置标签居中
    lv_obj_center(btn_inf->UC_P->dir_ui_p->close_lab);

    //设置按钮标签的文本 是 "close"
    lv_label_set_text(btn_inf->UC_P->dir_ui_p->close_lab,"close");

    // ----------------------------------------------------
    //设置关闭按钮中断函数
    lv_obj_add_event_cb(close_btn,close_img_win,LV_EVENT_SHORT_CLICKED,btn_inf->UC_P);

    //加载图片界面
    lv_scr_load(btn_inf->UC_P->dir_ui_p->img_ui);

    return ;
}

// ---------------------- 新增：上一张图片回调 -----------------20250902 20.50
void Prev_Img_Task(lv_event_t * e)
{
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)e->user_data;
    // -----------------------------20250902 21.28
    // 1. 全指针链检查：避免访问NULL指针
    if(UC_P == NULL || 
       UC_P->dir_ui_p == NULL || 
       UC_P->dir_ui_p->img_file_list == NULL ||  // 检查图片列表指针
       UC_P->dir_ui_p->current_img_obj == NULL)   // 检查当前图片控件指针
    {
        printf("非法指针：跳过上一张\n");
        return;
    }

    // 2. 图片数量有效性检查：避免列表为空却切换
    if(UC_P->dir_ui_p->img_file_count <= 1)
    {
        printf("图片数量≤1：无需切换\n");
        return;
    }

    // 3. 处理索引未初始化（-1）的情况
    if(UC_P->dir_ui_p->current_img_idx < 0)
    {
        UC_P->dir_ui_p->current_img_idx = 0;  // 默认定位到第一张
    }

    // -----------------------------------------

    // 图片列表无效或只有1张图，无需切换
    if(UC_P->dir_ui_p->img_file_list == NULL || UC_P->dir_ui_p->img_file_count <= 1)
        return;

    // 索引循环：0 → 最后一张
    UC_P->dir_ui_p->current_img_idx--;
    if(UC_P->dir_ui_p->current_img_idx < 0)
        UC_P->dir_ui_p->current_img_idx = UC_P->dir_ui_p->img_file_count - 1;

    // 20250908 06.05新增修改更新图片显示：
    // 第一步检查当前图片控件是否已被释放（如用户先点关闭再点上一张）
    if (UC_P->dir_ui_p->current_img_obj == NULL)
    {
        printf("上一张：当前图片控件已释放，退出\n");
        return;
    }

    // 第二步检查图片列表是否有效
    const char *new_path = NULL;
    if (UC_P->dir_ui_p->img_file_list != NULL && 
        UC_P->dir_ui_p->current_img_idx >= 0 && 
        UC_P->dir_ui_p->current_img_idx < UC_P->dir_ui_p->img_file_count)
    {
        new_path = UC_P->dir_ui_p->img_file_list[UC_P->dir_ui_p->current_img_idx];
    }
    else
    {
        printf("上一张：列表无效或索引越界，重置为0\n");
        UC_P->dir_ui_p->current_img_idx = 0;
        // 重置后再次检查列表
        if (UC_P->dir_ui_p->img_file_list != NULL && UC_P->dir_ui_p->img_file_count > 0)
            new_path = UC_P->dir_ui_p->img_file_list[0];
        else
        {
            printf("上一张：无图片列表，退出\n");
            return;
        }
    }

    // 第三步确保路径非空
    if(new_path == NULL)
    {
        printf("上一张：图片路径为空，退出\n");
        return;
    }

    // 第四步检查文件后缀（避免无效路径导致的strrchr返回NULL）
    char *ext = strrchr(new_path, '.');
    if(ext == NULL)
    {
        printf("上一张：图片无后缀，退出\n");
        return;
    }

    // 第五步更新图片前，检查LVGL控件是否仍有效（防止控件被意外删除）
    if (!lv_obj_is_valid(UC_P->dir_ui_p->current_img_obj))
    {
        printf("上一张：当前图片控件已失效，退出\n");
        UC_P->dir_ui_p->current_img_obj = NULL; // 置空避免后续重复访问
        return;
    }

    // 安全更新图片
    if (strcmp(ext, ".gif") == 0)
        lv_gif_set_src(UC_P->dir_ui_p->current_img_obj, new_path);
    else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".png") == 0)
        lv_img_set_src(UC_P->dir_ui_p->current_img_obj, new_path);

    lv_obj_center(UC_P->dir_ui_p->current_img_obj);
}

// ---------------------- 新增：下一张图片回调 ---------20250902 20.51
void Next_Img_Task(lv_event_t * e)
{
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)e->user_data;
    // ---------------------20250902 21.32
    // 1. 全指针链检查
    if(UC_P == NULL || 
       UC_P->dir_ui_p == NULL || 
       UC_P->dir_ui_p->img_file_list == NULL || 
       UC_P->dir_ui_p->current_img_obj == NULL)
    {
        printf("非法指针：跳过下一张\n");
        return;
    }

    // 2. 图片数量有效性检查
    if(UC_P->dir_ui_p->img_file_count <= 1)
    {
        printf("图片数量≤1：无需切换\n");
        return;
    }

    // 3. 处理索引未初始化
    if(UC_P->dir_ui_p->current_img_idx < 0)
    {
        UC_P->dir_ui_p->current_img_idx = 0;
    }
    // -----------------------------------

    // 图片列表无效或只有1张图，无需切换
    if(UC_P->dir_ui_p->img_file_list == NULL || UC_P->dir_ui_p->img_file_count <= 1)
        return;

    // 索引循环：最后一张 → 0  计算下一张图片索引
    UC_P->dir_ui_p->current_img_idx++;
    if(UC_P->dir_ui_p->current_img_idx >= UC_P->dir_ui_p->img_file_count)
        UC_P->dir_ui_p->current_img_idx = 0;

    // 更新图片显示
    const char *new_path = UC_P->dir_ui_p->img_file_list[UC_P->dir_ui_p->current_img_idx];

    // 20250903新增：若路径为NULL，重置索引并返回，避免后续非法访问 12.47
    if(new_path == NULL)
    {
        printf("图片路径为空：重置索引\n");
        UC_P->dir_ui_p->current_img_idx = 0;
        new_path = UC_P->dir_ui_p->img_file_list[0];  //  fallback到第一张图

        // 20250903新增：最终确保new_path非空---------13.19
        if(new_path == NULL)
        {
            printf("下一张：图片路径为空，退出\n");
            return;
        }
    }
        // ------------------------------

    char *ext = strrchr(new_path, '.');
    if(ext == NULL) return;

    if(strcmp(ext, ".gif") == 0)
        lv_gif_set_src(UC_P->dir_ui_p->current_img_obj, new_path); // 动图更新
    else if(strcmp(ext, ".jpg") == 0 || strcmp(ext, ".png") == 0)
        lv_img_set_src(UC_P->dir_ui_p->current_img_obj, new_path); // 静图更新

        lv_obj_center(UC_P->dir_ui_p->current_img_obj); // 保持居中
}
// -------------------------------------------------------------------

// ---------------------- 新增：删除图片核心回调 -----------------20250908
void Delete_Img_Task(lv_event_t * e)
{
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)e->user_data;
    // 全指针有效性检查（避免访问NULL导致崩溃）
    if (UC_P == NULL || 
        UC_P->dir_ui_p == NULL || 
        UC_P->dir_ui_p->img_file_list == NULL || 
        UC_P->dir_ui_p->current_img_obj == NULL)
    {
        printf("删除图片：非法指针，跳过操作\n");
        return;
    }

    // 检查当前图片索引有效性（避免越界）
    if (UC_P->dir_ui_p->current_img_idx < 0 || 
        UC_P->dir_ui_p->current_img_idx >= UC_P->dir_ui_p->img_file_count)
    {
        printf("删除图片：索引无效（%d），跳过操作\n", UC_P->dir_ui_p->current_img_idx);
        return;
    }

    // 提取实际文件路径（LVGL路径前缀为"S:"，需去掉以适配Linux系统调用）
    const char *lv_img_path = UC_P->dir_ui_p->img_file_list[UC_P->dir_ui_p->current_img_idx];
    if (lv_img_path == NULL || strncmp(lv_img_path, "S:", 2) != 0)
    {
        printf("删除图片：路径格式错误（%s），跳过操作\n", lv_img_path);
        return;
    }
    const char *real_file_path = lv_img_path + 2; // 去掉"S:"前缀，得到实际路径（如"/test.jpg"）

    // 提取当前目录（用于删除后重新扫描图片列表）
    char current_dir[256 * 2] = {0};
    char *last_slash = strrchr(real_file_path, '/');
    if (last_slash != NULL)
    {
        if (last_slash == real_file_path) // 根目录文件（如"/a.jpg"）
            strcpy(current_dir, "/");
        else // 子目录文件（如"/dir/b.png"）
        {
            strncpy(current_dir, real_file_path, last_slash - real_file_path);
            current_dir[last_slash - real_file_path] = '\0';
        }
    }
    else // 无斜杠（当前目录文件）
        strcpy(current_dir, ".");

    // 调用Linux系统调用unlink删除文件
    if (unlink(real_file_path) == -1)
    {
        perror("删除图片失败（unlink）"); // 打印错误原因（如权限不足、文件不存在）
        return;
    }
    printf("成功删除图片：%s\n", real_file_path);

    // 重新扫描当前目录图片，更新图片列表（自动释放旧列表内存）
    Scan_Img_Files(current_dir, UC_P);

    // 20250908新增修改：扫描后先检查图片列表是否为NULL（即使count>0，列表也可能分配失败）
    if (UC_P->dir_ui_p->img_file_count > 0 && UC_P->dir_ui_p->img_file_list == NULL)
    {
        printf("删除图片后：图片数量>0但列表为NULL，返回目录\n");
        close_img_win(e);
        return;
    }

    // 新增：无剩余图片或列表分配失败，均关闭窗口
    if (UC_P->dir_ui_p->img_file_count == 0 || UC_P->dir_ui_p->img_file_list == NULL)
    {
        printf("删除图片后：无剩余图片或列表无效，关闭图片窗口\n");
        close_img_win(e);
        return;
    }
    // ----------
    // 索引越上界：指向最后一张；索引越下界：指向第一张
    if (UC_P->dir_ui_p->current_img_idx >= UC_P->dir_ui_p->img_file_count)
    {
        UC_P->dir_ui_p->current_img_idx = UC_P->dir_ui_p->img_file_count - 1;
    }
    else if (UC_P->dir_ui_p->current_img_idx < 0)
    {
        UC_P->dir_ui_p->current_img_idx = 0;
    }
    // -------------------------------------------

    // 显示新的当前图片：第一步先检查图片列表是否存在---20250908新增修改 05.12
    if (UC_P->dir_ui_p->img_file_list == NULL)
    {
        printf("删除图片后：图片列表为NULL，返回目录\n");
        close_img_win(e);
        return;
    }

    // 第二步检查当前索引对应的列表元素是否非空（避免malloc失败导致的NULL元素）
    const char *new_lv_path = UC_P->dir_ui_p->img_file_list[UC_P->dir_ui_p->current_img_idx];
    if (new_lv_path == NULL)
    {
        printf("删除图片后：索引%d对应的图片路径为空，返回目录\n", UC_P->dir_ui_p->current_img_idx);
        close_img_win(e);
        return;
    }

    // 根据图片类型更新显示（GIF/静图分别处理）
    char *ext = strrchr(new_lv_path, '.');
    if (ext == NULL)
    {
        printf("删除图片后：新图片路径无后缀（%s），返回目录\n", new_lv_path);
        close_img_win(e);
        return;
    }
    // ------------------------------------------------------------------

    if (strcmp(ext, ".gif") == 0)
        lv_gif_set_src(UC_P->dir_ui_p->current_img_obj, new_lv_path); // 动图更新
    else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".png") == 0)
        lv_img_set_src(UC_P->dir_ui_p->current_img_obj, new_lv_path); // 静图更新

    lv_obj_center(UC_P->dir_ui_p->current_img_obj); // 保持图片居中显示
}
// -------------------------------------------------------------------

// ---------------------- 新增：紧急呼叫按钮回调（首页→紧急界面） ------20250903 10.24
void Emergency_Call_Task(lv_event_t * e)
{
    struct Ui_Ctrl * UC_P = (struct Ui_Ctrl *)e->user_data;
    if(UC_P == NULL || UC_P->start_ui_p == NULL || UC_P->emergency_ui_p == NULL)
    {
        printf("紧急呼叫：非法指针\n");
        return;
    }

    // 复用原有界面切换动画逻辑：从下往上滑（500ms）
    lv_obj_t * old_screen = UC_P->start_ui_p->start_ui;   // 旧屏幕：首页
    lv_obj_t * new_screen = UC_P->emergency_ui_p->emergency_ui; // 新屏幕：紧急界面
    lv_coord_t screen_height = lv_disp_get_ver_res(NULL);

    // 新屏幕初始位置：屏幕底部（不可见）
    lv_obj_set_y(new_screen, screen_height);
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    // 旧屏幕动画：向上滑出（Y=0→-screen_height）
    lv_anim_t a_slide_out;
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, -screen_height);
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 500);

    // 新屏幕动画：从下往上滑入（Y=screen_height→0）
    lv_anim_t a_slide_in;
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, screen_height, 0);
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 500);

    // 启动动画
    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);
}
// -------------------------------------------

// ---------------------- 新增：挂断按钮回调（紧急界面→首页） ------20250903 10.26
void Hangup_Call_Task(lv_event_t * e)
{
    struct Ui_Ctrl * UC_P = (struct Ui_Ctrl *)e->user_data;
    if(UC_P == NULL || UC_P->emergency_ui_p == NULL || UC_P->start_ui_p == NULL)
    {
        printf("挂断呼叫：非法指针\n");
        return;
    }

    // 复用原有界面切换动画逻辑：从上往下滑（500ms），与Return_Btn_Task动画范式一致
    lv_obj_t * old_screen = UC_P->emergency_ui_p->emergency_ui; // 旧屏幕：紧急界面
    lv_obj_t * new_screen = UC_P->start_ui_p->start_ui;        // 新屏幕：首页
    lv_coord_t screen_height = lv_disp_get_ver_res(NULL); // 获取屏幕高度（文档内默认800*480，即screen_height=480）
    
    // 新屏幕初始位置→屏幕顶部（Y=-screen_height，不可见，避开初始显示）
    lv_obj_set_y(new_screen, -screen_height);
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    
    // 旧屏幕动画：向下滑出（Y=0→screen_height，移出屏幕底部，与Return_Btn_Task旧屏逻辑一致）
    lv_anim_t a_slide_out;
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, screen_height); // 方向：上→下，无负号
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 500); // 动画时间500ms，与文档内其他切换动画统一
    
    // 新屏幕动画：从上往下滑入（Y=-screen_height→0，进入屏幕，与Return_Btn_Task新屏逻辑一致）
    lv_anim_t a_slide_in;
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, -screen_height, 0); // 方向：上→下，带负号
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 500);
    
    // 同步启动动画，确保滑出与滑入流畅衔接
    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);

    return ;
}
// --------------------------------

// --------20250903新增：首页→设备信息界面回调 ------ 13.58
void Device_Info_Enter_Task(lv_event_t * e)
{
    struct Ui_Ctrl * UC_P = (struct Ui_Ctrl *)e->user_data;
    if(UC_P == NULL || UC_P->start_ui_p == NULL || UC_P->device_info_ui_p == NULL)
    {
        printf("设备信息进入：非法指针\n");
        return;
    }

    // 复用从下往上滑动画（500ms，与进入目录界面一致）
    lv_obj_t * old_screen = UC_P->start_ui_p->start_ui;   // 旧屏幕：首页
    lv_obj_t * new_screen = UC_P->device_info_ui_p->device_info_ui; // 新屏幕：设备信息界面
    lv_coord_t screen_height = lv_disp_get_ver_res(NULL);

    // 新屏幕初始位置：屏幕底部（不可见）
    lv_obj_set_y(new_screen, screen_height);
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    // 旧屏幕动画：向上滑出（Y=0→-screen_height）
    lv_anim_t a_slide_out;
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, -screen_height);
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 500);

    // 新屏幕动画：从下往上滑入（Y=screen_height→0）
    lv_anim_t a_slide_in;
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, screen_height, 0);
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 500);

    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);
}
// -------------------------------------------------------------------

//显示第一个界面
int Show_Start_Ui(struct Ui_Ctrl * UC_P)
{
    UC_P->start_ui_p = (START_UI_P)malloc(sizeof(START_UI));
    if(UC_P->start_ui_p == (START_UI_P)NULL)
    {
        perror("malloc start ui p ...");
        return -1;
    }

    memset(UC_P->start_ui_p,0,sizeof(START_UI));

    //创建第一个界面的 容器控件
    UC_P->start_ui_p->start_ui = lv_obj_create((lv_obj_t *)NULL);
    printf("-----1 第一个容器：%p\n",UC_P);
    
    //创建一个静态图片控件
    UC_P->start_ui_p->background = lv_img_create(UC_P->start_ui_p->start_ui);

    //设置图片控件的路径
    lv_img_set_src(UC_P->start_ui_p->background,"S:/800480sky.png");

    //要在 当前活动屏幕 中 创建一个标签  显示LVGL--Linux Directory Browser    
    UC_P->start_ui_p->tittle_lab = lv_label_create(UC_P->start_ui_p->start_ui);

    //设置标题标签文本
    lv_obj_set_style_text_font(UC_P->start_ui_p->tittle_lab, &lv_font_montserrat_20, LV_STATE_DEFAULT); // 用默认字体
    lv_label_set_text(UC_P->start_ui_p->tittle_lab,"LVGL--Linux Directory Browser");

    // 设置标签文字颜色，状态为默认状态（LV_STATE_DEFAULT）------新增20250903 05.08
    lv_obj_set_style_text_color(UC_P->start_ui_p->tittle_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);

    //设置标签为长模式
    lv_label_set_long_mode(UC_P->start_ui_p->tittle_lab,LV_LABEL_LONG_SCROLL_CIRCULAR);
    //设置标题标签宽度
    lv_obj_set_width(UC_P->start_ui_p->tittle_lab,200);

    //设置标题标签位置
    lv_obj_set_pos(UC_P->start_ui_p->tittle_lab,300,10);

// -----------------------------------------------------------------
    // //创建一个标签 基于 当前活动屏幕
    UC_P->start_ui_p->start_lab = lv_label_create(UC_P->start_ui_p->start_ui);

    // //自己定义一个结构体变量
    LV_FONT_DECLARE(lv_myfont_kai_20)

    // //设置字体结构体是我们的 简体楷体20号结构体
    lv_obj_set_style_text_font(UC_P->start_ui_p->start_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);

    // 补充：设置字体颜色（白色，适配背景图，避免看不见）----20250903
    lv_obj_set_style_text_color(UC_P->start_ui_p->start_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    // 补充：设置文本居中
    lv_obj_set_style_text_align(UC_P->start_ui_p->start_lab, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    //---------------------------

    // //设置要显示的中文
    lv_label_set_text(UC_P->start_ui_p->start_lab,"基于LVGL的嵌入式Linux开发");

    lv_obj_set_size(UC_P->start_ui_p->start_lab,200,30);

    //设置标签为长模式
    lv_label_set_long_mode(UC_P->start_ui_p->start_lab,LV_LABEL_LONG_SCROLL_CIRCULAR);

    //设置标题标签位置
    lv_obj_set_pos(UC_P->start_ui_p->start_lab,300,150);
// ------------------------------------------------------------------

    //要在 当前活动屏幕 中 创建一个按钮
    UC_P->start_ui_p->enter_btn = lv_btn_create(UC_P->start_ui_p->start_ui);

    //设置进入按钮大小
    lv_obj_set_size(UC_P->start_ui_p->enter_btn,80,80);

    lv_obj_set_style_bg_opa(UC_P->start_ui_p->enter_btn,0,0);//设置进入按钮透明度

    //设置进入按钮居中
    lv_obj_center(UC_P->start_ui_p->enter_btn);

    //创建图片控件基于进入按钮为父类
    UC_P->start_ui_p->enter_img = lv_img_create(UC_P->start_ui_p->enter_btn);

    lv_obj_set_style_img_opa(UC_P->start_ui_p->enter_img,180,0);//设置图库图片控件透明度

    //设置按钮中图片控件路径
    lv_img_set_src(UC_P->start_ui_p->enter_img,"S:/8080icon_img.jpg");

    lv_obj_set_size(UC_P->start_ui_p->enter_img, 80, 80); // 图片大小（适配按钮）
    
    //设置按钮中图片控件居中
    lv_obj_center(UC_P->start_ui_p->enter_img);

    // 在图片下方创建新标签，显示 “相册”，标签父对象与按钮相同（start_ui），确保在按钮外部显示
    UC_P->start_ui_p->enter_lab = lv_label_create(UC_P->start_ui_p->start_ui);

    // 对齐到按钮的底部中间，距离按钮底部30px,X偏移-5，Y偏移30（在按钮下方30px）
    lv_obj_align_to(UC_P->start_ui_p->enter_lab, UC_P->start_ui_p->enter_btn, LV_ALIGN_BOTTOM_MID, -5, 30);

    // //自己定义一个结构体变量
    LV_FONT_DECLARE(lv_myfont_kai_20)

    // //设置字体结构体是我们的 简体楷体20号结构体
    lv_obj_set_style_text_font(UC_P->start_ui_p->enter_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);

    lv_label_set_text(UC_P->start_ui_p->enter_lab,"相册"); // 标签文本

    // 设置标签文字颜色，状态为默认状态（LV_STATE_DEFAULT）
    lv_obj_set_style_text_color(UC_P->start_ui_p->enter_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);

    // 新增居中对齐（与按钮底部中间对齐）-------------20250903
    lv_obj_set_style_text_align(UC_P->start_ui_p->enter_lab, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);

    //给进入按钮注册 中断事件
    lv_obj_add_event_cb(UC_P->start_ui_p->enter_btn,Enter_Btn_Task,LV_EVENT_SHORT_CLICKED,UC_P);

 // ---------------------- 新增：首页紧急呼叫按钮 ------20250903 10.25
    // 创建紧急呼叫按钮（父容器：首页根容器start_ui）
    UC_P->start_ui_p->emergency_btn = lv_btn_create(UC_P->start_ui_p->start_ui);
    lv_obj_set_size(UC_P->start_ui_p->emergency_btn, 120, 50); // 按钮大小（便于触摸）

    // 按钮位置：底部右侧，距离右边缘20px，底部50px（避开触摸盲区）
    lv_obj_align(UC_P->start_ui_p->emergency_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -50);

    // 按钮样式：红色背景（紧急提示），点击时加深
    lv_obj_set_style_bg_color(UC_P->start_ui_p->emergency_btn, lv_color_hex(0xFF4444), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(UC_P->start_ui_p->emergency_btn, lv_color_hex(0xD32F2F), LV_STATE_PRESSED);
    lv_obj_set_style_radius(UC_P->start_ui_p->emergency_btn, 8, LV_STATE_DEFAULT);

    // 创建紧急呼叫按钮标签（“紧急呼叫”）
    UC_P->start_ui_p->emergency_lab = lv_label_create(UC_P->start_ui_p->emergency_btn);
    lv_label_set_text(UC_P->start_ui_p->emergency_lab, "紧急呼叫");

    // 字体：楷体20号，白色文字
    LV_FONT_DECLARE(lv_myfont_kai_20);
    lv_obj_set_style_text_font(UC_P->start_ui_p->emergency_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(UC_P->start_ui_p->emergency_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);

    // 标签居中
    lv_obj_center(UC_P->start_ui_p->emergency_lab);

    // 绑定紧急呼叫按钮回调（点击切换到紧急界面）
    lv_obj_add_event_cb(UC_P->start_ui_p->emergency_btn, Emergency_Call_Task, LV_EVENT_SHORT_CLICKED, UC_P);
    // -------------------------------------------------------------------

    // ---------------------- 20250903新增：首页“设备信息”按钮 ------ 13.50
    // 创建设备信息按钮（父容器：首页根容器start_ui，大小80*80，位置在相册按钮右侧）
    UC_P->start_ui_p->device_info_btn = lv_btn_create(UC_P->start_ui_p->start_ui);

    lv_obj_set_size(UC_P->start_ui_p->device_info_btn, 80, 80); // 与相册按钮大小一致

    lv_obj_set_style_bg_opa(UC_P->start_ui_p->device_info_btn, 0, 0); // 透明背景

    // 位置：相册按钮右侧（相册按钮居中，x=360，新按钮x=450，y与相册按钮一致=220）
    lv_obj_set_pos(UC_P->start_ui_p->device_info_btn, 450, 200);

    // 创建按钮内图片
    UC_P->start_ui_p->device_info_img = lv_img_create(UC_P->start_ui_p->device_info_btn);

    lv_img_set_src(UC_P->start_ui_p->device_info_img, "S:/8080icon_sys.jpg"); // 需准备设备图标图片

    lv_obj_set_style_img_opa(UC_P->start_ui_p->device_info_img, 180, 0); // 透明度同相册图片

    lv_obj_set_size(UC_P->start_ui_p->device_info_img, 80, 80);

    lv_obj_center(UC_P->start_ui_p->device_info_img);

    // 创建按钮下方标签（“设备信息”，与相册标签风格一致）
    UC_P->start_ui_p->device_info_lab = lv_label_create(UC_P->start_ui_p->start_ui);

    // 对齐到设备信息按钮底部中间，距离底部30px
    lv_obj_align_to(UC_P->start_ui_p->device_info_lab, UC_P->start_ui_p->device_info_btn, LV_ALIGN_BOTTOM_MID, -24, 30);

    // 字体：楷体20号，白色
    LV_FONT_DECLARE(lv_myfont_kai_20);
    lv_obj_set_style_text_font(UC_P->start_ui_p->device_info_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(UC_P->start_ui_p->device_info_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);

    // 强制文本水平居中，同时设置文本基线对齐（中文渲染无偏移）-----20250903 14.49
    lv_obj_set_style_text_align(UC_P->start_ui_p->device_info_lab, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);

    // 设置标签宽度（与按钮宽度一致=80px，确保文本在按钮正下方居中显示，不超出范围）20250903 14.52
    lv_obj_set_width(UC_P->start_ui_p->device_info_lab, 80);

    // 文本过长时滚动（同相册标签，避免换行导致的对齐混乱）
    lv_label_set_long_mode(UC_P->start_ui_p->device_info_lab, LV_LABEL_LONG_SCROLL_CIRCULAR);
    
    lv_label_set_text(UC_P->start_ui_p->device_info_lab, "版本信息");

    // 绑定进入设备信息界面的回调
    lv_obj_add_event_cb(UC_P->start_ui_p->device_info_btn, Device_Info_Enter_Task, LV_EVENT_SHORT_CLICKED, UC_P);
    // -------------------------------------------------------------------

    //设置显示当前活动屏幕
    lv_scr_load(UC_P->start_ui_p->start_ui);
// ----------------------------------------------------------------------------20250829-0830
    return 0;
}


//显示第二个界面
int   Show_Dir_Ui(struct Ui_Ctrl * UC_P)   //显示第二个界面的函数
{
    UC_P->dir_ui_p = (DIR_UI_P)malloc(sizeof(DIR_UI));
    if(UC_P->dir_ui_p == (DIR_UI_P)NULL)
    {
        perror("malloc dir ui p ...");
        return -1;
    }

    memset(UC_P->dir_ui_p,0,sizeof(DIR_UI));

    //创建第二个界面的 容器控件
    UC_P->dir_ui_p->dir_ui = lv_obj_create((lv_obj_t *)NULL);

    // 给整个界面加青草绿：#E3EDCD背景，护眼-------------20250903 05.34
    lv_obj_set_style_bg_color(UC_P->dir_ui_p->dir_ui, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT); // 浅灰色0xF5F5F5

    // ---------------------- 新增：顶部盲区显示“当前目录路径”（y=20，触摸无效区）------20250903 06.54
    UC_P->dir_ui_p->current_dir_lab = lv_label_create(UC_P->dir_ui_p->dir_ui);

    // 初始显示根目录路径（DEFAULT_SHOW_DIR为"/"）
    char init_dir_text[256] = {0};
    sprintf(init_dir_text, "当前目录：%s", DEFAULT_SHOW_DIR);
    lv_label_set_text(UC_P->dir_ui_p->current_dir_lab, init_dir_text);

    // 字体：项目原有楷体20号，清晰易读
    LV_FONT_DECLARE(lv_myfont_kai_20);
    lv_obj_set_style_text_font(UC_P->dir_ui_p->current_dir_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    
    // 文本颜色：深灰色（#333333），与背景区分
    lv_obj_set_style_text_color(UC_P->dir_ui_p->current_dir_lab, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    
    // 位置：顶部左对齐，y=20（在0~80盲区内），x=10，避免边缘遮挡
    lv_obj_set_pos(UC_P->dir_ui_p->current_dir_lab, 10, 20);

    // ---------------------- 新增：顶部右侧创建“系统时间标签”------20250903 08.10
    UC_P->dir_ui_p->sys_time_lab = lv_label_create(UC_P->dir_ui_p->dir_ui);

    // 初始文本（线程启动后会实时更新）
    lv_label_set_text(UC_P->dir_ui_p->sys_time_lab, "2025-09-03 00:00:00");

    LV_FONT_DECLARE(lv_myfont_kai_20);
    // 字体：项目原有楷体20号，与目录路径标签字体统一
    lv_obj_set_style_text_font(UC_P->dir_ui_p->sys_time_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);

    // 文本颜色：深灰色（#333333），与背景区分
    lv_obj_set_style_text_color(UC_P->dir_ui_p->sys_time_lab, lv_color_hex(0x333333), LV_STATE_DEFAULT);

    // 位置：顶部右侧（y=20，在0~80盲区内），x=650（适配800分辨率，避免与左侧目录路径重叠）
    lv_obj_set_pos(UC_P->dir_ui_p->sys_time_lab, 600, 20);
    // -------------------------------------------------------------------

    //创建一个 目录列表 
    UC_P->dir_ui_p->dir_list = lv_list_create(UC_P->dir_ui_p->dir_ui);

    //设置目录列表大小 300 X 400
    lv_obj_set_size(UC_P->dir_ui_p->dir_list,300,400);

    //设置目录列表控件位置
    lv_obj_set_pos(UC_P->dir_ui_p->dir_list,10,60);

    // 给目录列表控件加绿豆沙：#C7EDCC背景，护眼-------------20250903 05.34
    lv_obj_set_style_bg_color(UC_P->dir_ui_p->dir_list, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    //创建 470 X 400 的小容器 用来存放文件图标
    UC_P->dir_ui_p->file_ui = lv_obj_create(UC_P->dir_ui_p->dir_ui);

    //设置小容器大小 470 X 400
    lv_obj_set_size(UC_P->dir_ui_p->file_ui,470,400);

    // 给目录列表控件加绿豆沙：#C7EDCC背景，护眼-------------20250903 05.34
    lv_obj_set_style_bg_color(UC_P->dir_ui_p->file_ui, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    //设置小容器的位置 320 60
    lv_obj_set_pos(UC_P->dir_ui_p->file_ui,320,60);

    //创建一个 跳到 第三个界面的按钮
    UC_P->dir_ui_p->exit_btn = lv_btn_create(UC_P->dir_ui_p->dir_ui);

    lv_obj_set_size(UC_P->dir_ui_p->exit_btn,60,20);

    lv_obj_set_pos(UC_P->dir_ui_p->exit_btn,80,460);

// --------------------------------新增20250902 17.02
    // 创建exit按钮的标签，父对象为退出按钮
    lv_obj_t *exit_label = lv_label_create(UC_P->dir_ui_p->exit_btn);
    // 设置标签文本为"exit"
    lv_label_set_text(exit_label, "exit");
    // 使标签在按钮内居中显示
    lv_obj_center(exit_label);
// -----------------------------------------------
    //设置该按钮的中断事件 点击之后 进入 第三个界面
    lv_obj_add_event_cb(UC_P->dir_ui_p->exit_btn,Exit_Btn_Task,LV_EVENT_SHORT_CLICKED,UC_P);

// --------------------------------新增20250902 17.15
    //创建一个 从目录检索器第二个界面返回首页的按钮
    UC_P->dir_ui_p->return_btn = lv_btn_create(UC_P->dir_ui_p->dir_ui);

    lv_obj_set_size(UC_P->dir_ui_p->return_btn,60,20);

    lv_obj_set_pos(UC_P->dir_ui_p->return_btn,10,460);

    // 创建返回按钮的标签，父对象为从第二个界面返回首页按钮
    lv_obj_t *return_label = lv_label_create(UC_P->dir_ui_p->return_btn);
    // 设置标签文本为"return"
    lv_label_set_text(return_label, "return");
    // 使标签在按钮内居中显示
    lv_obj_center(return_label);

   //设置该按钮的中断事件 点击之后 进入 返回首页界面
    lv_obj_add_event_cb(UC_P->dir_ui_p->return_btn,Return_Btn_Task,LV_EVENT_SHORT_CLICKED,UC_P);

// -----------------------------------------------

    //显示根目录下的文件夹
    if(Show_Dir_List(DEFAULT_SHOW_DIR,UC_P) == false)
    {
        perror("show dir list ...");
        return -1;
    }

    //设置显示 第二个界面
    //lv_scr_load(UC_P->dir_ui_p->dir_ui);

    return 0;
}

//显示第三个界面的函数
int  Show_End_Ui(struct Ui_Ctrl * UC_P)   
{
    
    UC_P->end_ui_p = (END_UI_P)malloc(sizeof(END_UI));
    if(UC_P->end_ui_p == (END_UI_P)NULL)
    {
        perror("malloc end ui p ...");
        return -1;
    }
    memset(UC_P->end_ui_p,0,sizeof(END_UI));

    //创建第三个界面的 容器载体
    UC_P->end_ui_p->end_ui = lv_obj_create((lv_obj_t *)NULL);

    //创建一个静态图片控件，父类为容器载体
    UC_P->end_ui_p->end_img = lv_img_create(UC_P->end_ui_p->end_ui);

    //设置静态图片的路径
    lv_img_set_src(UC_P->end_ui_p->end_img,"S:/800480end.jpg");

    return 0;
}

// ------------ 20250903新增：显示紧急呼叫界面函数 ------ 10.06
int Show_Emergency_Ui(struct Ui_Ctrl * UC_P)
{
    // 分配紧急界面结构体内存
    UC_P->emergency_ui_p = (EMERGENCY_UI_P)malloc(sizeof(EMERGENCY_UI));
    if(UC_P->emergency_ui_p == (EMERGENCY_UI_P)NULL)
    {
        perror("malloc emergency ui p ...");
        return -1;
    }
    memset(UC_P->emergency_ui_p, 0, sizeof(EMERGENCY_UI));

    // 创建紧急界面根容器（800*480，红色背景提示紧急状态）
    UC_P->emergency_ui_p->emergency_ui = lv_obj_create((lv_obj_t *)NULL);
    lv_obj_set_style_bg_color(UC_P->emergency_ui_p->emergency_ui, lv_color_hex(0xFF4444), LV_STATE_DEFAULT); // 紧急红色
    lv_obj_set_style_bg_opa(UC_P->emergency_ui_p->emergency_ui, 255, LV_STATE_DEFAULT); // 完全不透明

    // 创建“紧急呼叫中...”标签（居中显示，白色大字体）
    UC_P->emergency_ui_p->call_label = lv_label_create(UC_P->emergency_ui_p->emergency_ui);
    lv_label_set_text(UC_P->emergency_ui_p->call_label, "紧急呼叫中...\n112"); // 显示呼叫号码
    
    // 字体：项目原有楷体20号，适配中文显示
    LV_FONT_DECLARE(lv_myfont_kai_20);
    lv_obj_set_style_text_font(UC_P->emergency_ui_p->call_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);

    // 文本颜色：白色，与红色背景强烈对比，清晰可见
    lv_obj_set_style_text_color(UC_P->emergency_ui_p->call_label, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);

    // 文本居中对齐
    lv_obj_set_style_text_align(UC_P->emergency_ui_p->call_label, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);

    // 标签居中显示（屏幕中心）
    lv_obj_center(UC_P->emergency_ui_p->call_label);

    // 创建挂断按钮（底部居中，绿色背景，与紧急红色区分）
    UC_P->emergency_ui_p->hangup_btn = lv_btn_create(UC_P->emergency_ui_p->emergency_ui);
    lv_obj_set_size(UC_P->emergency_ui_p->hangup_btn, 100, 40); // 按钮大小（便于触摸）

    // 按钮位置：底部居中，距离底部50px（避开触摸盲区）
    lv_obj_align(UC_P->emergency_ui_p->hangup_btn, LV_ALIGN_BOTTOM_MID, 0, -50);

    // 按钮样式：绿色背景，点击时加深
    lv_obj_set_style_bg_color(UC_P->emergency_ui_p->hangup_btn, lv_color_hex(0x4CAF50), LV_STATE_DEFAULT); // 正常绿色
    lv_obj_set_style_bg_color(UC_P->emergency_ui_p->hangup_btn, lv_color_hex(0x388E3C), LV_STATE_PRESSED); // 点击深绿
    lv_obj_set_style_radius(UC_P->emergency_ui_p->hangup_btn, 8, LV_STATE_DEFAULT); // 圆角优化

    // 创建挂断按钮标签（“挂断”）
    UC_P->emergency_ui_p->hangup_lab = lv_label_create(UC_P->emergency_ui_p->hangup_btn);
    lv_label_set_text(UC_P->emergency_ui_p->hangup_lab, "挂断");

    LV_FONT_DECLARE(lv_myfont_kai_20);
    // 字体：楷体20号，白色文字
    lv_obj_set_style_text_font(UC_P->emergency_ui_p->hangup_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(UC_P->emergency_ui_p->hangup_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);

    // 标签居中
    lv_obj_center(UC_P->emergency_ui_p->hangup_lab);

    // 绑定挂断按钮回调（点击返回首页）
    lv_obj_add_event_cb(UC_P->emergency_ui_p->hangup_btn, Hangup_Call_Task, LV_EVENT_SHORT_CLICKED, UC_P);

    return 0;
}
// -------------------------------------------------------------------

// ------------ 20250903新增：创建设备信息界面函数 ------ 13.44
int Show_Device_Info_Ui(struct Ui_Ctrl * UC_P)
{
    // 分配设备信息界面结构体内存
    UC_P->device_info_ui_p = (DEVICE_INFO_UI_P)malloc(sizeof(DEVICE_INFO_UI));
    if(UC_P->device_info_ui_p == (DEVICE_INFO_UI_P)NULL)
    {
        perror("malloc device info ui p ...");
        return -1;
    }
    memset(UC_P->device_info_ui_p, 0, sizeof(DEVICE_INFO_UI));

    // 创建根容器（800*480，与其他界面背景色一致：绿豆沙#C7EDCC）
    UC_P->device_info_ui_p->device_info_ui = lv_obj_create((lv_obj_t *)NULL);
    lv_obj_set_style_bg_color(UC_P->device_info_ui_p->device_info_ui, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(UC_P->device_info_ui_p->device_info_ui, 255, LV_STATE_DEFAULT);

    // 创建返回按钮（复用第二个界面return_btn样式：位置x=10,y=460，大小60*20）
    UC_P->device_info_ui_p->return_btn = lv_btn_create(UC_P->device_info_ui_p->device_info_ui);
    lv_obj_set_size(UC_P->device_info_ui_p->return_btn, 60, 20);
    lv_obj_set_pos(UC_P->device_info_ui_p->return_btn, 10, 460);

    // 返回按钮标签
    UC_P->device_info_ui_p->return_lab = lv_label_create(UC_P->device_info_ui_p->return_btn);
    lv_label_set_text(UC_P->device_info_ui_p->return_lab, "return");
    lv_obj_center(UC_P->device_info_ui_p->return_lab);

    // 绑定返回回调（返回首页）
    lv_obj_add_event_cb(UC_P->device_info_ui_p->return_btn, Device_Info_Return_Task, LV_EVENT_SHORT_CLICKED, UC_P);

    // 创建退出按钮（复用第二个界面exit_btn样式：位置x=80,y=460，大小60*20）
    UC_P->device_info_ui_p->exit_btn = lv_btn_create(UC_P->device_info_ui_p->device_info_ui);
    lv_obj_set_size(UC_P->device_info_ui_p->exit_btn, 60, 20);
    lv_obj_set_pos(UC_P->device_info_ui_p->exit_btn, 80, 460);

    // 退出按钮标签
    UC_P->device_info_ui_p->exit_lab = lv_label_create(UC_P->device_info_ui_p->exit_btn);
    lv_label_set_text(UC_P->device_info_ui_p->exit_lab, "exit");
    lv_obj_center(UC_P->device_info_ui_p->exit_lab);

    // 绑定退出回调（退出到结束界面）
    lv_obj_add_event_cb(UC_P->device_info_ui_p->exit_btn, Device_Info_Exit_Task, LV_EVENT_SHORT_CLICKED, UC_P);

    // 创建设备信息标签（楷体20号，深灰色#333333，居中对齐）
    LV_FONT_DECLARE(lv_myfont_kai_20);
    // 设备型号
    UC_P->device_info_ui_p->info_lab1 = lv_label_create(UC_P->device_info_ui_p->device_info_ui);
    lv_label_set_text(UC_P->device_info_ui_p->info_lab1, "型号信息:GEC6818");
    lv_obj_set_style_text_font(UC_P->device_info_ui_p->info_lab1, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(UC_P->device_info_ui_p->info_lab1, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_set_pos(UC_P->device_info_ui_p->info_lab1, 300, 150); // 居中显示

    // LVGL版本
    UC_P->device_info_ui_p->info_lab2 = lv_label_create(UC_P->device_info_ui_p->device_info_ui);
    lv_label_set_text(UC_P->device_info_ui_p->info_lab2, "LVGL版本:v8.2");
    lv_obj_set_style_text_font(UC_P->device_info_ui_p->info_lab2, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(UC_P->device_info_ui_p->info_lab2, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_set_pos(UC_P->device_info_ui_p->info_lab2, 300, 200);

    // 屏幕分辨率
    UC_P->device_info_ui_p->info_lab3 = lv_label_create(UC_P->device_info_ui_p->device_info_ui);
    lv_label_set_text(UC_P->device_info_ui_p->info_lab3, "屏幕尺寸:800x480");
    lv_obj_set_style_text_font(UC_P->device_info_ui_p->info_lab3, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(UC_P->device_info_ui_p->info_lab3, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_set_pos(UC_P->device_info_ui_p->info_lab3, 300, 250);

    // 系统版本
    UC_P->device_info_ui_p->info_lab4 = lv_label_create(UC_P->device_info_ui_p->device_info_ui);
    lv_label_set_text(UC_P->device_info_ui_p->info_lab4, "Linux版本:Linux 4.1.15");
    lv_obj_set_style_text_font(UC_P->device_info_ui_p->info_lab4, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(UC_P->device_info_ui_p->info_lab4, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_set_pos(UC_P->device_info_ui_p->info_lab4, 300, 300);

// ---------------------- 20250905重构：首页“2048游戏”按钮（相册左边） ------
// 1. 创建游戏按钮（父容器：首页根容器start_ui，大小80*80，相册左边）
UC_P->start_ui_p->game2048_btn = lv_btn_create(UC_P->start_ui_p->start_ui);
lv_obj_set_size(UC_P->start_ui_p->game2048_btn, 80, 80); // 与相册按钮大小一致
lv_obj_set_style_bg_opa(UC_P->start_ui_p->game2048_btn, 0, 0); // 透明背景

// 位置：相册按钮左边（相册x=360，游戏按钮x=270，间距10，y=200）
lv_obj_set_pos(UC_P->start_ui_p->game2048_btn, 270, 200);

// 2. 创建按钮内图片（使用原游戏图标路径）
UC_P->start_ui_p->game2048_img = lv_img_create(UC_P->start_ui_p->game2048_btn);
lv_img_set_src(UC_P->start_ui_p->game2048_img, "S:/8080icon_game.jpg");
lv_obj_set_style_img_opa(UC_P->start_ui_p->game2048_img, 180, 0); // 透明度同相册图片
lv_obj_set_size(UC_P->start_ui_p->game2048_img, 80, 80);
lv_obj_center(UC_P->start_ui_p->game2048_img);

// 3. 创建按钮下方标签（“2048游戏”）
UC_P->start_ui_p->game2048_lab = lv_label_create(UC_P->start_ui_p->start_ui);
lv_obj_align_to(UC_P->start_ui_p->game2048_lab, 
                UC_P->start_ui_p->game2048_btn, 
                LV_ALIGN_BOTTOM_MID, 
                -24, 30); // 与相册标签对齐

// 字体与样式（复用相册标签）
LV_FONT_DECLARE(lv_myfont_kai_20);
lv_obj_set_style_text_font(UC_P->start_ui_p->game2048_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);
lv_obj_set_style_text_color(UC_P->start_ui_p->game2048_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
lv_obj_set_style_text_align(UC_P->start_ui_p->game2048_lab, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);

// 标签宽度与按钮一致，文本过长时滚动
lv_obj_set_width(UC_P->start_ui_p->game2048_lab, 80);
lv_label_set_long_mode(UC_P->start_ui_p->game2048_lab, LV_LABEL_LONG_SCROLL_CIRCULAR);
lv_label_set_text(UC_P->start_ui_p->game2048_lab, "2048游戏");

// 4. 绑定新的游戏进入回调
lv_obj_add_event_cb(UC_P->start_ui_p->game2048_btn, Game2048_Enter_Task, LV_EVENT_SHORT_CLICKED, UC_P);
// -------------------------------------------------------------------


    return 0;
}
// -------------------------------------------------------------------

// ---------------------- 20250905新增：2048游戏核心函数 ------13.46
// 初始化游戏数据和界面
static void game2048_init(GAME_2048_DATA_P game_data, struct Ui_Ctrl *UC_P)
{
    if (game_data == NULL || UC_P == NULL) return;

    // 初始化游戏数据
    memset(game_data, 0, sizeof(GAME_2048_DATA));
    game_data->score = 0;
    game_data->game_over = false;
    memset(game_data->matrix, 0, sizeof(game_data->matrix));
    memset(game_data->btn_labels, 0, sizeof(game_data->btn_labels)); // 初始化标签数组

    // 创建游戏根容器（800*480，绿豆沙背景，与其他界面统一）
    game_data->game_root = lv_obj_create(NULL);
    lv_obj_set_size(game_data->game_root, 800, 480);
    lv_obj_set_style_bg_color(game_data->game_root, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(game_data->game_root, 255, LV_STATE_DEFAULT);
    lv_scr_load(game_data->game_root); // 加载游戏根容器到屏幕（关键！）

    // 创建分数标签（顶部居中）
    game_data->score_label = lv_label_create(game_data->game_root);
    lv_label_set_text_fmt(game_data->score_label, "分数: %d", game_data->score);
    LV_FONT_DECLARE(lv_myfont_kai_20);
    lv_obj_set_style_text_font(game_data->score_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(game_data->score_label, lv_color_hex(0x333333), LV_STATE_DEFAULT);
    lv_obj_align(game_data->score_label, LV_ALIGN_TOP_MID, 0, 20);// 分数标签位置：顶部20px

    // 创建4x4按钮矩阵（居中显示）
    static const char *btnm_map[] = 
    {
        " ", " ", " ", " ", "\n",
        " ", " ", " ", " ", "\n",
        " ", " ", " ", " ", "\n",
        " ", " ", " ", " ", ""
    };
    game_data->btnm = lv_btnmatrix_create(game_data->game_root);
    lv_btnmatrix_set_map(game_data->btnm, btnm_map);

    // 尺寸：LV_PCT(65)×LV_PCT(60)（适配800×480，避免过大遮挡）
    lv_obj_set_size(game_data->btnm, LV_PCT(65), LV_PCT(60));

    // 位置：居中偏上（Y偏移-20），顶部距分数标签50px，底部距按钮80px
    lv_obj_align(game_data->btnm, LV_ALIGN_CENTER, 0, -20);

    // 按钮内边距：确保数字不拥挤
    lv_obj_set_style_pad_all(game_data->btnm, 10, LV_STATE_DEFAULT);// 按钮内边距：确保数字不拥挤
    lv_obj_set_style_border_width(game_data->btnm, 0, LV_STATE_DEFAULT);// 移除矩阵外边框，视觉简洁

    // 核心：为每个按钮创建标签并保存到btn_labels数组
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            uint8_t btn_idx = i * 4 + j;
            lv_obj_t *btn = lv_obj_get_child(game_data->btnm, btn_idx); // 获取矩阵中的按钮
            if (btn == NULL) continue;

            // 手动创建标签，父对象为按钮
            lv_obj_t *label = lv_label_create(btn);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_STATE_DEFAULT); // 用默认字体
            lv_obj_center(label); // 标签在按钮内居中

            game_data->btn_labels[i][j] = label; // 保存标签引用（后续绘制用）
        }
    }

    // 绑定触摸/手势事件（处理滑动）
    lv_obj_add_event_cb(game_data->btnm, game2048_handle_event, LV_EVENT_ALL, game_data);
    lv_obj_add_flag(game_data->btnm, LV_OBJ_FLAG_EVENT_BUBBLE);

    // 初始化矩阵（随机生成2个数字）
    game2048_add_random(game_data);
    game2048_add_random(game_data);
    game2048_draw(game_data);

    // 初始化游戏结束提示控件（默认隐藏，游戏结束时显示）
    game_data->game_over_label = lv_label_create(game_data->game_root);
    lv_label_set_text(game_data->game_over_label, "游戏结束！\n点击重新开始");
    lv_obj_set_style_text_font(game_data->game_over_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(game_data->game_over_label, lv_color_hex(0xFF4444), LV_STATE_DEFAULT); // 红色提示
    lv_obj_set_style_text_align(game_data->game_over_label, LV_TEXT_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_align(game_data->game_over_label, LV_ALIGN_CENTER, 0, -50); // 居中显示在矩阵上方
    lv_obj_add_flag(game_data->game_over_label, LV_OBJ_FLAG_HIDDEN); // 默认隐藏

    // 初始化重新开始按钮（默认隐藏，游戏结束时显示）
    game_data->restart_btn = lv_btn_create(game_data->game_root);
    lv_obj_set_size(game_data->restart_btn, 100, 40); // 按钮尺寸：便于触摸
    lv_obj_align_to(game_data->restart_btn, game_data->game_over_label, LV_ALIGN_BOTTOM_MID, 0, 20); // 提示下方
    lv_obj_set_style_bg_color(game_data->restart_btn, lv_color_hex(0x4CAF50), LV_STATE_DEFAULT); // 绿色按钮
    lv_obj_set_style_bg_color(game_data->restart_btn, lv_color_hex(0x388E3C), LV_STATE_PRESSED); // 按压深绿

    // 重新开始按钮标签
    lv_obj_t *restart_lab = lv_label_create(game_data->restart_btn);
    lv_label_set_text(restart_lab, "重新开始");
    lv_obj_set_style_text_font(restart_lab, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(restart_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT); // 白色文本
    lv_obj_center(restart_lab);

    // 绑定重新开始事件
    lv_obj_add_event_cb(game_data->restart_btn, game2048_restart_task, LV_EVENT_SHORT_CLICKED, game_data);
    lv_obj_add_flag(game_data->restart_btn, LV_OBJ_FLAG_HIDDEN); // 默认隐藏

}

// 随机在空位置添加2（90%）或4（10%）
static void game2048_add_random(GAME_2048_DATA_P game_data)
{
    if (game_data == NULL) return;

    // 统计空位置
    uint8_t empty_pos[16][2] = {0};
    uint8_t empty_cnt = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (game_data->matrix[i][j] == 0) {
                empty_pos[empty_cnt][0] = i;
                empty_pos[empty_cnt][1] = j;
                empty_cnt++;
            }
        }
    }
    if (empty_cnt == 0) return;

    // 在game2048_init中初始化随机种子（仅一次）---20250908 07.54
    static bool srand_inited = false;
    if (!srand_inited) {
        srand(time(NULL));
        srand_inited = true;
    }

    // 随机选择空位置（无需重复重置种子）
    uint8_t idx = rand() % empty_cnt;
    uint8_t x = empty_pos[idx][0];
    uint8_t y = empty_pos[idx][1];
    // -------------------------------
    // 90%概率生成2（对应matrix值1），10%生成4（对应matrix值2）
    game_data->matrix[x][y] = (rand() % 10 < 9) ? 1 : 2;
}

// ---------------------- 20250905新增：矩阵旋转函数（game2048_slide依赖）------
static void rotate_matrix(uint16_t matrix[4][4], uint8_t rotate_cnt)
{
    if (matrix == NULL || rotate_cnt == 0) return;

    // 临时矩阵存储旋转结果
    uint16_t temp[4][4] = {0};

    // 根据旋转次数执行对应旋转（1=90度，2=180度，3=270度）
    for (int r = 0; r < rotate_cnt; r++) 
    {
        // 第一步：复制当前矩阵到临时矩阵
        memcpy(temp, matrix, sizeof(temp));

        // 第二步：90度顺时针旋转（核心逻辑）
        for (int i = 0; i < 4; i++) 
        {
            for (int j = 0; j < 4; j++) 
            {
                matrix[i][j] = temp[3 - j][i];
            }
        }
    }

    // 打印旋转后的矩阵，确认数据正确
    printf("旋转后矩阵：\n");
    for (int i = 0; i < 4; i++) 
    {
        printf("%d %d %d %d\n", matrix[i][0], matrix[i][1], matrix[i][2], matrix[i][3]);
    }

}

// 处理滑动逻辑（合并数字，更新分数）
static bool game2048_slide(GAME_2048_DATA_P game_data, lv_dir_t dir)
{
    if (game_data == NULL || game_data->game_over) return false;

    bool moved = false;
    uint16_t temp_matrix[4][4] = {0};
    memcpy(temp_matrix, game_data->matrix, sizeof(temp_matrix));

    // 根据方向旋转矩阵（统一转为向上滑动逻辑）
    switch (dir) {
        case LV_DIR_LEFT:  rotate_matrix(game_data->matrix, 1); break;
        case LV_DIR_BOTTOM:  rotate_matrix(game_data->matrix, 2); break;
        case LV_DIR_RIGHT: rotate_matrix(game_data->matrix, 3); break;
        default: break; // 向上无需旋转
    }

    // 处理向上滑动（合并相同数字）
    for (int j = 0; j < 4; j++) { // 列
        uint16_t col[4] = {0};
        uint8_t col_idx = 0;

        // 1. 提取非空数字
        for (int i = 0; i < 4; i++) {
            if (game_data->matrix[i][j] != 0) {
                col[col_idx++] = game_data->matrix[i][j];
            }
        }

        // 2. 合并相同数字
        for (int i = 0; i < col_idx - 1; i++) {
            if (col[i] != 0 && col[i] == col[i + 1]) {
                col[i]++; // 合并为更高位（如1+1=2，对应2→4）
                col[i + 1] = 0;
                game_data->score += (1 << col[i]); // 加分（2^col[i]）
                moved = true;
            }
        }

        // 3. 重新整理列（移除空值）
        uint8_t new_col_idx = 0;
        uint16_t new_col[4] = {0};
        for (int i = 0; i < col_idx; i++) {
            if (col[i] != 0) {
                new_col[new_col_idx++] = col[i];
            }
        }

        // 4. 更新矩阵列
        for (int i = 0; i < 4; i++) {
            game_data->matrix[i][j] = (i < new_col_idx) ? new_col[i] : 0;
        }
    }

    // 旋转回原方向
    switch (dir) {
        case LV_DIR_LEFT:  rotate_matrix(game_data->matrix, 3); break;
        case LV_DIR_BOTTOM:  rotate_matrix(game_data->matrix, 2); break;
        case LV_DIR_RIGHT: rotate_matrix(game_data->matrix, 1); break;
        default: break;
    }

    // 判断是否移动过
    if (memcmp(temp_matrix, game_data->matrix, sizeof(temp_matrix)) != 0) {
        moved = true;
    }

    // 移动后添加新数字
    if (moved) {
        game2048_add_random(game_data);
    }

    // 判断游戏结束
    game_data->game_over = true;
    // 检查是否有空位置
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (game_data->matrix[i][j] == 0) {
                game_data->game_over = false;
                goto check_end;
            }
        }
    }
    // 检查是否有可合并数字
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            if (game_data->matrix[i][j] == game_data->matrix[i][j + 1] || 
                game_data->matrix[j][i] == game_data->matrix[j + 1][i]) {
                game_data->game_over = false;
                goto check_end;
            }
        }
    }
check_end:
    // 游戏结束：显示提示和重新开始按钮
    if (game_data->game_over) {
        lv_obj_clear_flag(game_data->game_over_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(game_data->restart_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(game_data->game_over_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(game_data->restart_btn, LV_OBJ_FLAG_HIDDEN);
    }

    return moved;
}

// 绘制游戏界面（更新按钮矩阵和分数）
static void game2048_draw(GAME_2048_DATA_P game_data)
{
    if (game_data == NULL || game_data->btnm == NULL) return;

    // 1. 更新分数标签（游戏结束时追加提示）
     if (game_data->game_over) 
     {
        lv_label_set_text_fmt(game_data->score_label, "分数: %d (游戏结束)", game_data->score);
    } else 
    {
        lv_label_set_text_fmt(game_data->score_label, "分数: %d", game_data->score);
    }


    // 2. 定义数字颜色映射（2048经典配色）
    struct {
        uint16_t num;
        lv_color_t bg_color;
        lv_color_t text_color;
    } color_map[] = {
        {0,  lv_color_hex(0xc7b9ac), lv_color_hex(0x6c635b)}, // 空
        {1,  lv_color_hex(0xeee4da), lv_color_hex(0x6c635b)}, // 2
        {2,  lv_color_hex(0xede0c8), lv_color_hex(0x6c635b)}, // 4
        {3,  lv_color_hex(0xf2b179), lv_color_hex(0xf8f5f0)}, // 8
        {4,  lv_color_hex(0xf59563), lv_color_hex(0xf8f5f0)}, // 16
        {5,  lv_color_hex(0xf67c5f), lv_color_hex(0xf8f5f0)}, // 32
        {6,  lv_color_hex(0xf75f3b), lv_color_hex(0xf8f5f0)}, // 64
        {7,  lv_color_hex(0xedcf72), lv_color_hex(0xf8f5f0)}, // 128
        {8,  lv_color_hex(0xedcc61), lv_color_hex(0xf8f5f0)}, // 256
        {9,  lv_color_hex(0xedc850), lv_color_hex(0xf8f5f0)}, // 512
        {10, lv_color_hex(0xedc53f), lv_color_hex(0xf8f5f0)}, // 1024
        {11, lv_color_hex(0xedc22e), lv_color_hex(0xf8f5f0)}, // 2048
        {12, lv_color_hex(0x3c3a32), lv_color_hex(0xf8f5f0)}  // 大于2048
    };

    // 3. 更新按钮矩阵
    for (int i = 0; i < 4; i++) 
    {
        for (int j = 0; j < 4; j++) 
        {
            uint16_t mat_val = game_data->matrix[i][j];
            char btn_text[8] = {0};

            // 设置按钮文本（空位置显示空格，其他显示2^mat_val）
            if (mat_val > 0) 
            {
                sprintf(btn_text, "%d", 1 << mat_val);
            } else 
            {
                strcpy(btn_text, " ");
            }

            // 修复：通过保存的标签引用设置文本
            if (game_data->btn_labels[i][j] != NULL) 
            {
                lv_label_set_text(game_data->btn_labels[i][j], btn_text);
            }

            // 设置按钮颜色
            lv_color_t bg_color = color_map[0].bg_color;
            lv_color_t text_color = color_map[0].text_color;
            for (size_t k = 0; k < sizeof(color_map)/sizeof(color_map[0]); k++) 
            {
                if (mat_val == color_map[k].num) 
                {
                    bg_color = color_map[k].bg_color;
                    text_color = color_map[k].text_color;
                    break;
                } else if (k == sizeof(color_map)/sizeof(color_map[0]) - 1 && mat_val > color_map[k].num) 
                {
                    bg_color = color_map[k].bg_color;
                    text_color = color_map[k].text_color;
                }
            }

            // 应用样式（按钮+标签）
            lv_obj_t *btn = lv_obj_get_child(game_data->btnm, i * 4 + j);
            if (btn != NULL) 
            {
                lv_obj_set_style_bg_color(btn, bg_color, LV_STATE_DEFAULT);
            }
            if (game_data->btn_labels[i][j] != NULL) 
            {
                lv_obj_set_style_text_color(game_data->btn_labels[i][j], text_color, LV_STATE_DEFAULT);
                lv_obj_set_style_radius(btn, 4, LV_STATE_DEFAULT);
            }
        }
    }
}

// ---------------------- 新增：2048游戏重新开始回调 ------
static void game2048_restart_task(lv_event_t *e)
{
    GAME_2048_DATA_P game_data = (GAME_2048_DATA_P)lv_event_get_user_data(e);
    if (game_data == NULL) return;

    // 1. 重置游戏数据
    game_data->score = 0;
    game_data->game_over = false;
    memset(game_data->matrix, 0, sizeof(game_data->matrix));

    // 2. 重新生成2个随机数字
    game2048_add_random(game_data);
    game2048_add_random(game_data);

    // 3. 隐藏游戏结束提示和重新开始按钮
    lv_obj_add_flag(game_data->game_over_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(game_data->restart_btn, LV_OBJ_FLAG_HIDDEN);

    // 4. 重新绘制界面（更新分数和矩阵）
    game2048_draw(game_data);
}


// 处理游戏触摸/手势事件
static void game2048_handle_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    GAME_2048_DATA_P game_data = (GAME_2048_DATA_P)lv_event_get_user_data(e);

    // 仅响应滑动手势（排除点击误触发）
    if (code == LV_EVENT_GESTURE && !game_data->game_over) 
    {
        // 处理滑动手势
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());// 获取滑动方向

        if (dir != LV_DIR_NONE) 
        {
            bool slide_ret = game2048_slide(game_data, dir); // 执行滑动合并
            printf("滑动结果：%s\n", slide_ret ? "成功（数字移动/合并）" : "失败（无移动）");
            game2048_draw(game_data);
        }
    }
}
// -------------------------------------------------------------------


// ---------------------- 20250903新增：设备信息界面→首页回调 ------ 13.59
void Device_Info_Return_Task(lv_event_t * e)
{
    struct Ui_Ctrl * UC_P = (struct Ui_Ctrl *)e->user_data;
    if(UC_P == NULL || UC_P->device_info_ui_p == NULL || UC_P->start_ui_p == NULL)
    {
        printf("设备信息返回：非法指针\n");
        return;
    }

    // 复用从上往下滑动画（500ms，与目录界面返回首页一致）
    lv_obj_t * old_screen = UC_P->device_info_ui_p->device_info_ui; // 旧屏幕：设备信息界面
    lv_obj_t * new_screen = UC_P->start_ui_p->start_ui;             // 新屏幕：首页
    lv_coord_t screen_height = lv_disp_get_ver_res(NULL);

    // 新屏幕初始位置：屏幕顶部（Y=-screen_height，不可见）
    lv_obj_set_y(new_screen, -screen_height);
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    // 旧屏幕动画：向下滑出（Y=0→screen_height）
    lv_anim_t a_slide_out;
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, screen_height);
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 500);

    // 新屏幕动画：从上往下滑入（Y=-screen_height→0）
    lv_anim_t a_slide_in;
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, -screen_height, 0);
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 500);

    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);

    UC_P->exit_mask = 0;
}
// -------------------------------------------------------------------

// ---------------------- 20250903新增：设备信息界面→结束界面回调 ------ 14.00
void Device_Info_Exit_Task(lv_event_t * e)
{
    struct Ui_Ctrl * UC_P = (struct Ui_Ctrl *)e->user_data;
    if(UC_P == NULL || UC_P->device_info_ui_p == NULL || UC_P->end_ui_p == NULL)
    {
        printf("设备信息退出：非法指针\n");
        return;
    }

    // 复用从下往上滑动画（500ms，与目录界面退出一致）
    lv_obj_t * old_screen = UC_P->device_info_ui_p->device_info_ui; // 旧屏幕：设备信息界面
    lv_obj_t * new_screen = UC_P->end_ui_p->end_ui;               // 新屏幕：结束界面
    lv_coord_t screen_height = lv_disp_get_ver_res(NULL);

    // 新屏幕初始位置：屏幕底部（不可见）
    lv_obj_set_y(new_screen, screen_height);
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    // 旧屏幕动画：向上滑出（Y=0→-screen_height）
    lv_anim_t a_slide_out;
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, -screen_height);
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 500);

    // 新屏幕动画：从下往上滑入（Y=screen_height→0）
    lv_anim_t a_slide_in;
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, screen_height, 0);
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 500);

    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);

    UC_P->exit_mask = 0;
}
// -------------------------------------------------------------------

// ---------------------- 20250905新增：首页→2048游戏回调 ------
// ---------------------- 20250908新增修改 分离动画与初始化，避免主线程阻塞 ----------------------
void Game2048_Enter_Task(lv_event_t *e)
{
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)e->user_data;
    if (UC_P == NULL || UC_P->start_ui_p == NULL) {
        printf("2048游戏：非法指针\n");
        return;
    }

    // 1. 释放旧游戏资源（轻量操作，快速执行）
    if (UC_P->game2048_data != NULL) {
        // 优化：延迟删除旧容器，避免同步释放耗时
        if (UC_P->game2048_data->game_root != NULL) {
            lv_obj_del_async(UC_P->game2048_data->game_root); // 异步删除，不阻塞
        }
        free(UC_P->game2048_data);
        UC_P->game2048_data = NULL;
    }

    // 2. 仅分配空的游戏数据结构（不初始化控件）
    UC_P->game2048_data = (GAME_2048_DATA_P)malloc(sizeof(GAME_2048_DATA));
    if (UC_P->game2048_data == NULL) {
        perror("malloc game2048 data");
        return;
    }
    memset(UC_P->game2048_data, 0, sizeof(GAME_2048_DATA));
    GAME_2048_DATA_P game_data = UC_P->game2048_data;

    // 3. 创建空的游戏根容器（仅用于动画，无复杂控件）
    game_data->game_root = lv_obj_create(NULL);
    lv_obj_set_size(game_data->game_root, 800, 480);
    lv_obj_set_style_bg_color(game_data->game_root, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    // 4. 优化界面切换动画：缩短时间（200ms），并绑定动画结束回调
    lv_obj_t *old_screen = UC_P->start_ui_p->start_ui;
    lv_obj_t *new_screen = game_data->game_root;
    lv_coord_t screen_height = lv_disp_get_ver_res(NULL);

    // 动画配置：200ms快速切换，避免长时间阻塞
    lv_anim_t a_slide_out;
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, -screen_height);
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 200); // 优化：缩短动画时间

    lv_anim_t a_slide_in;
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, screen_height, 0);
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 200);

    // 关键：动画结束后，调用初始化函数（异步执行，不阻塞动画）
    a_slide_in.user_data = UC_P;
    lv_anim_set_ready_cb(&a_slide_in, (lv_anim_ready_cb_t)game2048_init_delay);

    // 启动动画（此时仅空容器参与动画，流畅无阻塞）
    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);

}

// 20250908新增修改：动画结束后执行的初始化函数（核心控件创建）
static void game2048_init_delay(lv_anim_t *a)
{
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)a->user_data;
    if (UC_P == NULL || UC_P->game2048_data == NULL) return;
    GAME_2048_DATA_P game_data = UC_P->game2048_data;

    // 真正的游戏初始化（此时动画已结束，主线程空闲）
    game2048_init(game_data, UC_P);

    // 创建返回/退出按钮
    lv_obj_t *return_btn = lv_btn_create(game_data->game_root);
    lv_obj_set_size(return_btn, 60, 20);
    lv_obj_set_pos(return_btn, 10, 460);
    lv_obj_set_style_bg_color(return_btn, lv_color_hex(0x4CAF50), LV_STATE_DEFAULT);
    lv_obj_t *return_lab = lv_label_create(return_btn);
    lv_label_set_text(return_lab, "return");
    lv_obj_center(return_lab);
    lv_obj_add_event_cb(return_btn, Game2048_Return_Task, LV_EVENT_SHORT_CLICKED, UC_P);

    lv_obj_t *exit_btn = lv_btn_create(game_data->game_root);
    lv_obj_set_size(exit_btn, 60, 20);
    lv_obj_set_pos(exit_btn, 80, 460);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xFF4444), LV_STATE_DEFAULT);
    lv_obj_t *exit_lab = lv_label_create(exit_btn);
    lv_label_set_text(exit_lab, "exit");
    lv_obj_center(exit_lab);
    lv_obj_add_event_cb(exit_btn, Game2048_Exit_Task, LV_EVENT_SHORT_CLICKED, UC_P);
}

// 新增：释放2048游戏容器资源的回调函数
static void game2048_del_container(lv_timer_t *timer) 
{
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)timer->user_data;
    if (UC_P == NULL || UC_P->game2048_data == NULL) 
    {
        lv_timer_del(timer);
        return;
    }
    // 删除游戏根容器
    if (UC_P->game2048_data->game_root != NULL) {
        lv_obj_del(UC_P->game2048_data->game_root);
        UC_P->game2048_data->game_root = NULL;
    }
    // 释放游戏数据内存
    free(UC_P->game2048_data);
    UC_P->game2048_data = NULL;
    // 删除定时器（避免重复调用）
    lv_timer_del(timer);
}

// ---------------------- 20250905新增：2048游戏→首页回调 ------
void Game2048_Return_Task(lv_event_t *e)
{
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)e->user_data;
    if (UC_P == NULL || UC_P->game2048_data == NULL || UC_P->start_ui_p == NULL) {
        printf("2048返回：非法指针\n");
        return;
    }

    // 1. 界面切换动画（从上往下滑，复用现有逻辑）
    lv_obj_t *old_screen = UC_P->game2048_data->game_root;
    lv_obj_t *new_screen = UC_P->start_ui_p->start_ui;
    lv_coord_t screen_height = lv_disp_get_ver_res(NULL);

    lv_obj_set_y(new_screen, -screen_height);
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    // 旧屏幕向下滑出
    lv_anim_t a_slide_out;
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, screen_height);
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 500);

    // 新屏幕从上往下滑入
    lv_anim_t a_slide_in;
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, -screen_height, 0);
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 500);

    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);

    // 延迟释放游戏资源（确保动画完成）
    // 复用game2048_del_container）：
    lv_timer_create(game2048_del_container, 500, UC_P);

    UC_P->exit_mask = 0;
}

// ---------------------- 20250905新增：2048游戏→结束界面回调 ------
void Game2048_Exit_Task(lv_event_t *e)
{
    struct Ui_Ctrl *UC_P = (struct Ui_Ctrl *)e->user_data;
    if (UC_P == NULL || UC_P->game2048_data == NULL || UC_P->end_ui_p == NULL) {
        printf("2048退出：非法指针\n");
        return;
    }

    // 1. 界面切换动画（从下往上滑，复用现有逻辑）
    lv_obj_t *old_screen = UC_P->game2048_data->game_root;
    lv_obj_t *new_screen = UC_P->end_ui_p->end_ui;
    lv_coord_t screen_height = lv_disp_get_ver_res(NULL);

    lv_obj_set_y(new_screen, screen_height);
    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

    // 旧屏幕向上滑出
    lv_anim_t a_slide_out;
    lv_anim_init(&a_slide_out);
    lv_anim_set_var(&a_slide_out, old_screen);
    lv_anim_set_values(&a_slide_out, 0, -screen_height);
    lv_anim_set_exec_cb(&a_slide_out, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_out, 500);

    // 新屏幕从下往上滑入
    lv_anim_t a_slide_in;
    lv_anim_init(&a_slide_in);
    lv_anim_set_var(&a_slide_in, new_screen);
    lv_anim_set_values(&a_slide_in, screen_height, 0);
    lv_anim_set_exec_cb(&a_slide_in, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_time(&a_slide_in, 500);

    lv_anim_start(&a_slide_out);
    lv_anim_start(&a_slide_in);

    // 延迟释放游戏资源（确保动画完成）
    // （复用game2048_del_container）：
    lv_timer_create(game2048_del_container, 500, UC_P);

    UC_P->exit_mask = 0;
}
// -------------------------------------------------------------------


// 显示指定目录下的文件夹，第一次默认显示根目录下
bool Show_Dir_List(const char * obj_dir_path,struct Ui_Ctrl * UC_P)
{
    
    DIR * dp = opendir(obj_dir_path);
    if(dp == (DIR *)NULL)
    {
        perror("opendir ...");
        return false;
    }

    char * addr;
    int file_btn_x = 8;
    int file_btn_y = 8;
    while(1)
    {
        struct dirent * eq = readdir(dp);
        if(eq == (struct dirent *)NULL)
        {
            break;
        }

        if(strcmp(eq->d_name,".") == 0) continue;//忽略.

        if(eq->d_type == DT_DIR) //判断是不是文件夹
        {
            
            DBI_P btn_inf = Create_Node();
            if(btn_inf == (DBI_P)-1)
            {
                printf("创建目录按钮节点失败！\n");
                return false;
            }
            
            //把btn_inf添加链表中 --- 头插
            Head_Add_Node(UC_P->dir_btn_list_head,btn_inf);

            btn_inf->UC_P = UC_P;

            //把根目录  和当前的文件夹名字 一起拼接到
            //按..优化拼接
            
            if(strcmp(eq->d_name,"..") == 0)  
            {
                
                addr = strrchr(obj_dir_path,'/');//获取最后一个/的地址
                
                if(strcmp(obj_dir_path,"/") == 0 ||  addr == &obj_dir_path[0]) //如果最后一个/是第一个位置那么就是二级目录
                {
                    
                    strcpy(btn_inf->new_dir_name,DEFAULT_SHOW_DIR);
                }
                else
                {
                    
                    //把最后一个/变成/0
                    strcpy(btn_inf->new_dir_name,obj_dir_path);
                    addr = strrchr(btn_inf->new_dir_name,'/');//获取最后一个/的地址
                    *addr = '\0';

                }
            }
            else
            {
                 //不按..正常拼接
                if(obj_dir_path[strlen(obj_dir_path)-1] == '/')
                {
                    sprintf(btn_inf->new_dir_name,"%s%s",obj_dir_path,eq->d_name);
                }
                else
                {
                    sprintf(btn_inf->new_dir_name,"%s/%s",obj_dir_path,eq->d_name);
                }
            }

            //给list 添加按钮，按钮文本就是文件夹的名字
            lv_obj_t * dir_btn = lv_list_add_btn(UC_P->dir_ui_p->dir_list,LV_SYMBOL_DIRECTORY,eq->d_name);
        
            //给当前的目录按钮注册中断任务函数
            lv_obj_add_event_cb(dir_btn,Dir_Btn_Task,LV_EVENT_SHORT_CLICKED,btn_inf);
        }
    
        if(eq->d_type == DT_REG) //判断是不是文件
        {
            //创建文件按钮对应的链表结点，保存文件的完整路径，这个结点到时候是中断任务函数需要传参的
            DBI_P btn_inf = Create_Node();
            if(btn_inf == (DBI_P)-1)
            {
                printf("创建目录按钮节点失败！\n");
                return false;
            }
            
            //把btn_inf添加链表中 --- 头插
            Head_Add_Node(UC_P->dir_btn_list_head,btn_inf);

            btn_inf->UC_P = UC_P;


            if(obj_dir_path[strlen(obj_dir_path)-1] == '/')
            {
                sprintf(btn_inf->new_dir_name,"%s%s",obj_dir_path,eq->d_name);
            }
            else
            {
                sprintf(btn_inf->new_dir_name,"%s/%s",obj_dir_path,eq->d_name);
            }


            //给当前文件 创建一个按钮，给按钮设置一个标签，标签文本就是文件名字 按钮的父类是file_ui
            lv_obj_t * file_btn = lv_btn_create(UC_P->dir_ui_p->file_ui);
            
            //设置每一个按钮都是 80 x 80
            lv_obj_set_size(file_btn,80,80);
            
            //设置每一个按钮的坐标位置
            lv_obj_set_pos(file_btn,file_btn_x,file_btn_y);

            lv_obj_t * btn_lab = lv_label_create(file_btn);
            lv_label_set_text(btn_lab,eq->d_name);
            lv_label_set_long_mode(btn_lab,LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_set_width(btn_lab,80);


            file_btn_x+=90;
            if(file_btn_x > 360) //判断按钮需不需要换行显示
            {
                file_btn_x = 8;
                file_btn_y+= 90;
            }

            //给当前按钮 注册中断任务函数 jpg gif png 的按钮  
            //获取字符串最后出现的.的位置
            char * obj_p = strrchr(eq->d_name,'.');
            if(obj_p == NULL) continue;

            if((strcmp(obj_p,".jpg") == 0) || (strcmp(obj_p,".gif") == 0) || (strcmp(obj_p,".png") == 0))
            {
                lv_obj_add_event_cb(file_btn,File_Btn_Task,LV_EVENT_SHORT_CLICKED,btn_inf);
            }
        }
    }

    if(closedir(dp) == -1)
    {
        perror("closedir ...");
        return false;
    }

    return true;
}


void Dir_Look_Free(struct Ui_Ctrl * UC_P)  //程序结束释放函数
{
    //要删除 三个界面
    lv_obj_del(UC_P->start_ui_p->start_ui);
    lv_obj_del(UC_P->dir_ui_p->dir_ui);
    lv_obj_del(UC_P->end_ui_p->end_ui);

    lv_obj_del(UC_P->emergency_ui_p->emergency_ui); // 20250903新增：删除紧急界面容器 10.32
    lv_obj_del(UC_P->device_info_ui_p->device_info_ui); // 删除设备信息界面容器

    if(Destory_Dir_Btn_List(UC_P->dir_btn_list_head) == false)
    {
        printf("摧毁链表失败！\n");
        return ;
    }

    //释放链表头节点
    free(UC_P->dir_btn_list_head);

    //free 三个结构体堆空间
    free(UC_P->start_ui_p);
    free(UC_P->dir_ui_p);
    free(UC_P->end_ui_p);
    free(UC_P->emergency_ui_p); // 20250903新增：释放紧急界面结构体 10.32
    free(UC_P->device_info_ui_p); // 20250903新增：释放设备信息界面结构体 14.03
    
    // ---------------------- 20250905新增：释放2048游戏资源 ------
    if (UC_P->game2048_data != NULL) {
        if (UC_P->game2048_data->game_root != NULL) {
            lv_obj_del(UC_P->game2048_data->game_root);
        }
        free(UC_P->game2048_data);
        UC_P->game2048_data = NULL;
    }
    // -------------------------------------------------------------------
    
    return ;
}

// ---------------------- 新增：扫描目录下所有图片文件 -----20250902 20.13
void Scan_Img_Files(const char *dir_path, struct Ui_Ctrl *UC_P)
{
    if(UC_P == NULL || UC_P->dir_ui_p == NULL) return;

    // 先释放原有图片列表，避免内存泄漏
    if(UC_P->dir_ui_p->img_file_list != NULL)
    {
        for(int i=0; i<UC_P->dir_ui_p->img_file_count; i++)
        {
            if(UC_P->dir_ui_p->img_file_list[i] != NULL)
                free(UC_P->dir_ui_p->img_file_list[i]);
        }
        free(UC_P->dir_ui_p->img_file_list);
        UC_P->dir_ui_p->img_file_list = NULL;
    }
    UC_P->dir_ui_p->img_file_count = 0;
    UC_P->dir_ui_p->current_img_idx = -1;

    // 打开目录
    DIR *dp = opendir(dir_path);
    if(dp == NULL)
    {
        perror("Scan_Img_Files opendir failed");
        return;
    }

    struct dirent *eq;
    // 第一步：统计图片文件数量
    while((eq = readdir(dp)) != NULL)
    {
        // 跳过.和..目录
        if(strcmp(eq->d_name, ".") == 0 || strcmp(eq->d_name, "..") == 0)
            continue;
        // 只处理普通文件
        if(eq->d_type != DT_REG)
            continue;
        // 判断是否为支持的图片格式
        char *ext = strrchr(eq->d_name, '.');
        if(ext != NULL && (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".png") == 0 || strcmp(ext, ".gif") == 0))
        {
            UC_P->dir_ui_p->img_file_count++;
        }
    }

    // 第二步：分配内存存储图片路径（带S:前缀）
    if(UC_P->dir_ui_p->img_file_count > 0)
    {
        UC_P->dir_ui_p->img_file_list = (char **)malloc(sizeof(char *) * UC_P->dir_ui_p->img_file_count);
        if(UC_P->dir_ui_p->img_file_list == NULL)
        {
            perror("Scan_Img_Files malloc img_file_list failed");
            closedir(dp);
            return;
        }
        memset(UC_P->dir_ui_p->img_file_list, 0, sizeof(char *) * UC_P->dir_ui_p->img_file_count);

        // 重新遍历目录，填充图片路径
        rewinddir(dp);
        int idx = 0;
        while((eq = readdir(dp)) != NULL)
        {
            if(strcmp(eq->d_name, ".") == 0 || strcmp(eq->d_name, "..") == 0)
                continue;
            if(eq->d_type != DT_REG)
                continue;
            char *ext = strrchr(eq->d_name, '.');
            if(ext == NULL)
                continue;
            if(strcmp(ext, ".jpg") == 0 || strcmp(ext, ".png") == 0 || strcmp(ext, ".gif") == 0)
            {
                char full_path[256*2] = {0};
                // 拼接带S:的完整路径
                if(dir_path[strlen(dir_path)-1] == '/')
                    sprintf(full_path, "S:%s%s", dir_path, eq->d_name);
                else
                    sprintf(full_path, "S:%s/%s", dir_path, eq->d_name);
                // 分配路径内存并复制
                UC_P->dir_ui_p->img_file_list[idx] = (char *)malloc(strlen(full_path) + 1);

                // ------------------20250908新增修改 05.19
                // 初始化当前索引的列表元素为NULL（避免野指针）
                UC_P->dir_ui_p->img_file_list[idx] = NULL;

                UC_P->dir_ui_p->img_file_list[idx] = (char *)malloc(strlen(full_path) + 1);
                if(UC_P->dir_ui_p->img_file_list[idx] == NULL)
                {
                    perror("Scan_Img_Files malloc img path failed");
                    UC_P->dir_ui_p->img_file_count--;
                    idx--; 
                    continue;
                }
                strcpy(UC_P->dir_ui_p->img_file_list[idx], full_path);
                idx++;

                // 新增：扫描结束后，确保列表元素数量与img_file_count一致（防止遗漏递减）
                if (idx != UC_P->dir_ui_p->img_file_count)
                {
                    UC_P->dir_ui_p->img_file_count = idx;
                    printf("Scan_Img_Files：校准图片数量为%d\n", idx);
                }
            }
        }
    }

    closedir(dp);
}
// -------------------------------------------------------------------


DBI_P Create_Node()//创建目录按钮头节点  
{
    DBI_P new_node = (DBI_P)malloc(sizeof(DBI));
    if(new_node == (DBI_P)NULL)
    {
        perror("malloc new node ...");
        return (DBI_P)-1;
    }

    memset(new_node,0,sizeof(DBI));

    new_node->next = new_node;
    new_node->prev = new_node;

    return new_node;
}


bool  Head_Add_Node(DBI_P head_node,DBI_P new_node)
{
    if(head_node == (DBI_P)NULL)
    {
        printf("头结点异常，无法添加！\n");
        return false;
    }

    new_node->next        = head_node->next;
    head_node->next->prev = new_node;
    new_node->prev        = head_node;
    head_node->next       = new_node;

    return true;
}


bool  Destory_Dir_Btn_List(DBI_P head_node)
{
    if(head_node == (DBI_P)NULL)
    {
        printf("头结点异常，无法摧毁！\n");
        return false;
    }

    
    while(head_node->next != head_node)
    {
        DBI_P free_node = head_node->next;

        free_node->next->prev = free_node->prev;
        free_node->prev->next = free_node->next;

        free_node->next = (DBI_P)NULL;
        free_node->prev = (DBI_P)NULL;

        free(free_node);
    }

    return true;
}