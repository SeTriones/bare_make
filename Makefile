# 指定根目录
export DISTRIB_ROOT=$(PWD)

# 使用什么命令make
MAKE:=make

# 伪目标
.PHONY: all clean

# make -c 的意思是到某个目录去，执行make之后返回
all:
	$(MAKE) -C Common
	$(MAKE) -C Core

clean:
	$(MAKE) -C Common clean
	$(MAKE) -C Core clean


