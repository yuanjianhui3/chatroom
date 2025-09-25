//@file smart_home.c 智能家居模块

#include "smart_home.h"
#include "../common/iot_mqtt.h"
#include "../cJSON/cJSON.h"
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

// 烟雾报警阈值（方案要求）
#define SMOKE_ALARM_THRESHOLD 80  // ppm
// 蜂鸣器驱动IOCTL命令（按gec6818_beep.ko定义）
#define BEEP_IOCTL_ON  0
#define BEEP_IOCTL_OFF 1

// LED控制回调（按方案要求：按钮控制亮灭）
static void LED_Ctrl_Cb(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    SMART_HOME_UI_P ui = ext_uc->home_ui;

    // 切换LED状态
    ui->led_on = !ui->led_on;
    Smart_Home_LED_Ctrl(ext_uc, ui->led_on);

    // 更新UI显示
    lv_label_set_text(ui->led_state_lab, ui->led_on ? "LED状态：亮" : "LED状态：灭");

    // 上报LED状态到华为云（方案要求：设备数据上传）
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "cmd", 5);          // 5=设备状态上报
    cJSON_AddStringToObject(root, "device", "led");
    cJSON_AddBoolToObject(root, "state", ui->led_on);
    char *json_str = cJSON_PrintUnformatted(root);
    Iot_Mqtt_Publish(IOT_MQTT_TOPIC_UP, json_str, strlen(json_str));
    cJSON_Delete(root);
    free(json_str);
}

// 远程呼叫回调（按方案要求：一键发送短信）
static void Remote_Call_Cb(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    if(Smart_Home_Remote_Call(ext_uc) == 0) {
        lv_msgbox_create(NULL, "提示", "呼叫短信已发送！", NULL, true);
    } else {
        lv_msgbox_create(NULL, "提示", "呼叫失败！", NULL, true);
    }
}

// 返回首页回调
static void Home_Back_Cb(lv_event_t *e)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)lv_event_get_user_data(e);
    SMART_HOME_UI_P ui = ext_uc->home_ui;
    Ext_Scr_Switch(ui->home_scr, ext_uc->base_uc->start_ui_p->start_ui, LV_DIR_TOP);
}

// 构建智能家居UI
static void Smart_Home_Build_Scr(EXT_UI_CTRL_P ext_uc)
{
    SMART_HOME_UI_P ui = ext_uc->home_ui;
    LV_FONT_DECLARE(lv_font_simsun_20);

    ui->home_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui->home_scr, lv_color_hex(0x27AE60), LV_STATE_DEFAULT);

    // LED控制区（方案要求）
    lv_obj_t *led_title = lv_label_create(ui->home_scr);
    lv_obj_set_pos(led_title, 100, 80);
    lv_obj_set_style_text_color(led_title, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(led_title, &lv_font_simsun_20, LV_STATE_DEFAULT);
    lv_label_set_text(led_title, "LED照明控制");

    ui->led_btn = lv_btn_create(ui->home_scr);
    lv_obj_set_size(ui->led_btn, 100, 35);
    lv_obj_set_pos(ui->led_btn, 100, 120);
    lv_obj_t *led_btn_lab = lv_label_create(ui->led_btn);
    lv_label_set_text(led_btn_lab, "打开LED");
    lv_obj_center(led_btn_lab);
    lv_obj_add_event_cb(ui->led_btn, LED_Ctrl_Cb, LV_EVENT_SHORT_CLICKED, ext_uc);

    ui->led_state_lab = lv_label_create(ui->home_scr);
    lv_obj_set_pos(ui->led_state_lab, 220, 125);
    lv_obj_set_style_text_color(ui->led_state_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->led_state_lab, &lv_font_simsun_20, LV_STATE_DEFAULT);
    lv_label_set_text(ui->led_state_lab, "LED状态：灭");

    // 烟雾报警区（方案要求）
    lv_obj_t *smoke_title = lv_label_create(ui->home_scr);
    lv_obj_set_pos(smoke_title, 100, 200);
    lv_obj_set_style_text_color(smoke_title, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(smoke_title, &lv_font_simsun_20, LV_STATE_DEFAULT);
    lv_label_set_text(smoke_title, "烟雾报警");

    ui->smoke_lab = lv_label_create(ui->home_scr);
    lv_obj_set_pos(100, 240);
    lv_obj_set_style_text_color(ui->smoke_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->smoke_lab, &lv_font_simsun_20, LV_STATE_DEFAULT);
    lv_label_set_text(ui->smoke_lab, "烟雾浓度：0 ppm");

    ui->alarm_state_lab = lv_label_create(ui->home_scr);
    lv_obj_set_pos(250, 240);
    lv_obj_set_style_text_color(ui->alarm_state_lab, lv_color_hex(0xFFFFFF), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->alarm_state_lab, &lv_font_simsun_20, LV_STATE_DEFAULT);
    lv_label_set_text(ui->alarm_state_lab, "报警状态：正常");

    // 远程呼叫区（方案要求）
    ui->call_btn = lv_btn_create(ui->home_scr);
    lv_obj_set_size(ui->call_btn, 120, 40);
    lv_obj_set_pos(100, 320);
    lv_obj_t *call_lab = lv_label_create(ui->call_btn);
    lv_label_set_text(call_lab, "远程呼叫");
    lv_obj_center(call_lab);
    lv_obj_add_event_cb(ui->call_btn, Remote_Call_Cb, LV_EVENT_SHORT_CLICKED, ext_uc);

    // 返回首页按钮
    ui->back_btn = lv_btn_create(ui->home_scr);
    lv_obj_set_size(ui->back_btn, 80, 30);
    lv_obj_set_pos(30, 420);
    lv_obj_t *back_lab = lv_label_create(ui->back_btn);
    lv_label_set_text(back_lab, "返回首页");
    lv_obj_center(back_lab);
    lv_obj_add_event_cb(ui->back_btn, Home_Back_Cb, LV_EVENT_SHORT_CLICKED, ext_uc);
}

// 模拟烟雾浓度检测（线程，按方案要求：数据模拟）
void *Smart_Home_Smoke_Detect(void *arg)
{
    EXT_UI_CTRL_P ext_uc = (EXT_UI_CTRL_P)arg;
    SMART_HOME_UI_P ui = ext_uc->home_ui;
    srand(time(NULL));

    while(ext_uc->mqtt_running) {
        // 模拟烟雾浓度（0-120 ppm，方案要求）
        int smoke_val = rand() % 120;
        char smoke_buf[32];
        sprintf(smoke_buf, "烟雾浓度：%d ppm", smoke_val);
        lv_label_set_text(ui->smoke_lab, smoke_buf);

        // 更新跨模块烟雾状态
        Ext_State_Update(ext_uc, "smoke", smoke_val);

        // 烟雾超标→报警（按方案要求：蜂鸣器+短信通知）
        if(smoke_val >= SMOKE_ALARM_THRESHOLD) {
            // 蜂鸣器报警
            ioctl(ui->beep_fd, BEEP_IOCTL_ON, 1);
            // 更新报警状态UI
            lv_label_set_text(ui->alarm_state_lab, "报警状态：异常");
            // 上报报警信息到华为云（方案要求）
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "cmd", 6);          // 6=报警上报
            cJSON_AddStringToObject(root, "device", "smoke");
            cJSON_AddNumberToObject(root, "value", smoke_val);
            cJSON_AddBoolToObject(root, "alarm", true);
            char *json_str = cJSON_PrintUnformatted(root);
            Iot_Mqtt_Publish(IOT_MQTT_TOPIC_UP, json_str, strlen(json_str));
            cJSON_Delete(root);
            free(json_str);
            // 自动发送短信通知（方案要求）
            Smart_Home_Remote_Call(ext_uc);
        } else {
            // 正常→关闭蜂鸣器
            ioctl(ui->beep_fd, BEEP_IOCTL_OFF, 1);
            lv_label_set_text(ui->alarm_state_lab, "报警状态：正常");
            // 上报正常状态（方案要求）
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "cmd", 6);
            cJSON_AddStringToObject(root, "device", "smoke");
            cJSON_AddNumberToObject(root, "value", smoke_val);
            cJSON_AddBoolToObject(root, "alarm", false);
            char *json_str = cJSON_PrintUnformatted(root);
            Iot_Mqtt_Publish(IOT_MQTT_TOPIC_UP, json_str, strlen(json_str));
            cJSON_Delete(root);
            free(json_str);
        }

        sleep(2); // 2s更新一次
    }
    return NULL;
}

// LED控制（操作驱动文件，参考led_drv.c、led_test.c）
void Smart_Home_LED_Ctrl(EXT_UI_CTRL_P ext_uc, bool on)
{
    SMART_HOME_UI_P ui = ext_uc->home_ui;
    char led_buf[2] = {0, 7}; // 第一个字节：状态（1=亮，0=灭）；第二个字节：LED编号（D7）
    led_buf[0] = on ? 1 : 0;
    write(ui->led_fd, led_buf, sizeof(led_buf));
}

// 远程呼叫（HTTP发送短信，使用华为云SMS服务，方案要求）
int Smart_Home_Remote_Call(EXT_UI_CTRL_P ext_uc)
{
    // 华为云SMS API请求（需提前开通服务，替换参数）
    char http_req[1024];
    sprintf(http_req, 
        "POST /sms/batchSendSms HTTP/1.1\r\n"
        "Host: sms.cn-north-4.myhuaweicloud.com\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Authorization: YOUR_AUTH_TOKEN\r\n"
        "Content-Length: %d\r\n\r\n"
        "endpoint=YOUR_ENDPOINT&phone=13800138000&templateId=TEMPLATE_ID&templateParas=[\"设备呼叫\"]",
        strlen("endpoint=YOUR_ENDPOINT&phone=13800138000&templateId=TEMPLATE_ID&templateParas=[\"设备呼叫\"]")
    );

    // 原生socket发送HTTP请求（同天气预报模块逻辑）
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) return -1;

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(80);
    inet_pton(AF_INET, "100.125.0.1", &serv_addr.sin_addr); // 华为云SMS地址

    int ret = -1;
    if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
        send(sockfd, http_req, strlen(http_req), 0);
        char recv_buf[512] = {0};
        recv(sockfd, recv_buf, sizeof(recv_buf), 0);
        // 简单判断响应是否成功
        if(strstr(recv_buf, "200 OK") != NULL) ret = 0;
    }

    close(sockfd);
    return ret;
}

// 初始化智能家居
int Smart_Home_Init(EXT_UI_CTRL_P ext_uc)
{
    ext_uc->home_ui = (SMART_HOME_UI_P)malloc(sizeof(SMART_HOME_UI));
    memset(ext_uc->home_ui, 0, sizeof(SMART_HOME_UI));
    ext_uc->home_ui->led_on = false;

    // 打开LED/蜂鸣器驱动（参考led_test.c、beep_test.c）
    ext_uc->home_ui->led_fd = open("/dev/led_drv", O_RDWR);
    ext_uc->home_ui->beep_fd = open("/dev/beep", O_RDWR);
    if(ext_uc->home_ui->led_fd < 0 || ext_uc->home_ui->beep_fd < 0) {
        printf("请先安装驱动：insmod led_drv.ko; insmod gec6818_beep.ko\n");
        return -1;
    }

    // 构建UI
    Smart_Home_Build_Scr(ext_uc);

    // 启动烟雾检测线程
    if(pthread_create(&ext_uc->home_ui->smoke_thread, NULL, Smart_Home_Smoke_Detect, ext_uc) != 0) {
        printf("烟雾检测线程创建失败\n");
        return -1;
    }
    pthread_detach(ext_uc->home_ui->smoke_thread);

    return 0;
}

// 释放资源
void Smart_Home_Free(EXT_UI_CTRL_P ext_uc)
{
    // 关闭设备驱动
    close(ext_uc->home_ui->led_fd);
    close(ext_uc->home_ui->beep_fd);

    // 等待烟雾线程退出
    pthread_cancel(ext_uc->home_ui->smoke_thread);
    pthread_join(ext_uc->home_ui->smoke_thread, NULL);

    lv_obj_del(ext_uc->home_ui->home_scr);
    free(ext_uc->home_ui);
    ext_uc->home_ui = NULL;
}