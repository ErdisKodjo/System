/*
 * event_log.c — Windows Event Log pour afros-winbridge.
 *
 * Lit et écrit les événéments dans les fichiers .evt:
 *   /var/log/afros-winbridge/Application.evt
 *   /var/log/afros-winbridge/System.evt
 *   /var/log/afros-winbridge/Security.evt
 *
 * Format binaire minimal: en-tête ELFLOG (32 octets) + records variables.
 */

#include "../include/wine_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

#define EVT_DIR "/var/log/afros-winbridge"

/* --- Types d'événements ---------------------------------------------- */

#define EVENTLOG_ERROR_TYPE       0x0001
#define EVENTLOG_WARNING_TYPE     0x0002
#define EVENTLOG_INFORMATION_TYPE 0x0004
#define EVENTLOG_AUDIT_SUCCESS    0x0008
#define EVENTLOG_AUDIT_FAILURE    0x0010

/* --- En-tête de log --------------------------------------------------- */

#pragma pack(push, 1)
typedef struct _EVT_HEADER {
    DWORD signature;        /* "AFEL" = AfriOS Event Log */
    DWORD version;
    DWORD first_record;
    DWORD last_record;
    DWORD record_count;
    DWORD reserved;
    DWORD header_size;
} EVT_HEADER;

typedef struct _EVT_RECORD {
    DWORD  length;
    DWORD  reserved;
    DWORD  record_number;
    DWORD  time_generated;
    DWORD  time_written;
    DWORD  event_id;
    WORD   event_type;
    WORD   num_strings;
    DWORD  data_size;
    /* Suivi de: source_name (NUL), strings, data */
} EVT_RECORD;
#pragma pack(pop)

#define EVT_SIG 0x4C454641   /* "AFEL" */

/* --- Handle de log ouvert -------------------------------------------- */

typedef struct _EVT_LOG_HANDLE {
    char  path[256];
    FILE *f;
    BOOL  writable;
    DWORD next_record;
} EVT_LOG_HANDLE;

#define MAX_EVT_HANDLES 16
static EVT_LOG_HANDLE g_handles[MAX_EVT_HANDLES];
static pthread_mutex_t g_evt_lock = PTHREAD_MUTEX_INITIALIZER;

/* Construit le chemin complet d'un log. */
static void evt_log_path(char *out, size_t sz, const char *name)
{
    snprintf(out, sz, "%s/%s.evt", EVT_DIR, name);
}

/* --- API publique ------------------------------------------------------ */

/* Ouvre un log par nom ("Application", "System", "Security"). */
HANDLE EvtOpenLog(const char *name, BOOL writable)
{
    EVT_LOG_HANDLE *h = NULL;
    int i;
    char path[256];
    pthread_mutex_lock(&g_evt_lock);
    for (i = 0; i < MAX_EVT_HANDLES; i++) {
        if (!g_handles[i].f) { h = &g_handles[i]; break; }
    }
    if (!h) { pthread_mutex_unlock(&g_evt_lock); return NULL; }
    evt_log_path(path, sizeof(path), name ? name : "Application");
    strncpy(h->path, path, sizeof(h->path) - 1);
    h->writable = writable;
    h->f = fopen(path, writable ? "r+b" : "rb");
    if (!h->f && writable) {
        h->f = fopen(path, "w+b");
        if (h->f) {
            EVT_HEADER hdr = {0};
            hdr.signature = EVT_SIG;
            hdr.version = 1;
            hdr.header_size = sizeof(EVT_HEADER);
            fwrite(&hdr, sizeof(hdr), 1, h->f);
            fflush(h->f);
        }
    }
    if (!h->f) { pthread_mutex_unlock(&g_evt_lock); return NULL; }
    h->next_record = 1;
    pthread_mutex_unlock(&g_evt_lock);
    return (HANDLE)h;
}

/* Écrit un événement dans le log. */
NTSTATUS EvtReportEvent(HANDLE hlog, WORD event_type, DWORD event_id,
                        const char *source, const char *message)
{
    EVT_LOG_HANDLE *h = (EVT_LOG_HANDLE *)hlog;
    EVT_RECORD rec;
    DWORD total_len;
    if (!h || !h->writable) return STATUS_ACCESS_DENIED;
    memset(&rec, 0, sizeof(rec));
    rec.time_generated = (DWORD)time(NULL);
    rec.time_written   = rec.time_generated;
    rec.event_id       = event_id;
    rec.event_type     = event_type;
    rec.num_strings    = 1;
    rec.record_number  = h->next_record++;
    /* Taille = header + source (NUL) + message (NUL). */
    rec.data_size = (DWORD)((message ? strlen(message) : 0) + 1);
    total_len = sizeof(rec) + (DWORD)(source ? strlen(source) + 1 : 1)
                + rec.data_size;
    rec.length = total_len;
    fseek(h->f, 0, SEEK_END);
    fwrite(&rec, sizeof(rec), 1, h->f);
    fwrite(source ? source : "", 1, (source ? strlen(source) : 0) + 1, h->f);
    if (message) fwrite(message, 1, strlen(message) + 1, h->f);
    else { const char e = '\0'; fwrite(&e, 1, 1, h->f); }
    fflush(h->f);
    return STATUS_SUCCESS;
}

/* Lit le prochain événement. */
NTSTATUS EvtReadEvent(HANDLE hlog, EVT_RECORD *out, char *buf, DWORD buf_max)
{
    EVT_LOG_HANDLE *h = (EVT_LOG_HANDLE *)hlog;
    EVT_RECORD rec;
    if (!h || !out) return STATUS_INVALID_PARAMETER;
    if (fread(&rec, sizeof(rec), 1, h->f) != 1)
        return STATUS_END_OF_FILE;
    *out = rec;
    if (rec.length > sizeof(rec) && buf && buf_max > 0) {
        DWORD to_read = rec.length - sizeof(rec);
        if (to_read > buf_max) to_read = buf_max;
        fread(buf, 1, to_read, h->f);
    }
    return STATUS_SUCCESS;
}

/* Ferme un handle de log. */
NTSTATUS EvtCloseLog(HANDLE hlog)
{
    EVT_LOG_HANDLE *h = (EVT_LOG_HANDLE *)hlog;
    if (!h) return STATUS_INVALID_PARAMETER;
    if (h->f) { fclose(h->f); h->f = NULL; }
    return STATUS_SUCCESS;
}
