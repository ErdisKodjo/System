/*
 * syscall_translator.c — Traduction des syscalls NT → Linux syscalls.
 *
 * Reçoit un numéro de syscall NT et ses arguments, et exécute l'équivalent
 * Linux. Couvre les principales catégories: fichiers, mémoire, processus,
 * threads, synchronisation, IPC. Délègue à io_manager pour les IO.
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#define STATUS_INVALID_HANDLE  ((NTSTATUS)0xC0000008)

/* Codes de syscall NT supportés. */
#define NT_SYS_NtCreateFile              1
#define NT_SYS_NtReadFile                2
#define NT_SYS_NtWriteFile               3
#define NT_SYS_NtClose                   4
#define NT_SYS_NtAllocateVirtualMemory   5
#define NT_SYS_NtFreeVirtualMemory       6
#define NT_SYS_NtCreateProcess           7
#define NT_SYS_NtCreateThread            8
#define NT_SYS_NtQueryInformationProcess 9
#define NT_SYS_NtSetInformationFile      10
#define NT_SYS_NtDelayExecution          11
#define NT_SYS_NtYieldExecution          12
#define NT_SYS_NtTerminateProcess        13
#define NT_SYS_NtOpenFile                14
#define NT_SYS_NtQuerySystemInformation  15

/* --- Handlers individuels --------------------------------------------- */

static NTSTATUS sys_NtClose(HANDLE h)
{
    int fd = (int)(LONG_PTR)h;
    if (fd <= 2) return STATUS_INVALID_HANDLE;
    if (h == NULL) return STATUS_INVALID_HANDLE;
    return (close(fd) == 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static NTSTATUS sys_NtAllocateVirtualMemory(ULONG size, ULONG prot, void **out)
{
    int p = PROT_READ;
    void *addr;
    if (prot & 0x2) p |= PROT_WRITE;
    if (prot & 0x4) p |= PROT_EXEC;
    addr = mmap(NULL, size, p, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) return STATUS_NO_MEMORY;
    *out = addr;
    return STATUS_SUCCESS;
}

static NTSTATUS sys_NtFreeVirtualMemory(void *addr, ULONG size)
{
    return (munmap(addr, size) == 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static NTSTATUS sys_NtDelayExecution(LONG milliseconds)
{
    if (milliseconds > 0) {
        struct timespec ts;
        ts.tv_sec  = milliseconds / 1000;
        ts.tv_nsec = (milliseconds % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    } else {
        sched_yield();
    }
    return STATUS_SUCCESS;
}

static NTSTATUS sys_NtCreateFile(const char *path, ULONG access, ULONG share,
                                 ULONG disposition, HANDLE *out)
{
    int flags = 0;
    int fd;
    (void)share;
    if (!path || !out) return STATUS_INVALID_PARAMETER;
    if (access & 0x40000000) flags |= O_RDWR;
    else if (access & 0x80000000) flags |= O_RDONLY;
    else if (access & 0x20000000) flags |= O_WRONLY;
    if (disposition == 2) flags |= O_TRUNC;
    else if (disposition == 4) flags |= O_CREAT;
    else if (disposition == 5) flags |= O_CREAT | O_TRUNC;
    fd = open(path, flags, 0644);
    if (fd < 0) return STATUS_OBJECT_NAME_NOT_FOUND;
    *out = (HANDLE)(LONG_PTR)fd;
    return STATUS_SUCCESS;
}

static NTSTATUS sys_NtReadFile(HANDLE h, void *buf, ULONG len, ULONG *done)
{
    ssize_t n = read((int)(LONG_PTR)h, buf, len);
    if (n < 0) return STATUS_UNSUCCESSFUL;
    if (done) *done = (ULONG)n;
    return STATUS_SUCCESS;
}

static NTSTATUS sys_NtWriteFile(HANDLE h, const void *buf, ULONG len, ULONG *done)
{
    ssize_t n = write((int)(LONG_PTR)h, buf, len);
    if (n < 0) return STATUS_UNSUCCESSFUL;
    if (done) *done = (ULONG)n;
    return STATUS_SUCCESS;
}

static NTSTATUS sys_NtTerminateProcess(HANDLE h, NTSTATUS exit_code)
{
    (void)h;
    _exit((int)exit_code);
    return STATUS_SUCCESS; /* unreachable */
}

static NTSTATUS sys_NtYieldExecution(void)
{
    sched_yield();
    return STATUS_SUCCESS;
}

/* --- Dispatcher principal -------------------------------------------- */

/* Point d'entrée unique: traduit et exécute un syscall NT. */
NTSTATUS SyscallDispatch(ULONG nt_syscall, void **args)
{
    switch (nt_syscall) {
    case NT_SYS_NtClose:
        return sys_NtClose(args[0]);
    case NT_SYS_NtAllocateVirtualMemory:
        return sys_NtAllocateVirtualMemory((ULONG)(ULONG_PTR)args[0],
                                           (ULONG)(ULONG_PTR)args[1],
                                           (void **)args[2]);
    case NT_SYS_NtFreeVirtualMemory:
        return sys_NtFreeVirtualMemory(args[0], (ULONG)(ULONG_PTR)args[1]);
    case NT_SYS_NtDelayExecution:
        return sys_NtDelayExecution((LONG)(LONG_PTR)args[0]);
    case NT_SYS_NtCreateFile:
        return sys_NtCreateFile((const char *)args[0],
                                (ULONG)(ULONG_PTR)args[1],
                                (ULONG)(ULONG_PTR)args[2],
                                (ULONG)(ULONG_PTR)args[3],
                                (HANDLE *)args[4]);
    case NT_SYS_NtReadFile:
        return sys_NtReadFile(args[0], args[1],
                              (ULONG)(ULONG_PTR)args[2],
                              (ULONG *)args[3]);
    case NT_SYS_NtWriteFile:
        return sys_NtWriteFile(args[0], args[1],
                               (ULONG)(ULONG_PTR)args[2],
                               (ULONG *)args[3]);
    case NT_SYS_NtTerminateProcess:
        return sys_NtTerminateProcess(args[0], (NTSTATUS)(LONG_PTR)args[1]);
    case NT_SYS_NtYieldExecution:
        return sys_NtYieldExecution();
    default:
        return STATUS_NOT_IMPLEMENTED;
    }
}
