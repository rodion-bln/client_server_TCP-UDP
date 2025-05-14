#include "utils.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#define BUFLEN 1600

void print_feedback(const std::string& cmd) {
    std::string topic;
    if (cmd.find("subscribe") == 0) {
        topic = cmd.substr(10);
        topic.erase(topic.find_last_not_of(" \r\n") + 1);
        std::cout << "Subscribed to topic " << topic << std::endl;
    } else if (cmd.find("unsubscribe") == 0) {
        topic = cmd.substr(12);
        topic.erase(topic.find_last_not_of(" \r\n") + 1);
        std::cout << "Unsubscribed from topic " << topic << std::endl;
    }
}

void send_command(int sockfd, const std::string &cmd) {
    uint16_t len = htons(cmd.size());
    if (send_all(sockfd, &len, sizeof(len)) < 0) {
        return;
    }
    if (send_all(sockfd, (void *)cmd.c_str(), cmd.size()) < 0) {
        return;
    }
    print_feedback(cmd);
}

bool receive_message(int sockfd, char* buffer, size_t buffer_size) {
    uint16_t len;
    if (recv_all(sockfd, &len, sizeof(len)) <= 0) return false;
    len = ntohs(len);
    if (len >= buffer_size) {
        return false;
    }
    memset(buffer, 0, buffer_size);
    if (recv_all(sockfd, buffer, len) <= 0) return false;
    return true;
}

void print_message(const std::string& msg) {
    std::string out = msg;
    size_t ip_end = out.find(" - ");
    if (ip_end != std::string::npos && out.substr(0, ip_end).find('.') != std::string::npos) {
        out = out.substr(ip_end + 3);
    }
    if (out.find("FLOAT - ") != std::string::npos) {
        size_t val_pos = out.rfind(" - ");
        if (val_pos != std::string::npos) {
            std::string before = out.substr(0, val_pos + 3);
            double val = std::stod(out.substr(val_pos + 3));
            std::cout << before << std::fixed << std::setprecision(4) << val << std::endl;
            return;
        }
    } else if (out.find("SHORT_REAL - ") != std::string::npos) {
        size_t val_pos = out.rfind(" - ");
        if (val_pos != std::string::npos) {
            std::string before = out.substr(0, val_pos + 3);
            double val = std::stod(out.substr(val_pos + 3));
            std::cout << before << std::fixed << std::setprecision(2) << val << std::endl;
            return;
        }
    }
    std::cout << out << std::endl;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <ID_CLIENT> <IP_SERVER> <PORT_SERVER>" << std::endl;
        return 1;
    }

    std::string id = argv[1];
    std::string server_ip = argv[2];
    int server_port = atoi(argv[3]);
    if (id.length() > 10) {
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");
    int flag = 1;
    DIE(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)) < 0, "setsockopt TCP_NODELAY");

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    DIE(inet_aton(server_ip.c_str(), &serv_addr.sin_addr) == 0, "inet_aton");
    DIE(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0, "connect");
    DIE(send_all(sockfd, (void *)id.c_str(), id.size()) < 0, "send id");

    fd_set read_fds;
    char buffer[BUFLEN];

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sockfd, &read_fds);

        int ret = select(sockfd + 1, &read_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(buffer, 0, BUFLEN);
            fgets(buffer, BUFLEN - 1, stdin);
            std::string cmd(buffer);
            if (cmd == "exit\n") break;
            if (cmd.find("subscribe") == 0 || cmd.find("unsubscribe") == 0) {
                send_command(sockfd, cmd);
            }
        }

        if (FD_ISSET(sockfd, &read_fds)) {
            if (!receive_message(sockfd, buffer, BUFLEN)) {
                break;
            }
            print_message(buffer);
        }
    }

    close(sockfd);
    return 0;
}
