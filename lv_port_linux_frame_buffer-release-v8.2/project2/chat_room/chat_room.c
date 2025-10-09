//@file chat_room.c 聊天室客户端

#include "../common/chat_adapt.h"  // 新增：引入函数声明
#include "chat_room.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <ifaddrs.h>  // 动态获取IP所需头文件
#include "../common/chat_adapt.h"

// 服务器配置（新手需替换为华为云/阿里云IP和端口）
#define SERVER_IP "8.134.200.90"  // 如"121.43.xxx.xxx"
#define SERVER_PORT 8888                // 需与服务器端口一致

static CHAT_CTRL_P g_chat_ctrl = NULL; // 全局控制指针
static pthread_t recv_thread_id;     // 接收服务器消息线程
static pthread_mutex_t msg_mutex;    // 线程安全互斥锁

// 20250927新增：字体声明（复用dir_look的楷体20号，确保中文显示）
LV_FONT_DECLARE(lv_myfont_kai_20);

// 函数前置声明（符合模块化规范）
static void Login_Click(lv_event_t *e);

static void Reg_Click(lv_event_t *e);
static void Do_Register(lv_event_t *e);
static void Friend_Click(lv_event_t *e);
static void Send_Msg_Click(lv_event_t *e);

static void Connect_Server_Click(lv_event_t *e);
static void *Recv_Server_Msg(void *arg);

//20250927新增
static void Back_To_Friend(lv_event_t *e);
static void Handle_Server_Msg(NetMsg *msg);
static void Create_Setting_Scr(void); // 新增设置界面
static void Add_Friend_Click(lv_event_t *e); // 新增添加好友
static void Set_Signature_Click(lv_event_t *e); // 新增设置签名
static char *Get_Local_IP(void); // 新增动态获取开发板IP

static void Back_To_Home(lv_event_t *e);  // 20250927新增补充缺失声明
static void Create_Login_Scr(void);  // 补充声明
static lv_obj_t *Create_Textarea(lv_obj_t *parent, const char *hint_text);
static lv_obj_t *Create_Label(lv_obj_t *parent, const char *text, lv_coord_t y);
static int Send_To_Server(NetMsg *msg);
static int Connect_Server(void);

static void Chat_Room_Exit_Task(lv_event_t *e); //20250928新增
static void Logout_Btn_Task(lv_event_t *e);
static bool is_thread_created = false; // 20250928新增：线程创建标志

static void Refresh_Friend_List(lv_event_t *e); // 20250929新增：刷新好友列表回调函数声明
static void Set_Avatar_Click(lv_event_t *e); // 20250929新增：设置头像回调函数声明
static void Enter_Group_Chat(lv_event_t *e); // 20250929新增：进入群聊

static char g_saved_cur_account[32] = {0};// 20251009新增：保存登录账号（不随g_chat_ctrl释放，用于重新进入时恢复状态）

// -------------------------- 工具函数 --------------------------

// 20250927新增：动态获取开发板IP（适配eth0网卡，新手无需修改）- 14.30
static char *Get_Local_IP(void) {
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    static char ip[16] = {0};

    if(getifaddrs(&ifap) == -1) {
        perror("getifaddrs failed");
        return "127.0.0.1";
    }

    for(ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr->sa_family == AF_INET && strcmp(ifa->ifa_name, "eth0") == 0) {
            sa = (struct sockaddr_in *)ifa->ifa_addr;
            strcpy(ip, inet_ntoa(sa->sin_addr));
            break;
        }
    }
    freeifaddrs(ifap);
    return ip[0] ? ip : "192.168.1.100"; // 默认值兜底
}
// -------------------------------------------

// 20251009新增：获取聊天记录文件路径（格式：/tmp/chat_当前账号_好友账号.txt）---------
static char *Get_Chat_Log_Path(const char *friend_acc) {
    static char path[64];
    snprintf(path, 64, "/tmp/chat_%s_%s.txt", g_chat_ctrl->cur_account, friend_acc);
    return path;
}

// 新增：保存聊天记录到本地文件
static void Save_Chat_Log(const char *friend_acc, const char *sender, const char *msg) {
    if (!friend_acc || !sender || !msg) return;
    const char *path = Get_Chat_Log_Path(friend_acc);
    FILE *fp = fopen(path, "a+"); // 追加模式
    if (!fp) {
        perror("Save_Chat_Log fopen failed");
        return;
    }
    // 记录格式：[时间] 发送者: 消息\n
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[32];
    strftime(time_str, 32, "[%Y-%m-%d %H:%M:%S]", t);
    fprintf(fp, "%s %s: %s\n", time_str, sender, msg);
    fclose(fp);
}

// 新增：加载聊天记录到聊天界面
static void Load_Chat_Log(const char *friend_acc) {
    if (!friend_acc || !g_chat_ctrl->chat_content_ta) return;
    const char *path = Get_Chat_Log_Path(friend_acc);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        // 文件不存在时返回空（首次聊天）
        return;
    }
    // 读取文件内容到缓冲区。20251009修改：静态变量（存储在数据段，不占用栈空间）
    static char buf[4096] = {0};    // 静态4KB缓冲区（仅初始化1次）
    static char line[256];          // 静态256字节行缓存
    memset(buf, 0, sizeof(buf));   // 20251009新增：每次调用清空缓冲区（避免历史数据残留）

    while (fgets(line, sizeof(line), fp)) {
        // 20251009修改：确保不超出缓冲区大小（增加安全校验）
        if (strlen(buf) + strlen(line) < sizeof(buf) - 1) 
        {

            strncat(buf, line, sizeof(buf) - strlen(buf) - 1);

        } else {
            printf("Load_Chat_Log: 聊天记录超出缓冲区上限，部分内容未加载\n");
            break;
        }
    }
    fclose(fp);
    // 加载到聊天文本框
    lv_textarea_set_text(g_chat_ctrl->chat_content_ta, buf);
    // 滚动到底部
    lv_textarea_set_cursor_pos(g_chat_ctrl->chat_content_ta, strlen(buf));
}
//-----------------------------------------

// 创建输入框（复用UI代码，减少冗余）
static lv_obj_t *Create_Textarea(lv_obj_t *parent, const char *hint_text) {
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, 250, 40);
    lv_textarea_set_placeholder_text(ta, hint_text);
    lv_obj_set_style_text_font(ta, &lv_myfont_kai_20, LV_STATE_DEFAULT); // 复用楷体
    return ta;
}

// 创建标签（简化控件创建）
static lv_obj_t *Create_Label(lv_obj_t *parent, const char *text, lv_coord_t y) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_text_font(label, &lv_myfont_kai_20, LV_STATE_DEFAULT); // 楷体
    lv_obj_set_style_text_color(label, lv_color_hex(0x333333), LV_STATE_DEFAULT); // 深灰
    return label;
}

// 连接云服务器（封装TCP客户端逻辑，TCP协议，适配云服务器）
static int Connect_Server() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("socket create failed");
        return -1;
    }

    // 服务器地址结构
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if(inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("invalid server IP");
        close(sockfd);
        return -1;
    }

    // 20250927新增连接服务器（超时处理：新手友好） 14.35
    struct timeval timeout = {3, 0}; // 20251008新增修改：仅保留3秒发送超时（避免发送阻塞）
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect server failed");
        close(sockfd);
        return -1;
    }
    printf("连接服务器成功\n\n");
    return sockfd;
}

// 发送消息到服务器（封装发送逻辑）
static int Send_To_Server(NetMsg *msg) {
    if(!g_chat_ctrl || g_chat_ctrl->sockfd < 0) return -1;
    return send(g_chat_ctrl->sockfd, msg, sizeof(NetMsg), 0);
}

// -------------------------- 界面切换与创建 --------------------------
// 20251009新增修改：返回首页。仅切换 LVGL 界面，不释放g_chat_ctrl、sockfd和线程，确保登录状态保留。
static void Back_To_Home(lv_event_t *e) {
    lv_obj_t *scr_home = (lv_obj_t *)lv_event_get_user_data(e);

    if (!scr_home || !lv_obj_is_valid(scr_home)) return;
    // 仅切换界面，不释放聊天室资源（保留g_chat_ctrl和登录状态）
    lv_scr_load(scr_home);
    printf("返回首页：保留聊天室资源，账号=%s\n", g_chat_ctrl->cur_account);
}

// 返回好友列表
static void Back_To_Friend(lv_event_t *e) {
    lv_scr_load(g_chat_ctrl->scr_friend);
}

// 注册按钮回调（切换到注册界面）
static void Reg_Click(lv_event_t *e) {
    lv_scr_load(g_chat_ctrl->scr_register);
}

static void Setting_Btn_Task(lv_event_t *e)    //20250928新增
{
    // 1. 校验全局UI控制结构体有效性（避免空指针异常）
    extern CHAT_CTRL_P g_chat_ctrl;  // 声明全局聊天模块控制结构体
    if(g_chat_ctrl == NULL || g_chat_ctrl->scr_setting == NULL)
    {
        printf("Setting interface not initialized!\n");  // 调试log
        return;
    }

    // 2. 切换至设置界面（LVGL8.2标准界面切换API）
    lv_scr_load(g_chat_ctrl->scr_setting);
}


// 登录按钮回调（发送登录请求，适配ACK应答）
static void Login_Click(lv_event_t *e) 
{
    NetMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_LOGIN;
    // 获取输入框内容
    lv_obj_t *account_ta = lv_obj_get_child(g_chat_ctrl->scr_login, 1); // 账号输入框（索引1）
    lv_obj_t *pwd_ta = lv_obj_get_child(g_chat_ctrl->scr_login, 2);     // 密码输入框（索引2）

    const char *account = lv_textarea_get_text(account_ta);//20250930新增：登录输入非空校验
    const char *pwd = lv_textarea_get_text(pwd_ta);
    // 新增：输入非空校验
    if(strlen(account) == 0 || strlen(pwd) == 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "登录失败：账号/密码不能为空");
        return;
    }
    strncpy(msg.user.account, lv_textarea_get_text(account_ta), 31);
    strncpy(msg.user.password, lv_textarea_get_text(pwd_ta), 31);

    strncpy(msg.user.ip, Get_Local_IP(), 15); // 20250927新增：动态获取IP
    msg.user.port = 8000; // 固定本地端口（新手无需修改）
    
    // 发送登录请求
    if(Send_To_Server(&msg) < 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "登录失败：连接异常");
        return;
    }

    // 20250930新增：仅用于测试：在Login_Click函数末尾添加（测试后删除）
    // 强制跳转到好友列表，确认界面是否正常
    // if(g_chat_ctrl->scr_friend && lv_obj_is_valid(g_chat_ctrl->scr_friend)){
    //     lv_scr_load(g_chat_ctrl->scr_friend);
    //     lv_refr_now(lv_disp_get_default());
    //     printf("测试：强制跳转到好友列表\n");
    // }

}

// 创建登录界面（复用dir_look背景色：绿豆沙#C7EDCC）
static void Create_Login_Scr(void) 
{
    g_chat_ctrl->scr_login = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_login, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    // 20250927新增：状态提示标签（索引0）
    Create_Label(g_chat_ctrl->scr_login, "请先连接服务器", 30);

    // 账号输入框（索引1）
    lv_obj_t *account_ta = Create_Textarea(g_chat_ctrl->scr_login, "请输入账号");
    lv_obj_align(account_ta, LV_ALIGN_TOP_MID, 0, 80);
    // 20250929新增：绑定键盘
    Dir_Look_Bind_Textarea_Keyboard(account_ta, g_chat_ctrl->scr_login);

    // 密码输入框（索引2）
    lv_obj_t *pwd_ta = Create_Textarea(g_chat_ctrl->scr_login, "请输入密码");
    lv_textarea_set_password_mode(pwd_ta, true);
    lv_obj_align(pwd_ta, LV_ALIGN_TOP_MID, 0, 140);
    // 20250929新增：绑定键盘
    Dir_Look_Bind_Textarea_Keyboard(pwd_ta, g_chat_ctrl->scr_login);

    // 登录按钮
    lv_obj_t *login_btn = lv_btn_create(g_chat_ctrl->scr_login);
    lv_obj_set_size(login_btn, 105, 30);
    lv_obj_align(login_btn, LV_ALIGN_TOP_MID, -60, 200);
    lv_obj_t *login_label = lv_label_create(login_btn);
    lv_label_set_text(login_label, "登录");
    lv_obj_set_style_text_font(login_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增，适配中文字体
    lv_obj_center(login_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 注册按钮
    lv_obj_t *reg_btn = lv_btn_create(g_chat_ctrl->scr_login);
    lv_obj_set_size(reg_btn, 105, 30);
    lv_obj_align(reg_btn, LV_ALIGN_TOP_MID, 60, 200);
    lv_obj_t *reg_label = lv_label_create(reg_btn);
    lv_label_set_text(reg_label, "注册");
    lv_obj_set_style_text_font(reg_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增，适配中文字体
    lv_obj_center(reg_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 返回首页按钮
    lv_obj_t *back_btn = lv_btn_create(g_chat_ctrl->scr_login);
    lv_obj_set_size(back_btn, 105, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回首页");
    lv_obj_set_style_text_font(back_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增，适配中文字体
    lv_obj_center(back_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 连接服务器按钮
    lv_obj_t *connect_btn = lv_btn_create(g_chat_ctrl->scr_login);
    lv_obj_set_size(connect_btn, 110, 30);
    lv_obj_align(connect_btn, LV_ALIGN_TOP_MID, 0, 250);
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "连接服务器");
    lv_obj_set_style_text_font(connect_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增，适配中文字体
    lv_obj_center(connect_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 20250928新增：退出按钮（释放所有聊天室资源）
    lv_obj_t *exit_btn = lv_btn_create(g_chat_ctrl->scr_login);
    lv_obj_set_size(exit_btn, 105, 30);
    lv_obj_align(exit_btn, LV_ALIGN_BOTTOM_LEFT, 130, -20); // 返回首页按钮右侧
    lv_obj_t *exit_label = lv_label_create(exit_btn);
    lv_label_set_text(exit_label, "退出");
    lv_obj_set_style_text_font(exit_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_center(exit_label);
    // 绑定退出回调
    lv_obj_add_event_cb(exit_btn, Chat_Room_Exit_Task, LV_EVENT_CLICKED, NULL);

    // 绑定事件
    lv_obj_add_event_cb(connect_btn, Connect_Server_Click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(back_btn, Back_To_Home, LV_EVENT_CLICKED, g_chat_ctrl->scr_home);
    lv_obj_add_event_cb(login_btn, Login_Click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(reg_btn, Reg_Click, LV_EVENT_CLICKED, NULL);
}

// 20250928新增：退出按钮回调（释放所有资源）
static void Chat_Room_Exit_Task(lv_event_t *e)
{
    printf("退出按钮被点击\n"); //20250928新增

    // 20250928新增修复1：提前保存首页界面指针（避免释放后访问）
    lv_obj_t *scr_home = NULL;
    if (g_chat_ctrl) {
        scr_home = g_chat_ctrl->scr_home; // 提前保存首页指针
        printf("保存首页指针: %p\n", scr_home);
    }
    // -------------------
    // 发送离线通知（若已登录）
    if (g_chat_ctrl && g_chat_ctrl->sockfd >= 0 && strlen(g_chat_ctrl->cur_account) > 0) {
        NetMsg offline_msg;
        memset(&offline_msg, 0, sizeof(offline_msg));
        offline_msg.type = MSG_LOGOUT; // 新增消息类型：退出登录
        strncpy(offline_msg.user.account, g_chat_ctrl->cur_account, 31);
        Send_To_Server(&offline_msg);
        printf("发送离线通知完成\n");
    }
    Chat_Room_Exit(); // 调用原有释放函数，释放聊天室资源（g_chat_ctrl 会被置空）
    printf("Chat_Room_Exit 执行完成\n");

    // 20250928新增修复2：使用提前保存的指针加载首页，避免访问 NULL
    if (scr_home != NULL) {
        printf("准备加载首页: %p\n", scr_home);
        lv_scr_load(scr_home);
        printf("首页加载完成\n");
    }else {
        printf("错误：首页指针为NULL\n");
    }
    //-----------------------------------------
}

// 连接服务器按钮回调
static void Connect_Server_Click(lv_event_t *e) {
    if(g_chat_ctrl->sockfd >= 0) {
        // 已经连接，提示用户
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "已经连接到服务器");
        return;
    }
    
    // 尝试连接服务器
    g_chat_ctrl->sockfd = Connect_Server();
    if(g_chat_ctrl->sockfd < 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "连接服务器失败,请检查IP/端口");
        return;
    }
    
    // 启动接收线程
    pthread_create(&recv_thread_id, NULL, Recv_Server_Msg, NULL);
    is_thread_created = true; // 20250928新增
    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "连接服务器成功！请登录");
}


// 注册按钮回调（发送注册请求，全量信息上报）
static void Do_Register(lv_event_t *e) 
{
    NetMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_REGISTER;
    // 获取输入框内容
    lv_obj_t *account_ta = lv_obj_get_child(g_chat_ctrl->scr_register, 1);
    lv_obj_t *pwd_ta = lv_obj_get_child(g_chat_ctrl->scr_register, 2);
    lv_obj_t *nick_ta = lv_obj_get_child(g_chat_ctrl->scr_register, 3);

    const char *account = lv_textarea_get_text(account_ta); //20250930新增：注册输入非空校验
    const char *pwd = lv_textarea_get_text(pwd_ta);
    const char *nick = lv_textarea_get_text(nick_ta);


    // 20250930新增：输入非空校验。20251008新增修改：细化非空判断
    if(strlen(account) == 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册失败：账号不能为空");
        return;
    } else if(strlen(pwd) == 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册失败：密码不能为空");
        return;
    } else if(strlen(nick) == 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册失败：昵称不能为空");
        return;
    }

    strncpy(msg.user.account, lv_textarea_get_text(account_ta), 31);
    strncpy(msg.user.password, lv_textarea_get_text(pwd_ta), 31);
    strncpy(msg.user.nickname, lv_textarea_get_text(nick_ta), 31);
    // 填充本地IP（开发板IP，可通过ifconfig获取）
    // strncpy(msg.user.ip, "192.168.1.100", 15); // 初学者需修改为实际开发板IP

    strncpy(msg.user.ip, Get_Local_IP(), 15); // 20250927新增：动态IP
    msg.user.port = 8000; // 固定本地端口

    strncpy(msg.user.signature, "默认签名", 15); // 20250927新增：默认签名
    strncpy(msg.user.avatar, "S:/avatar/default.png", 20); // 20250927新增：默认头像

    // 发送注册请求
    if(Send_To_Server(&msg) < 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册失败：连接异常");
        return;
    }
    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册中...");
}

// 创建注册界面
static void Create_Register_Scr() 
{
    g_chat_ctrl->scr_register = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_register, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    // 标题状态提示标签（索引0）
    Create_Label(g_chat_ctrl->scr_register, "请填写注册信息", 30);

    // 账号、密码、昵称输入框
    lv_obj_t *account_ta = Create_Textarea(g_chat_ctrl->scr_register, "请设置账号（唯一）");
    lv_obj_align(account_ta, LV_ALIGN_TOP_MID, 0, 80);// 索引1
    // 20250929新增：绑定键盘
    Dir_Look_Bind_Textarea_Keyboard(account_ta, g_chat_ctrl->scr_register);

    lv_obj_t *pwd_ta = Create_Textarea(g_chat_ctrl->scr_register, "请设置密码");
    lv_textarea_set_password_mode(pwd_ta, true);
    lv_obj_align(pwd_ta, LV_ALIGN_TOP_MID, 0, 140);// 索引2
    // 20250929新增：绑定键盘
    Dir_Look_Bind_Textarea_Keyboard(pwd_ta, g_chat_ctrl->scr_register);

    lv_obj_t *nick_ta = Create_Textarea(g_chat_ctrl->scr_register, "请设置昵称");
    lv_obj_align(nick_ta, LV_ALIGN_TOP_MID, 0, 200);// 索引3
    // 20250929新增：绑定键盘
    Dir_Look_Bind_Textarea_Keyboard(nick_ta, g_chat_ctrl->scr_register);

    // 注册按钮
    lv_obj_t *reg_btn = lv_btn_create(g_chat_ctrl->scr_register);
    lv_obj_set_size(reg_btn, 105, 30);
    lv_obj_align(reg_btn, LV_ALIGN_TOP_MID, 0, 260);
    lv_obj_t *reg_label = lv_label_create(reg_btn);
    lv_label_set_text(reg_label, "注册");
    lv_obj_set_style_text_font(reg_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增，适配中文字体
    lv_obj_center(reg_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 返回登录按钮
    lv_obj_t *back_btn = lv_btn_create(g_chat_ctrl->scr_register);
    lv_obj_set_size(back_btn, 105, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回登录");
    lv_obj_add_event_cb(back_btn, Back_To_Friend, LV_EVENT_CLICKED, NULL); // 复用返回好友列表回调
    lv_obj_set_style_text_font(back_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);//适配中文字体
    lv_obj_center(back_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 绑定事件
    lv_obj_add_event_cb(back_btn, Back_To_Home, LV_EVENT_CLICKED, g_chat_ctrl->scr_login);//20250927新增
    lv_obj_add_event_cb(reg_btn, Do_Register, LV_EVENT_CLICKED, NULL);
}

// 好友列表项点击（进入聊天窗口）
static void Friend_Click(lv_event_t *e) 
{
    lv_obj_t *item = lv_event_get_current_target(e);

    // 20251008新增：获取好友信息（账号+头像）
    typedef struct { char account[32]; char avatar[64]; } FriendInfo;
    FriendInfo *info = lv_obj_get_user_data(item);
    if (!info) return;

    // 获取好友昵称
    lv_obj_t *label = lv_obj_get_child(item, 0); // 获取按钮中的第一个子对象（标签）
    const char *friend_name = lv_label_get_text(label);

    // 20250929新增：存储当前聊天好友账号----------------------20251008修改
    strncpy(g_chat_ctrl->chat_friend_account, info->account, sizeof(g_chat_ctrl->chat_friend_account)-1);

    // 20251009新增：进入单聊时删除群聊标题（避免重叠）
    if (g_chat_ctrl->group_chat_title && lv_obj_is_valid(g_chat_ctrl->group_chat_title)) {
        lv_obj_del(g_chat_ctrl->group_chat_title);
        g_chat_ctrl->group_chat_title = NULL; // 置空避免野指针
    }

    // 1. 更新聊天标题（先删除旧标题避免重叠）
    if(g_chat_ctrl->chat_title && lv_obj_is_valid(g_chat_ctrl->chat_title)) {
        lv_obj_del(g_chat_ctrl->chat_title);
    }

    // 新增：更新聊天窗口标题 20251008修改
    g_chat_ctrl->chat_title = lv_label_create(g_chat_ctrl->scr_chat);
    lv_label_set_text_fmt( g_chat_ctrl->chat_title , "聊天：%s", friend_name);
    lv_obj_align( g_chat_ctrl->chat_title , LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font( g_chat_ctrl->chat_title , &lv_myfont_kai_20, LV_STATE_DEFAULT);

    //20251009新增：2.更新好友头像（无效路径用默认头像）
    lv_obj_t *avatar_img = lv_obj_get_child(g_chat_ctrl->chat_avatar_btn, 0);
    if(avatar_img) {
        if(strlen(info->avatar) == 0 || access(info->avatar, R_OK) != 0) {
            lv_img_set_src(avatar_img, "S:/8080icon_img.jpg");
        } else {
            lv_img_set_src(avatar_img, info->avatar);
        }
    }

    // 3. 清空历史聊天记录
    lv_textarea_set_text(g_chat_ctrl->chat_content_ta, "");

    // 20251009新增：加载历史聊天记录
    Load_Chat_Log(info->account);

    // 4. 切换到聊天界面并强制刷新
    lv_scr_load(g_chat_ctrl->scr_chat);
    lv_refr_now(lv_disp_get_default());
    printf("进入单聊：%s（账号：%s）\n\n", friend_name, info->account);
}

// ----20250927新增--------------------
// 添加好友回调
static void Add_Friend_Click(lv_event_t *e) {
    NetMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_ADD_FRIEND;
    strncpy(msg.user.account, g_chat_ctrl->cur_account, 31);

    // 获取输入的好友账号
    lv_obj_t *friend_ta = lv_obj_get_child(g_chat_ctrl->scr_setting, 1);
    strncpy(msg.content, lv_textarea_get_text(friend_ta), 255);

    if(Send_To_Server(&msg) > 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "添加请求已发送");
        lv_textarea_set_text(friend_ta, "");
    }
}

// 设置个性签名回调
static void Set_Signature_Click(lv_event_t *e) {
    NetMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_SET_SIGNATURE;
    strncpy(msg.user.account, g_chat_ctrl->cur_account, 31);

    // 获取输入的签名
    lv_obj_t *sign_ta = lv_obj_get_child(g_chat_ctrl->scr_setting, 2);

    const char *sign_text = lv_textarea_get_text(sign_ta);  //20250930新增：

    // 20250930新增：输入非空校验
    if(strlen(sign_text) == 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "签名不能为空");
        return;
    }

    strncpy(msg.user.signature, sign_text, sizeof(msg.user.signature)-1);//20250930修改

    if(Send_To_Server(&msg) > 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "签名设置成功");
        lv_textarea_set_text(sign_ta, "");
    }
}

    // ------------------------------20250929新增：设置头像回调函数----------------
static void Set_Avatar_Click(lv_event_t *e) {
    NetMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_SET_AVATAR;
    strncpy(msg.user.account, g_chat_ctrl->cur_account, 31);
    // 获取头像路径
    lv_obj_t *avatar_ta = lv_obj_get_child(g_chat_ctrl->scr_setting, 3);
    strncpy(msg.user.avatar, lv_textarea_get_text(avatar_ta), sizeof(msg.user.avatar)-1);
    
    if(Send_To_Server(&msg) > 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "头像设置中...");
        lv_textarea_set_text(avatar_ta, "");
    }
}

// 创建设置界面（扩展功能：添加好友/个性签名）
static void Create_Setting_Scr() {
    g_chat_ctrl->scr_setting = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_setting, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    // 状态标签（索引0）
    Create_Label(g_chat_ctrl->scr_setting, "设置中心", 30);

    // 添加好友输入框（索引1）
    lv_obj_t *friend_ta = Create_Textarea(g_chat_ctrl->scr_setting, "请输入好友账号");
    lv_obj_align(friend_ta, LV_ALIGN_TOP_MID, 0, 80);
    // 20250929新增：绑定键盘
    Dir_Look_Bind_Textarea_Keyboard(friend_ta, g_chat_ctrl->scr_setting);

    // 个性签名输入框（索引2）
    lv_obj_t *sign_ta = Create_Textarea(g_chat_ctrl->scr_setting, "请输入个性签名");
    lv_obj_align(sign_ta, LV_ALIGN_TOP_MID, 0, 140);
    // 20250929新增：绑定键盘
    Dir_Look_Bind_Textarea_Keyboard(sign_ta, g_chat_ctrl->scr_setting);

    // 20250929新增：头像路径输入框（索引3，新增）创建设置头像 UI------------------
    lv_obj_t *avatar_ta = Create_Textarea(g_chat_ctrl->scr_setting, "头像路径（如S:/avatar/1.png）");

    lv_obj_set_size(avatar_ta, 350, 60); // 20250930新增：扩大宽度到300 20251008修改350

    lv_textarea_set_one_line(avatar_ta, false); // 20251008新增：允许换行
    lv_obj_set_scrollbar_mode(avatar_ta, LV_SCROLLBAR_MODE_AUTO);// 20251008新增：启用滚动

    lv_obj_align(avatar_ta, LV_ALIGN_TOP_MID, 0, 220);  //20251008修改：200改为220，避开上方按钮
    Dir_Look_Bind_Textarea_Keyboard(avatar_ta, g_chat_ctrl->scr_setting);
    
    // 设置头像按钮（新增）
    lv_obj_t *avatar_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_set_size(avatar_btn, 120, 40);
    // 20251008新增：按钮在文本框下方（y轴+60），避免重叠
    lv_obj_align(avatar_btn, LV_ALIGN_TOP_MID, 0, 270); //20251008修改
    lv_obj_t *avatar_label = lv_label_create(avatar_btn);
    lv_label_set_text(avatar_label, "设置头像");
    lv_obj_set_style_text_font(avatar_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_center(avatar_label);
    lv_obj_add_event_cb(avatar_btn, Set_Avatar_Click, LV_EVENT_CLICKED, NULL);
    // -----------------------------

    // 添加好友按钮
    lv_obj_t *add_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_align(add_btn, LV_ALIGN_TOP_MID, 190, 80);
    lv_obj_t *add_label = lv_label_create(add_btn);
    lv_label_set_text(add_label, "添加好友");
    lv_obj_set_style_text_font(add_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_center(add_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 设置签名按钮
    lv_obj_t *sign_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_align(sign_btn, LV_ALIGN_TOP_MID, 190, 140);
    lv_obj_t *sign_label = lv_label_create(sign_btn);
    lv_label_set_text(sign_label, "设置签名");
    lv_obj_set_style_text_font(sign_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_center(sign_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 返回好友列表按钮
    lv_obj_t *back_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_set_size(back_btn, 105, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回好友");
    lv_obj_set_style_text_font(back_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_center(back_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 绑定事件
    lv_obj_add_event_cb(back_btn, Back_To_Friend, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(add_btn, Add_Friend_Click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(sign_btn, Set_Signature_Click, LV_EVENT_CLICKED, NULL);
}

// ------------------------------------------

// 创建好友列表界面
static void Create_Friend_Scr() 
{
    g_chat_ctrl->scr_friend = lv_obj_create(NULL);
    //20250930新增：确保g_chat_ctrl->scr_friend被正确赋值给全局变量，且添加日志确认
    if(g_chat_ctrl->scr_friend == NULL) {
        printf("Create_Friend_Scr：创建scr_friend失败！\n");
        return;
    }
    // printf("Create_Friend_Scr：scr_friend创建成功（%p）\n", g_chat_ctrl->scr_friend);
    
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_friend, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    // 20251009新增：清除隐藏标志，确保界面可见
    lv_obj_clear_flag(g_chat_ctrl->scr_friend, LV_OBJ_FLAG_HIDDEN);

    // 标题
    Create_Label(g_chat_ctrl->scr_friend, "好友列表", 20);

    // 好友列表（列表控件）
    g_chat_ctrl->friend_list = lv_list_create(g_chat_ctrl->scr_friend);

    if (g_chat_ctrl->friend_list == NULL) 
    {   //20251009新增
        printf("Create_Friend_Scr：创建好友列表控件失败！\n");
        return;
    }

    lv_obj_set_size(g_chat_ctrl->friend_list, 300, 350);// 高度350，超出自动滚动
    lv_obj_align(g_chat_ctrl->friend_list, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(g_chat_ctrl->friend_list, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);//20250927新增
    // printf("Create_Friend_Scr：好友列表控件创建成功（地址：%p）\n", g_chat_ctrl->friend_list);      //20251009新增

    // 返回首页按钮
    lv_obj_t *home_btn = lv_btn_create(g_chat_ctrl->scr_friend);
    lv_obj_set_size(home_btn, 105, 30);
    lv_obj_align(home_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *home_label = lv_label_create(home_btn);
    lv_label_set_text(home_label, "返回首页");
    lv_obj_set_style_text_font(home_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增
    lv_obj_center(home_label);  // 20250928新增补充：明确标签居中（确保文字居中）
    
    // 设置按钮（扩展功能入口）
    lv_obj_t *set_btn = lv_btn_create(g_chat_ctrl->scr_friend);
    lv_obj_set_size(set_btn, 105, 30);
    lv_obj_align(set_btn, LV_ALIGN_BOTTOM_LEFT, 130, -20);
    lv_obj_t *set_label = lv_label_create(set_btn);
    lv_label_set_text(set_label, "设置");
    lv_obj_set_style_text_font(set_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增
    lv_obj_center(set_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 20250928新增：退出登录按钮
    lv_obj_t *logout_btn = lv_btn_create(g_chat_ctrl->scr_friend);
    lv_obj_set_size(logout_btn, 105, 30);
    lv_obj_align(logout_btn, LV_ALIGN_BOTTOM_LEFT, 240, -20); // 设置按钮右侧
    lv_obj_t *logout_label = lv_label_create(logout_btn);
    lv_label_set_text(logout_label, "退出登录");
    lv_obj_set_style_text_font(logout_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_center(logout_label);
    // 绑定退出登录回调
    lv_obj_add_event_cb(logout_btn, Logout_Btn_Task, LV_EVENT_CLICKED, NULL);

    // 20250929新增：刷新好友列表按钮------------------
    lv_obj_t *refresh_btn = lv_btn_create(g_chat_ctrl->scr_friend);
    lv_obj_set_size(refresh_btn, 105, 30);
    lv_obj_align(refresh_btn, LV_ALIGN_BOTTOM_LEFT, 350, -20); // 退出按钮右侧
    lv_obj_t *refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_label, "刷新列表");
    lv_obj_set_style_text_font(refresh_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_center(refresh_label);
    lv_obj_add_event_cb(refresh_btn, Refresh_Friend_List, LV_EVENT_CLICKED, NULL);
    //------------------

    // 20250929新增：进入群聊按钮-------------
    lv_obj_t *group_btn = lv_btn_create(g_chat_ctrl->scr_friend);
    lv_obj_set_size(group_btn, 105, 30);
    lv_obj_align(group_btn, LV_ALIGN_BOTTOM_LEFT, 460, -20); // 刷新按钮右侧
    lv_obj_t *group_label = lv_label_create(group_btn);
    lv_label_set_text(group_label, "进入群聊");
    lv_obj_set_style_text_font(group_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_center(group_label);
    lv_obj_add_event_cb(group_btn, Enter_Group_Chat, LV_EVENT_CLICKED, NULL);

    // 绑定事件
    lv_obj_add_event_cb(home_btn, Back_To_Home, LV_EVENT_CLICKED, g_chat_ctrl->scr_home);
    lv_obj_add_event_cb(set_btn, Setting_Btn_Task, LV_EVENT_CLICKED, g_chat_ctrl->scr_setting);

}

// 新增：退出登录回调（通知服务器+返回登录界面）
static void Logout_Btn_Task(lv_event_t *e)
{
    // 1. 发送离线消息到服务器
    NetMsg offline_msg;
    memset(&offline_msg, 0, sizeof(offline_msg));
    offline_msg.type = MSG_LOGOUT;
    strncpy(offline_msg.user.account, g_chat_ctrl->cur_account, 31);
    Send_To_Server(&offline_msg);

    // 2. 清空当前账号，返回登录界面
    memset(g_chat_ctrl->cur_account, 0, sizeof(g_chat_ctrl->cur_account));
    lv_scr_load(g_chat_ctrl->scr_login);
}

// ------------------------------------20250929新增：新增Refresh_Friend_List回调实现-------
static void Refresh_Friend_List(lv_event_t *e) {
    // 请求在线用户列表（复用MSG_GET_ONLINE_USER）
    NetMsg get_user_msg = {.type = MSG_GET_ONLINE_USER};
    if (Send_To_Server(&get_user_msg) > 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "正在刷新...");
    } else {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "刷新失败：连接异常");
    }
}

static void Enter_Group_Chat(lv_event_t *e) {   //20250929新增：进入群聊界面

    // 20251008新增：确保聊天界面有效
    if(!g_chat_ctrl->scr_chat || !lv_obj_is_valid(g_chat_ctrl->scr_chat)) {
        printf("Enter_Group_Chat：scr_chat无效\n");
        return;
    }

    // 20251009新增：进入群聊时删除单聊标题（避免重叠）
    if (g_chat_ctrl->chat_title && lv_obj_is_valid(g_chat_ctrl->chat_title)) {
        lv_obj_del(g_chat_ctrl->chat_title);
        g_chat_ctrl->chat_title = NULL; // 置空避免野指针
    }

    // 20250930新增：先删除已有标题（避免重叠）20251008修改
    if(g_chat_ctrl->group_chat_title && lv_obj_is_valid(g_chat_ctrl->group_chat_title)) {
        lv_obj_del(g_chat_ctrl->group_chat_title);
    }

    // 20250930修改：群聊窗口标题（全局保存，方便后续删除）
    g_chat_ctrl->group_chat_title = lv_label_create(g_chat_ctrl->scr_chat);
    lv_label_set_text(g_chat_ctrl->group_chat_title, "群聊：默认群");
    lv_obj_align(g_chat_ctrl->group_chat_title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(g_chat_ctrl->group_chat_title, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    
    // 20250930修改：群聊提示（用全局聊天文本框）
    lv_obj_t *chat_content = g_chat_ctrl->chat_content_ta;
    lv_textarea_set_text(chat_content, "已进入群聊，消息将发送给所有人\n");
    lv_obj_set_style_text_font(chat_content, &lv_myfont_kai_20, LV_STATE_DEFAULT);

    lv_scr_load(g_chat_ctrl->scr_chat);
}

// 发送消息回调，（20250929新增修改：支持群聊）20251009新增大改
 static void Send_Msg_Click(lv_event_t *e) 
 {
    // 1. 全量指针校验（避免NULL访问）
    if (!g_chat_ctrl || !g_chat_ctrl->chat_msg_ta || !lv_obj_is_valid(g_chat_ctrl->chat_msg_ta) ||
        !g_chat_ctrl->chat_content_ta || !lv_obj_is_valid(g_chat_ctrl->chat_content_ta)) {
        printf("Send_Msg_Click：无效控件，跳过发送\n");
        return;
    }

    // 2. 获取输入消息（用全局变量chat_msg_ta，替代索引）（增加有效性校验，避免LVGL控件延迟导致的空文本）
    const char *msg_text = lv_textarea_get_text(g_chat_ctrl->chat_msg_ta);
    // 20251009新增：二次校验：过滤纯空格消息，确保文本有效
    char trim_msg[256] = {0};
    sscanf(msg_text, "%[^\n]", trim_msg); // 去除换行符
    if (strlen(trim_msg) == 0) { 
        printf("Send_Msg_Click：空消息或纯空格，跳过发送\n");
        return;
    }

    // 3. 校验当前登录账号（避免空账号发送）
    if (strlen(g_chat_ctrl->cur_account) == 0) {
        printf("Send_Msg_Click：未登录，跳过发送\n");
        return;
    }

    NetMsg msg;
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.user.account, g_chat_ctrl->cur_account, 31);

    // 4. 判断单聊/群聊（增加标题有效性校验）
    bool is_group_chat = false;
    if (g_chat_ctrl->group_chat_title && lv_obj_is_valid(g_chat_ctrl->group_chat_title)) {
        if (strstr(lv_label_get_text(g_chat_ctrl->group_chat_title), "群聊") != NULL) {
            is_group_chat = true;
        }
    }

    if (is_group_chat) {
        // 群聊：格式"default:消息"
        msg.type = MSG_GROUP_CHAT;
        snprintf(msg.content, sizeof(msg.content)-1, "default:%s", msg_text);
    } else {
        // 单聊：校验好友账号非空
        if (strlen(g_chat_ctrl->chat_friend_account) == 0) {
            printf("Send_Msg_Click：未选择好友，跳过发送\n");
            return;
        }
        msg.type = MSG_SEND_MSG;
        snprintf(msg.content, sizeof(msg.content)-1, "%s:%s", 
                 g_chat_ctrl->chat_friend_account, msg_text);
    }

    // 5. 发送消息并更新本地聊天记录
    if (Send_To_Server(&msg) > 0) 
    {
        lv_textarea_set_text(g_chat_ctrl->chat_msg_ta, ""); // 清空输入框
        const char *sender = is_group_chat ? "我(群聊)" : "我";
        char new_msg[300] = {0};
        const char *current_content = lv_textarea_get_text(g_chat_ctrl->chat_content_ta);
        // 拼接新消息（确保不越界）
        snprintf(new_msg, sizeof(new_msg)-1, "%s: %s\n%s", sender, msg_text, current_content);
        lv_textarea_set_text(g_chat_ctrl->chat_content_ta, new_msg);
        lv_textarea_set_cursor_pos(g_chat_ctrl->chat_content_ta, strlen(new_msg)); // 滚动到底部

        // 20251009新增：保存聊天记录到本地文件
        if (is_group_chat) {
            Save_Chat_Log("group_default", sender, trim_msg);
        } else {
            Save_Chat_Log(g_chat_ctrl->chat_friend_account, sender, trim_msg);
        }
    }
 }

// 创建聊天窗口界面
static void Create_Chat_Scr() 
{
    g_chat_ctrl->scr_chat = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_chat, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    // 20250930修改：聊天内容区域用lv_textarea（支持滚动）替代lv_label
    lv_obj_t *chat_content = lv_textarea_create(g_chat_ctrl->scr_chat);
    lv_obj_set_size(chat_content, 300, 300);
    lv_obj_align(chat_content, LV_ALIGN_TOP_MID, 0, 50);// 20250930修改：20-50下移，避免与标题重叠

    // 20250930修改：在 LVGL v8.2 中实现只读效果的方法
    lv_textarea_set_one_line(chat_content, false); // 允许换行
    lv_obj_clear_flag(chat_content, LV_OBJ_FLAG_CLICKABLE); // 只读，禁用点击
    lv_obj_add_flag(chat_content, LV_OBJ_FLAG_EVENT_BUBBLE); // 事件冒泡

    lv_textarea_set_text(chat_content, "聊天内容..."); // 20250930修改：初始提示文本
    lv_obj_set_style_text_font(chat_content, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增，中文字体适配

    g_chat_ctrl->chat_content_ta = chat_content; // 20250930新增：全局保存，方便后续更新

    // 20250930新增：80*80头像按钮（参考相册按钮逻辑）
    g_chat_ctrl->chat_avatar_btn = lv_btn_create(g_chat_ctrl->scr_chat);
    lv_obj_set_size(g_chat_ctrl->chat_avatar_btn, 80, 80);
    lv_obj_align(g_chat_ctrl->chat_avatar_btn, LV_ALIGN_TOP_LEFT, 20, 50);
    // 默认头像（复用相册图标）
    lv_obj_t *avatar_img = lv_img_create(g_chat_ctrl->chat_avatar_btn);
    lv_img_set_src(avatar_img, "S:/8080icon_img.jpg");
    lv_obj_center(avatar_img);

    // 消息输入框（全局变量存储，替代索引）20251009修改：给g_chat_ctrl->chat_msg_ta赋值为 Send_Msg_Click 提供可靠访问
    g_chat_ctrl->chat_msg_ta = Create_Textarea(g_chat_ctrl->scr_chat, "请输入消息");
    lv_obj_align(g_chat_ctrl->chat_msg_ta, LV_ALIGN_BOTTOM_MID, 0, -60);
    // 20250929新增：绑定键盘
    Dir_Look_Bind_Textarea_Keyboard(g_chat_ctrl->chat_msg_ta, g_chat_ctrl->scr_chat);

    // 发送按钮
    lv_obj_t *send_btn = lv_btn_create(g_chat_ctrl->scr_chat);
    lv_obj_set_size(send_btn, 60, 40);
    lv_obj_align(send_btn, LV_ALIGN_BOTTOM_MID, 160, -60);
    lv_obj_t *send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "发送");
    lv_obj_set_style_text_font(send_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增，中文字体适配
    lv_obj_center(send_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 返回好友列表按钮
    lv_obj_t *back_btn = lv_btn_create(g_chat_ctrl->scr_chat);
    lv_obj_set_size(back_btn, 105, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回好友");
    lv_obj_set_style_text_font(back_btn, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增，中文字体适配
    lv_obj_center(back_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    lv_obj_add_event_cb(back_btn, Back_To_Friend, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(send_btn, Send_Msg_Click, LV_EVENT_CLICKED, NULL);
}

// -------------------------- 网络接收线程 --------------------------

// 20250927新增：LVGL UI更新封装（确保主线程操作）
static void Lvgl_Update_UI(void *param) 
{   // 20250930新增：异步执行时再次校验全局变量有效性
    NetMsg *msg = (NetMsg *)param;
    if(!g_chat_ctrl) {
        printf("Lvgl_Update_UI：g_chat_ctrl已释放，跳过处理\n");
        free(param);
        return;
    }

    // 20250930修改：处理服务器消息（包含登录ACK的界面切换）
    Handle_Server_Msg(msg);
    free(param); // 20250929新增关键修改：释放堆分配的 NetMsg 内存，避免泄漏
}

// 处理服务器应答消息
static void Handle_Server_Msg(NetMsg *msg) 
{
    // 20251009新增：过滤无效消息类型（MSG_REGISTER=1~MSG_LOGOUT=11）
    if(msg->type < MSG_REGISTER || msg->type > MSG_LOGOUT) {
        printf("客户端：过滤无效消息类型（%d），忽略处理\n", msg->type);
        return;
    }

    switch(msg->type) 
    {
        case MSG_ACK: 
        {
            // 注册/登录应答（content为"register"/"login"，user.port为1成功/0失败）
            if(strcmp(msg->content, "register") == 0) 
            {
                if(msg->user.port == 1) 
                { // ACK=1成功
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册成功！请登录");
                    usleep(1000000); // 延迟1秒切换
                    lv_scr_load(g_chat_ctrl->scr_login);
                } else 
                {   // ACK=0失败
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册失败：账号已存在");
                }
            } 

            // 20251009新增：兼容两种情况：1.content为"login"（正常） 2.content为空（结构体未完全同步前）
            else if(strcmp(msg->content, "login") == 0 || strlen(msg->content) == 0)
            {
                // 20250930新增：打印完整ACK数据，确认进入登录处理分支。是否收到正确的result和账号
                printf("客户端收到登录ACK：type=%d, content=%s（允许为空）, result=%d, account=%s\n\n",
                    msg->type, msg->content, msg->user.port, msg->user.account);

                if(msg->user.port == 1) 
                { // ACK=1成功。无论content是否为空，均判定登录成功（容错处理）
                    strncpy(g_chat_ctrl->cur_account, msg->user.account, 31);
                    printf("客户端：登录成功，账号：%s\n\n", g_chat_ctrl->cur_account);

                    // 20251009新增大改：1. 强制重建好友界面（彻底解决初始化残留问题）
                    if(g_chat_ctrl->scr_friend && lv_obj_is_valid(g_chat_ctrl->scr_friend)) {
                        lv_obj_del(g_chat_ctrl->scr_friend); // 删除旧界面
                    }
                    Create_Friend_Scr(); // 重新创建好友界面
                    if(!g_chat_ctrl->scr_friend || !lv_obj_is_valid(g_chat_ctrl->scr_friend)) {
                        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "登录成功，好友界面重建失败");
                        printf("客户端：好友界面重建失败，无法切换\n");
                        break;
                    }
                
                    // 2. 强制切换界面并刷新（LVGL8.2必须双重刷新）
                    lv_obj_clear_flag(g_chat_ctrl->scr_friend, LV_OBJ_FLAG_HIDDEN);
                    lv_scr_load(g_chat_ctrl->scr_friend); // 切换界面
                    lv_disp_t *disp = lv_disp_get_default();
                    if(disp) {
                        lv_refr_now(disp); // 强制刷新显示
                        printf("客户端：好友列表界面已切换并强制刷新\n");
                    } else {
                        // 备选方案：无默认显示设备时，手动触发LVGL任务处理
                        printf("客户端：无默认显示设备，触发LVGL任务强制刷新\n");
                        lv_timer_handler(); // 强制处理LVGL事件
                        usleep(100000); // 延迟100ms确保刷新生效
                        lv_timer_handler();
                    }
                
                    // 3. 显示登录成功提示（确保用户看到反馈）
                    lv_obj_t *friend_status_label = lv_obj_get_child(g_chat_ctrl->scr_friend, 0);
                    if(friend_status_label) {
                        lv_label_set_text(friend_status_label, "登录成功！当前用户已显示");
                    }
                
                    // 4. 手动添加当前用户（兜底，确保列表有内容）
                    if(g_chat_ctrl->friend_list && lv_obj_is_valid(g_chat_ctrl->friend_list)) 
                    {
                        lv_obj_clean(g_chat_ctrl->friend_list); // 清空旧列表
                        typedef struct { char account[32]; char avatar[64]; } FriendInfo;
                        FriendInfo *self_info = malloc(sizeof(FriendInfo));
                        if (self_info) {
                            memset(self_info, 0, sizeof(FriendInfo));
                            strncpy(self_info->account, g_chat_ctrl->cur_account, 31);
                            strncpy(self_info->avatar, "S:/8080icon_img.jpg", 63);
                            // 创建列表项
                            char item_text[120];
                            snprintf(item_text, 120, "%s(当前用户)[在线]", g_chat_ctrl->cur_account);
                            lv_obj_t *self_item = lv_list_add_btn(g_chat_ctrl->friend_list, NULL, item_text);
                            if (self_item) {
                                lv_obj_set_user_data(self_item, self_info);
                                lv_obj_add_event_cb(self_item, Friend_Click, LV_EVENT_CLICKED, NULL);
                                printf("客户端：已添加当前用户到好友列表\n\n");
                            } else {
                                free(self_info);
                                printf("客户端：创建当前用户列表项失败\n");
                            }
                        } else {
                            printf("客户端：分配当前用户信息内存失败\n");
                        }
                    }

                    // 5 发送请求在线用户列表（不阻塞界面）
                    NetMsg get_user_msg = {.type = MSG_GET_ONLINE_USER};
                    if(Send_To_Server(&get_user_msg) > 0) {
                        printf("客户端：已发送请求在线用户列表\n\n");
                    } else {
                        printf("客户端：发送列表请求失败（sockfd=%d）\n", g_chat_ctrl->sockfd);
                        if(friend_status_label) {
                            lv_label_set_text(friend_status_label, "登录成功，获取好友列表失败");
                        }
                    }

                } else 
                { // ACK=0 登录失败
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "登录失败：账号/密码错误");
                    printf("客户端：登录失败，ACK=%d\n", msg->user.port);
                }
            }
            else if(strcmp(msg->content, "add_friend") == 0) //20250927新增
            {
                if(msg->user.port == 1) {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "添加好友成功");
                    Refresh_Friend_List(NULL); // 20250930新增：自动刷新好友列表
                } else {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "添加失败：用户不存在");
                }
            }
            //-------------------20250929新增：处理签名修改 ACK-------------
            else if(strcmp(msg->content, "set_signature") == 0)
            {
                if(msg->user.port == 1) {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "签名修改成功，刷新列表生效");
                    Refresh_Friend_List(NULL); // 修改：自动刷新列表,传NULL，函数内部无参数依赖
                } else {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "签名修改失败");
                }
                break;
            }

            // 20251008新增：处理头像设置ACK
            else if(strcmp(msg->content, "set_avatar") == 0)
            {
                if(msg->user.port == 1) 
                {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "头像设置成功");
                    // 更新聊天窗口头像
                    if(g_chat_ctrl->chat_avatar_btn && lv_obj_is_valid(g_chat_ctrl->chat_avatar_btn)) 
                    {
                        lv_obj_t *avatar_img = lv_obj_get_child(g_chat_ctrl->chat_avatar_btn, 0);
                        if(avatar_img) 
                        {
                            // 无效路径用默认头像
                            if(strlen(msg->user.avatar) == 0 || access(msg->user.avatar, R_OK) != 0) 
                            {
                                lv_img_set_src(avatar_img, "S:/8080icon_img.jpg");
                            } else 
                            {
                                lv_img_set_src(avatar_img, msg->user.avatar);
                            }
                        }
                    }
                    Refresh_Friend_List(NULL); // 刷新好友列表头像
                } else {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "头像设置失败");
                }
                break;
            }
            break;
        }
        case MSG_USER_LIST: 
        {   
            // 20251008新增：无在线用户时，手动添加当前登录用户
            if(strlen(msg->content) == 0) {
                printf("客户端：无在线用户，添加当前用户到列表\n\n");
                char self_info[256];
                // 20251009修改：格式：账号:昵称:签名:头像:状态（匹配服务器返回格式）
                snprintf(self_info, 256, "%s:%s:%s:%s:在线", 
                         g_chat_ctrl->cur_account,  // 账号
                         g_chat_ctrl->cur_account,  // 昵称（默认用账号）
                         "当前用户",               // 签名
                         "S:/8080icon_img.jpg");   // 头像（默认路径）
                strncpy(msg->content, self_info, sizeof(msg->content)-1);
            }
        
            // 20250929新增：释放旧列表项的账号内存（防止泄漏）--------------
            lv_obj_t *child;

            // 1. 获取好友列表的子对象数量
            uint32_t child_count = lv_obj_get_child_cnt(g_chat_ctrl->friend_list);
            // 2. 循环遍历所有子对象
            for (uint32_t i = 0; i < child_count; i++) {
                child = lv_obj_get_child(g_chat_ctrl->friend_list, i); // 获取第i个子对象
                char *acc = (char *)lv_obj_get_user_data(child);
                if (acc != NULL) 
                {   free(acc);// 释放之前存储的好友信息（防止泄漏）
                    lv_obj_set_user_data(child, NULL); 
                }
            }//-------------------------------------------------------

            // 更新好友列表（格式：账号:昵称:签名|账号:昵称:签名|....）
            lv_obj_clean(g_chat_ctrl->friend_list); // 清空原有列表

            char *token = strtok(msg->content, "|");

            while(token) 
            {
                char account[32], nickname[32], signature[64], avatar[64], status[10];
                sscanf(token, "%[^:]:%[^:]:%s", account, nickname, signature);

                // 20250930新增：解析状态字段 20251008修改解析格式：账号:昵称:签名:头像:状态（服务器端已修改）
                sscanf(token, "%[^:]:%[^:]:%[^:]:%[^:]:%s", account, nickname, signature, avatar, status);

                // 添加列表项（显示昵称+签名+状态）20250927新增显示在线状态（MSG_USER_LIST仅返回在线用户）
                char item_text[120];    //20250930修改100-120

                // 20251009修改：确保文本不溢出，显示“昵称(签名)[状态]”处理签名为空的情况，避免显示异常，统一格式。
                snprintf(item_text, sizeof(item_text)-1, "%s(%s)[%s]", nickname, strlen(signature) ? signature : "无签名", status);

                // 添加列表项并存储好友信息（账号+头像）
                lv_obj_t *item = lv_list_add_btn(g_chat_ctrl->friend_list, NULL, item_text);

                typedef struct { char account[32]; char avatar[64]; } FriendInfo;
                FriendInfo *info = malloc(sizeof(FriendInfo));
                strncpy(info->account, account, sizeof(info->account)-1);
                strncpy(info->avatar, avatar, sizeof(info->avatar)-1);
                lv_obj_set_user_data(item, info);

                lv_obj_add_event_cb(item, Friend_Click, LV_EVENT_CLICKED, NULL);// 绑定点击事件（进入单聊）
                token = strtok(NULL, "|");
            }
                // 20250929新增：刷新提示
                lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "好友列表已更新");

                //20250930新增：确认 MSG_USER_LIST 的接收，收到服务器返回的用户列表
                printf("客户端收到在线用户列表：%s\n\n", msg->content);
            break;
        }
        case MSG_SEND_MSG: {
            // 接收聊天消息（格式：发送者昵称: 消息内容）
            lv_obj_t *chat_content = g_chat_ctrl->chat_content_ta; // 20250930修改：复用全局文本框
            char new_msg[300];

            // 20250930修改：拼接昵称+消息
            snprintf(new_msg, 300, "%s: %s\n%s", msg->user.nickname, msg->content, lv_textarea_get_text(chat_content));
            lv_textarea_set_text(chat_content, new_msg); // 20250930修改

            // 20250930新增：自动滚动到底部
            lv_textarea_set_cursor_pos(chat_content, strlen(lv_textarea_get_text(chat_content)));

            // 20251009新增：保存接收的消息到本地文件
            Save_Chat_Log(g_chat_ctrl->chat_friend_account, msg->user.nickname, msg->content);

            break;
        }
        case MSG_GROUP_CHAT: 
        {
            // 20251009新增大改：1. 拦截群聊标题空指针（避免非法访问）
            if (!g_chat_ctrl->group_chat_title || !lv_obj_is_valid(g_chat_ctrl->group_chat_title)) {
                if (g_chat_ctrl->scr_friend && lv_obj_is_valid(g_chat_ctrl->scr_friend)) {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), 
                                     "收到群聊消息，请进入群聊查看");
                }
                break;
            }
            // 2. 确认当前在群聊窗口
            if (strstr(lv_label_get_text(g_chat_ctrl->group_chat_title), "群聊") == NULL) {
                if (g_chat_ctrl->scr_friend && lv_obj_is_valid(g_chat_ctrl->scr_friend)) {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), 
                                     "收到群聊消息，请进入群聊查看");
                }
                break;
            }
            // 3. 使用全局文本框（lv_textarea），匹配控件类型（核心修复）
            lv_obj_t *chat_content = g_chat_ctrl->chat_content_ta;
            if (!chat_content || !lv_obj_is_valid(chat_content)) {
                printf("MSG_GROUP_CHAT：聊天文本框无效\n");
                break;
            }
            // 4. 拼接消息并更新（使用textarea接口）
            char new_msg[300] = {0};
            const char *current_content = lv_textarea_get_text(chat_content);
            snprintf(new_msg, sizeof(new_msg)-1, "%s(群聊): %s\n%s", 
                     msg->user.nickname, msg->content, current_content);
            lv_textarea_set_text(chat_content, new_msg);
            lv_textarea_set_cursor_pos(chat_content, strlen(new_msg)); // 自动滚动到底部

            // 20251009新增：保存群聊消息到本地文件
            Save_Chat_Log("group_default", msg->user.nickname, msg->content);
            break;
        }

        default:
            printf("未知消息类型: %d\n", msg->type);
            break;        
    }
}

// 接收服务器消息线程（避免阻塞UI）
static void *Recv_Server_Msg(void *arg) 
{   //循环条件：g_chat_ctrl有效且未退出时继续//20250930修改：1替换g_chat_ctrl && !g_chat_ctrl->exiting 20251008
    while(g_chat_ctrl && !g_chat_ctrl->exiting)
    {   
        // 20250930新增关键：先检查g_chat_ctrl和exiting，避免访问NULL
        if(!g_chat_ctrl || g_chat_ctrl->exiting) {
            printf("Recv_Server_Msg：线程退出（g_chat_ctrl=%p, exiting=%d）\n", g_chat_ctrl, g_chat_ctrl ? g_chat_ctrl->exiting : 1);
            break;
        }

       // 20250929新增修改：堆分配消息内存（关键修改：避免栈变量销毁问题）-----
        NetMsg *msg = (NetMsg *)malloc(sizeof(NetMsg));
        if (!msg) { // 内存分配失败处理
            perror("malloc NetMsg failed");
            sleep(1);
            continue;
        }

        memset(msg, 0, sizeof(*msg)); 
        //20250928修改 接收服务器消息
        int ret = recv(g_chat_ctrl->sockfd, msg, sizeof(NetMsg), 0);
        
        // 处理接收结果
        if(ret > 0) 
        {
            // 20251009新增：打印接收的消息详情（确认解析是否正确）
            printf("客户端收到消息：type=%d（MSG_ACK=3）, content=%s, user.port=%d\n\n",  msg->type, msg->content, msg->user.port);
            
            // 接收成功，加锁处理UI（LVGL需主线程操作）
            pthread_mutex_lock(&msg_mutex);
            if(g_chat_ctrl && !g_chat_ctrl->exiting) {
                lv_async_call(Lvgl_Update_UI, msg); // 异步更新UI
            } else {
                free(msg);
            }
            pthread_mutex_unlock(&msg_mutex);
        } else if(ret == 0) {
            // ret=0：服务器主动关闭连接
            printf("Recv_Server_Msg：服务器断开连接（ret=0）\n\n");
            free(msg);
            break;
        } else { // ret == -1
            // 仅致命错误退出，EINTR（信号中断）或临时错误可重试
            if(errno == EINTR || errno == ETIMEDOUT) {
                printf("Recv_Server_Msg：recv被信号中断/超时（errno=%d），重试\n", errno);
                free(msg);
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK) 
            {
                // 20251008新增修改：超时，继续循环
                printf("Recv_Server_Msg：接收超时，继续等待...\n");
                free(msg);
                continue;
            }
            else {
                printf("Recv_Server_Msg：接收失败（ret=-1, errno=%d），关闭连接\n", errno);
                free(msg);
                break;
            }
        }
    }
    printf("接收线程安全退出\n\n");

    return NULL;
}

// -------------------------- 模块初始化与退出 --------------------------
void Chat_Room_Init(struct Ui_Ctrl *uc, lv_obj_t *scr_home, bool connect_now)
{

    // 20251009新增：若已存在控制结构体，直接切换到好友列表（不重新初始化）
    if (g_chat_ctrl && lv_obj_is_valid(g_chat_ctrl->scr_friend)) {
        lv_scr_load(g_chat_ctrl->scr_friend);
        printf("聊天室已初始化，直接进入好友列表\n");
        return;
    }

    // 初始化全局控制结构体
    g_chat_ctrl = (CHAT_CTRL_P)malloc(sizeof(CHAT_CTRL));
    memset(g_chat_ctrl, 0, sizeof(CHAT_CTRL));
    g_chat_ctrl->uc = uc;
    g_chat_ctrl->scr_home = scr_home;
    g_chat_ctrl->sockfd = -1; // 20250927新增：初始未连接

    // 初始化互斥锁
    pthread_mutex_init(&msg_mutex, NULL);

    // 创建所有界面
    Create_Friend_Scr();
    Create_Login_Scr();
    Create_Register_Scr();
    Create_Chat_Scr();
    Create_Setting_Scr(); // 新增设置界面

    // 20251009新增：打印初始化状态日志
    printf("\nChat_Room_Init：界面初始化完成\n");
    // printf("  - scr_friend（好友界面）：%p\n", g_chat_ctrl->scr_friend);
    // printf("  - friend_list（好友列表控件）：%p\n", g_chat_ctrl->friend_list);
    // printf("  - scr_chat（聊天界面）：%p\n", g_chat_ctrl->scr_chat);

    // 20251008新增：强制设置好友列表界面为有效
    if(g_chat_ctrl->scr_friend) {
        lv_obj_clear_flag(g_chat_ctrl->scr_friend, LV_OBJ_FLAG_HIDDEN);
    }

    // 20250930新增日志：确认scr_friend创建后的值
    printf("Chat_Room_Init：scr_friend=%p\n\n", g_chat_ctrl->scr_friend);

    // 根据参数决定是否立即连接服务器
    if(connect_now) 
    {   // 连接云服务器
        g_chat_ctrl->sockfd = Connect_Server();

        if(g_chat_ctrl->sockfd < 0) 
        {
            lv_obj_t *err_label = lv_label_create(scr_home);
            lv_label_set_text(err_label, "连接服务器失败！");
            lv_obj_align(err_label, LV_ALIGN_CENTER, 0, 0);
            return;
        }

        // 启动接收线程
        pthread_create(&recv_thread_id, NULL, Recv_Server_Msg, NULL);
        is_thread_created = true; // 20250928新增
    } 

    // 20251009新增：1. 判断是否有保存的登录账号，有则直接进入好友列表
    //确保重新进入时主动连接服务器、请求好友列表，无连接时显示本地账号，避免重新登录。
    if (strlen(g_saved_cur_account) > 0) {
        // 恢复登录账号
        strncpy(g_chat_ctrl->cur_account, g_saved_cur_account, sizeof(g_chat_ctrl->cur_account)-1);
        printf("恢复登录：账号=%s\n\n", g_chat_ctrl->cur_account);

        // 20251009新增：2.重新连接服务器（退出时已关闭sockfd）
        if (g_chat_ctrl->sockfd < 0) {
            g_chat_ctrl->sockfd = Connect_Server();

            if (g_chat_ctrl->sockfd < 0) 
            {
                printf("恢复登录：重新连接服务器失败\n\n");
                lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "恢复登录：连接服务器失败");
                // 连接失败仍进入好友列表（显示本地账号）
                goto load_friend_scr;
            } else {
                // 重启接收线程（确保能接收用户列表）
                if (!is_thread_created) {
                    pthread_create(&recv_thread_id, NULL, Recv_Server_Msg, NULL);
                    is_thread_created = true;
                    printf("恢复登录：重启接收线程\n\n");
                }
            }
        }

        load_friend_scr:
        // 3. 重建好友列表（避免旧界面残留）
        if (g_chat_ctrl->scr_friend && lv_obj_is_valid(g_chat_ctrl->scr_friend)) {
            lv_obj_del(g_chat_ctrl->scr_friend);
        }
        Create_Friend_Scr();

        // 20251009新增：4. 加载好友列表并请求在线用户
        if (g_chat_ctrl->scr_friend && lv_obj_is_valid(g_chat_ctrl->scr_friend)) {
            lv_scr_load(g_chat_ctrl->scr_friend);

            //主动请求好友列表（关键：确保从服务器同步数据）
        if (g_chat_ctrl->sockfd >= 0) {
            NetMsg get_user_msg = {.type = MSG_GET_ONLINE_USER};
            if (Send_To_Server(&get_user_msg) > 0) {
                printf("恢复登录：已请求在线用户列表\n\n");
                lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "恢复登录：同步好友列表中...");
            } else {
                lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "获取好友列表失败");
            }
        } else {
            lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "未连接服务器（仅显示本地账号）");

            // 20251009新增：手动添加当前用户到列表
            if (g_chat_ctrl->friend_list && lv_obj_is_valid(g_chat_ctrl->friend_list)) 
            {
                lv_obj_clean(g_chat_ctrl->friend_list);
                typedef struct { char account[32]; char avatar[64]; } FriendInfo;
                FriendInfo *self_info = malloc(sizeof(FriendInfo));
                if (self_info) 
                {
                    memset(self_info, 0, sizeof(FriendInfo));
                    strncpy(self_info->account, g_chat_ctrl->cur_account, 31);
                    strncpy(self_info->avatar, "S:/8080icon_img.jpg", 63);
                    char item_text[120];
                    snprintf(item_text, sizeof(item_text)-1, "%s(当前用户)[离线]", g_chat_ctrl->cur_account);
                    lv_obj_t *self_item = lv_list_add_btn(g_chat_ctrl->friend_list, NULL, item_text);
                    if (self_item) 
                    {
                        lv_obj_set_user_data(self_item, self_info);
                        lv_obj_add_event_cb(self_item, Friend_Click, LV_EVENT_CLICKED, NULL);
                    } else {
                        free(self_info);
                    }
                }
            }
        }
            printf("恢复登录状态，进入好友列表\n");
            return;
        }
    }

    // 无保存账号，默认加载登录界面
    lv_scr_load(g_chat_ctrl->scr_login);
    // 其他界面：scr_login、scr_register、scr_friend、scr_chat、scr_setting
}

void Chat_Room_Exit() 
{
    if(!g_chat_ctrl) return;

    printf("开始退出聊天室...\n\n");  //20250928新增

    // 1. 设置退出标志
    g_chat_ctrl->exiting = true;
    // 20250928新增修改2. 发送离线通知（如果有登录）
    if(strlen(g_chat_ctrl->cur_account) > 0) {
        NetMsg offline_msg;
        memset(&offline_msg, 0, sizeof(offline_msg));
        offline_msg.type = MSG_LOGOUT;
        strncpy(offline_msg.user.account, g_chat_ctrl->cur_account, 31);
        
        // 在设置exiting标志后，但关闭socket前发送
        if(g_chat_ctrl->sockfd >= 0) {
            send(g_chat_ctrl->sockfd, &offline_msg, sizeof(NetMsg), 0);
        }
    }

    // 关闭socket（先关闭，强制线程退出 recv 阻塞，触发接收线程退出）
    if(g_chat_ctrl->sockfd >= 0) 
    {
        shutdown(g_chat_ctrl->sockfd, SHUT_RDWR);   //20250928新增修改
        close(g_chat_ctrl->sockfd);
        g_chat_ctrl->sockfd = -1;
    }
    // 4. 仅当线程已创建时才等待退出
    if(is_thread_created) 
    { // 20250928新增判断,解决段错误
        pthread_join(recv_thread_id, NULL);// 等待线程完全退出
        is_thread_created = false; // 重置标志
        printf("接收线程已退出\n\n");
    }
    // 销毁互斥锁
    pthread_mutex_destroy(&msg_mutex);

    // 释放界面资源（20250928新增修改NULL检查）
    if(g_chat_ctrl->scr_login && lv_obj_is_valid(g_chat_ctrl->scr_login))
        lv_obj_del(g_chat_ctrl->scr_login);
    if(g_chat_ctrl->scr_register && lv_obj_is_valid(g_chat_ctrl->scr_register))
        lv_obj_del(g_chat_ctrl->scr_register);
    if(g_chat_ctrl->scr_friend && lv_obj_is_valid(g_chat_ctrl->scr_friend)) 
        lv_obj_del(g_chat_ctrl->scr_friend);
    if(g_chat_ctrl->scr_chat && lv_obj_is_valid(g_chat_ctrl->scr_chat))
        lv_obj_del(g_chat_ctrl->scr_chat);
    if(g_chat_ctrl->scr_setting && lv_obj_is_valid(g_chat_ctrl->scr_setting))
        lv_obj_del(g_chat_ctrl->scr_setting);

    // 20251009新增：保存当前登录账号到静态变量（退出前保存）
    if (g_chat_ctrl) {
        if (strlen(g_chat_ctrl->cur_account) > 0) {
            strncpy(g_saved_cur_account, g_chat_ctrl->cur_account, sizeof(g_saved_cur_account)-1);
            printf("Chat_Room_Exit：保存登录账号=%s\n\n", g_saved_cur_account);
        } else {
            memset(g_saved_cur_account, 0, sizeof(g_saved_cur_account));
            printf("Chat_Room_Exit：无登录账号，清空保存\n\n");
        }
    }

    // 释放内存，全局控制结构体
    free(g_chat_ctrl);
    g_chat_ctrl = NULL;

    printf("聊天室资源完全释放\n\n");
}