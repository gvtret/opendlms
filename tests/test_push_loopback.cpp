/**
 * \file test_push_loopback.cpp
 *
 * \brief Catch2 tests for TCP push loopback
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#include <cstring>
#include "catch.hpp"

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define INVALID_SOCKET (-1)
#define closesocket(s) close(s)
typedef int SOCKET;
}

TEST_CASE("Push loopback — socket creation", "[push]")
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(s != INVALID_SOCKET);
    close(s);
}
