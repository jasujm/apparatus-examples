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

#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include "lib.h"

static const int SERVER_FD = 0;
static const int SIGNAL_FD = 1;
static const int FIRST_CONNECTION = 2;
static const int MAX_CONNECTIONS = 10;
static const int POLLFDS = FIRST_CONNECTION + MAX_CONNECTIONS;

int main()
{
    sigset_t sigset;
    struct signalfd_siginfo siginfo;
    int server_fd, socket_fd, signal_fd, i, total_connections, connection_completed, flags;
    short revents, events_out;
    struct pollfd pollfds[MAX_CONNECTIONS + 2];
    struct context* connection;
    struct context* connections[MAX_CONNECTIONS];

    /* Setting up signalfd to read signals is explained in:
     * https://www.jmoisio.eu/en/blog/2020/04/20/handling-signals-correctly-in-a-linux-application/ */

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGTERM);
    sigprocmask(SIG_SETMASK, &sigset, NULL);

    server_fd = create_server();
    pollfds[SERVER_FD].fd = server_fd;
    pollfds[SERVER_FD].events = POLLIN;

    signal_fd = signalfd(-1, &sigset, 0);
    pollfds[SIGNAL_FD].fd = signal_fd;
    pollfds[SIGNAL_FD].events = POLLIN;

    /* Setting up the connection objects. Initially there are no connections so
     * add dummy entries to the polling list. */

    for (i = FIRST_CONNECTION; i < POLLFDS; ++i) {
        pollfds[i].fd = -1;
        pollfds[i].events = 0;
    }
    memset(connections, 0, sizeof(connections));
    total_connections = 0;

    while (1) {
        for (i = 0; i < POLLFDS; ++i) {
            pollfds[i].revents = 0;
        }
        if (poll(pollfds, POLLFDS, -1) < 0) {
            handle_error("poll");
        }

        /* Handle an incoming connection. */
        if (pollfds[0].revents & POLLERR) {
            handle_error("server failure");
        } else if (pollfds[0].revents & POLLIN) {
            /* Accept an incoming connection. Immediately set nonblocking mode. */
            if ((socket_fd = accept(server_fd, NULL, NULL)) < 0) {
                handle_error("accept");
            }
            flags = fcntl(socket_fd, F_GETFD, 0);
            if (fcntl(socket_fd, F_SETFD, flags | O_NONBLOCK)) {
                handle_error("fcntl");
            }
            /* Create context for the connection. Setup pollfds for the socket
             * just created. */
            for (i = 0; i < MAX_CONNECTIONS; ++i) {
                if (!connections[i]) {
                    connection = create_connection(socket_fd, &events_out);
                    if (!connection) {
                        handle_error("create_connection");
                    }
                    connections[i] = connection;
                    pollfds[FIRST_CONNECTION + i].fd = socket_fd;
                    pollfds[FIRST_CONNECTION + i].events = events_out;
                    ++total_connections;
                    assert(total_connections <= MAX_CONNECTIONS);
                    break;
                }
            }
            /* If we reached the maximum number of concurrent connections,
             * remove the server socket from the polling list. The negation is a
             * neat trick explained in the poll() man page:
             * https://man7.org/linux/man-pages/man2/poll.2.html */
            if (total_connections == MAX_CONNECTIONS) {
                assert(pollfds[SERVER_FD].fd > 0);
                pollfds[SERVER_FD].fd = -pollfds[SERVER_FD].fd;
            }
        }

        /* Check if a signal was received. If it was, read the signal info and
         * break away from the event loop. */
        if (pollfds[1].revents & POLLERR) {
            handle_error("signal_fd failure");
        } else if (pollfds[1].revents & POLLIN) {
            if (read(signal_fd, &siginfo, sizeof(siginfo)) != sizeof(siginfo)) {
                handle_error("read siginfo");
            }
            break;
        }

        /* Handle connections. */
        for (i = 0; i < MAX_CONNECTIONS; ++i) {
            revents = pollfds[i + FIRST_CONNECTION].revents;
            if (revents & POLLERR) {
                handle_error("socket failure");
            } else if (revents) {
                connection = connections[i];
                events_out = 0;
                connection_completed = 0;
                /* For each connection that has events, we call
                 * handle_connection() with the context object, and in the next
                 * round listen to the events returned by the handler -- a sort
                 * of coroutine! */
                if (handle_connection(connection, revents, &events_out, &connection_completed)) {
                    handle_error("handle_connection");
                }
                pollfds[i + FIRST_CONNECTION].events = events_out;
                /* If a connection was completed, free the context object. If
                 * the number of connections was capped, the server is now free
                 * to serve more clients, so add the server socket back to the
                 * polling list. */
                if (connection_completed) {
                    destroy_connection(connection);
                    connections[i] = NULL;
                    pollfds[i + FIRST_CONNECTION].fd = -1;
                    if (total_connections == MAX_CONNECTIONS) {
                        assert(pollfds[SERVER_FD].fd < 0);
                        pollfds[SERVER_FD].fd = -pollfds[SERVER_FD].fd;
                    }
                    --total_connections;
                    assert(total_connections >= 0);
                }
            }
        }
    }

    /* We're done! Just clean up and exit. */
    fprintf(stderr, "Exiting via %s\n", strsignal(siginfo.ssi_signo));
    close(signal_fd);
    close(server_fd);
    for (i = 0; i < MAX_CONNECTIONS; ++i) {
        if (connections[i]) {
            destroy_connection(connections[i]);
        }
    }
    return 0;
}
