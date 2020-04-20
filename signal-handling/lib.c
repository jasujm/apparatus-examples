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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

const int REUSEADDR = 1;
const uint16_t PORT = 9999;
const int SOCKET_BACKLOG = 10;

void handle_error(const char* s)
{
    perror(s);
    exit(1);
}

int create_server()
{
    int server_fd;
    struct sockaddr_in addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handle_error("socket");
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

void handle_connection(int socket_fd)
{
    char buf[1024];
    char* buf_end;
    ssize_t len;

    memset(buf, 0, sizeof(buf));
    if ((len = read(socket_fd, buf, sizeof(buf) - 1)) < 0) {
        handle_error("read");
    }
    buf_end = memchr(buf, '\n', len);
    if (buf_end) {
        fprintf(stderr, "Message received: %s", buf);
        if (write(socket_fd, buf, strlen(buf)) != strlen(buf)) {
            handle_error("write");
        }
    }
}
