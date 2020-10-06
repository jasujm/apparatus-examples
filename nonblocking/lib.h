#pragma once

struct context;

int create_server();
void handle_error(const char* s);

struct context* create_connection(int socket_fd, short* events_out);
int handle_connection(struct context* ctx, short revents, short* events_out, int* connection_completed);
void destroy_connection(struct context* ctx);
