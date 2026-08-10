/*
 * io_manager.c — Sous-système IO de afros-winbridge.
 *
 * Implémente une queue d'IRP (I/O Request Packets) avec routines de
 * complétion et support d'IO asynchrone. Encapsule les appels read/write
 * POSIX et notifie les completion routines.
 */

#include "../include/wine_compat.h"
#include "../include/pe_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

/* --- Types internes --------------------------------------------------- */

typedef struct _IRP IRP;
typedef void (*IO_COMPLETION_ROUTINE)(IRP *irp, NTSTATUS status, ULONG info);

typedef enum _IRP_KIND {
    IRP_READ,
    IRP_WRITE,
    IRP_FLUSH,
    IRP_CLOSE
} IRP_KIND;

struct _IRP {
    IRP_KIND               kind;
    HANDLE                 handle;
    void                  *buffer;
    ULONG                  length;
    ULONGLONG              offset;
    IO_COMPLETION_ROUTINE  completion;
    void                  *user_ctx;
    NTSTATUS               final_status;
    ULONG                  final_info;
    IRP                   *next;
};

/* --- Queue d'IRP globale --------------------------------------------- */

static IRP            *g_pending_head = NULL;
static IRP            *g_pending_tail = NULL;
static pthread_mutex_t g_irp_lock      = PTHREAD_MUTEX_INITIALIZER;

/* Alloue un nouvel IRP. */
static IRP *irp_alloc(IRP_KIND kind, HANDLE h, void *buf, ULONG len,
                      ULONGLONG off, IO_COMPLETION_ROUTINE cb, void *ctx)
{
    IRP *irp = (IRP *)calloc(1, sizeof(IRP));
    if (!irp) return NULL;
    irp->kind       = kind;
    irp->handle     = h;
    irp->buffer     = buf;
    irp->length     = len;
    irp->offset     = off;
    irp->completion = cb;
    irp->user_ctx   = ctx;
    return irp;
}

/* Exécute un IRP de façon synchrone (mode bloquant). */
static NTSTATUS irp_execute(IRP *irp)
{
    int fd = (int)(LONG_PTR)irp->handle;
    if (irp->kind == IRP_READ) {
        ssize_t n;
        if (irp->offset != (ULONGLONG)-1)
            lseek(fd, (off_t)irp->offset, SEEK_SET);
        n = read(fd, irp->buffer, irp->length);
        irp->final_info = (n < 0) ? 0 : (ULONG)n;
        irp->final_status = (n < 0) ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS;
    } else if (irp->kind == IRP_WRITE) {
        ssize_t n;
        if (irp->offset != (ULONGLONG)-1)
            lseek(fd, (off_t)irp->offset, SEEK_SET);
        n = write(fd, irp->buffer, irp->length);
        irp->final_info = (n < 0) ? 0 : (ULONG)n;
        irp->final_status = (n < 0) ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS;
    } else if (irp->kind == IRP_FLUSH) {
        irp->final_status = (fsync(fd) == 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
    } else if (irp->kind == IRP_CLOSE) {
        irp->final_status = (close(fd) == 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
    }
    return irp->final_status;
}

/* --- API publique ------------------------------------------------------ */

/* Crée un handle fichier encapsulé (wrapper open). */
NTSTATUS IoCreateFile(const char *path, ULONG access, ULONG disposition, HANDLE *out)
{
    int flags = 0;
    int fd;
    if (!path || !out) return STATUS_INVALID_PARAMETER;
    if (access & 0x40000000) flags |= O_RDWR;
    else if (access & 0x80000000) flags |= O_RDONLY;
    else if (access & 0x20000000) flags |= O_WRONLY;
    if (disposition == 4 || disposition == 5) flags |= O_CREAT;
    if (disposition == 2 || disposition == 5) flags |= O_TRUNC;
    fd = open(path, flags, 0644);
    if (fd < 0) return STATUS_OBJECT_NAME_NOT_FOUND;
    *out = (HANDLE)(LONG_PTR)fd;
    return STATUS_SUCCESS;
}

/* Lecture synchrone. */
NTSTATUS IoReadFile(HANDLE h, void *buf, ULONG len, ULONGLONG off, ULONG *done)
{
    IRP *irp;
    NTSTATUS s;
    irp = irp_alloc(IRP_READ, h, buf, len, off, NULL, NULL);
    if (!irp) return STATUS_NO_MEMORY;
    s = irp_execute(irp);
    if (done) *done = irp->final_info;
    free(irp);
    return s;
}

/* Écriture synchrone. */
NTSTATUS IoWriteFile(HANDLE h, const void *buf, ULONG len, ULONGLONG off, ULONG *done)
{
    IRP *irp;
    NTSTATUS s;
    irp = irp_alloc(IRP_WRITE, h, (void *)buf, len, off, NULL, NULL);
    if (!irp) return STATUS_NO_MEMORY;
    s = irp_execute(irp);
    if (done) *done = irp->final_info;
    free(irp);
    return s;
}

/* Met en file d'attente un IRP asynchrone avec callback. */
NTSTATUS IoQueueAsync(IRP_KIND kind, HANDLE h, void *buf, ULONG len,
                      ULONGLONG off, IO_COMPLETION_ROUTINE cb, void *ctx)
{
    IRP *irp = irp_alloc(kind, h, buf, len, off, cb, ctx);
    if (!irp) return STATUS_NO_MEMORY;
    pthread_mutex_lock(&g_irp_lock);
    if (g_pending_tail) g_pending_tail->next = irp;
    else                g_pending_head = irp;
    g_pending_tail = irp;
    pthread_mutex_unlock(&g_irp_lock);
    return STATUS_PENDING;
}

/* Traite la queue d'IRP et appelle les completion routines. */
DWORD IoCompleteRequest(void)
{
    DWORD processed = 0;
    IRP *irp;
    pthread_mutex_lock(&g_irp_lock);
    irp = g_pending_head;
    g_pending_head = g_pending_tail = NULL;
    pthread_mutex_unlock(&g_irp_lock);
    while (irp) {
        IRP *next = irp->next;
        irp_execute(irp);
        if (irp->completion)
            irp->completion(irp, irp->final_status, irp->final_info);
        free(irp);
        processed++;
        irp = next;
    }
    return processed;
}
