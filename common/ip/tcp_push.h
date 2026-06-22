/**
 * \file tcp_push.h
 *
 * \brief TCP push notification transport for COSEM data
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef TCP_PUSH_H
#define TCP_PUSH_H

#include <stdint.h>

#ifdef USE_WINDOWS_OS
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define closesocket(s) close(s)
typedef int SOCKET;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tcp_push_handler)(const uint8_t *data, uint32_t len);

int  tcp_push_connect(const char *host, uint16_t port);
int  tcp_push_send(SOCKET sock, const uint8_t *data, uint32_t len);
void tcp_push_close(SOCKET sock);

#ifdef __cplusplus
}
#endif

#endif /* TCP_PUSH_H */
