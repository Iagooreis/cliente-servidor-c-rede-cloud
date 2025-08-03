#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define TAM_BUFFER 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <host> <porta> <diretorio>\n", argv[0]);
        return 1;
    }

    char *host      = argv[1];
    char *porta_str = argv[2];
    char *diretorio = argv[3];

    // 1) Resolver endereço (IPv4 ou IPv6) com getaddrinfo
    struct addrinfo hints, *res, *p;
    int sock = -1, rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;    // AF_INET ou AF_INET6
    hints.ai_socktype = SOCK_STREAM;  // TCP

    if ((rv = getaddrinfo(host, porta_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // 2) Tentar conectar em cada addrinfo até conseguir
    for (p = res; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;  // falhou, tenta próxima

        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
            // conectado com sucesso
            break;
        }
        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);

    if (sock < 0) {
        fprintf(stderr, "Não foi possível conectar a %s:%s\n", host, porta_str);
        return 1;
    }

    printf("Conectado a %s:%s\n", host, porta_str);

    // 3) Enviar “READY”
    if (send(sock, "READY", 5, 0) != 5) {
        perror("send READY");
        close(sock);
        return 1;
    }
    printf("Enviado: READY\n");

    // 4) Receber “READY ACK”
    char ack[TAM_BUFFER];
    ssize_t n = read(sock, ack, TAM_BUFFER);
    if (n <= 0) {
        perror("read ACK");
        close(sock);
        return 1;
    }
    ack[n] = '\0';
    if (strcmp(ack, "READY ACK") != 0) {
        fprintf(stderr, "ACK inesperado: %s\n", ack);
        close(sock);
        return 1;
    }
    printf("Recebido: %s\n", ack);

    // -------------------------------------------------------------
    // 5) Benchmark: enviar mensagens de tamanho 2^i (i = 0..16)
    printf("\n--- Iniciando benchmark de tamanhos 2^i bytes ---\n");
    for (int i = 0; i <= 16; i++) {
        int T = 1 << i;                // tamanho exato em bytes
        char *buffer = malloc(T);
        if (!buffer) {
            perror("malloc");
            close(sock);
            exit(1);
        }
        memset(buffer, 'A', T);        // preencher com qualquer byte

        struct timeval ini, fim;
        gettimeofday(&ini, NULL);

        ssize_t enviados = send(sock, buffer, T, 0);
        if (enviados != T) {
            perror("send benchmark");
            free(buffer);
            close(sock);
            exit(1);
        }

        gettimeofday(&fim, NULL);
        double delta = (fim.tv_sec - ini.tv_sec) +
                       (fim.tv_usec - ini.tv_usec) * 1e-6;
        printf("  2^%-2d = %5d bytes  →  Tempo: %.6f s  →  Throughput: %.2f B/s\n",
               i, T, delta, (double)T / delta);

        free(buffer);
        usleep(10000);  // curta pausa para não congestionar demais a fila TCP
    }
    printf("--- Benchmark concluído ---\n\n");
    // -------------------------------------------------------------

    // 6) Iniciar medição de tempo e bytes para o envio da lista de arquivos
    struct timeval ini_list, fim_list;
    gettimeofday(&ini_list, NULL);
    long total_bytes = 0;

    // 7) Enviar nome do diretório + '\n'
    size_t len_dir = strlen(diretorio);
    if (send(sock, diretorio, len_dir, 0) != (ssize_t)len_dir ||
        send(sock, "\n", 1, 0) != 1) {
        perror("send diretorio");
        close(sock);
        return 1;
    }
    total_bytes += len_dir + 1;
    printf("Enviado nome do diretório: %s\n", diretorio);

    // 8) Varrer o diretório local e enviar cada nome + '\n'
    DIR *dir = opendir(diretorio);
    if (!dir) {
        perror("opendir");
        close(sock);
        return 1;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;

        char envio[TAM_BUFFER];
        if (strcmp(ent->d_name, "bye") == 0) {
            // Se existir arquivo “bye”, aplicar byte-stuffing “bye~”
            snprintf(envio, sizeof(envio), "bye~");
        } else {
            snprintf(envio, sizeof(envio), "%s", ent->d_name);
        }
        size_t len = strlen(envio);
        if (send(sock, envio, len, 0) != (ssize_t)len ||
            send(sock, "\n", 1, 0) != 1) {
            perror("send nome arquivo");
            closedir(dir);
            close(sock);
            return 1;
        }
        total_bytes += len + 1;
        printf("Enviado: %s\n", envio);
    }
    closedir(dir);

    // 9) Enviar token de término “bye\n”
    if (send(sock, "bye\n", 4, 0) != 4) {
        perror("send bye");
        close(sock);
        return 1;
    }
    total_bytes += 4;
    printf("Enviado token de término: bye\n");

    // 10) Sinalizar fim de escrita ao servidor
    shutdown(sock, SHUT_WR);

    // 11) Medir tempo final e throughput agregado da lista de arquivos
    gettimeofday(&fim_list, NULL);
    double delta_list = (fim_list.tv_sec - ini_list.tv_sec) +
                        (fim_list.tv_usec - ini_list.tv_usec) * 1e-6;
    printf("\n--- Estatísticas da lista de arquivos ---\n");
    printf("Total de bytes enviados: %ld\n", total_bytes);
    printf("Tempo decorrido: %.6f s\n", delta_list);
    printf("Throughput: %.2f B/s\n", total_bytes / delta_list);

    close(sock);
    return 0;
}
