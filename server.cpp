#include "utils.h"
#include <iostream>
#include <unordered_map>
#include <set>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <algorithm>

#define BUFLEN 1600
#define MAX_CLIENTS 1000
#define ID_LEN 11
#define TOPIC_LEN 51

enum MsgType {
    INT_TYPE = 0,
    SHORT_REAL_TYPE = 1,
    FLOAT_TYPE = 2,
    STRING_TYPE = 3
};

struct TcpClient {
    int sockfd;
    std::string id;
    std::set<std::string> subscriptions;
};

std::unordered_map<std::string, TcpClient> clients;
std::unordered_map<int, std::string> fd_to_id;

bool is_subscribed(const TcpClient& client, const std::string& topic) {
    for (const auto& sub : client.subscriptions) {
        if (match(sub, topic)) {
            return true;
        }
    }
    return false;
}

void send_to_subscribers(const std::string& topic, const std::string& msg) {
    for (const auto& [id, client] : clients) {
        if (client.sockfd < 0) continue;
        if (is_subscribed(client, topic)) {
            uint16_t len = htons(msg.size());
            if (send_all(client.sockfd, &len, sizeof(len)) < 0) {
                continue;
            }
            if (send_all(client.sockfd, (void*)msg.c_str(), msg.size()) < 0) {
                continue;
            }
        }
    }
}

void process_client_command(int sockfd, const std::string& command) {
    if (fd_to_id.find(sockfd) == fd_to_id.end()) {
        return;
    }
    std::string id = fd_to_id[sockfd];
    if (command.find("subscribe") == 0) {
        std::string topic = command.substr(10);
        topic.erase(topic.find_last_not_of(" \r\n") + 1);
        clients[id].subscriptions.insert(topic);
    } 
    else if (command.find("unsubscribe") == 0) {
        std::string topic = command.substr(12);
        topic.erase(topic.find_last_not_of(" \r\n") + 1);
        clients[id].subscriptions.erase(topic);
    }
}

void process_udp_message(const char* buffer, int len, struct sockaddr_in& src_addr) {
    if (len < 51) {
        return;
    }
    char topic[TOPIC_LEN] = {0};
    memcpy(topic, buffer, 50);
    topic[50] = '\0';
    uint8_t type = buffer[50];
    const char* content = buffer + 51;
    int content_len = len - 51;
    std::ostringstream out;
    out << topic;
    switch (type) {
        case INT_TYPE: {
            if (content_len < 5) return;
            uint8_t sign = content[0];
            uint32_t value;
            memcpy(&value, content + 1, sizeof(uint32_t));
            value = ntohl(value);
            int32_t result = sign ? -((int32_t)value) : (int32_t)value;
            out << " - INT - " << result;
            break;
        }
        case SHORT_REAL_TYPE: {
            if (content_len < 2) return;
            uint16_t value;
            memcpy(&value, content, sizeof(uint16_t));
            value = ntohs(value);
            double result = value / 100.0;
            out << " - SHORT_REAL - " << std::fixed << std::setprecision(2) << result;
            break;
        }
        case FLOAT_TYPE: {
            if (content_len < 6) return;
            uint8_t sign = content[0];
            uint32_t value;
            memcpy(&value, content + 1, sizeof(uint32_t));
            value = ntohl(value);
            uint8_t power = content[5];
            double result = value / pow(10, power);
            if (sign) result = -result;
            out << " - FLOAT - " << std::fixed << std::setprecision(4) << result;
            break;
        }
        case STRING_TYPE: {
            out << " - STRING - " << std::string(content);
            break;
        }
        default:
            return;
    }
    send_to_subscribers(topic, out.str());
}

void disconnect_client(int fd, fd_set &read_fds) {
    if (fd_to_id.count(fd)) {
        std::string id = fd_to_id[fd];
        printf("Client %s disconnected.\n", id.c_str());
        clients[id].sockfd = -1;
        fd_to_id.erase(fd);
    }
    close(fd);
    FD_CLR(fd, &read_fds);
}

bool receive_command(int fd, char* buffer, uint16_t &len) {
    int r = recv_all(fd, &len, sizeof(len));
    if (r <= 0) return false;
    len = ntohs(len);
    if (len >= BUFLEN) return false;
    memset(buffer, 0, BUFLEN);
    r = recv_all(fd, buffer, len);
    return r > 0;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc != 2) {
        std::cerr << "Usage: ./server <PORT>" << std::endl;
        return 1;
    }
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sock < 0, "socket tcp");
    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sock < 0, "socket udp");
    int flag = 1;
    DIE(setsockopt(tcp_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int)) < 0, "setsockopt TCP_NODELAY");
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    DIE(bind(tcp_sock, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr)) < 0, "bind tcp");
    DIE(bind(udp_sock, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr)) < 0, "bind udp");
    DIE(listen(tcp_sock, MAX_CLIENTS) < 0, "listen");
    fd_set read_fds, tmp_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(tcp_sock, &read_fds);
    FD_SET(udp_sock, &read_fds);
    int fdmax = std::max(tcp_sock, udp_sock);
    char buffer[BUFLEN];
    while (true) {
        tmp_fds = read_fds;
        DIE(select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) < 0, "select");
        for (int i = 0; i <= fdmax; i++) {
            if (!FD_ISSET(i, &tmp_fds)) continue;
            if (i == STDIN_FILENO) {
                memset(buffer, 0, BUFLEN);
                fgets(buffer, BUFLEN - 1, stdin);
                if (strncmp(buffer, "exit", 4) == 0) {
                    for (auto& [id, client] : clients) {
                        if (client.sockfd >= 0) close(client.sockfd);
                    }
                    close(tcp_sock);
                    close(udp_sock);
                    return 0;
                }
            } else if (i == tcp_sock) {
                struct sockaddr_in cli_addr;
                socklen_t cli_len = sizeof(cli_addr);
                int newsockfd = accept(tcp_sock, (struct sockaddr*)&cli_addr, &cli_len);
                DIE(newsockfd < 0, "accept");
                int flag = 1;
                setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
                char id[ID_LEN] = {0};
                if (recv(newsockfd, id, ID_LEN - 1, 0) <= 0) {
                    close(newsockfd);
                    continue;
                }
                std::string id_str(id);
                if (clients.count(id_str) > 0 && clients[id_str].sockfd != -1) {
                    printf("Client %s already connected.\n", id_str.c_str());
                    close(newsockfd);
                    continue;
                }
                clients[id_str].sockfd = newsockfd;
                clients[id_str].id = id_str;
                fd_to_id[newsockfd] = id_str;
                FD_SET(newsockfd, &read_fds);
                fdmax = std::max(fdmax, newsockfd);
                printf("New client %s connected from %s:%d.\n",
                       id_str.c_str(),
                       inet_ntoa(cli_addr.sin_addr),
                       ntohs(cli_addr.sin_port));
            } else if (i == udp_sock) {
                struct sockaddr_in src_addr;
                socklen_t src_len = sizeof(src_addr);
                memset(buffer, 0, BUFLEN);
                int n = recvfrom(udp_sock, buffer, BUFLEN, 0,
                                 (struct sockaddr*)&src_addr, &src_len);
                if (n < 0) {
                    continue;
                }
                process_udp_message(buffer, n, src_addr);
            } else {
                uint16_t len;
                if (!receive_command(i, buffer, len)) {
                    disconnect_client(i, read_fds);
                    continue;
                }
                std::string command(buffer, len);
                process_client_command(i, command);
            }
        }
    }
    return 0;
}
