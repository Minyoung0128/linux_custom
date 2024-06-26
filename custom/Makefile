NAME = cls

obj-m += ${NAME}.o

all:
	make -C /lib/modules/6.9.5.min0/build M=$(PWD) modules
clean:

	make -C /lib/modules/6.9.5.min0/build M=$(PWD) clean
	rm -f Module.symvers modules.order
	rm -f ${NAME}.o ${NAME}.mod  ${NAME}.mod.c  ${NAME}.mod.o

fclean: clean
	rm -f  ${NAME}.ko

re: fclean all
