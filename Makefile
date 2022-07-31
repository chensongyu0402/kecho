KDIR = /lib/modules/$(shell uname -r)/build
TARGET_MODULE = kecho
CFLAGS_user = -std=gnu99 -Wall -Wextra -Werror
CPUID=7

obj-m += $(TARGET_MODULE).o
$(TARGET_MODULE)-objs := \
    kecho_mod.o \
    echo_server.o

obj-m += drop-tcp-socket.o

GIT_HOOKS := .git/hooks/applied
all: $(GIT_HOOKS) bench user-echo-server
	make -C $(KDIR) M=$(PWD) KBUILD_VERBOSE=$(VERBOSE) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

check: all
	@scripts/test.sh

bench: bench.c
	$(CC) -o $@ $(CFLAGS_user) -pthread $<
plot:
	gnuplot scripts/bench.gp

user-echo-server: user-echo-server.c user-echo-server.h
	$(CC) -o $@ $(CFLAGS_user) $<

tsrs: thread-safe-random-string.c
	$(CC) -o $@.o $(CFLAGS_user) -pthread $<

clean:
	make -C $(KDIR) M=$(PWD) clean
	$(RM) user-echo-server bench bench.txt

load: all
	sudo insmod $(TARGET_MODULE).ko

load-bench:
	sudo insmod $(TARGET_MODULE).ko bench=1

unload:
	sudo rmmod $(TARGET_MODULE) || true > /dev/null

telnet:
	telnet 127.0.0.1 12345


exp_mode:
	sudo bash -c "echo 0 > /proc/sys/kernel/randomize_va_space"
	sudo bash -c "echo performance > /sys/devices/system/cpu/cpu$(CPUID)/cpufreq/scaling_governor"
	sudo bash -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"

exp_recover:
	sudo bash -c "echo 2 >  /proc/sys/kernel/randomize_va_space"
	sudo bash -c "echo powersave > /sys/devices/system/cpu/cpu$(CPUID)/cpufreq/scaling_governor"
	sudo bash -c "echo 0 > /sys/devices/system/cpu/intel_pstate/no_turbo"

local-load: 
	$(MAKE) exp_mode
	$(MAKE) unload
	$(MAKE) load-bench

local-test:
	sudo taskset -c $(CPUID) ./bench

local-unload: 
	$(MAKE) unload
	$(MAKE) exp_recover

user-load:
	$(MAKE) user-echo-server
	$(MAKE) exp_mode
	sudo taskset -c $(CPUID) ./user-echo-server