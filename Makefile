# ָ����Ŀ¼
export DISTRIB_ROOT=$(PWD)

# ʹ��ʲô����make
MAKE:=make

# αĿ��
.PHONY: all clean

# make -c ����˼�ǵ�ĳ��Ŀ¼ȥ��ִ��make֮�󷵻�
all:
	$(MAKE) -C Common
	$(MAKE) -C Core

clean:
	$(MAKE) -C Common clean
	$(MAKE) -C Core clean


