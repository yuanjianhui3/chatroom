#
# Makefile
#
CC_X86 = gcc  # 新增：x86编译器

CC = arm-linux-gcc
LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= ${shell pwd}
CFLAGS ?= -O3 -g0 -I$(LVGL_DIR)/ -I./project2/chat_room -I./project2/common -Wall -Wshadow -Wundef -Wmissing-prototypes -Wno-discarded-qualifiers -Wall -Wextra -Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith -fno-strict-aliasing -Wno-error=cpp -Wuninitialized -Wmaybe-uninitialized -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wclobbered -Wdeprecated -Wempty-body -Wtype-limits -Wstack-usage=2048 -Wno-unused-value -Wno-unused-parameter -Wno-missing-field-initializers -Wuninitialized -Wmaybe-uninitialized -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wtype-limits -Wsizeof-pointer-memaccess -Wno-format-nonliteral -Wpointer-arith -Wno-cast-qual -Wmissing-prototypes -Wunreachable-code -Wno-switch-default -Wreturn-type -Wmultichar -Wno-discarded-qualifiers -Wformat-security -Wno-ignored-qualifiers -Wno-sign-compare

# 20251008新增：解决errno定义问题
CFLAGS += -D_GNU_SOURCE

LDFLAGS ?= -lm -pthread -Wall
BIN = demo


#Collect the files to compile
MAINSRC = ./main.c ./dir_look/dir_look.c

# include $(LVGL_DIR)/lvgl/lvgl.mk
# include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

CSRCS +=$(LVGL_DIR)/mouse_cursor_icon.c 
CSRCS += ./project2/chat_room/chat_room.c  # 新增：聊天室核心逻辑
CSRCS += ./project2/common/chat_adapt.c    # 新增：dir_look适配文件

OBJEXT ?= .o

AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))

MAINOBJ = $(MAINSRC:.c=$(OBJEXT))

SRCS = $(ASRCS) $(CSRCS) $(MAINSRC)
OBJS = $(AOBJS) $(COBJS)

## MAINOBJ -> OBJFILES

all: default

%.o: %.c
	@$(CC)  $(CFLAGS) -c $< -o $@
	@echo "CC $<"
    
default: $(AOBJS) $(COBJS) $(MAINOBJ)
	$(CC) -o $(BIN) $(MAINOBJ) $(AOBJS) $(COBJS) $(LDFLAGS) -llvgl -L.

clean: 
	rm -f $(BIN) $(AOBJS) $(COBJS) $(MAINOBJ) 

# 新增：服务器端单独编译规则（执行make server即可生成服务器程序）
SERVER_BIN = project2/chat_server/chat_server
SERVER_SRC = project2/chat_server/chat_server.c

server:
	$(CC_X86) $(SERVER_SRC) -o $(SERVER_BIN) $(LDFLAGS)
	@echo "Server compiled: $(SERVER_BIN)"