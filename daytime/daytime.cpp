#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

constexpr int default_port = 13;
constexpr int timeout_sec = 3;

int fail(const std::string& what, int sock = -1)
{
    std::cerr << "Error: " << what << ": " << std::strerror(errno) << '\n';
    if (sock >= 0)
        close(sock);
    return 1;
}

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> [port]\n"
                  << "Default port: " << default_port << '\n';
        return 1;
    }
    const char* ip = argv[1];
    int port = argc > 2 ? std::atoi(argv[2]) : default_port;
    if (port < 1 || port > 65535) {
        std::cerr << "Error: invalid port\n";
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server.sin_addr) != 1) {
        std::cerr << "Error: invalid IPv4 address: " << ip << '\n';
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return fail("socket");

    timeval timeout{timeout_sec, 0};
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0)
        return fail("setsockopt", sock);

    const char request = '\n';
    if (sendto(sock, &request, 1, 0, reinterpret_cast<sockaddr*>(&server),
               sizeof server) < 0)
        return fail("sendto", sock);

    char reply[256];
    ssize_t n = recvfrom(sock, reply, sizeof reply - 1, 0, nullptr, nullptr);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            errno = ETIMEDOUT;
        return fail("recvfrom", sock);
    }
    close(sock);

    reply[n] = '\0';
    std::cout << "Server time: " << reply;
    if (n == 0 || reply[n - 1] != '\n')
        std::cout << '\n';
    return 0;
}
