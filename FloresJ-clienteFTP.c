/* ===========================================================================
   Implementa comandos RFC 959:
   USER, PASS, PASV, PORT, RETR, STOR, MKD, PWD, DELE, REST

   Usa los archivos:
   - connectsock.c
   - connectTCP.c
   - errexit.c

   Concurrencia: cada transferencia (RETR/STOR) se ejecuta en un proceso hijo.
   ============================================================================ */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

int connectTCP(const char *host, const char *service); /* archivo connectTCP.c */
int errexit(const char *format, ...);                  /* archivo errexit.c */

#define BUFSIZE 8192

/* ============================================================================
   Función que lee UNA línea del servidor FTP (del socket de control)
   ============================================================================ */
ssize_t read_line(int fd, char *buf, size_t max) {
    size_t i = 0;
    while (i + 1 < max) {
        ssize_t r = read(fd, buf + i, 1);
        if (r == 1) {
            if (buf[i] == '\n') { i++; break; }
            i++;
        } else if (r == 0) { break; }
        else if (errno != EINTR) return -1;
    }
    buf[i] = '\0';
    return i;
}

/* ============================================================================
   Respuesta completa del servidor
   ============================================================================ */
int get_reply(int ctrl_fd, char *out, size_t max) {
    out[0] = '\0';
    char line[1024];
    while (1) {
        ssize_t n = read_line(ctrl_fd, line, sizeof line);
        if (n <= 0) return -1;
        strncat(out, line, max - strlen(out) - 1);

        if (strlen(line) >= 4 &&
            isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2]) &&
            line[3] == ' ')
            break;
    }
    return 0;
}

/* ============================================================================
   send_cmd: envía un comando FTP formateado y recibe la respuesta
   ============================================================================ */
int send_cmd(int ctrl_fd, char *reply_buf, size_t reply_max, const char *fmt, ...) {
    char cmd[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);

    strcat(cmd, "\r\n");

    write(ctrl_fd, cmd, strlen(cmd));

    if (reply_buf)
        if (get_reply(ctrl_fd, reply_buf, reply_max) < 0) return -1;

    return 0;
}

/* ============================================================================
   PASV: Extraer IP y puerto desde respuesta tipo:
   227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
   ============================================================================ */
int parse_pasv(const char *reply, char *ip, size_t ipmax, int *port) {
    const char *p = strchr(reply, '(');
    if (!p) return -1;
    int h1,h2,h3,h4,p1,p2;
    if (sscanf(p+1, "%d,%d,%d,%d,%d,%d", &h1,&h2,&h3,&h4,&p1,&p2) != 6)
        return -1;
    snprintf(ip, ipmax, "%d.%d.%d.%d", h1,h2,h3,h4);
    *port = p1*256 + p2;
    return 0;
}

/* ============================================================================
   Abrir socket para modo activo ("PORT")
   ============================================================================ */
int open_listen_any(int *out_port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr*)&addr, sizeof addr);
    listen(s, 1);

    socklen_t len = sizeof addr;
    getsockname(s, (struct sockaddr*)&addr, &len);
    *out_port = ntohs(addr.sin_port);

    return s;
}

/* Construye h1,h2,h3,h4,p1,p2 para comando PORT */
void build_port_arg(const char *ip, int port, char *out, size_t max) {
    int h1,h2,h3,h4;
    sscanf(ip, "%d.%d.%d.%d", &h1,&h2,&h3,&h4);
    int p1 = port / 256;
    int p2 = port % 256;
    snprintf(out, max, "%d,%d,%d,%d,%d,%d", h1,h2,h3,h4,p1,p2);
}

int accept_data(int listen_fd) {
    struct sockaddr_storage sa;
    socklen_t len = sizeof sa;
    return accept(listen_fd, (struct sockaddr*)&sa, &len);
}

/* PASV: conectar al servidor en modo pasivo (host:port) */
int connect_pasv(const char *host, int port) {
    char sport[16];
    sprintf(sport, "%d", port);
    return connectTCP(host, sport);
}

/* ============================================================================
   RETR: descarga archivo en un proceso hijo (CONCURRENCIA)
   ============================================================================ */
int do_retr(int ctrl_fd, const char *filename, int use_pasv,
            const char *pasv_host, int pasv_port, int listen_fd) {

    pid_t pid = fork();
    if (pid == 0) {
        char rep[2048];

        /* TYPE I: modo binario obligatorio para archivos */
        send_cmd(ctrl_fd, rep, sizeof rep, "TYPE I");

        int datafd;
        if (use_pasv) {
            datafd = connect_pasv(pasv_host, pasv_port);
            send_cmd(ctrl_fd, rep, sizeof rep, "RETR %s", filename);
        } else {
            send_cmd(ctrl_fd, rep, sizeof rep, "RETR %s", filename);
            datafd = accept_data(listen_fd);
        }

        int outfd = open(filename, O_CREAT|O_TRUNC|O_WRONLY, 0644);

        char buf[BUFSIZE];
        ssize_t r;
        while ((r = read(datafd, buf, BUFSIZE)) > 0)
            write(outfd, buf, r);

        close(outfd);
        close(datafd);
        exit(0);
    }
    return 0;
}

/* ============================================================================
   STOR: sube archivo (CONCURRENTE)
   ============================================================================ */
int do_stor(int ctrl_fd, const char *filename, int use_pasv,
            const char *pasv_host, int pasv_port, int listen_fd) {

    pid_t pid = fork();
    if (pid == 0) {
        char rep[2048];
        send_cmd(ctrl_fd, rep, sizeof rep, "TYPE I");

        int datafd;
        if (use_pasv) {
            datafd = connect_pasv(pasv_host, pasv_port);
            send_cmd(ctrl_fd, rep, sizeof rep, "STOR %s", filename);
        } else {
            send_cmd(ctrl_fd, rep, sizeof rep, "STOR %s", filename);
            datafd = accept_data(listen_fd);
        }

        int infd = open(filename, O_RDONLY);

        char buf[BUFSIZE];
        ssize_t r;
        while ((r = read(infd, buf, BUFSIZE)) > 0)
            write(datafd, buf, r);

        close(infd);
        close(datafd);
        exit(0);
    }
    return 0;
}

/* ============================================================================
   MAIN — INTERFAZ INTERACTIVA
   ============================================================================ */
int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Uso: %s <servidor> <puerto>\n", argv[0]);
        exit(1);
    }

    /* ============================ 
       USER conecta al servidor FTP
       ============================ */
    int ctrl = connectTCP(argv[1], argv[2]);
    if (ctrl < 0) errexit("No puedo conectar al servidor");

    char reply[4096];
    get_reply(ctrl, reply, sizeof reply);
    printf("%s", reply);

    char line[512], cmd[64], arg[256];
    int last_pasv = 0;
    char pasv_host[64];
    int pasv_port;

    int listen_fd = -1;
    int listen_port;
    char local_ip[64];

    while (1) {
        printf("ftp> ");
        if (!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\r\n")] = 0;

        arg[0] = '\0';
        sscanf(line, "%s %255[^\n]", cmd, arg);
        for (char *c = cmd; *c; ++c) *c = toupper(*c);

        /* ============================================================================
           USER — Enviar nombre de usuario al servidor FTP
           ============================================================================ */
        if (!strcmp(cmd, "USER")) {
            send_cmd(ctrl, reply, sizeof reply, "USER %s", arg);
            printf("%s", reply);

        /* ============================================================================
           PASS — Enviar contraseña del usuario
           ============================================================================ */
        } else if (!strcmp(cmd, "PASS")) {
            send_cmd(ctrl, reply, sizeof reply, "PASS %s", arg);
            printf("%s", reply);

        /* ============================================================================
           MKD — Crear directorio en servidor FTP
           ============================================================================ */
        } else if (!strcmp(cmd, "MKD")) {
            send_cmd(ctrl, reply, sizeof reply, "MKD %s", arg);
            printf("%s", reply);

        /* ============================================================================
           PWD — Mostrar directorio actual en el servidor
           ============================================================================ */
        } else if (!strcmp(cmd, "PWD")) {
            send_cmd(ctrl, reply, sizeof reply, "PWD");
            printf("%s", reply);

        /* ============================================================================
           DELE — Borrar archivo del servidor
           ============================================================================ */
        } else if (!strcmp(cmd, "DELE")) {
            send_cmd(ctrl, reply, sizeof reply, "DELE %s", arg);
            printf("%s", reply);

        /* ============================================================================
           REST — Reiniciar transferencia a partir de cierto byte
           ============================================================================ */
        } else if (!strcmp(cmd, "REST")) {
            send_cmd(ctrl, reply, sizeof reply, "REST %s", arg);
            printf("%s", reply);

        /* ============================================================================
           PASV — Solicita al servidor un puerto de datos pasivo
           ============================================================================ */
        } else if (!strcmp(cmd, "PASV")) {
            send_cmd(ctrl, reply, sizeof reply, "PASV");
            printf("%s", reply);
            parse_pasv(reply, pasv_host, sizeof pasv_host, &pasv_port);
            last_pasv = 1;

        /* ============================================================================
           PORT — Activa modo activo, donde el cliente abre un puerto para recibir datos
           ============================================================================ */
        } else if (!strcmp(cmd, "PORT")) {

            if (listen_fd > 0) close(listen_fd);

            listen_fd = open_listen_any(&listen_port);

            struct sockaddr_in loc;
            socklen_t ln = sizeof loc;
            getsockname(ctrl, (struct sockaddr*)&loc, &ln);
            inet_ntop(AF_INET, &loc.sin_addr, local_ip, sizeof local_ip);

            char portarg[128];
            build_port_arg(local_ip, listen_port, portarg, sizeof portarg);

            send_cmd(ctrl, reply, sizeof reply, "PORT %s", portarg);
            printf("%s", reply);

            last_pasv = 0;

        /* ============================================================================
           RETR — Descargar un archivo del servidor
           ============================================================================ */
        } else if (!strcmp(cmd, "RETR")) {
            if (last_pasv)
                do_retr(ctrl, arg, 1, pasv_host, pasv_port, -1);
            else
                do_retr(ctrl, arg, 0, NULL, 0, listen_fd);

            printf("Descarga iniciada en proceso hijo.\n");

        /* ============================================================================
           STOR — Subir un archivo al servidor
           ============================================================================ */
        } else if (!strcmp(cmd, "STOR")) {
            if (last_pasv)
                do_stor(ctrl, arg, 1, pasv_host, pasv_port, -1);
            else
                do_stor(ctrl, arg, 0, NULL, 0, listen_fd);

            printf("Subida iniciada en proceso hijo.\n");

        /* ============================================================================
           QUIT — Terminar sesión
           ============================================================================ */
        } else if (!strcmp(cmd, "QUIT")) {
            send_cmd(ctrl, reply, sizeof reply, "QUIT");
            printf("%s", reply);
            break;

        } else {
            printf("Comando no soportado.\n");
        }

        while (waitpid(-1, NULL, WNOHANG) > 0);
    }

    close(ctrl);
    if (listen_fd > 0) close(listen_fd);
    return 0;
}
