//
// Created by sligh on 2026-01-09.
//

#ifndef LOG_H
#define LOG_H

#define ERRBUF_SIZE 300

#include "spdlog/spdlog.h"

#ifdef _WIN32

void print_errno() {
    char errbuf[ERRBUF_SIZE];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, WSAGetLastError(), 0, errbuf, sizeof(errbuf),
                  NULL);
    spdlog::error("{}", errbuf);
}

#else

#include <errno.h>
#include <string.h>

void print_errno() {
    spdlog::error("{}", strerror(errno));
}

#endif

#endif // LOG_H
