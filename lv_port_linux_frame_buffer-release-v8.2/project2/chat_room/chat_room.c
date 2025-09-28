//@file chat_room.c 聊天室客户端

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

    // 密码输入框（索引2）
    lv_obj_t *pwd_ta = Create_Textarea(g_chat_ctrl->scr_login, "请输入密码");
    lv_textarea_set_password_mode(pwd_ta, true);
    lv_obj_align(pwd_ta, LV_ALIGN_TOP_MID, 0, 140);

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
    // 发送离线通知（若已登录）
    if (g_chat_ctrl && g_chat_ctrl->sockfd >= 0 && strlen(g_chat_ctrl->cur_account) > 0) {
        NetMsg offline_msg;
        memset(&offline_msg, 0, sizeof(offline_msg));
        offline_msg.type = MSG_LOGOUT; // 新增消息类型：退出登录
        strncpy(offline_msg.user.account, g_chat_ctrl->cur_account, 31);
        Send_To_Server(&offline_msg);
    }
    Chat_Room_Exit(); // 调用原有释放函数
    lv_scr_load(g_chat_ctrl->scr_home); // 返回首页
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

    lv_obj_t *pwd_ta = Create_Textarea(g_chat_ctrl->scr_register, "请设置密码");
    lv_textarea_set_password_mode(pwd_ta, true);
    lv_obj_align(pwd_ta, LV_ALIGN_TOP_MID, 0, 140);// 索引2

    lv_obj_t *nick_ta = Create_Textarea(g_chat_ctrl->scr_register, "请设置昵称");
    lv_obj_align(nick_ta, LV_ALIGN_TOP_MID, 0, 200);// 索引3

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
    
    lv_obj_t *label = lv_obj_get_child(item, 0); // 获取按钮中的第一个子对象（标签）
    const char *friend_name = lv_label_get_text(label);

    printf("chat with %s\n", friend_name);
    lv_scr_load(g_chat_ctrl->scr_chat);
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
    strncpy(msg.user.signature, lv_textarea_get_text(sign_ta), 63);

    if(Send_To_Server(&msg) > 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_setting, 0), "签名设置成功");
        lv_textarea_set_text(sign_ta, "");
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

    // 个性签名输入框（索引2）
    lv_obj_t *sign_ta = Create_Textarea(g_chat_ctrl->scr_setting, "请输入个性签名");
    lv_obj_align(sign_ta, LV_ALIGN_TOP_MID, 0, 140);

    // 添加好友按钮
    lv_obj_t *add_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_set_size(add_btn, 105, 30);
    lv_obj_align(add_btn, LV_ALIGN_TOP_MID, -60, 200);
    lv_obj_t *add_label = lv_label_create(add_btn);
    lv_label_set_text(add_label, "添加好友");
    lv_obj_set_style_text_font(add_label, &lv_myfont_kai_20, LV_STATE_DEFAULT);
    lv_obj_center(add_label);  // 20250928新增补充：明确标签居中（确保文字居中）

    // 设置签名按钮
    lv_obj_t *sign_btn = lv_btn_create(g_chat_ctrl->scr_setting);
    lv_obj_set_size(sign_btn, 105, 30);
    lv_obj_align(sign_btn, LV_ALIGN_TOP_MID, 60, 200);
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
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_friend, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);

    // 标题
    Create_Label(g_chat_ctrl->scr_friend, "在线好友", 20);

    // 好友列表（列表控件）
    g_chat_ctrl->friend_list = lv_list_create(g_chat_ctrl->scr_friend);
    lv_obj_set_size(g_chat_ctrl->friend_list, 300, 350);
    lv_obj_align(g_chat_ctrl->friend_list, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(g_chat_ctrl->friend_list, lv_color_hex(0xC7EDCC), LV_STATE_DEFAULT);//20250927新增

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

// 发送消息回调
 static void Send_Msg_Click(lv_event_t *e) 
 {
     NetMsg msg;
     memset(&msg, 0, sizeof(msg));
     msg.type = MSG_SEND_MSG;
     strncpy(msg.user.account, g_chat_ctrl->cur_account, 31);
     // 获取输入框内容
     lv_obj_t *msg_ta = lv_obj_get_child(g_chat_ctrl->scr_chat, 1);
     strncpy(msg.content, lv_textarea_get_text(msg_ta), 255);
     // 发送消息
     if(Send_To_Server(&msg) > 0) {
         lv_textarea_set_text(msg_ta, ""); // 清空输入框
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
static void Lvgl_Update_UI(void *param) {
    Handle_Server_Msg((NetMsg *)param);
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
                    strncpy(g_chat_ctrl->cur_account, msg->user.account, 31);
                    // 登录成功后请求在线用户列表
                    NetMsg get_user_msg = {.type = MSG_GET_ONLINE_USER};
                    Send_To_Server(&get_user_msg);
                    lv_scr_load(g_chat_ctrl->scr_friend);
                } else 
                { // ACK=0失败
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "登录失败：账号/密码错误");
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
            break;
        }
        case MSG_USER_LIST: {
            // 更新好友列表（格式：账号:昵称:签名|账号:昵称:签名|....）
            char *token = strtok(msg->content, "|");
            lv_obj_clean(g_chat_ctrl->friend_list); // 清空原有列表

            while(token) {
                char account[32], nickname[32], signature[64];
                sscanf(token, "%[^:]:%[^:]:%s", account, nickname, signature);
                // 添加列表项（显示昵称+签名）

                char item_text[100];
                snprintf(item_text, 100, "%s(%s)", nickname, signature);//20250927新增

                lv_obj_t *item = lv_list_add_btn(g_chat_ctrl->friend_list, NULL, item_text);
                lv_obj_add_event_cb(item, Friend_Click, LV_EVENT_CLICKED, NULL);
                token = strtok(NULL, "|");
            }
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

        default:
            printf("未知消息类型: %d\n", msg->type);
            break;        
    }
}

// 接收服务器消息线程（避免阻塞UI）
static void *Recv_Server_Msg(void *arg) 
{
    NetMsg msg;
    while(1) {
        memset(&msg, 0, sizeof(msg));
        int ret = recv(g_chat_ctrl->sockfd, &msg, sizeof(NetMsg), 0);
        if(ret <= 0) {
            perror("recv failed or server closed");
            break;
        }

        // 线程安全处理UI（LVGL需在主线程更新，此处简化为直接操作）
        pthread_mutex_lock(&msg_mutex);
        lv_async_call(Lvgl_Update_UI, &msg); // 异步调用UI更新
        pthread_mutex_unlock(&msg_mutex);
    }
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
    } 

    // 进入登录界面
    lv_scr_load(g_chat_ctrl->scr_login);
}

void Chat_Room_Exit() 
{
    if(!g_chat_ctrl) return;
    // 关闭socket
    if(g_chat_ctrl->sockfd >= 0) close(g_chat_ctrl->sockfd);

    // 销毁线程
    pthread_cancel(recv_thread_id);
    pthread_join(recv_thread_id, NULL);

    // 销毁互斥锁
    pthread_mutex_destroy(&msg_mutex);

    // 释放界面资源
    lv_obj_del(g_chat_ctrl->scr_login);
    lv_obj_del(g_chat_ctrl->scr_register);
    lv_obj_del(g_chat_ctrl->scr_friend);
    lv_obj_del(g_chat_ctrl->scr_chat);
    lv_obj_del(g_chat_ctrl->scr_setting);

    // 释放内存
    free(g_chat_ctrl);
    g_chat_ctrl = NULL;
}