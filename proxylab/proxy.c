#include "csapp.h"
#include "sbuf.h"
#include "cache.h"

/* Number of threads and size of shared buffer */
#define NTHREADS 4
#define SBUFSIZE 16

typedef struct {
    char host[MAXLINE];
    char port[MAXLINE];
    char path[MAXLINE];
} uri_t;

void doit(int fd);

void parse_uri(char *uri, uri_t *uri_info);

void build_requesthdrs(rio_t *rio_client, uri_t *uri_info, char *requesthdrs);

void *thread(void *vargp);

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

void sigpipe_handler(int sig);

sbuf_t sbuf;    /* Shared buffer of connected descriptors */

cache_t cache;  /* Cache of web objects */

int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    Signal(SIGPIPE, sigpipe_handler);
    listenfd = Open_listenfd(argv[1]);

    cache_init(&cache);

    sbuf_init(&sbuf, SBUFSIZE);
    for (int i = 0; i < NTHREADS; i++)  /* Create worker threads */
        Pthread_create(&tid, NULL, thread, NULL);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
    }
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char requesthdrs[MAXLINE];
    rio_t rio_client, rio_server;
    uri_t *uri_info;
    int serverfd;
    size_t n;
    block_t *block;
    char key[MAXLINE];
    char obj[MAX_OBJECT_SIZE];
    int obj_size;

    /* Read request line and headers */
    Rio_readinitb(&rio_client, fd);
    if (!Rio_readlineb(&rio_client, buf, MAXLINE)) return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }

    strcpy(key, uri);
    if ((block = cache_read(&cache, key))) {
        lock(block);
        Rio_writen(fd, block->val, strlen(block->val));
        unlock(block);
        return;
    }

    uri_info = (uri_t *) Calloc(1, sizeof(uri_t));
    parse_uri(uri, uri_info);
    build_requesthdrs(&rio_client, uri_info, requesthdrs);

    /* Connect to server */
    serverfd = Open_clientfd(uri_info->host, uri_info->port);
    free(uri_info);
    Rio_readinitb(&rio_server, serverfd);
    Rio_writen(serverfd, requesthdrs, strlen(requesthdrs));

    obj_size = 0;
    while ((n = Rio_readlineb(&rio_server, buf, MAXLINE)) != 0) {
        obj_size += n;
        if (obj_size < MAX_OBJECT_SIZE)
            strcat(obj, buf);
        printf("proxy received %d bytes, sending to client\n", (int) n);
        Rio_writen(fd, buf, n);
    }
    Close(serverfd);

    if (obj_size < MAX_OBJECT_SIZE)
        cache_write(&cache, key, obj);
}

/*
 * parse_uri - parse URI to get host, port, and path
 */
void parse_uri(char *uri, uri_t *uri_info) {
    static const char *port_http = "80";
    char *host_ptr;
    char *port_ptr;
    char *path_ptr;

    if ((host_ptr = strstr(uri, "//")) != NULL) {
        if ((port_ptr = strstr(host_ptr + 2, ":")) != NULL) {
            int tmp;
            sscanf(port_ptr + 1, "%d%s", &tmp, uri_info->path);
            sprintf(uri_info->port, "%d", tmp);
            *port_ptr = '\0';
        } else {
            if ((path_ptr = strstr(host_ptr + 2, "/")) != NULL) {
                strcpy(uri_info->port, port_http);
                strcpy(uri_info->path, path_ptr);
                *path_ptr = '\0';
            }
        }
        strcpy(uri_info->host, host_ptr + 2);
    } else {
        strcpy(uri_info->port, port_http);
        path_ptr = strstr(uri, "/");
        strcpy(uri_info->path, path_ptr != NULL ? path_ptr : "/");
    }
}

/*
 * build_requesthdrs - build HTTP request headers
 */
void build_requesthdrs(rio_t *rio_client, uri_t *uri_info, char *requesthdrs) {
    static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
    static const char *conn_hdr = "Connection: close\r\n";
    static const char *proxy_conn_hdr = "Proxy-Connection: close\r\n";
    static const char *req_hdr_fmt = "GET %s HTTP/1.0\r\n";
    static const char *host_hdr_fmt = "Host: %s\r\n";
    static const char *eof_hdr = "\r\n";
    static const char *host_key = "Host";
    static const char *conn_key = "Connection";
    static const char *proxy_conn_key = "Proxy-Connection";
    static const char *user_agent_key = "User-Agent";

    char buf[MAXLINE], req_hdr[MAXLINE], host_hdr[MAXLINE], other_hdrs[MAXLINE];
    sprintf(req_hdr, req_hdr_fmt, uri_info->path);
    /* Parse headers from client */
    while (Rio_readlineb(rio_client, buf, MAXLINE) > 0) {
        if (!strcmp(buf, eof_hdr)) break;

        if (!strncasecmp(buf, host_key, strlen(host_key))) {
            strcpy(host_hdr, buf);
            continue;
        }

        if (strncasecmp(buf, conn_key, strlen(conn_key)) &&
            strncasecmp(buf, proxy_conn_key, strlen(proxy_conn_key)) &&
            strncasecmp(buf, user_agent_key, strlen(user_agent_key))) {
            strcat(other_hdrs, buf);
        }

        if (strlen(host_hdr) == 0) {
            sprintf(host_hdr, host_hdr_fmt, uri_info->host);
        }

        sprintf(requesthdrs,
                "%s%s%s%s%s%s%s",
                req_hdr,
                host_hdr,
                conn_hdr,
                proxy_conn_hdr,
                user_agent_hdr,
                other_hdrs,
                eof_hdr);
    }
}

/*
 * thread - a thread to server the client
 */
void *thread(void *vargp) {
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf);    /* Remove connfd from buffer */
        doit(connfd);                       /* Service client */
        Close(connfd);
    }
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

void sigpipe_handler(int sig) {
    printf("SIGPIPE handled\n");
    return;
}
