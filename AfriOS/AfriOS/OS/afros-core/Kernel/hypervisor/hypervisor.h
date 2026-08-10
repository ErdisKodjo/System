/**
 * @file hypervisor.h
 * @brief Hyperviseur de type 2 pour AfriOS - Virtualisation légère
 */

#ifndef AFROS_HYPERVISOR_H
#define AFROS_HYPERVISOR_H

#include <stdint.h>
#include <stdbool.h>
#include "../include/afros_types.h"

// Modes de virtualisation
typedef enum {
    VIRT_MODE_NATIVE = 0,      // Pas de virtualisation
    VIRT_MODE_HARDWARE,        // VT-x/AMD-V
    VIRT_MODE_SOFTWARE,        // Binary translation
    VIRT_MODE_PARAVIRTUALIZED  // Para-virtualisation
} virt_mode_t;

// Types de VM
typedef enum {
    VM_TYPE_LINUX = 0,
    VM_TYPE_WINDOWS,
    VM_TYPE_ANDROID,
    VM_TYPE_CUSTOM
} vm_type_t;

// Configuration d'une VM
typedef struct {
    char name[64];
    vm_type_t type;
    uint32_t vcpu_count;
    uint64_t memory_size_mb;
    char disk_image[256];
    char cdrom_image[256];
    bool network_enabled;
    bool usb_passthrough;
    bool gpu_passthrough;
    uint32_t network_mode;  // 0=NAT, 1=Bridged, 2=Host-only
    char mac_address[18];
} vm_config_t;

// État d'une VM
typedef enum {
    VM_STATE_STOPPED = 0,
    VM_STATE_RUNNING,
    VM_STATE_PAUSED,
    VM_STATE_SAVED,
    VM_STATE_ERROR
} vm_state_t;

// Structure d'une machine virtuelle
typedef struct {
    uint32_t id;
    vm_config_t config;
    vm_state_t state;
    void* vcpus;           // Table des vCPUs
    void* memory;          // Mémoire guest
    uint64_t memory_size;
    void* devices;         // Périphériques virtuels
    void* vmcs;            // VMCS (Intel) ou VMCB (AMD)
    uint64_t guest_regs[16];  // Registres guest
    uint64_t exit_reason;
    uint64_t exit_qualification;
    bool is_initialized;
} virtual_machine_t;

// Périphériques virtuels supportés
typedef enum {
    VDEV_VIRTIO_NET = 0,
    VDEV_VIRTIO_BLK,
    VDEV_VIRTIO_CONSOLE,
    VDEV_VIRTIO_GPU,
    VDEV_E1000,
    VDEV_RTL8139,
    VDEV_AHCI,
    VDEV_NVME,
    VDEV_USB_XHCI,
    VDEV_SOUND_HDA
} vdev_type_t;

// Structure d'un périphérique virtuel
typedef struct {
    vdev_type_t type;
    char name[32];
    uint32_t irq;
    uint64_t mmio_base;
    uint32_t mmio_size;
    void* private_data;
    bool is_enabled;
} vdev_t;

// Callbacks pour les sorties VM
typedef struct {
    afros_status_t (*on_io_exit)(virtual_machine_t* vm, uint16_t port, bool is_in, uint32_t* value);
    afros_status_t (*on_mmio_read)(virtual_machine_t* vm, uint64_t addr, uint32_t size, uint64_t* value);
    afros_status_t (*on_mmio_write)(virtual_machine_t* vm, uint64_t addr, uint32_t size, uint64_t value);
    afros_status_t (*on_cpuid)(virtual_machine_t* vm, uint32_t leaf, uint32_t subleaf, 
                               uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx);
    afros_status_t (*on_msr_read)(virtual_machine_t* vm, uint32_t msr, uint64_t* value);
    afros_status_t (*on_msr_write)(virtual_machine_t* vm, uint32_t msr, uint64_t value);
    afros_status_t (*on_interrupt)(virtual_machine_t* vm, uint8_t vector);
} vmexit_handler_t;

// Fonctions principales de l'hyperviseur
afros_status_t hypervisor_init(void);
bool hypervisor_is_supported(void);
virt_mode_t hypervisor_get_mode(void);

// Gestion des VM
afros_status_t vm_create(vm_config_t* config, virtual_machine_t** vm);
afros_status_t vm_destroy(virtual_machine_t* vm);
afros_status_t vm_start(virtual_machine_t* vm);
afros_status_t vm_stop(virtual_machine_t* vm);
afros_status_t vm_pause(virtual_machine_t* vm);
afros_status_t vm_resume(virtual_machine_t* vm);
afros_status_t vm_reset(virtual_machine_t* vm);
afros_status_t vm_save_state(virtual_machine_t* vm, const char* filepath);
afros_status_t vm_load_state(virtual_machine_t* vm, const char* filepath);

// Contrôle des vCPUs
afros_status_t vm_add_vcpu(virtual_machine_t* vm);
afros_status_t vm_remove_vcpu(virtual_machine_t* vm, uint32_t vcpu_id);
afros_status_t vm_set_vcpu_regs(virtual_machine_t* vm, uint32_t vcpu_id, uint64_t* regs);
afros_status_t vm_get_vcpu_regs(virtual_machine_t* vm, uint32_t vcpu_id, uint64_t* regs);

// Gestion de la mémoire
afros_status_t vm_allocate_memory(virtual_machine_t* vm, uint64_t size_mb);
afros_status_t vm_free_memory(virtual_machine_t* vm);
afros_status_t vm_map_memory(virtual_machine_t* vm, uint64_t gpa, uint64_t hpa, uint64_t size);
afros_status_t vm_unmap_memory(virtual_machine_t* vm, uint64_t gpa);

// Gestion des périphériques
afros_status_t vm_add_device(virtual_machine_t* vm, vdev_t* device);
afros_status_t vm_remove_device(virtual_machine_t* vm, vdev_type_t type);
afros_status_t vm_enable_passthrough(virtual_machine_t* vm, uint16_t vendor_id, uint16_t device_id);
afros_status_t vm_disable_passthrough(virtual_machine_t* vm, uint16_t vendor_id, uint16_t device_id);

// Gestion des interruptions
afros_status_t vm_inject_interrupt(virtual_machine_t* vm, uint8_t vector);
afros_status_t vm_eoi(virtual_machine_t* vm, uint8_t vector);

// Performance et monitoring
afros_status_t vm_get_stats(virtual_machine_t* vm, void* stats_buf);
afros_status_t vm_set_priority(virtual_machine_t* vm, uint32_t priority);

// Support spécifique Intel VT-x
#ifdef __INTEL_COMPILER
afros_status_t vmx_init(void);
afros_status_t vmx_setup_vmcs(virtual_machine_t* vm);
afros_status_t vmx_launch_vm(virtual_machine_t* vm);
afros_status_t vmx_handle_exit(virtual_machine_t* vm);
#endif

// Support spécifique AMD-V
#ifdef __AMD__
afros_status_t svm_init(void);
afros_status_t svm_setup_vmcb(virtual_machine_t* vm);
afros_status_t svm_launch_vm(virtual_machine_t* vm);
afros_status_t svm_handle_exit(virtual_machine_t* vm);
#endif

#endif // AFROS_HYPERVISOR_H
