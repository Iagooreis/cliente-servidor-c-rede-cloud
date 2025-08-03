#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define PORTA "8080"
#define TAM_BUFFER 1024
#define MAX_BASE_LEN 200

int main() {
    struct addrinfo hints, *res6 = NULL, *res4 = NULL, *p;
    int servidor_fd = -1;
    int yes = 1;
    int rv;

    // 1) Tentar bind IPv6 primeiro
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORTA, &hints, &res6)) != 0) {
        fprintf(stderr, "getaddrinfo IPv6: %s\n", gai_strerror(rv));
        res6 = NULL;
    }

    if (res6) {
        for (p = res6; p != NULL; p = p->ai_next) {
            servidor_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (servidor_fd < 0) continue;
            setsockopt(servidor_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
            setsockopt(servidor_fd, IPPROTO_IPV6, IPV6_V6ONLY, &(int){0}, sizeof(int));
            if (bind(servidor_fd, p->ai_addr, p->ai_addrlen) < 0) {
                close(servidor_fd);
                servidor_fd = -1;
                continue;
            }
            printf("Bind IPv6 bem-sucedido em :::%s\n", PORTA);
            break;
        }
        freeaddrinfo(res6);
    }

    // 2) Se não conseguiu IPv6, faz fallback para IPv4
    if (servidor_fd < 0) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags    = AI_PASSIVE;

        if ((rv = getaddrinfo(NULL, PORTA, &hints, &res4)) != 0) {
            fprintf(stderr, "getaddrinfo IPv4: %s\n", gai_strerror(rv));
            return 1;
        }

        for (p = res4; p != NULL; p = p->ai_next) {
            servidor_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (servidor_fd < 0) continue;
            setsockopt(servidor_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
            if (bind(servidor_fd, p->ai_addr, p->ai_addrlen) < 0) {
                close(servidor_fd);
                servidor_fd = -1;
                continue;
            }
            printf("Bind IPv4 bem-sucedido em 0.0.0.0:%s\n", PORTA);
            break;
        }
        freeaddrinfo(res4);

        if (servidor_fd < 0) {
            fprintf(stderr, "Erro: não foi possível fazer bind em IPv4 ou IPv6\n");
            return 1;
        }
    }

    // 3) Ouvir no socket obtido
    if (listen(servidor_fd, 3) < 0) {
        perror("listen");
        close(servidor_fd);
        return 1;
    }
    printf("Servidor ouvindo (fallback se necessário) na porta %s...\n", PORTA);

    // 4) Aceitar conexão
    struct sockaddr_storage cliente_addr;
    socklen_t cliente_addrlen = sizeof(cliente_addr);
    int cliente_fd = accept(servidor_fd,
                            (struct sockaddr *)&cliente_addr,
                            &cliente_addrlen);
    if (cliente_fd < 0) {
        perror("accept");
        close(servidor_fd);
        return 1;
    }

    // Mostrar IP/porta do cliente (IPv4 ou IPv6)
    char addrstr[INET6_ADDRSTRLEN];
    if (cliente_addr.ss_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)&cliente_addr;
        inet_ntop(AF_INET, &sin->sin_addr, addrstr, sizeof(addrstr));
    } else {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&cliente_addr;
        inet_ntop(AF_INET6, &sin6->sin6_addr, addrstr, sizeof(addrstr));
    }
    unsigned short cliente_port = (cliente_addr.ss_family == AF_INET)
        ? ntohs(((struct sockaddr_in *)&cliente_addr)->sin_port)
        : ntohs(((struct sockaddr_in6 *)&cliente_addr)->sin6_port);
    printf("Conexão aceita de %s (porta %d)\n", addrstr, cliente_port);

    // 5) Ler “READY”
    char buf[TAM_BUFFER];
    ssize_t n = read(cliente_fd, buf, TAM_BUFFER);
    if (n <= 0) {
        perror("read READY");
        close(cliente_fd);
        close(servidor_fd);
        return 1;
    }
    buf[n] = '\0';
    if (strcmp(buf, "READY") != 0) {
        fprintf(stderr, "Comando inesperado (esperava READY): %s\n", buf);
        close(cliente_fd);
        close(servidor_fd);
        return 1;
    }
    printf("Recebido comando: %s\n", buf);

    // 6) Enviar “READY ACK”
    if (send(cliente_fd, "READY ACK", 9, 0) != 9) {
        perror("send ACK");
        close(cliente_fd);
        close(servidor_fd);
        return 1;
    }
    printf("Enviado: READY ACK\n");

    // 7) ===== DESCARTAR benchmark (131071 bytes de 'A') =====
    {
        const int total_bench = (1 << 17) - 1;  // soma 2^0 + 2^1 + ... + 2^16
        char lixo;
        for (int i = 0; i < total_bench; i++) {
            if (read(cliente_fd, &lixo, 1) <= 0) {
                perror("Erro ao descartar benchmark");
                close(cliente_fd);
                close(servidor_fd);
                return 1;
            }
        }
    }

    // 8) ===== Agora ler apenas até o '\n' para montar “diretório” =====
    int idx = 0;
    char ch;
    char diretorio[TAM_BUFFER];
    while (read(cliente_fd, &ch, 1) == 1) {
        if (ch == '\n') break;
        if (idx < TAM_BUFFER - 1) diretorio[idx++] = ch;
    }
    diretorio[idx] = '\0';
    printf("Diretório informado pelo cliente: %s\n", diretorio);

    // 9) Extrair “base” (parte após última '/')
    char *base = strrchr(diretorio, '/');
    if (base) base++;
    else     base = diretorio;

    // 10) Limitar base a MAX_BASE_LEN
    size_t base_len = strlen(base);
    if (base_len > MAX_BASE_LEN) {
        base += (base_len - MAX_BASE_LEN);
        base_len = MAX_BASE_LEN;
    }

    // 11) Construir dinamicamente "<hostname>-<base>.txt"
    char hostname[HOST_NAME_MAX];
    if (gethostname(hostname, sizeof(hostname)) < 0) {
        perror("gethostname");
        strcpy(hostname, "host");
    }
    size_t needed = strlen(hostname) + 1 /* '-' */
                    + base_len + 4 /* ".txt" */ + 1 /* '\0' */;
    char *nome_saida = malloc(needed);
    if (!nome_saida) {
        perror("malloc nome_saida");
        close(cliente_fd);
        close(servidor_fd);
        return 1;
    }
    snprintf(nome_saida, needed, "%s-%s.txt", hostname, base);

    FILE *saida = fopen(nome_saida, "w");
    if (!saida) {
        perror("fopen");
        free(nome_saida);
        close(cliente_fd);
        close(servidor_fd);
        return 1;
    }

    // 12) Ler cada linha até encontrar “bye”
    idx = 0;
    while (read(cliente_fd, &ch, 1) == 1) {
        if (ch != '\n') {
            if (idx < TAM_BUFFER - 1) buf[idx++] = ch;
        } else {
            buf[idx] = '\0';
            idx = 0;
            if (strcmp(buf, "bye") == 0) break;
            if (strcmp(buf, "bye~") == 0) strcpy(buf, "bye");  // destuffing
            fprintf(saida, "%s\n", buf);
            printf("Gravado: %s\n", buf);
        }
    }

    fclose(saida);
    printf("Arquivo de nomes gravado em '%s'\n", nome_saida);

    free(nome_saida);
    close(cliente_fd);
    close(servidor_fd);
    return 0;
}
