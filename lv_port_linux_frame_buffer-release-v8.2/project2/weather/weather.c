//@file weather.c  天气模块

#include "weather.h"
#include "../cJSON/cJSON.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

// 方案要求：HTTP获取天气（使用易天气免费API，需自行申请appid）
#define WEATHER_API_URL "GET /api?version=v91&appid=YOUR_APPID&appsecret=YOUR_APPSECRET&city=广州 HTTP/1.1\r\nHost: v0.yiketianqi.com\r\nConnection: close\r\n\r\n"
#define WEATHER_HOST "v0.yiketianqi.com"
#define WEATHER_PORT 80

// 温度联动阈值（方案要求）
#define TEMP_LED_THRESHOLD_1 28  // 闪烁间隔1s
#define TEMP_LED_THRESHOLD_2 32  // 闪烁间隔0.5s
#define TEMP_BEEP_THRESHOLD 35   // 蜂鸣器报警

// 返回首页回调
static void Weather_Back_Cb(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    Weather_UI_P ui = ext_uc->weather_ui;
    Ext_Scr_Switch(ui->weather_scr, ext_uc->base_uc->start_ui_p->start_ui, LV_DIR_TOP);
}

// 温度检测定时器回调（联动LED/蜂鸣器，方案要求）
static void Temp_Check_Timer(lv_timer_t *timer)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)timer->user_data;
    int curr_temp = Ext_State_Get(ext_uc, "temp");
    Weather_Temp_Linkage(ext_uc, curr_temp);
}

// 构建天气预报UI（最简实现，方案要求）
static void Weather_Build_Scr(EXT_UI_CTRL_P ext_uc)
{
    Weather_UI_P ui = ext_uc->weather_ui;
    LV_FONT_DECLARE(lv_font_simsun_24);

    ui->weather_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui->weather_scr, lv_color_hex(0x3498DB), LV_STATE_DEFAULT);

    // 城市显示
    ui->city_lab = lv_label_create(ui->weather_scr);
    lv_obj_set_pos(ui->city_lab, 320, 80);
    lv_obj_set_style_text_color(ui->city_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->city_lab, &lv_font_simsun_24, LV_STATE_DEFAULT);
    lv_label_set_text(ui->city_lab, "城市：广州");

    // 温度显示（核心数据，用于联动）
    ui->temp_lab = lv_label_create(ui->weather_scr);
    lv_obj_set_pos(ui->temp_lab, 320, 150);
    lv_obj_set_style_text_color(ui->temp_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->temp_lab, &lv_font_simsun_24, LV_STATE_DEFAULT);
    lv_label_set_text(ui->temp_lab, "温度：--℃");

    // 天气状况
    ui->cond_lab = lv_label_create(ui->weather_scr);
    lv_obj_set_pos(ui->cond_lab, 320, 220);
    lv_obj_set_style_text_color(ui->cond_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->cond_lab, &lv_font_simsun_24, LV_STATE_DEFAULT);
    lv_label_set_text(ui->cond_lab, "状况：--");

    // 返回首页按钮
    ui->back_btn = lv_btn_create(ui->weather_scr);
    lv_obj_set_size(ui->back_btn, 80, 30);
    lv_obj_set_pos(30, 420);
    lv_obj_t *back_lab = lv_label_create(ui->back_btn);
    lv_label_set_text(back_lab, "返回首页");
    lv_obj_center(back_lab);
    lv_obj_add_event_cb(ui->back_btn, Weather_Back_Cb, LV_EVENT_SHORT_CLICKED, ext_uc);
}

// 原生socket实现HTTP请求（最简方式，无依赖，方案要求）
static int Weather_Http_Request(char *recv_buf, int buf_len)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        printf("HTTP socket创建失败\n");
        return -1;
    }

    // 解析域名
    struct hostent *host = gethostbyname(WEATHER_HOST);
    if(host == NULL) {
        close(sockfd);
        printf("解析域名失败\n");
        return -1;
    }

    // 服务端地址
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(WEATHER_PORT);
    memcpy(&serv_addr.sin_addr.s_addr, host->h_addr, host->h_length);

    // 连接服务器
    if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        printf("HTTP连接失败\n");
        return -1;
    }

    // 发送HTTP请求
    send(sockfd, WEATHER_API_URL, strlen(WEATHER_API_URL), 0);

    // 接收响应
    int recv_len = 0;
    while(recv_len < buf_len - 1) {
        int len = recv(sockfd, recv_buf + recv_len, buf_len - 1 - recv_len, 0);
        if(len <= 0) break;
        recv_len += len;
    }
    recv_buf[recv_len] = '\0';
    close(sockfd);

    return recv_len > 0 ? 0 : -1;
}

// 解析天气JSON数据
static void Weather_Parse_Json(const char *json_str, EXT_UI_CTRL_P ext_uc)
{
    Weather_UI_P ui = ext_uc->weather_ui;
    cJSON *root = cJSON_Parse(json_str);
    if(root == NULL) return;

    // 提取温度（方案要求：用于联动控制）
    cJSON *temp = cJSON_GetObjectItem(root, "temp");
    // 提取天气状况
    cJSON *cond = cJSON_GetObjectItem(root, "wea");

    if(temp && cond) {
        // 更新UI显示
        char temp_buf[32];
        sprintf(temp_buf, "温度：%s℃", temp->valuestring);
        lv_label_set_text(ui->temp_lab, temp_buf);

        char cond_buf[32];
        sprintf(cond_buf, "状况：%s", cond->valuestring);
        lv_label_set_text(ui->cond_lab, cond_buf);

        // 更新跨模块温度状态（用于联动智能家居）
        int temp_val = atoi(temp->valuestring);
        Ext_State_Update(ext_uc, "temp", temp_val);
    }

    cJSON_Delete(root);
}

// 温度联动控制（按方案要求：LED闪烁频率+蜂鸣器报警）
void Weather_Temp_Linkage(EXT_UI_CTRL_P ext_uc, int temp)
{
    // 联动智能家居模块的LED/蜂鸣器（通过设备文件操作，无依赖）
    int led_fd = open("/dev/led_drv", O_RDWR);
    int beep_fd = open("/dev/beep", O_RDWR);
    if(led_fd < 0 || beep_fd < 0) return;

    static int led_state = 0;
    char led_buf[2] = {0, 7}; // 控制D7 LED（参考led_test.c）

    // 温度升高→闪烁间隔缩短（方案要求）
    if(temp >= TEMP_BEEP_THRESHOLD) {
        // 温度达标→蜂鸣器长响
        ioctl(beep_fd, 0, 1); // 0=开（按gec6818_beep.ko驱动定义）
        // LED常亮
        led_state = 1;
    } else if(temp >= TEMP_LED_THRESHOLD_2) {
        // 32-34℃→0.5s闪烁
        ioctl(beep_fd, 1, 1); // 1=关
        led_state = !led_state;
        usleep(500000);
    } else if(temp >= TEMP_LED_THRESHOLD_1) {
        // 28-31℃→1s闪烁
        ioctl(beep_fd, 1, 1);
        led_state = !led_state;
        usleep(1000000);
    } else {
        // 低于28℃→LED常关
        ioctl(beep_fd, 1, 1);
        led_state = 0;
    }

    // 控制LED状态
    led_buf[0] = led_state;
    write(led_fd, led_buf, sizeof(led_buf));

    close(led_fd);
    close(beep_fd);
}

// 初始化天气预报
int Weather_Init(EXT_UI_CTRL_P ext_uc)
{
    ext_uc->weather_ui = (WEATHER_UI_P)malloc(sizeof(WEATHER_UI));
    memset(ext_uc->weather_ui, 0, sizeof(WEATHER_UI));

    // 构建UI
    Weather_Build_Scr(ext_uc);

    // 启动温度检测定时器（1s检测一次）
    ext_uc->weather_ui->temp_check_timer = lv_timer_create(Temp_Check_Timer, 1000, ext_uc);

    return 0;
}

// 获取实时天气（HTTP协议，方案要求）
int Weather_Get_RealTime(EXT_UI_CTRL_P ext_uc)
{
    char recv_buf[4096] = {0};
    // 发送HTTP请求
    if(Weather_Http_Request(recv_buf, sizeof(recv_buf)) != 0) {
        lv_label_set_text(ext_uc->weather_ui->temp_lab, "温度：获取失败");
        return -1;
    }

    // 截取JSON响应体（跳过HTTP头）
    char *json_str = strstr(recv_buf, "{");
    if(json_str == NULL) {
        lv_label_set_text(ext_uc->weather_ui->temp_lab, "温度：解析失败");
        return -1;
    }

    // 解析并更新UI
    Weather_Parse_Json(json_str, ext_uc);
    return 0;
}

// 释放资源
void Weather_Free(EXT_UI_CTRL_P ext_uc)
{
    lv_timer_del(ext_uc->weather_ui->temp_check_timer);
    lv_obj_del(ext_uc->weather_ui->weather_scr);
    free(ext_uc->weather_ui);
    ext_uc->weather_ui = NULL;
}