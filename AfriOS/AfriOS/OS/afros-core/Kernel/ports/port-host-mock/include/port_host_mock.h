#ifndef PORT_HOST_MOCK_H
#define PORT_HOST_MOCK_H

/*
 * port_host_mock.h — Header for the Linux host-mock port.
 *
 * Provides the same arch_*_ops tables (arch_cpu_ops, arch_memory_ops,
 * arch_interrupt_ops, arch_timer_ops, arch_console_ops, arch_storage_ops)
 * as the real ports (port-x86_64, port-arm64, ...) but with userspace-safe
 * implementations backed by the host libc / POSIX. This lets the HAL test
 * runner (afros-core/Kernel/hal/tests/hal_test_runner.c) execute on a
 * standard Linux CI runner without requiring bare-metal or QEMU.
 *
 * Activate by:
 *   - linking the test executable against the afros-port-host-mock object
 *     library (see Kernel/hal/tests/CMakeLists.txt, AFROS_HAL_TEST_HOST_MOCK
 *     option), or
 *   - direct gcc invocation with -DAFROS_HOST_MOCK=1 (see
 *     scripts/run-hal-tests.sh for the fallback path).
 *
 * The macro AFROS_PORT_HOST_MOCK is defined to 1 so any consumer can branch
 * on it (e.g. to skip a bare-metal-only test path).
 */

#define AFROS_PORT_HOST_MOCK 1

/* Path of the backing file used by storage_port.c for read/write/flush.
 * Override with the AFROS_HOST_MOCK_STORAGE_PATH env var at runtime. */
#define AFROS_HOST_MOCK_STORAGE_PATH_DEFAULT "/tmp/afros-host-mock-storage.img"

/* Mock CPU configuration surfaced by cpu_port.c::get_info(). */
#define AFROS_HOST_MOCK_CPU_CORE_ID       0u
#define AFROS_HOST_MOCK_CPU_CLUSTER_ID    0u
#define AFROS_HOST_MOCK_CPU_IS_BIG        1
#define AFROS_HOST_MOCK_CPU_FREQ_MHZ      2400u

/* Mock storage geometry surfaced by storage_port.c::get_info(). */
#define AFROS_HOST_MOCK_STORAGE_BLOCK_SIZE   512u
#define AFROS_HOST_MOCK_STORAGE_BLOCK_COUNT  2048u   /* 1 MiB backing file */
#define AFROS_HOST_MOCK_STORAGE_READ_ONLY    0

/* Mock timer reported by timer_port.c::get_frequency_hz(). */
#define AFROS_HOST_MOCK_TIMER_FREQUENCY_HZ   1000000000u  /* 1 GHz (CLOCK_MONOTONIC ns) */

#endif /* PORT_HOST_MOCK_H */
