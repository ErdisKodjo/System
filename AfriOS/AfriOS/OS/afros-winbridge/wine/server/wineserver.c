/*
 * wineserver.c — Serveur central de afros-winbridge.
 *
 * Le wineserver est un démon Unix qui:
 *   - écoute sur un socket Unix (/var/run/afros-wineserver.sock);
 *   - maintient le registre des clients (processus Win32);
 *   - dispatche les requêtes (registry, handles, sync, etc.);
 *   - gère les signaux (SIGCHLD, SIGTERM, SIGHUP).
 */

#include "../include/wine_compat.h"
#include "../include/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/wait.h>

#define WINESERVER_SOCKET "/var/run/afros-wineserver.sock"
#define MAX_CLIENTS       64

/* --- Type de requête client → serveur -------------------------------- */

typedef enum _REQ_KIND {
    REQ_PING = 0,
    REQ_REGISTRY_OPEN,
    REQ_REGISTRY_QUERY,
    REQ_REGISTRY_SET,
    REQ_PROCESS_CREATE,
    REQ_PROCESS_TERMINATE,
    REQ_HANDLE_ALLOC,
    REQ_HANDLE_CLOSE,
    REQ_SYNC_WAIT,
    REQ_SYNC_SIGNAL,
} REQ_KIND;

typedef struct _REQ_HEADER {
    DWORD    kind;
    DWORD    payload_len;
    ULONG    client_pid;
} REQ_HEADER;

/* --- État global du serveur ------------------------------------------ */

typedef struct _SERVER_STATE {
    int       listen_fd;
    int       client_fds[MAX_CLIENTS];
    int       client_count;
    volatile  sig_atomic_t shutdown;
} SERVER_STATE;

static SERVER_STATE g_state;

/* --- Gestion des signaux --------------------------------------------- */

static void on_sigchld(int sig)
{
    int status;
    pid_t pid;
    (void)sig;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* Reap zombie child. */
    }
}

static void on_sigterm(int sig)
{
    (void)sig;
    g_state.shutdown = 1;
}

/* Installe les handlers de signaux. */
static NTSTATUS install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) != 0) return STATUS_UNSUCCESSFUL;

    sa.sa_handler = on_sigterm;
    if (sigaction(SIGTERM, &sa, NULL) != 0) return STATUS_UNSUCCESSFUL;
    if (sigaction(SIGINT,  &sa, NULL) != 0) return STATUS_UNSUCCESSFUL;
    if (sigaction(SIGHUP,  &sa, NULL) != 0) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

/* --- API publique ------------------------------------------------------ */

/* Initialise le serveur (socket, handlers, hives). */
NTSTATUS WineserverInit(void)
{
    struct sockaddr_un addr;
    memset(&g_state, 0, sizeof(g_state));
    g_state.listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_state.listen_fd < 0) return STATUS_UNSUCCESSFUL;
    unlink(WINESERVER_SOCKET);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, WINESERVER_SOCKET, sizeof(addr.sun_path) - 1);
    if (bind(g_state.listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(g_state.listen_fd);
        return STATUS_UNSUCCESSFUL;
    }
    if (listen(g_state.listen_fd, MAX_CLIENTS) != 0) {
        close(g_state.listen_fd);
        return STATUS_UNSUCCESSFUL;
    }
    install_signal_handlers();
    /* Charge tous les hives. */
    HiveManagerInit();
    SystemHiveInit();
    SoftwareHiveInit();
    SamHiveInit();
    return STATUS_SUCCESS;
}

/* Dispatche une requête d'un client. */
static NTSTATUS dispatch_request(int client_fd, const REQ_HEADER *hdr,
                                 const void *payload)
{
    (void)payload;
    switch (hdr->kind) {
    case REQ_PING: {
        const char *resp = "PONG";
        write(client_fd, resp, 5);
        return STATUS_SUCCESS;
    }
    case REQ_REGISTRY_OPEN:
    case REQ_REGISTRY_QUERY:
    case REQ_REGISTRY_SET:
    case REQ_PROCESS_CREATE:
    case REQ_PROCESS_TERMINATE:
    case REQ_HANDLE_ALLOC:
    case REQ_HANDLE_CLOSE:
    case REQ_SYNC_WAIT:
    case REQ_SYNC_SIGNAL:
        /* Stubs: le vrai travail est fait par les modules spécialisés. */
        return STATUS_NOT_IMPLEMENTED;
    default:
        return STATUS_INVALID_PARAMETER;
    }
}

/* Boucle principale: accept() + select() + dispatch. */
NTSTATUS WineserverMain(void)
{
    fd_set rfds;
    int i, max_fd;
    while (!g_state.shutdown) {
        FD_ZERO(&rfds);
        FD_SET(g_state.listen_fd, &rfds);
        max_fd = g_state.listen_fd;
        for (i = 0; i < g_state.client_count; i++) {
            FD_SET(g_state.client_fds[i], &rfds);
            if (g_state.client_fds[i] > max_fd)
                max_fd = g_state.client_fds[i];
        }
        if (select(max_fd + 1, &rfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            return STATUS_UNSUCCESSFUL;
        }
        if (FD_ISSET(g_state.listen_fd, &rfds)) {
            int c = accept(g_state.listen_fd, NULL, NULL);
            if (c >= 0 && g_state.client_count < MAX_CLIENTS)
                g_state.client_fds[g_state.client_count++] = c;
            else if (c >= 0) close(c);
        }
        for (i = 0; i < g_state.client_count; i++) {
            int fd = g_state.client_fds[i];
            if (FD_ISSET(fd, &rfds)) {
                REQ_HEADER hdr;
                char buf[1024];
                if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) continue;
                if (hdr.payload_len > 0 && hdr.payload_len < sizeof(buf))
                    read(fd, buf, hdr.payload_len);
                dispatch_request(fd, &hdr, buf);
            }
        }
    }
    /* Cleanup. */
    for (i = 0; i < g_state.client_count; i++) close(g_state.client_fds[i]);
    close(g_state.listen_fd);
    unlink(WINESERVER_SOCKET);
    return STATUS_SUCCESS;
}
