# 编译器
CXX = g++

# 编译器选项
CXXFLAGS = -g -pthread -std=c++11

# 目标文件名
TARGET = main

# 源文件目录
SRC_DIR = src
TEST_DIR = tests
INCLUDE_DIR = include

# 源文件
SRCS = $(wildcard $(SRC_DIR)/*.cpp) \
       $(wildcard $(TEST_DIR)/*.cpp)

# 头文件搜索路径
INCLUDES = -I$(INCLUDE_DIR)

# 目标文件
OBJS = $(SRCS:.cpp=.o)

# 显示编译信息
$(info Sources: $(SRCS))
$(info Objects: $(OBJS))

# 目标规则
all: $(TARGET)

# 链接目标文件
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(CXXFLAGS)
	rm -f $(OBJS)

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 清理规则
clean:
	rm -f $(TARGET) $(OBJS)

# 伪目标
.PHONY: all clean