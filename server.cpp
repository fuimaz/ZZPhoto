#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include <event2/event.h>
#include <event2/bufferevent.h>

#define LISTEN_BACKLOG 1024

static int time_out_count = 0;
static int err_conn_count = 0;
static int err_accept_count = 0;

void do_accept(evutil_socket_t listener, short event, void *arg);
void read_cb(struct bufferevent *bev, void *arg);
void error_cb(struct bufferevent *bev, short event, void *arg);
void write_cb(struct bufferevent *bev, void *arg);

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("参数个数不正确 %d\n", argc);
        return 0;
    }
    
    int port = atoi(argv[1]);
    evutil_socket_t listener;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener > 0);
    evutil_make_listen_socket_reuseable(listener);
    
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);
    
    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("bind");
        return 1;
    }
    
    if (listen(listener, LISTEN_BACKLOG) < 0) {
        perror("listen");
        return 1;
    }
    
    printf ("Listening...\n");
    
    evutil_make_socket_nonblocking(listener);
    
    struct event_base *base = event_base_new();
    assert(base != NULL);
    struct event *listen_event;
    listen_event = event_new(base, listener, EV_READ | EV_PERSIST, do_accept, (void*)base);
    event_add(listen_event, NULL);
    event_base_dispatch(base);
    
    printf("The End.");
    return 0;
}

void do_accept(evutil_socket_t listener, short event, void *arg)
{
    struct event_base *base = (struct event_base *)arg;
    evutil_socket_t fd;
    struct sockaddr_in sin;
    socklen_t slen;
    
    fd = accept(listener, (struct sockaddr *)&sin, &slen);
    evutil_make_socket_nonblocking(fd);
    if (fd < 0) {
        printf("err in accept, err ccode %d, 发生accept错误的个数 %d", errno, ++err_accept_count);
        return;
    }
    
    printf("ACCEPT: fd = %u\n", fd);
    
    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, read_cb, NULL, error_cb, arg);
    bufferevent_enable(bev, EV_READ | EV_WRITE | EV_PERSIST);
}

void read_cb(struct bufferevent *bev, void *arg)
{
#define MAX_LINE    256
    char line[MAX_LINE+1];
    int n;
    evutil_socket_t fd = bufferevent_getfd(bev);
    
    while (n = bufferevent_read(bev, line, MAX_LINE), n > 0) {
        line[n] = '\0';
        bufferevent_write(bev, line, n);
    }
}

void write_cb(struct bufferevent *bev, void *arg) {}

void error_cb(struct bufferevent *bev, short event, void *arg)
{
    evutil_socket_t fd = bufferevent_getfd(bev);
    printf("fd = %u, ", fd);
    if (event & BEV_EVENT_TIMEOUT) {
        time_out_count++;
        printf("Timed out, 发生超时连接个数 %d", time_out_count);
        printf(", err code %d\n", errno);
    }
    else if (event & BEV_EVENT_EOF) {
        printf("connection closed\n");
    }
    else if (event & BEV_EVENT_ERROR) {
        err_conn_count++;
        printf("some other error, 连接被异常关闭个数 %d", err_conn_count);
        printf(", err code %d\n", errno);
    }
    bufferevent_free(bev);
}
