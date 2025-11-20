/*
 * connectsock.c – versión moderna y compatible con Ubuntu (WSL)
 */

#define _POSIX_C_SOURCE 200112L     // Habilita getaddrinfo()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

extern int errexit(const char *format, ...);

int connectsock(const char *host, const char *service, const char *transport)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sock, status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;  // IPv4 o IPv6
    hints.ai_socktype = (strcmp(transport, "udp") == 0) ? SOCK_DGRAM : SOCK_STREAM;

    // getaddrinfo reemplaza totalmente a gethostbyname y getservbyname
    status = getaddrinfo(host, service, &hints, &result);
    if (status != 0) {
        errexit("getaddrinfo error: %s\n", gai_strerror(status));
    }

    // Intentar cada dirección hasta conectar
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1)
            continue;

        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            freeaddrinfo(result);
            return sock; // ¡Conectado!
        }

        close(sock);
    }

    freeaddrinfo(result);
    errexit("No se pudo conectar a %s:%s\n", host, service);
    return -1; // no se alcanza
}
