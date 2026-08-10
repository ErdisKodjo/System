# Makefile for AfriOS Core x86_64 Bare-Metal

PORT ?= x86_64
CC = gcc
LD = ld
CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -fno-builtin-printf -fno-builtin-memcpy -fno-builtin-memset
CFLAGS += -mno-red-zone -mno-mmx -mno-sse -mno-sse2
CFLAGS += -Wall -Wextra -I./Kernel/hal/include -I./Kernel/afros/include -I./Kernel/drivers -I./Kernel/ports/port-x86_64/include -g -mcmodel=large
LDFLAGS = -nostdlib -T ./Kernel/hal/scripts/linker.ld

HAL_SRC = Kernel/hal/src/hal_init.c Kernel/hal/src/device_manager.c Kernel/hal/src/network_manager.c Kernel/hal/src/gpu_manager.c Kernel/hal/src/virtualization.c Kernel/hal/src/io_subsystem.c

PORT_SRC = Kernel/ports/port-$(PORT)/src/cpu_port.c Kernel/ports/port-$(PORT)/src/memory_port.c Kernel/ports/port-$(PORT)/src/interrupt_port.c Kernel/ports/port-$(PORT)/src/timer_port.c Kernel/ports/port-$(PORT)/src/console_port.c Kernel/ports/port-$(PORT)/src/storage_port.c

PORT_ASM = Kernel/ports/port-$(PORT)/src/boot.S

DRIVERS_SRC = Kernel/drivers/pci/afros_pci.c

KERNEL_SRC = Kernel/afros/main.c

C_OBJ = $(HAL_SRC:.c=.o) $(PORT_SRC:.c=.o) $(DRIVERS_SRC:.c=.o) $(KERNEL_SRC:.c=.o)
ASM_OBJ = $(PORT_ASM:.S=.o)

OBJ = $(C_OBJ) $(ASM_OBJ)

TARGET = afros_kernel.bin
TARGET_ELF = afros_kernel.elf

all: $(TARGET_ELF) $(TARGET)
	@echo "Build complete"

$(TARGET_ELF): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

$(TARGET): $(TARGET_ELF)
	objcopy -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) $(TARGET_ELF)

run-qemu: $(TARGET_ELF)
	qemu-system-x86_64 -kernel $(TARGET_ELF) -nographic -serial stdio

.PHONY: all clean run-qemu
