/*
 * cpu_port.c — Host-mock CPU operations.
 *
 * Returns mock values that match the contract of cpu_abstraction.h but
 * without touching any real CPU control registers:
 *   - get_info       -> fills afros_cpu_info_t with mock values
 *                       (core_id=0, cluster_id=0, is_big=true, freq=2400 MHz)
 *   - set_frequency  -> no-op, returns AFROS_SUCCESS
 *                       (the test runner accepts SUCCESS or NOT_SUPPORTED)
 *   - sleep_core     -> no-op, returns SUCCESS (would block on real hw)
 *   - wakeup_core    -> no-op, returns SUCCESS
 *   - migrate_task   -> no-op, returns SUCCESS
 *   - init           -> no-op
 *
 * On a real port (x86_64, arm64), init() executes `cli; lgdt; sti` /
 * sets CPACR / etc. We deliberately do none of that — host-mock targets
 * have no control registers to program.
 */
#include "cpu_abstraction.h"
#include "port_host_mock.h"

static afros_status_t cpu_init_impl(void) {
    /* No-op on host — there are no privileged CPU control registers
     * to program (no GDT/IDT, no CPACR, no mstatus). */
    return AFROS_SUCCESS;
}

static afros_status_t cpu_get_info_impl(uint32_t cpu_id, afros_cpu_info_t *info) {
    if (!info) return AFROS_ERROR_INVALID_PARAM;
    /* The host-mock simulates a single big core. The mock values are
     * the same as port-x86_64::cpu_get_info_impl() so the contract
     * assertion (info->core_id == 0 after get_info(0,...)) holds. */
    info->core_id          = AFROS_HOST_MOCK_CPU_CORE_ID;
    info->cluster_id       = AFROS_HOST_MOCK_CPU_CLUSTER_ID;
    info->is_big           = AFROS_HOST_MOCK_CPU_IS_BIG;
    info->current_freq_mhz = AFROS_HOST_MOCK_CPU_FREQ_MHZ;
    (void)cpu_id; /* host-mock: every cpu_id returns the same mock core */
    return AFROS_SUCCESS;
}

static afros_status_t cpu_set_frequency_impl(uint32_t cpu_id, uint32_t freq_mhz) {
    /* No-op on host — DVFS is meaningless on a CI runner. The test
     * runner accepts both SUCCESS and NOT_SUPPORTED; we return SUCCESS
     * so the "cpu_set_frequency_all_cores" test takes the success path
     * (more useful for host-mock coverage than a skip). */
    (void)cpu_id;
    (void)freq_mhz;
    return AFROS_SUCCESS;
}

static afros_status_t cpu_sleep_core_impl(uint32_t cpu_id) {
    /* No-op — sleep_core on real hardware may halt the core, which is
     * catastrophic in a single-threaded test. Return SUCCESS so the
     * contract "sleep_core returns SUCCESS or NOT_SUPPORTED" holds. */
    (void)cpu_id;
    return AFROS_SUCCESS;
}

static afros_status_t cpu_wakeup_core_impl(uint32_t cpu_id) {
    (void)cpu_id;
    return AFROS_SUCCESS;
}

static afros_status_t cpu_migrate_task_impl(uint32_t from_cpu, uint32_t to_cpu, uint32_t task_id) {
    /* No-op — task migration is a scheduler-level construct; the CPU
     * port just needs to ack it. */
    (void)from_cpu;
    (void)to_cpu;
    (void)task_id;
    return AFROS_SUCCESS;
}

cpu_ops_t arch_cpu_ops = {
    .init          = cpu_init_impl,
    .get_info      = cpu_get_info_impl,
    .set_frequency = cpu_set_frequency_impl,
    .sleep_core    = cpu_sleep_core_impl,
    .wakeup_core   = cpu_wakeup_core_impl,
    .migrate_task  = cpu_migrate_task_impl
};
