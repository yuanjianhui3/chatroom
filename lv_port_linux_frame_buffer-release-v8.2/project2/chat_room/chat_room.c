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

// 云服务器配置（初学者需修改为自己的云服务器IP和端口）
#define SERVER_IP "你的华为云服务器公网IP"
#define SERVER_PORT 8888

static ChatCtrl *g_chat_ctrl = NULL; // 全局控制指针
static pthread_t recv_thread_id;     // 接收服务器消息线程
static pthread_mutex_t msg_mutex;    // 线程安全互斥锁

// 函数前置声明（符合模块化规范）
static void login_click(lv_event_t *e);

// 前置声明（所有内部函数都要加）
static void reg_click(lv_event_t *e);
static void do_register(lv_event_t *e);
static void friend_click(lv_event_t *e);
static void send_msg_click(lv_event_t *e);

void Chat_Room_Exit(void);

// -------------------------- 工具函数 --------------------------
// 创建输入框（复用UI代码，减少冗余）
static lv_obj_t *create_textarea(lv_obj_t *parent, const char *hint_text) {
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, 250, 40);
    lv_textarea_set_placeholder_text(ta, hint_text);
    return ta;
}

// 创建标签（简化控件创建）
static lv_obj_t *create_label(lv_obj_t *parent, const char *text, lv_coord_t y) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_align(label, LV_ALIGN_TOP_CENTER, 0, y);
    return label;
}

// 连接云服务器（封装TCP客户端逻辑）
static int connect_server() {
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

    // 连接服务器
    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect server failed");
        close(sockfd);
        return -1;
    }
    printf("connect server success\n");
    return sockfd;
}

// 发送消息到服务器（封装发送逻辑）
static int send_to_server(NetMsg *msg) {
    if(!g_chat_ctrl || g_chat_ctrl->sockfd < 0) return -1;
    return send(g_chat_ctrl->sockfd, msg, sizeof(NetMsg), 0);
}

// -------------------------- 界面切换与创建 --------------------------
// 返回首页
static void back_to_home(lv_event_t *e) {
    lv_obj_t *scr_home = (lv_obj_t *)lv_event_get_user_data(e);
    lv_scr_load(scr_home);
}

// 返回好友列表
static void back_to_friend(lv_event_t *e) {
    lv_scr_load(g_chat_ctrl->scr_friend);
}

// 注册按钮回调（切换到注册界面）
static void reg_click(lv_event_t *e) {
    lv_scr_load(g_chat_ctrl->scr_register);
}

// 登录按钮回调（发送登录请求）
static void login_click(lv_event_t *e) 
{
    NetMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_LOGIN;
    // 获取输入框内容
    lv_obj_t *account_ta = lv_obj_get_child(g_chat_ctrl->scr_login, 1); // 账号输入框（索引1）
    lv_obj_t *pwd_ta = lv_obj_get_child(g_chat_ctrl->scr_login, 2);     // 密码输入框（索引2）
    strncpy(msg.user.account, lv_textarea_get_text(account_ta), 31);
    strncpy(msg.user.password, lv_textarea_get_text(pwd_ta), 31);
    // 发送登录请求
    if(send_to_server(&msg) < 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "登录失败：连接异常");
        return;
    }
}

// 创建登录界面
static void create_login_scr() 
{
    g_chat_ctrl->scr_login = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_login, lv_color_hex(0xFFFFFF), 0);

    // 标题
    create_label(g_chat_ctrl->scr_login, "聊天室登录", 30);

    // 账号输入框
    lv_obj_t *account_ta = create_textarea(g_chat_ctrl->scr_login, "请输入账号");
    lv_obj_align(account_ta, LV_ALIGN_TOP_CENTER, 0, 80);

    // 密码输入框
    lv_obj_t *pwd_ta = create_textarea(g_chat_ctrl->scr_login, "请输入密码");
    lv_textarea_set_password_mode(pwd_ta, true);
    lv_obj_align(pwd_ta, LV_ALIGN_TOP_CENTER, 0, 140);

    // 登录按钮
    lv_obj_t *login_btn = lv_btn_create(g_chat_ctrl->scr_login);
    lv_obj_set_size(login_btn, 100, 40);
    lv_obj_align(login_btn, LV_ALIGN_TOP_CENTER, -60, 200);
    lv_obj_t *login_label = lv_label_create(login_btn);
    lv_label_set_text(login_label, "登录");

    // 注册按钮
    lv_obj_t *reg_btn = lv_btn_create(g_chat_ctrl->scr_login);
    lv_obj_set_size(reg_btn, 100, 40);
    lv_obj_align(reg_btn, LV_ALIGN_TOP_CENTER, 60, 200);
    lv_obj_t *reg_label = lv_label_create(reg_btn);
    lv_label_set_text(reg_label, "注册");

    // 返回首页按钮
    lv_obj_t *back_btn = lv_btn_create(g_chat_ctrl->scr_login);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回首页");
    lv_obj_add_event_cb(back_btn, back_to_home, LV_EVENT_CLICKED, g_chat_ctrl->scr_home);

    lv_obj_add_event_cb(login_btn, login_click, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(reg_btn, reg_click, LV_EVENT_CLICKED, NULL);
}

// 注册按钮回调（发送注册请求）
static void do_register(lv_event_t *e) 
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
    strncpy(msg.user.ip, "192.168.1.100", 15); // 初学者需修改为实际开发板IP
    msg.user.port = 8000; // 固定本地端口
    // 发送注册请求
    if(send_to_server(&msg) < 0) {
        lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册失败：连接异常");
        return;
    }
    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册中...");
}

// 创建注册界面
static void create_register_scr() {
    g_chat_ctrl->scr_register = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_register, lv_color_hex(0xFFFFFF), 0);

    // 标题
    create_label(g_chat_ctrl->scr_register, "用户注册", 30);

    // 账号、密码、昵称输入框
    lv_obj_t *account_ta = create_textarea(g_chat_ctrl->scr_register, "请设置账号（唯一）");
    lv_obj_align(account_ta, LV_ALIGN_TOP_CENTER, 0, 80);

    lv_obj_t *pwd_ta = create_textarea(g_chat_ctrl->scr_register, "请设置密码");
    lv_textarea_set_password_mode(pwd_ta, true);
    lv_obj_align(pwd_ta, LV_ALIGN_TOP_CENTER, 0, 140);

    lv_obj_t *nick_ta = create_textarea(g_chat_ctrl->scr_register, "请设置昵称");
    lv_obj_align(nick_ta, LV_ALIGN_TOP_CENTER, 0, 200);

    // 注册按钮
    lv_obj_t *reg_btn = lv_btn_create(g_chat_ctrl->scr_register);
    lv_obj_set_size(reg_btn, 100, 40);
    lv_obj_align(reg_btn, LV_ALIGN_TOP_CENTER, 0, 260);
    lv_obj_t *reg_label = lv_label_create(reg_btn);
    lv_label_set_text(reg_label, "注册");

    // 返回登录按钮
    lv_obj_t *back_btn = lv_btn_create(g_chat_ctrl->scr_register);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回登录");
    lv_obj_add_event_cb(back_btn, back_to_friend, LV_EVENT_CLICKED, NULL); // 复用返回好友列表回调

    lv_obj_add_event_cb(reg_btn, do_register, LV_EVENT_CLICKED, NULL);
}

// 好友列表项点击（进入聊天窗口）
static void friend_click(lv_event_t *e) {
    lv_obj_t *item = lv_event_get_current_target(e);
    const char *friend_name = lv_label_get_text(lv_list_get_btn_label(item));
    printf("chat with %s\n", friend_name);
    lv_scr_load(g_chat_ctrl->scr_chat);
}

// 创建好友列表界面
static void create_friend_scr() {
    g_chat_ctrl->scr_friend = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_friend, lv_color_hex(0xFFFFFF), 0);

    // 标题
    create_label(g_chat_ctrl->scr_friend, "在线好友", 20);

    // 好友列表（列表控件）
    g_chat_ctrl->friend_list = lv_list_create(g_chat_ctrl->scr_friend);
    lv_obj_set_size(g_chat_ctrl->friend_list, 300, 350);
    lv_obj_align(g_chat_ctrl->friend_list, LV_ALIGN_TOP_CENTER, 0, 60);

    // 返回首页按钮
    lv_obj_t *home_btn = lv_btn_create(g_chat_ctrl->scr_friend);
    lv_obj_set_size(home_btn, 80, 30);
    lv_obj_align(home_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *home_label = lv_label_create(home_btn);
    lv_label_set_text(home_label, "返回首页");
    lv_obj_add_event_cb(home_btn, back_to_home, LV_EVENT_CLICKED, g_chat_ctrl->scr_home);

    // 设置按钮（扩展功能入口）
    lv_obj_t *set_btn = lv_btn_create(g_chat_ctrl->scr_friend);
    lv_obj_set_size(set_btn, 80, 30);
    lv_obj_align(set_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_t *set_label = lv_label_create(set_btn);
    lv_label_set_text(set_label, "设置");
}

// 发送消息回调
 static void send_msg_click(lv_event_t *e) 
 {
     NetMsg msg;
     memset(&msg, 0, sizeof(msg));
     msg.type = MSG_SEND_MSG;
     strncpy(msg.user.account, g_chat_ctrl->cur_account, 31);
     // 获取输入框内容
     lv_obj_t *msg_ta = lv_obj_get_child(g_chat_ctrl->scr_chat, 1);
     strncpy(msg.content, lv_textarea_get_text(msg_ta), 255);
     // 发送消息
     if(send_to_server(&msg) > 0) {
         lv_textarea_set_text(msg_ta, ""); // 清空输入框
     }
 }

// 创建聊天窗口界面
static void create_chat_scr() {
    g_chat_ctrl->scr_chat = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_chat_ctrl->scr_chat, lv_color_hex(0xFFFFFF), 0);

    // 聊天内容区域（文本区域，不可编辑）
    lv_obj_t *chat_content = lv_textarea_create(g_chat_ctrl->scr_chat);
    lv_obj_set_size(chat_content, 300, 300);
    lv_obj_align(chat_content, LV_ALIGN_TOP_CENTER, 0, 20);
    lv_textarea_set_editable(chat_content, false);
    lv_textarea_set_placeholder_text(chat_content, "聊天内容...");

    // 消息输入框
    lv_obj_t *msg_ta = create_textarea(g_chat_ctrl->scr_chat, "请输入消息");
    lv_obj_align(msg_ta, LV_ALIGN_BOTTOM_CENTER, 0, -60);

    // 发送按钮
    lv_obj_t *send_btn = lv_btn_create(g_chat_ctrl->scr_chat);
    lv_obj_set_size(send_btn, 60, 40);
    lv_obj_align(send_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -60);
    lv_obj_t *send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "发送");

    // 返回好友列表按钮
    lv_obj_t *back_btn = lv_btn_create(g_chat_ctrl->scr_chat);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "返回好友");
    lv_obj_add_event_cb(back_btn, back_to_friend, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(send_btn, send_msg_click, LV_EVENT_CLICKED, NULL);
}

// -------------------------- 网络接收线程 --------------------------
// 处理服务器应答消息
static void handle_server_msg(NetMsg *msg) {
    switch(msg->type) {
        case MSG_ACK: {
            // 注册/登录应答（content为"register"/"login"）
            if(strcmp(msg->content, "register") == 0) {
                if(msg->user.port == 1) { // ACK=1成功
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册成功！请登录");
                    sleep(1);
                    lv_scr_load(g_chat_ctrl->scr_login);
                } else {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_register, 0), "注册失败：账号已存在");
                }
            } else if(strcmp(msg->content, "login") == 0) {
                if(msg->user.port == 1) { // ACK=1成功
                    strncpy(g_chat_ctrl->cur_account, msg->user.account, 31);
                    // 登录成功后请求在线用户列表
                    NetMsg get_user_msg = {.type = MSG_GET_ONLINE_USER};
                    send_to_server(&get_user_msg);
                    lv_scr_load(g_chat_ctrl->scr_friend);
                } else {
                    lv_label_set_text(lv_obj_get_child(g_chat_ctrl->scr_login, 0), "登录失败：账号/密码错误");
                }
            }
            break;
        }
        case MSG_USER_LIST: {
            // 更新好友列表（格式：账号1:昵称1|账号2:昵称2|...）
            char *token = strtok(msg->content, "|");
            lv_list_clear(g_chat_ctrl->friend_list); // 清空原有列表
            while(token) {
                char account[32], nickname[32];
                sscanf(token, "%[^:]:%s", account, nickname);
                // 添加列表项
                lv_obj_t *item = lv_list_add_btn(g_chat_ctrl->friend_list, NULL, nickname);
                lv_obj_add_event_cb(item, friend_click, LV_EVENT_CLICKED, NULL);
                token = strtok(NULL, "|");
            }
            break;
        }
        case MSG_SEND_MSG: {
            // 接收聊天消息（格式：发送者昵称: 消息内容）
            lv_obj_t *chat_content = lv_obj_get_child(g_chat_ctrl->scr_chat, 0);
            char new_msg[300];
            snprintf(new_msg, 300, "%s: %s\n%s", msg->user.nickname, msg->content, lv_textarea_get_text(chat_content));
            lv_textarea_set_text(chat_content, new_msg);
            break;
        }
    }
}

// 接收服务器消息线程（避免阻塞UI）
static void *recv_server_msg(void *arg) {
    NetMsg msg;
    while(1) {
        memset(&msg, 0, sizeof(msg));
        int ret = recv(g_chat_ctrl->sockfd, &msg, sizeof(NetMsg), 0);
        if(ret <= 0) {
            perror("recv failed or server closed");
            break;
        }

        // 线程安全处理UI（LVGL需在主线程更新，此处简化为直接操作，初学者可用）
        pthread_mutex_lock(&msg_mutex);
        handle_server_msg(&msg);
        pthread_mutex_unlock(&msg_mutex);
    }
    return NULL;
}

// -------------------------- 模块初始化与退出 --------------------------
void Chat_Room_Init(struct Ui_Ctrl *uc, lv_obj_t *scr_home) {
    // 初始化全局控制结构体
    g_chat_ctrl = (ChatCtrl *)malloc(sizeof(ChatCtrl));
    memset(g_chat_ctrl, 0, sizeof(ChatCtrl));
    g_chat_ctrl->uc = uc;
    g_chat_ctrl->scr_home = scr_home;

    // 连接云服务器
    g_chat_ctrl->sockfd = connect_server();
    if(g_chat_ctrl->sockfd < 0) {
        lv_obj_t *err_label = lv_label_create(scr_home);
        lv_label_set_text(err_label, "连接服务器失败！");
        lv_obj_align(err_label, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    // 初始化互斥锁
    pthread_mutex_init(&msg_mutex, NULL);

    // 创建所有界面
    create_login_scr();
    create_register_scr();
    create_friend_scr();
    create_chat_scr();

    // 启动接收线程
    pthread_create(&recv_thread_id, NULL, recv_server_msg, NULL);

    // 进入登录界面
    lv_scr_load(g_chat_ctrl->scr_login);
}

void Chat_Room_Exit() {
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

    // 释放内存
    free(g_chat_ctrl);
    g_chat_ctrl = NULL;
}