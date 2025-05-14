#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <cstddef>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>

#define DIE(assertion, call_description) \
    do { \
        if (assertion) { \
            perror(call_description); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

inline std::vector<std::string> split_topic(const std::string &topic) {
    std::vector<std::string> tokens;
    std::stringstream ss(topic);
    std::string item;
    while (getline(ss, item, '/')) tokens.push_back(item);
    return tokens;
}

inline bool match(const std::string &pattern, const std::string &topic) {
    auto p = split_topic(pattern), t = split_topic(topic);
    if (p.size() == 1 && p[0] == "*") return true;
    size_t pi = 0, ti = 0;
    while (pi < p.size() && ti < t.size()) {
        if (p[pi] == "*") {
            if (pi == p.size() - 1) return true;
            for (size_t skip = ti; skip <= t.size(); ++skip) {
                std::string np, nt;
                for (size_t i = pi + 1; i < p.size(); ++i) {
                    np += p[i] + (i < p.size() - 1 ? "/" : "");
                }
                for (size_t i = skip; i < t.size(); ++i) {
                    nt += t[i] + (i < t.size() - 1 ? "/" : "");
                }
                if ((nt.empty() && np.empty()) || (!nt.empty() && !np.empty() && match(np, nt)))
                    return true;
            }
            return false;
        } else if (p[pi] == "+") {
            ++pi; ++ti;
        } else if (p[pi] == t[ti]) {
            ++pi; ++ti;
        } else {
            return false;
        }
    }
    return pi == p.size() && ti == t.size();
}

inline int recv_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_received = 0;
    char *buff = static_cast<char*>(buffer);
    while (bytes_received < len) {
        ssize_t ret = recv(sockfd, buff + bytes_received, len - bytes_received, 0);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (ret == 0) return -1;
        bytes_received += ret;
    }
    return bytes_received;
}

inline int send_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    char *buff = static_cast<char*>(buffer);
    while (bytes_sent < len) {
        ssize_t ret = send(sockfd, buff + bytes_sent, len - bytes_sent, 0);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        bytes_sent += ret;
    }
    return 0;
}

#endif
