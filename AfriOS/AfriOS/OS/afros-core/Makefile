# Makefile pour AfriOS Core
# Port matériel : arm64 par défaut (aligné sur AFROS_PORT du CMake, voir
# Kernel/ports/README.md). Invoquer avec `make PORT=x86_64` etc. pour changer.

PORT ?= arm64

CC = gcc
CFLAGS = -Wall -Wextra -I./Kernel/hal/include -I./Kernel/afros/include -I./Kernel/drivers -g

# Sources
HAL_SRC = Kernel/hal/src/hal_init.c \
          Kernel/hal/src/device_manager.c \
          Kernel/hal/src/network_manager.c \
          Kernel/hal/src/gpu_manager.c \
          Kernel/hal/src/virtualization.c \
          Kernel/hal/src/io_subsystem.c

# cpu_manager.c/memory_manager.c ont été déplacés vers ports/port-<arch>/ :
# c'est le port sélectionné qui fournit arch_cpu_ops/arch_memory_ops/etc.
PORT_SRC = Kernel/ports/port-$(PORT)/src/cpu_port.c \
           Kernel/ports/port-$(PORT)/src/memory_port.c \
           Kernel/ports/port-$(PORT)/src/interrupt_port.c \
           Kernel/ports/port-$(PORT)/src/timer_port.c \
           Kernel/ports/port-$(PORT)/src/console_port.c \
           Kernel/ports/port-$(PORT)/src/storage_port.c

DRIVERS_SRC = Kernel/drivers/pci/afros_pci.c

# NOTE: la liste ci-dessous a été complétée pour refléter tous les fichiers
# réellement présents sous Kernel/afros/ (big_little*.c, power_aware_sched.c,
# numa_balancer.c, tcp_westwood_africa.c, intermittent_net.c,
# protocol_compression.c, brownout_protection.c, opportunistic_*.c
# n'étaient pas compilés alors qu'ils existaient déjà sur disque).
KERNEL_SRC = Kernel/afros/main.c \
             Kernel/afros/scheduler/task_migration.c \
             Kernel/afros/scheduler/afros_scheduler.c \
             Kernel/afros/scheduler/afros_cfs.c \
             Kernel/afros/scheduler/power_aware.c \
             Kernel/afros/scheduler/power_aware_sched.c \
             Kernel/afros/scheduler/big_little.c \
             Kernel/afros/scheduler/big_little_support.c \
             Kernel/afros/power/solar_aware.c \
             Kernel/afros/power/brownout_protection.c \
             Kernel/afros/power/opportunistic_compute.c \
             Kernel/afros/power/opportunistic_sleep.c \
             Kernel/afros/memory/numa_aware.c \
             Kernel/afros/memory/numa_balancer.c \
             Kernel/afros/memory/adaptive_reclaim.c \
             Kernel/afros/memory/adaptive_swap.c \
             Kernel/afros/memory/compression.c \
             Kernel/afros/fs/afrosfs/file.c \
             Kernel/afros/fs/afrosfs/journal.c \
             Kernel/afros/network/intelligent_routing.c \
             Kernel/afros/network/intermittent_net.c \
             Kernel/afros/network/tcp_acceleration.c \
             Kernel/afros/network/tcp_westwood_africa.c \
             Kernel/afros/network/protocol_optimization.c \
             Kernel/afros/network/protocol_compression.c \
             Kernel/afros/security/secure_boot.c

# Objets
OBJ = $(HAL_SRC:.c=.o) $(PORT_SRC:.c=.o) $(DRIVERS_SRC:.c=.o) $(KERNEL_SRC:.c=.o)

# Binaire final
TARGET = afros_kernel.elf

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: all
	./$(TARGET)

.PHONY: all clean run
