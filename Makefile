# Makefile
obj-m += ads131m08.o
ads131m08-objs := ads131m08_main.o ads131m08_iface.o ads131m08_gpio.o \
                  ads131m08_spi.o ads131m08_data.o list.o
#queue.o 
#list.o 

#EXTRA_CFLAGS += -fno-pic
#EXTRA_CFLAGS += -DCONFIG_MODVERSIONS

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
