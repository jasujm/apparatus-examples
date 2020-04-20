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

/*
 * This implementation intentionally contains bad practices in signal
 * handling. The author doe not recommend using it in an actual
 * application.
 */

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int create_server();
void handle_connection(int socket_fd);
void handle_error(const char* s);

int signal_received = 0;

void handle_signal(int signum)
{
    signal_received = signum;
}

int main()
{
    int server_fd, socket_fd;
    struct pollfd pollfds[1];

    signal(SIGTERM, &handle_signal);

    server_fd = create_server();
    pollfds[0].fd = server_fd;
    pollfds[0].events = POLLIN;

    /* Check if a signal was received. */
    while (!signal_received) {

        /* But what if we receive signal here? */

        /* Poll the incoming events. This may be interrupted by a signal. */
        pollfds[0].revents = 0;
        if (poll(pollfds, 1, -1) < 0 && errno != EINTR) {
            handle_error("poll");
        }

        /* Handle an incoming connection. */
        if (pollfds[0].revents & POLLIN) {
            if ((socket_fd = accept(server_fd, NULL, NULL)) < 0) {
                handle_error("accept");
            }
            handle_connection(socket_fd);
            close(socket_fd);
        }
    }

    fprintf(stderr, "Exiting via %s\n", strsignal(signal_received));
    close(server_fd);
    return 0;
}
