#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

constexpr int default_port = 7;
constexpr int timeout_sec = 3;

int fail(const std::string& what, int sock = -1)
{
    std::cerr << "Error: " << what << ": " << std::strerror(errno) << '\n';
    if (sock >= 0)
        close(sock);
    return 1;
}

bool send_all(int sock, const std::string& data)
{
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(sock, data.data() + sent, data.size() - sent, 0);
        if (n <= 0)
            return false;
        sent += n;
    }
    return true;
}

bool recv_exact(int sock, std::string& out, size_t size)
{
    out.clear();
    char buf[1024];
    while (out.size() < size) {
        size_t want = std::min(sizeof buf, size - out.size());
        ssize_t n = recv(sock, buf, want, 0);
        if (n == 0)
            errno = ECONNRESET;
        if (n <= 0)
            return false;
        out.append(buf, n);
    }
    return true;
}

int main(int argc, char* argv[])
{
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> [port]\n"
                  << "Default port: " << default_port << '\n'
                  << "Sends each line from stdin and prints the echo. "
                     "Ctrl+D to quit.\n";
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

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return fail("socket");

    timeval timeout{timeout_sec, 0};
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0
        || setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0)
        return fail("setsockopt", sock);

    if (connect(sock, reinterpret_cast<sockaddr*>(&server), sizeof server) < 0) {
        if (errno == EINPROGRESS)
            errno = ETIMEDOUT;
        return fail("connect", sock);
    }
    std::cerr << "Connected to " << ip << ':' << port << '\n';

    std::string line, reply;
    while (std::getline(std::cin, line)) {
        line += '\n';
        if (!send_all(sock, line))
            return fail("send", sock);
        if (!recv_exact(sock, reply, line.size()))
            return fail("recv", sock);
        std::cout << "Echo: " << reply << std::flush;
    }

    close(sock);
    return 0;
}
