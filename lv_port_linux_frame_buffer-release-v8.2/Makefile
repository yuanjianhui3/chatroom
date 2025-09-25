# Makefile
CC = arm-linux-gcc

# 2. 核心路径定义：LVGL根目录与项目结构对齐
LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= ${shell pwd}
PROJECT2_DIR = $(LVGL_DIR)/project2

# 3. 编译选项（补充新增模块头文件路径，方案模块独立性要求）
CFLAGS ?= -O3 -g0 \
        -I$(LVGL_DIR)/ \
        -I$(LVGL_DIR)/dir_look/ \
        -I$(PROJECT2_DIR)/common/ \
        -I$(PROJECT2_DIR)/chat_room/ \
        -I$(PROJECT2_DIR)/weather/ \
        -I$(PROJECT2_DIR)/smart_home/ \
        -I$(PROJECT2_DIR)/cJSON/ \
        -I$(LVGL_DIR)/lvgl/ \
        -I$(LVGL_DIR)/lv_drivers/ \
        -Wall -Wshadow -Wundef -Wmissing-prototypes -Wno-discarded-qualifiers \
        -Wextra -Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith \
        -fno-strict-aliasing -Wno-error=cpp -Wuninitialized -Wmaybe-uninitialized \
        -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits \
        -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wno-cast-qual \
        -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar \
        -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic \
        -Wno-sign-compare -Wdouble-promotion -Wclobbered -Wdeprecated \
        -Wempty-body -Wstack-usage=2048 -Wno-unused-value

# 4. 链接选项（补充MQTT库，方案IOT平台MQTT通信要求）
LDFLAGS ?= -lm -pthread \
            -L$(LVGL_DIR)/lib/paho-mqtt-c/lib \
            -lpaho-mqtt3c \
            -llvgl -llv_drivers \
            -L$(LVGL_DIR)/

# 5. 目标文件与输出
BIN = demo  # 目标文件名（贴合方案项目名称）

# 6. 源文件收集（纳入原有框架+新增模块，方案功能完整性要求）
# 6.1 原有核心源文件
MAINSRC = $(LVGL_DIR)/main.c $(LVGL_DIR)/dir_look/dir_look.c

# 6.2 新增功能模块源文件（方案2.6所有功能模块）
PROJECT2_SRCS = \
    $(PROJECT2_DIR)/common/ext_ui_adapt.c \
    $(PROJECT2_DIR)/common/iot_mqtt.c \
    $(PROJECT2_DIR)/chat_room/chat_room.c \
    $(PROJECT2_DIR)/weather/weather.c \
    $(PROJECT2_DIR)/smart_home/smart_home.c \
    $(PROJECT2_DIR)/cJSON/cJSON.c

# 6.3 LVGL相关源文件（启用.mk自动收集，避免手动遗漏）
include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

# 6.4 其他辅助源文件
CSRCS += $(LVGL_DIR)/mouse_cursor_icon.c $(PROJECT2_SRCS)

# 7. 目标文件定义
OBJEXT ?= .o
AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))
MAINOBJ = $(MAINSRC:.c=$(OBJEXT))
OBJS = $(AOBJS) $(COBJS)

# 8. 编译规则
all: default

# 单文件编译
%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $<"

# 链接生成可执行文件（整合所有目标文件，命令行前为TAB）
default: $(AOBJS) $(COBJS) $(MAINOBJ)
	$(CC) -o $(BIN) $(MAINOBJ) $(AOBJS) $(COBJS) $(LDFLAGS)

# 清理规则（确保所有中间文件可删除，命令行前为TAB）
clean: 
	rm -f $(BIN) $(AOBJS) $(COBJS) $(MAINOBJ) $(PROJECT2_DIR)/**/*.o