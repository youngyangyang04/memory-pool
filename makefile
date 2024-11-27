# 编译器
CXX = g++

# 编译器选项
CXXFLAGS = -g -pthread

# 目标文件名
TARGET = main

# 源文件
SRC = $(wildcard *.cpp)

# 中间目标文件（.o 文件）
OBJ = $(SRC:.cpp=.o)

# 目标规则
all: $(TARGET)

# 链接目标文件
$(TARGET): $(OBJ)
	$(CXX) $^ -o $@ $(CXXFLAGS)
	rm -f $(OBJ) # 删除中间文件

# 编译规则
%.o: %.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS)

# 清理规则
clean:
	rm -f $(TARGET) $(OBJ)

# 伪目标
.PHONY: all clean
