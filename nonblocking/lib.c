/*
 * Copyright (c) 2020 Jaakko Moisio
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum state
{
    READING,
    WRITING,
    DONE,
};

struct context
{
    int fd;
    enum state state;
    char buf[65536];
    size_t bytes;
    char* buf_end;
};

const int REUSEADDR = 1;
const uint16_t PORT = 9999;
const int SOCKET_BACKLOG = 10;

void handle_error(const char* s)
{
    perror(s);
    exit(1);
}

struct context* create_connection(int socket_fd, short* events_out)
{
    struct context* ctx = (struct context*)malloc(sizeof(struct context));
    if (ctx) {
        ctx->fd = socket_fd;
        ctx->state = READING;
        memset(ctx->buf, 0, sizeof(ctx->buf));
        ctx->bytes = 0;
        ctx->buf_end = NULL;
        *events_out = POLLIN;
    }
    return ctx;
}

int create_server()
{
    int server_fd, flags;
    struct sockaddr_in addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handle_error("socket");
    }

    flags = fcntl(server_fd, F_GETFD, 0);
    if (fcntl(server_fd, F_SETFD, flags | O_NONBLOCK)) {
        handle_error("fcntl");
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &REUSEADDR,
                   sizeof(REUSEADDR)) < 0) {
        handle_error("setsockopt");
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        handle_error("bind");
    }

    if (listen(server_fd, SOCKET_BACKLOG) < 0) {
        handle_error("listen");
    }

    return server_fd;
}

int handle_connection(struct context* ctx, short revents, short* events_out, int* connection_completed)
{
    ssize_t result, max_bytes;

    /* POLLHUP means that the other end has closed the connection (hung up!). No
       need to continue. */
    if (revents & POLLHUP) {
        *connection_completed = 1;
        return 0;
    }

    /* The switch statement jumps to the right position based on the state
     * stored in the context. In a way the handle_connection() function together
     * with the context object holding the state of the connection form a
     * coroutine: When the coroutine needs to wait for an event (the socket
     * becomes readable/writable), it awaits by telling the caller which events
     * are interesting and returning. The caller will call handle_connection()
     * again when the event happens and thanks to the state variable,
     * handle_connection() will know where to pick the execution up! */

    assert(ctx);
    switch (ctx->state) {
    case READING:
        max_bytes = sizeof(ctx->buf) - ctx->bytes - 1;
        /* Because the socket is in nonblocking mode, we may not get everything
         * at once. If we don't receive the linefeed ending the message, just
         * try again later. */
        result = read(ctx->fd, ctx->buf + ctx->bytes, max_bytes);
        if (result < 0) {
            return -1;
        }
        ctx->bytes += result;
        ctx->buf_end = memchr(ctx->buf, '\n', ctx->bytes);
        if (ctx->buf_end) {
            ctx->state = WRITING;
            ctx->bytes = 0;
            *events_out = POLLOUT;
        } else {
            *events_out = POLLIN;
            *connection_completed = 0;
            break;
        }
        // fallthrough
    case WRITING:
        max_bytes = strlen(ctx->buf) - ctx->bytes;
        /* Similarly as with reading, writing may not write all the bytes at
         * once, and we may need to wait for the socket to become readable
         * again. */
        result = write(ctx->fd, ctx->buf + ctx->bytes, max_bytes);
        if (result < 0) {
            return -1;
        }
        ctx->bytes += result;
        if (result == max_bytes) {
            ctx->state = DONE;
        } else {
            *events_out = POLLOUT;
            *connection_completed = 0;
            break;
        }
        // fallthrough
    case DONE:
        *events_out = 0;
        *connection_completed = 1;
    };

    return 0;
}

void destroy_connection(struct context* ctx)
{
    assert(ctx);
    close(ctx->fd);
    free(ctx);
}
