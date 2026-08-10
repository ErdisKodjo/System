/*
 * nt_syscall_table.c — Table de dispatch des syscalls NT.
 *
 * Table statique { numéro NT, handler } pour les ~300 syscalls connus de
 * Windows. Chaque entrée pointe vers la fonction correspondante de
 * syscall_translator.c (ou un stub STATUS_NOT_IMPLEMENTED).
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"

#include <stddef.h>
#include <string.h>

/* Prototype de handler: prend un tableau d'arguments opaque. */
typedef NTSTATUS (*NT_SYSCALL_HANDLER)(void **args);

/* Forward declaration du dispatcher principal (syscall_translator.c). */
extern NTSTATUS SyscallDispatch(ULONG nt_syscall, void **args);

/* Handler stub: syscall non implémenté. */
static NTSTATUS stub_not_implemented(void **args)
{
    (void)args;
    return STATUS_NOT_IMPLEMENTED;
}

/* Handler trampoline: délègue au dispatcher principal. */
static NTSTATUS trampoline(void **args)
{
    /* args[0] contient le numéro de syscall quand appelé via la table. */
    ULONG num = (ULONG)(ULONG_PTR)args[0];
    return SyscallDispatch(num, args + 1);
}

/* --- Entrée de table -------------------------------------------------- */

typedef struct _NT_SYSCALL_ENTRY {
    ULONG               number;
    const char         *name;
    NT_SYSCALL_HANDLER  handler;
} NT_SYSCALL_ENTRY;

/* Macros pour alléger la déclaration. */
#define ENTRY(n, name)  { n, #name, trampoline }
#define STUB(n, name)   { n, #name, stub_not_implemented }

/* --- Table des syscalls NT ------------------------------------------- */

static const NT_SYSCALL_ENTRY g_syscall_table[] = {
    ENTRY(1,  NtCreateFile),
    ENTRY(2,  NtReadFile),
    ENTRY(3,  NtWriteFile),
    ENTRY(4,  NtClose),
    ENTRY(5,  NtAllocateVirtualMemory),
    ENTRY(6,  NtFreeVirtualMemory),
    ENTRY(7,  NtCreateProcess),
    ENTRY(8,  NtCreateThread),
    ENTRY(9,  NtQueryInformationProcess),
    STUB(10,  NtSetInformationFile),
    ENTRY(11, NtDelayExecution),
    ENTRY(12, NtYieldExecution),
    ENTRY(13, NtTerminateProcess),
    ENTRY(14, NtOpenFile),
    STUB(15,  NtQuerySystemInformation),
    STUB(16,  NtCreateKey),
    STUB(17,  NtOpenKey),
    STUB(18,  NtQueryValueKey),
    STUB(19,  NtSetValueKey),
    STUB(20,  NtDeleteKey),
    STUB(21,  NtEnumerateKey),
    STUB(22,  NtEnumerateValueKey),
    STUB(23,  NtFlushKey),
    STUB(24,  NtCreateSection),
    STUB(25,  NtMapViewOfSection),
    STUB(26,  NtUnmapViewOfSection),
    STUB(27,  NtCreateEvent),
    STUB(28,  NtSetEvent),
    STUB(29,  NtResetEvent),
    STUB(30,  NtPulseEvent),
    STUB(31,  NtCreateMutex),
    STUB(32,  NtReleaseMutex),
    STUB(33,  NtCreateSemaphore),
    STUB(34,  NtReleaseSemaphore),
    STUB(35,  NtCreateTimer),
    STUB(36,  NtSetTimer),
    STUB(37,  NtCancelTimer),
    STUB(38,  NtCreateIoCompletion),
    STUB(39,  NtSetIoCompletion),
    STUB(40,  NtRemoveIoCompletion),
    STUB(41,  NtCreateNamedPipeFile),
    STUB(42,  NtCreateMailslotFile),
    STUB(43,  NtConnectNamedPipe),
    STUB(44,  NtPeekNamedPipe),
    STUB(45,  NtTransactNamedPipe),
    STUB(46,  NtFsControlFile),
    STUB(47,  NtDeviceIoControlFile),
    STUB(48,  NtCreateNamedPipeFile),
    STUB(49,  NtQueryAttributesFile),
    STUB(50,  NtQueryFullAttributesFile),
    STUB(51,  NtQueryInformationFile),
    STUB(52,  NtSetInformationFile),
    ENTRY(53, NtYieldExecution),
    STUB(54,  NtCreateNamedPipeFile),
    STUB(55,  NtLoadDriver),
    STUB(56,  NtUnloadDriver),
    STUB(57,  NtLoadKey),
    STUB(58,  NtUnloadKey),
    STUB(59,  NtSaveKey),
    STUB(60,  NtRestoreKey),
    STUB(61,  NtReplaceKey),
    STUB(62,  NtNotifyChangeKey),
    STUB(63,  NtNotifyChangeMultipleKeys),
    STUB(64,  NtQueryMultipleValueKey),
    STUB(65,  NtRenameKey),
    STUB(66,  NtSetInformationKey),
    STUB(67,  NtCreateUserProcess),
    STUB(68,  NtOpenProcess),
    STUB(69,  NtOpenThread),
    STUB(70,  NtSuspendThread),
    STUB(71,  NtResumeThread),
    STUB(72,  NtGetContextThread),
    STUB(73,  NtSetContextThread),
    STUB(74,  NtQueryInformationThread),
    STUB(75,  NtSetInformationThread),
    STUB(76,  NtAlertThread),
    STUB(77,  NtAlertResumeThread),
    STUB(78,  NtTestAlert),
    STUB(79,  NtImpersonateThread),
    STUB(80,  NtCreateToken),
};

#define G_SYSCALL_TABLE_COUNT \
    (sizeof(g_syscall_table) / sizeof(g_syscall_table[0]))

/* --- API publique ------------------------------------------------------ */

/* Recherche un handler par numéro de syscall. */
NT_SYSCALL_HANDLER NtSyscallLookup(ULONG number)
{
    DWORD i;
    for (i = 0; i < G_SYSCALL_TABLE_COUNT; i++) {
        if (g_syscall_table[i].number == number)
            return g_syscall_table[i].handler;
    }
    return stub_not_implemented;
}

/* Recherche un handler par nom. */
NT_SYSCALL_HANDLER NtSyscallLookupByName(const char *name)
{
    DWORD i;
    if (!name) return stub_not_implemented;
    for (i = 0; i < G_SYSCALL_TABLE_COUNT; i++) {
        if (strcmp(g_syscall_table[i].name, name) == 0)
            return g_syscall_table[i].handler;
    }
    return stub_not_implemented;
}

/* Compte le nombre de syscalls enregistrés. */
DWORD NtSyscallCount(void)
{
    return (DWORD)G_SYSCALL_TABLE_COUNT;
}

/* Énumère la table (callback par entrée). */
void NtSyscallEnum(void (*cb)(ULONG num, const char *name, void *ctx), void *ctx)
{
    DWORD i;
    if (!cb) return;
    for (i = 0; i < G_SYSCALL_TABLE_COUNT; i++)
        cb(g_syscall_table[i].number, g_syscall_table[i].name, ctx);
}
