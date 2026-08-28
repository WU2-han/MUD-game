# ============================================
#  修仙传奇 MUD - Makefile (C++版)
#  编译器: g++ (MinGW / MSYS2)
# ============================================

CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -I./include -finput-charset=UTF-8 -fexec-charset=UTF-8
LDFLAGS  =

# 源文件
SRC_DIR  = src
MOD_DIR  = modules

SRCS     = $(SRC_DIR)/main.cpp \
           $(SRC_DIR)/player.cpp \
           $(SRC_DIR)/room.cpp \
           $(SRC_DIR)/npc.cpp \
           $(SRC_DIR)/command.cpp \
           $(SRC_DIR)/event.cpp \
           $(SRC_DIR)/module.cpp \
           $(SRC_DIR)/save.cpp \
           $(MOD_DIR)/cultivate/cultivate.cpp \
           $(MOD_DIR)/world/world.cpp \
           $(MOD_DIR)/bag/bag.cpp \
           $(MOD_DIR)/quest/quest.cpp

OBJS     = $(SRCS:.cpp=.o)
TARGET   = mud.exe

# 默认目标
all: $(TARGET)

# 链接
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^
	@echo "构建完成: $(TARGET)"

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# 运行
run: $(TARGET)
	./$(TARGET)

# 清理
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f $(SRC_DIR)/*.o $(MOD_DIR)/*/*.o

# 清理存档
clean-saves:
	rm -f data/saves/*.sav

# 完全清理
distclean: clean clean-saves

.PHONY: all run clean clean-saves distclean