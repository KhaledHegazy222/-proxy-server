#include <stdio.h>
#include "csapp.h"
#include "client.h"
#include "cache.h"
/* You won't lose style points for including this long line in your code */
// static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void *thread(void *argv);

int main(int argc, char **argv)
{

    init_cache();
    if (argc != 2)
    {
        Rio_writen(STDERR_FILENO, "Invalid Arguments\nExpected: ./proxy <port>\n", 44);
        return 1;
    }
    char *port = argv[1];
    int listen_fd;
    listen_fd = open_listenfd(port);
    Rio_writen(STDOUT_FILENO, "Proxy is ready to receive requests\n", 36);

    while (1)
    {
        socklen_t addr_len = sizeof(struct sockaddr);
        struct sockaddr addr;
        int conn_fd = Accept(listen_fd, &addr, &addr_len);
        int *conn_fd_ptr = Malloc(sizeof(int));
        *conn_fd_ptr = conn_fd;
        pthread_t tid;
        pthread_create(&tid, NULL, thread, conn_fd_ptr);
    }

    close(listen_fd);

    return 0;
}

void *thread(void *argv)
{
    Pthread_detach(pthread_self());
    int client_fd = *((int *)argv);
    handle_client(client_fd);
    close(client_fd);
    free(argv);
    return NULL;
}