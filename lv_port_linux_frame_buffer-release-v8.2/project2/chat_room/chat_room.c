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
static void Friend_Click_Enter_Chat(lv_event_t *e);
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

// 补充Show_Chat_Log和Create_Chat_Scr的前置声明
static void Show_Chat_Log(const char *nickname, const char *avatar_path, const char *msg, int is_self);
static void Create_Chat_Scr(void);

static void Select_Avatar(lv_event_t *e);

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
    struct timeval timeout = {3, 0}; // 3秒超时
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect server failed");
        close(sockfd);
        return -1;
    }
    printf("connect server success\n");
    return sockfd;
}

// 发送消息到服务器（封装发送逻辑）
static int Send_To_Server(NetMsg *msg) {
    if(!g_chat_ctrl || g_chat_ctrl->sockfd < 0) return -1;
    return send(g_chat_ctrl->sockfd, msg, sizeof(NetMsg), 0);
}

// -------------------------- 界面切换与创建 --------------------------
// 返回首页
static void Back_To_Home(lv_event_t *e) {
    lv_obj_t *scr_home = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(scr_home);
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
    strncpy(msg.user.account, lv_textarea_get_text(account_ta), 31);
    strncpy(msg.user.password, lv_textarea_get_text(pwd_ta), 31);

    strncpy(msg.user.ip, Get_Local_IP(), 15); // 20250927新增：动态获取IP
    msg.user.port = 8000; // 固定本地端口（新手无需修改）
    
    // 发送登录请求
    if(Send_To_Server(&msg) < 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "登录失败：连接异常");
        return;
    }
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

static void Send_Chat_Msg(lv_event_t *e) {   //20250929新增
    const char *msg_text = lv_textarea_get_text(g_chat_ctrl->chat_input_ta);
    if (strlen(msg_text) == 0) return;
    
    // 1. 构造单聊消息（同步服务器）
    NetMsg chat_msg;
    memset(&chat_msg, 0, sizeof(chat_msg));

    // 新增：判断群聊/单聊（解决段错误：避免chat_title空指针）
    if (g_chat_ctrl->is_group_chat) {
        chat_msg.type = MSG_GROUP_CHAT;
        strncpy(chat_msg.content, msg_text, 191); // 群聊消息直接传内容
    } else {
        const char *target_friend = g_chat_ctrl->cur_chat_friend;       //20250929新增修改
        chat_msg.type = MSG_SINGLE_CHAT;
        snprintf(chat_msg.content, sizeof(chat_msg.content), "%s:%s", target_friend, msg_text);
    }
    
    // 携带发送者信息（昵称+头像）
    strncpy(chat_msg.user.nickname, g_chat_ctrl->cur_nickname, 31);
    strncpy(chat_msg.user.avatar, g_chat_ctrl->cur_avatar, 63);
    
    // 2. 发送给服务器
    if (Send_To_Server(&chat_msg) > 0) {
        // 3. 本地显示自己的消息（带头像80*80）
        Show_Chat_Log(g_chat_ctrl->cur_nickname, g_chat_ctrl->cur_avatar, msg_text, 1);
        // 清空输入框
        lv_textarea_set_text(g_chat_ctrl->chat_input_ta, "");
    } else {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_chat, 0), "发送失败：连接断开");
    }
}

// 辅助函数：显示聊天记录（带头像）
static void Show_Chat_Log(const char *nickname, const char *avatar_path, const char *msg, int is_self) {
    // 1. 创建消息行容器
    lv_obj_t *msg_row = lv_obj_create(g_chat_ctrl->chat_log_cont);
    lv_obj_set_size(msg_row, 630, 100);
    lv_obj_set_style_bg_opa(msg_row, 0, LV_STATE_DEFAULT); // 透明背景
    
    // 2. 80*80头像按钮
    lv_obj_t *msg_avatar = lv_btn_create(msg_row);
    lv_obj_set_size(msg_avatar, 80, 80);
    lv_obj_t *avatar_img = lv_img_create(msg_avatar);
    lv_img_set_src(avatar_img, avatar_path);
    lv_obj_align(avatar_img, LV_ALIGN_CENTER, 0, 0);
    
    // 3. 消息内容标签（带昵称）
    lv_obj_t *msg_label = lv_label_create(msg_row);
    char msg_full[200];
    snprintf(msg_full, 200, "%s：%s", nickname, msg);
    lv_label_set_text(msg_label, msg_full);
    lv_obj_set_style_text_font(msg_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    // 对齐头像
    if (is_self) {
        lv_obj_align(msg_label, LV_ALIGN_BOTTOM_LEFT, -100, 0);
    } else {
        lv_obj_align(msg_label, LV_ALIGN_BOTTOM_LEFT, 100, 0);
    }
    
    // 4. 滚动到底部（“依序显示，可滚动查看”）20250930新增：// 获取最后一个子对象并滚动到它
    int child_count = lv_obj_get_child_cnt(g_chat_ctrl->chat_log_cont);
    if (child_count > 0) {
        lv_obj_t *last_child = lv_obj_get_child(g_chat_ctrl->chat_log_cont, child_count - 1);
        lv_obj_scroll_to_view(last_child, LV_ANIM_ON);
    }
}

// 好友列表项点击（进入聊天窗口）
static void Friend_Click_Enter_Chat(lv_event_t *e) 
{
    lv_obj_t *item = lv_event_get_current_target(e);
    lv_obj_t *avatar_btn = lv_obj_get_child(item, 0);

    char *friend_account = (char *)lv_obj_get_user_data(avatar_btn); // 20250929新增：获取好友账号

    // 1. 创建单聊窗口（复用原有scr_chat，避免新窗口冗余）
    if (!g_chat_ctrl->scr_chat) {
        g_chat_ctrl->scr_chat = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(g_chat_ctrl->scr_chat, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
        
        // 1.1 聊天记录容器（支持滚动，方案要求）
        g_chat_ctrl->chat_log_cont = lv_obj_create(g_chat_ctrl->scr_chat);
        lv_obj_set_size(g_chat_ctrl->chat_log_cont, 650, 400);
        lv_obj_align(g_chat_ctrl->chat_log_cont, LV_ALIGN_TOP_MID, 0, 60);
        lv_obj_set_scroll_dir(g_chat_ctrl->chat_log_cont, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(g_chat_ctrl->chat_log_cont, LV_SCROLLBAR_MODE_AUTO);
        
        // 1.2 消息输入框
        g_chat_ctrl->chat_input_ta = lv_textarea_create(g_chat_ctrl->scr_chat);
        lv_obj_set_size(g_chat_ctrl->chat_input_ta, 500, 60);
        lv_obj_align(g_chat_ctrl->chat_input_ta, LV_ALIGN_BOTTOM_MID, -80, -20);
        lv_textarea_set_placeholder_text(g_chat_ctrl->chat_input_ta, "输入消息...");
        
        // 1.3 发送按钮
        lv_obj_t *send_btn = lv_btn_create(g_chat_ctrl->scr_chat);
        lv_obj_set_size(send_btn, 100, 60);
        lv_obj_align(send_btn, LV_ALIGN_BOTTOM_MID, 270, -20);
        lv_obj_t *send_label = lv_label_create(send_btn);
        lv_label_set_text(send_label, "发送");
        lv_obj_set_style_text_font(send_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
        lv_obj_center(send_label);
        lv_obj_add_event_cb(send_btn, Send_Chat_Msg, LV_EVENT_CLICKED, friend_account);
        
        // 1.4 返回好友列表按钮
        lv_obj_t *back_btn = lv_btn_create(g_chat_ctrl->scr_chat);
        lv_obj_set_size(back_btn, 120, 40);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 20, 20);
        lv_obj_t *back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, "返回好友");
        lv_obj_set_style_text_font(back_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
        lv_obj_center(back_label);
        lv_obj_add_event_cb(back_btn, Back_To_Friend, LV_EVENT_CLICKED, NULL);
    }
    
    // 2. 设置单聊窗口标题（好友昵称）
    lv_obj_t *chat_title = lv_label_create(g_chat_ctrl->scr_chat);
    char title_text[64];
    snprintf(title_text, 64, "单聊：%s", lv_label_get_text(lv_obj_get_child(item, 1)));
    lv_label_set_text(chat_title, title_text);
    lv_obj_align(chat_title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(chat_title, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    
    // 3. 存储当前聊天好友账号
    strncpy(g_chat_ctrl->cur_chat_friend, friend_account, 31);
    
    // 4. 跳转单聊窗口
    lv_scr_load(g_chat_ctrl->scr_chat);
}

// ----20250927新增--------------------
// 添加好友回调
static void Add_Friend_Click(lv_event_t *e) {
    lv_obj_t *friend_acc_ta = lv_obj_get_child(g_chat_ctrl->scr_setting, 5);
    const char *friend_acc = lv_textarea_get_text(friend_acc_ta);
    if (strlen(friend_acc) == 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "好友账号不能为空");
        return;
    }
    
    NetMsg add_msg;
    memset(&add_msg, 0, sizeof(add_msg));
    add_msg.type = MSG_ADD_FRIEND;
    strncpy(add_msg.content, friend_acc, 31); // 好友账号
    strncpy(add_msg.user.account, g_chat_ctrl->cur_account, 31); // 自己账号
    
    if (Send_To_Server(&add_msg) > 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "添加中...");
        lv_textarea_set_text(friend_acc_ta, "");
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
    strncpy(msg.user.signature, lv_textarea_get_text(sign_ta), 63);

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


    // 1. 头像设置区域-----20250929新增修改----------------
    g_chat_ctrl->avatar_preview_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_set_size(g_chat_ctrl->avatar_preview_btn, 80, 80);
    lv_obj_t *preview_img = lv_img_create(g_chat_ctrl->avatar_preview_btn);
    lv_img_set_src(preview_img, g_chat_ctrl->cur_avatar); // 使用当前头像
    lv_obj_align(preview_img, LV_ALIGN_CENTER, 0, 0);
    
    // 2. 80*80头像预览按钮（方案要求）
    g_chat_ctrl->avatar_preview_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_set_size(g_chat_ctrl->avatar_preview_btn, 80, 80);
    lv_obj_align(g_chat_ctrl->avatar_preview_btn, LV_ALIGN_TOP_MID, 0, 60);
    // 加载默认头像（服务器返回路径）
    lv_img_set_src(g_chat_ctrl->avatar_preview_btn, g_chat_ctrl->cur_avatar);
    
    // 3. 头像路径选择按钮（简化：默认提供3个80*80头像选项）
    lv_obj_t *avatar1_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_set_size(avatar1_btn, 60, 60);
    lv_obj_align(avatar1_btn, LV_ALIGN_TOP_MID, -100, 160);
    lv_obj_t *avatar1_img = lv_img_create(avatar1_btn);
    lv_img_set_src(avatar1_img, "/avatar/avatar1_80x80.png");
    lv_obj_center(avatar1_img);
    lv_obj_add_event_cb(avatar1_btn, Select_Avatar, LV_EVENT_CLICKED, "/avatar/avatar1_80x80.png");

    lv_obj_t *avatar2_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_set_size(avatar2_btn, 60, 60);
    lv_obj_align(avatar2_btn, LV_ALIGN_TOP_MID, 0, 160);// 居中	
    lv_obj_t *avatar2_img = lv_img_create(avatar2_btn);
    lv_img_set_src(avatar2_img, "/avatar/avatar2_80x80.png");
    lv_obj_center(avatar2_img);
    lv_obj_add_event_cb(avatar2_btn, Select_Avatar, LV_EVENT_CLICKED, "/avatar/avatar2_80x80.png");

    lv_obj_t *avatar3_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_set_size(avatar3_btn, 60, 60);
    lv_obj_align(avatar3_btn, LV_ALIGN_TOP_MID, 100, 160);// 右侧
    lv_obj_t *avatar3_img = lv_img_create(avatar3_btn);
    lv_img_set_src(avatar3_img, "/avatar/avatar3_80x80.png");
    lv_obj_center(avatar3_img);
    lv_obj_add_event_cb(avatar3_btn, Select_Avatar, LV_EVENT_CLICKED, "/avatar/avatar3_80x80.png");
    
    // 4. 保存头像按钮
    lv_obj_t *save_avatar_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_set_size(save_avatar_btn, 120, 40);
    lv_obj_align(save_avatar_btn, LV_ALIGN_TOP_MID, 0, 250);
    lv_obj_t *save_avatar_label = lv_label_create(save_avatar_btn);
    lv_label_set_text(save_avatar_label, "保存头像");
    lv_obj_set_style_text_font(save_avatar_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_center(save_avatar_label);
    lv_obj_add_event_cb(save_avatar_btn, Set_Avatar_Click, LV_EVENT_CLICKED, NULL);

    // 绑定事件
    lv_obj_add_event_cb(back_btn, Back_To_Friend, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(add_btn, Add_Friend_Click, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(sign_btn, Set_Signature_Click, LV_EVENT_CLICKED, NULL);
}
// ------------------------------------------

// 选择头像回调（更新预览）
static void Select_Avatar(lv_event_t *e) {
    const char *avatar_path = (const char *)lv_event_get_user_data(e);
    lv_img_set_src(g_chat_ctrl->avatar_preview_btn, avatar_path);
    // 暂存选择的头像路径
    strncpy(g_chat_ctrl->temp_avatar_path, avatar_path, 63);
}

// 保存头像到服务器
static void Save_Avatar_To_Server(lv_event_t *e) {
    NetMsg avatar_msg;
    memset(&avatar_msg, 0, sizeof(avatar_msg));
    avatar_msg.type = MSG_SET_AVATAR;
    strncpy(avatar_msg.user.account, g_chat_ctrl->cur_account, 31);
    strncpy(avatar_msg.user.avatar, g_chat_ctrl->temp_avatar_path, 63);
    
    if (Send_To_Server(&avatar_msg) > 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "头像设置中...");
    }
}

// 创建好友列表界面
static void Create_Friend_Scr() 
{
    g_chat_ctrl->scr_friend = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_friend, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    // 1. 好友列表标题
    lv_obj_t *title = lv_label_create(g_chat_ctrl->scr_friend);
    lv_label_set_text(title, "好友列表（在线）");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(title, &lv_myfont_kai_20, LV_STATE_DEFAULT); // 楷体字库

    // 好友列表（列表控件）（20250929新增：滚动配置，“支持上下滚动”）
    g_chat_ctrl->friend_list = lv_list_create(g_chat_ctrl->scr_friend);
    lv_obj_set_size(g_chat_ctrl->friend_list, 600, 400);//适配GEC6818 7寸屏。300, 350
    lv_obj_align(g_chat_ctrl->friend_list, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_chat_ctrl->friend_list, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);//20250927新增
    // 20250929新增：启用垂直滚动（核心配置）
    lv_obj_set_scroll_dir(g_chat_ctrl->friend_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_chat_ctrl->friend_list, LV_SCROLLBAR_MODE_AUTO); // 自动显示滚动条

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

// ------------------------------------20250929新增：新增刷新列表回调（请求服务器在线用户）-------
static void Refresh_Friend_List(lv_event_t *e) {
    // 请求服务器在线用户信息（账号/昵称/头像/签名/状态）（复用MSG_GET_ONLINE_USER）
    NetMsg get_user_msg = {.type = MSG_GET_ONLINE_USER};
    if (Send_To_Server(&get_user_msg) > 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "正在刷新...");
    } else {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "刷新失败：连接异常");
    }
}

static void Enter_Group_Chat(lv_event_t *e) {   //20250929新增：进入群聊界面
    // 1. 复用聊天窗口（避免新窗口冗余）
    if (!g_chat_ctrl->scr_chat) {
        // 复用单聊窗口的创建逻辑（同4.1，无需重复代码）
        Create_Chat_Scr(); // 可封装单聊/群聊共用的窗口创建函数
    }
    
    // 2. 解决标题重叠：先删除旧标题（核心优化）
    lv_obj_t *old_title = lv_obj_get_child(g_chat_ctrl->scr_chat, 0);
    if (old_title && strstr(lv_label_get_text(old_title), "聊天：") != NULL) {
        lv_obj_del(old_title);
    }
    
    // 3. 创建新群聊标题
    lv_obj_t *group_title = lv_label_create(g_chat_ctrl->scr_chat);
    lv_label_set_text(group_title, "群聊：默认群（所有人）");
    lv_obj_align(group_title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(group_title, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    
    // 4. 标记当前为群聊模式（避免段错误）
    g_chat_ctrl->is_group_chat = 1;
    
    // 5. 跳转群聊窗口
    lv_scr_load(g_chat_ctrl->scr_chat);
}

// 发送消息回调，（20250929新增修改：支持群聊）
 static void Send_Msg_Click(lv_event_t *e) 
 {
    NetMsg msg;
    memset(&msg, 0, sizeof(msg));

    lv_obj_t *msg_ta = lv_obj_get_child(g_chat_ctrl->scr_chat, 1);
    const char *msg_text = lv_textarea_get_text(msg_ta);
    
    // 判断是否群聊（通过标题判断，简化逻辑）
    lv_obj_t *chat_title = lv_obj_get_child(g_chat_ctrl->scr_chat, 2);
    if (strstr(lv_label_get_text(chat_title), "群聊") != NULL) {
        msg.type = MSG_GROUP_CHAT;
        snprintf(msg.content, sizeof(msg.content), "default:%s", msg_text); // 默认群ID
    } else {
        msg.type = MSG_SEND_MSG;
        snprintf(msg.content, sizeof(msg.content), "%s:%s", 
                 g_chat_ctrl->chat_friend_account, msg_text);
    }
    strncpy(msg.user.account, g_chat_ctrl->cur_account, 31);
    
    if(Send_To_Server(&msg) > 0) {
        lv_textarea_set_text(msg_ta, "");
        // 自己显示
        lv_obj_t *chat_content = lv_obj_get_child(g_chat_ctrl->scr_chat, 0);
        char new_msg[300];
        const char *sender = (strstr(lv_label_get_text(chat_title), "群聊") != NULL) ? "我(群聊)" : "我";
        snprintf(new_msg, 300, "%s: %s\n%s", sender, msg_text, lv_label_get_text(chat_content));
        lv_label_set_text(chat_content, new_msg);
    }
 }

// 创建聊天窗口界面
static void Create_Chat_Scr() 
{
    g_chat_ctrl->scr_chat = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_chat, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    // 聊天内容区域（标签，不可编辑可滚动）
    lv_obj_t *chat_content = lv_label_create(g_chat_ctrl->scr_chat);
    lv_obj_set_size(chat_content, 300, 300);
    lv_obj_align(chat_content, LV_ALIGN_TOP_MID, 0, 20);

    lv_label_set_long_mode(chat_content, LV_LABEL_LONG_SCROLL); // 长文本滚动
    lv_label_set_text(chat_content, "聊天内容..."); // 初始提示文本
    lv_obj_set_style_text_font(chat_content, &lv_myfont_kai_20, LV_STATE_DEFAULT);//20250927新增，中文字体适配

    // 消息输入框（索引1）
    lv_obj_t *msg_ta = Create_Textarea(g_chat_ctrl->scr_chat, "请输入消息");
    lv_obj_align(msg_ta, LV_ALIGN_BOTTOM_MID, 0, -60);
    // 20250929新增：绑定键盘
    Dir_Look_Bind_Textarea_Keyboard(msg_ta, g_chat_ctrl->scr_chat);

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

static void Save_Signature_To_Server(lv_event_t *e) {
    lv_obj_t *sign_ta = lv_obj_get_child(g_chat_ctrl->scr_setting, 3);
    const char *signature = lv_textarea_get_text(sign_ta);
    if (strlen(signature) == 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "签名不能为空");
        return;
    }
    
    NetMsg sign_msg;
    memset(&sign_msg, 0, sizeof(sign_msg));
    sign_msg.type = MSG_SET_SIGNATURE;
    strncpy(sign_msg.user.account, g_chat_ctrl->cur_account, 31);
    strncpy(sign_msg.user.signature, signature, 63);
    
    if (Send_To_Server(&sign_msg) > 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "签名设置中...");
    }
}


// -------------------------- 网络接收线程 --------------------------

// 20250927新增：LVGL UI更新封装（确保主线程操作）
static void Lvgl_Update_UI(void *param) {
    Handle_Server_Msg((NetMsg *)param);
    free(param); // 20250929新增关键修改：释放堆分配的 NetMsg 内存，避免泄漏
}

// 处理服务器应答消息
static void Handle_Server_Msg(NetMsg *msg) 
{
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
            else if(strcmp(msg->content, "login") == 0)
            {
                if(msg->user.port == 1) 
                { // ACK=1成功

                    // 20250929新增：保存当前用户完整信息（解决“无法进入好友列表”核心原因）
                    strncpy(g_chat_ctrl->cur_account, msg->user.account, 31);
                    strncpy(g_chat_ctrl->cur_nickname, msg->user.nickname, 31);
                    strncpy(g_chat_ctrl->cur_avatar, msg->user.avatar, 63); // 80*80头像路径

                    // 登录成功后请求在线用户列表
                    NetMsg get_online_msg = {.type = MSG_GET_ONLINE_USER};
                    Send_To_Server(&get_online_msg );

                    // 20250929新增：跳转好友列表（原逻辑未执行因cur_account为空）
                    lv_scr_load(g_chat_ctrl->scr_friend);
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "登录成功！");
                } else 
                { // ACK=0失败。提示重新输入
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "账号/密码错误，请重试");
                    lv_textarea_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 2), ""); // 20250929新增：清空密码
                }
            }        
            else if(strcmp(msg->content, "add_friend") == 0) //20250927新增
            {
                if(msg->user.port == 1) {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "添加好友成功");
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
            break;
        }
        case MSG_USER_LIST: 
        {
            // 20250929新增：释放旧列表项的账号内存（防止泄漏）--------------
            lv_obj_t *child;

            // 1. 获取好友列表的子对象数量（LVGL v8.2支持）
            uint32_t child_count = lv_obj_get_child_cnt(g_chat_ctrl->friend_list);
            // 2. 循环遍历所有子对象
            for (uint32_t i = 0; i < child_count; i++) {
                child = lv_obj_get_child(g_chat_ctrl->friend_list, i); // 获取第i个子对象
                char *acc = (char *)lv_obj_get_user_data(child);
                if (acc != NULL) 
                {   free(acc);
                    lv_obj_set_user_data(child, NULL); 
                }
            }//-------------------------------------------------------

            // 更新好友列表（格式：账号:昵称:头像路径:签名:在线状态|...）
            lv_obj_clean(g_chat_ctrl->friend_list); // 清空原有列表（避免重叠）

            char *token = strtok(msg->content, "|");

            while(token != NULL) {
                char account[32], nickname[32], avatar[64], signature[64];
                int online;     //20250929新增
                sscanf(token, "%[^:]:%[^:]:%[^:]:%[^:]:%d", account, nickname, avatar, signature, &online);

                // 3. 创建好友列表项（带头像按钮80*80，方案要求）
                lv_obj_t *list_item = lv_list_add_btn(g_chat_ctrl->friend_list, NULL, "");
                lv_obj_set_size(list_item, 600, 100); // 列表项高度适配头像
                    
                // 3.1 80*80头像按钮（替换路径为图像按钮）
                lv_obj_t *avatar_btn = lv_btn_create(list_item);
                lv_obj_set_size(avatar_btn, 80, 80);
                lv_obj_t *avatar_img = lv_img_create(avatar_btn);
                lv_img_set_src(avatar_img, avatar);
                lv_obj_align(avatar_img, LV_ALIGN_CENTER, 0, 0);
                    
                // 3.2 好友信息标签（昵称+签名+在线状态）
                lv_obj_t *info_label = lv_label_create(list_item);
                char info_text[128];
                snprintf(info_text, 128, "%s\n%s\n%s", 
                         nickname, signature, 
                         online ? "[在线]" : "[离线]"); // 方案要求显示在线状态
                lv_label_set_text(info_label, info_text);
                lv_obj_align(info_label, LV_ALIGN_BOTTOM_LEFT, 100, 0);
                lv_obj_set_style_text_font(info_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
        
            // 3.3 绑定点击事件（进入单聊）
            lv_obj_add_event_cb(list_item, Friend_Click_Enter_Chat, LV_EVENT_CLICKED, NULL);

                    token = strtok(NULL, "|");
                }
                    // 20250929新增：刷新提示
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "好友列表已更新");
                break;
            }
            case MSG_SEND_MSG: {
                // 接收聊天消息（格式：发送者昵称: 消息内容）
                lv_obj_t *chat_content = lv_obj_get_child(g_chat_ctrl->scr_chat, 0);
                char new_msg[300];
                snprintf(new_msg, 300, "%s: %s\n%s", msg->user.nickname, msg->content, lv_textarea_get_text(chat_content));
                lv_label_set_text(chat_content, new_msg); // 修改为使用标签
                break;
            }
            case MSG_GROUP_CHAT: {
                // 20250929新增：群聊消息格式：发送者昵称+内容
                lv_obj_t *chat_title = lv_obj_get_child(g_chat_ctrl->scr_chat, 2);
                if (strstr(lv_label_get_text(chat_title), "群聊") == NULL) {
                    // 若当前不在群聊窗口，弹窗提示（简化为标签显示）
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), 
                                     "收到群聊消息，请进入群聊查看");
                    break;
            }
            // 在群聊窗口显示消息
            lv_obj_t *chat_content = lv_obj_get_child(g_chat_ctrl->scr_chat, 0);
            char new_msg[300];
            snprintf(new_msg, 300, "%s(群聊): %s\n%s", 
                     msg->user.nickname, msg->content, lv_label_get_text(chat_content));
            lv_label_set_text(chat_content, new_msg);
            break;
        }

        case MSG_SINGLE_CHAT_RECV: {
            // 20250929新增：检查是否在单聊窗口
            if (!g_chat_ctrl->scr_chat) {
                // 不在聊天窗口：提示用户
                lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "收到新消息，请进入聊天窗口查看");
                break;
            }
            // 显示对方消息（is_self=0，左对齐）
            Show_Chat_Log(msg->user.nickname, msg->user.avatar, msg->content, 0);
            break;
        }

        case MSG_GROUP_CHAT_RECV: {
            if (!g_chat_ctrl->scr_chat || !g_chat_ctrl->is_group_chat) {
                // 不在群聊窗口：提示
                lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_friend, 0), "收到群聊消息，请进入群聊查看");
                break;
            }
            // 显示群聊消息（带“群聊”标识）
            char group_nickname[64];
            snprintf(group_nickname, 64, "%s(群聊)", msg->user.nickname);
            Show_Chat_Log(group_nickname, msg->user.avatar, msg->content, 0);
            break;
        }

        default:
            printf("未知消息类型: %d\n", msg->type);
            break;        
    }
}

// 接收服务器消息线程（避免阻塞UI）
static void *Recv_Server_Msg(void *arg) 
{

    while(g_chat_ctrl && !g_chat_ctrl->exiting)
    {   
       // 20250929新增修改：堆分配消息内存（关键修改：避免栈变量销毁问题）-----
        NetMsg *msg = (NetMsg *)malloc(sizeof(NetMsg));
        if (!msg) { // 内存分配失败处理
            perror("malloc NetMsg failed");
            sleep(1);
            continue;
        }
        //20250928修改
        memset(msg, 0, sizeof(*msg));
        int ret = recv(g_chat_ctrl->sockfd, msg, sizeof(NetMsg), 0);
        
        if(ret <= 0 || !g_chat_ctrl || g_chat_ctrl->exiting) {
            free(msg); // 20250929新增：接收失败，释放内存
            break;
        }

        // 线程安全处理UI（LVGL需在主线程更新，此处简化为直接操作）
        pthread_mutex_lock(&msg_mutex);

        // 再次检查，避免竞态条件
        if(g_chat_ctrl && !g_chat_ctrl->exiting) 
        {
            lv_async_call(Lvgl_Update_UI, msg); //传递堆变量地址，异步执行时数据仍有效
        } else {
            free(msg); // 20250929新增：无需处理，直接释放
        }

        pthread_mutex_unlock(&msg_mutex);
    }
    printf("接收线程安全退出\n");

    return NULL;
}

// -------------------------- 模块初始化与退出 --------------------------
void Chat_Room_Init(struct Ui_Ctrl *uc, lv_obj_t *scr_home, bool connect_now)
{
    // 初始化全局控制结构体
    g_chat_ctrl = (CHAT_CTRL_P)malloc(sizeof(CHAT_CTRL));
    memset(g_chat_ctrl, 0, sizeof(CHAT_CTRL));
    g_chat_ctrl->uc = uc;
    g_chat_ctrl->scr_home = scr_home;
    g_chat_ctrl->sockfd = -1; // 20250927新增：初始未连接

    // 初始化互斥锁
    pthread_mutex_init(&msg_mutex, NULL);

    // 创建所有界面
    Create_Login_Scr();
    Create_Register_Scr();
    Create_Friend_Scr();
    Create_Chat_Scr();
    Create_Setting_Scr(); // 新增设置界面

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

    // 进入登录界面
    lv_scr_load(g_chat_ctrl->scr_friend);
    // 其他界面：scr_login、scr_register、scr_friend、scr_chat、scr_setting
}

void Chat_Room_Exit() 
{
    if(!g_chat_ctrl) return;

    printf("开始退出聊天室...\n");  //20250928新增

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
        printf("接收线程已退出\n");
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

    // 释放内存，全局控制结构体
    free(g_chat_ctrl);
    g_chat_ctrl = NULL;

    printf("聊天室资源完全释放\n");
}