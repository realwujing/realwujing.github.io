# Makefile
obj-m += my_seq_file.o

# 指定内核源码路径
KDIR := /lib/modules/$(shell uname -r)/build

# 默认目标
all:
	make -C $(KDIR) M=$(PWD) modules

# 清理目标
clean:
	make -C $(KDIR) M=$(PWD) clean