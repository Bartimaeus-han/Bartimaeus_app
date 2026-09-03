#define _CRT_SECURE_NO_WARNINGS
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread> // 다중 클라이언트 동시 중계

// OS별
#ifdef _WIN32
// clang-format off
#include <WinSock2.h>
#include <WS2tcpip.h>
// clang-format on

#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// 소켓 핸들은 전역으로 설정
#ifdef _WIN32
using SocketHandle = SOCKET;
const SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
const SocketHandle kInvalidSocket = -1;
#endif

// Reverse Proxy는 클라이언트(외부 인터넷)와 백엔드 서버 사이에서 트래픽을 양방향 중계하는 스트림 파이프라인 (Reverse Proxy stream pipeline relaying bidirectional traffic between client and backend)
void forward_stream(SocketHandle src, SocketHandle dst) {
    // x86/64 architecture에서 CPU의 MMU와 OS 커널이 메모리를 관리하는 기본 단위가 정확하게 4KB이다. (Page size is 4KB in x86/64 architecture)
    // Ethernet의 MTU는 1500byte, header제외 MSS는 약 1460byte -> 4096은 TCP packet 2~3개분량 (Ethernet MTU is 1500 bytes, MSS ~1460 bytes -> 4096 fits 2-3 TCP packets)
    // char buffer[4096]은 malloc/new를 사용하지 않고도 즉시 함수의 stack 영역으로 할당된다. (char buffer[4096] is allocated immediately on stack without malloc/new)
    // 실무에서의 프록시 및 웹 서버들도 I/O buffer size로 4 or 8 KB를 기본으로 채택 중이다. (Real-world proxies adopt 4KB or 8KB as default I/O buffer size)
    char buffer[4096];

    while (true) {
        // external -> reverse proxy (외부 클라이언트 -> 리버스 프록시)
        int bytes_read = recv(src, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0)
            break;

        // reverse proxy -> backend server (리버스 프록시 -> 백엔드 서버)
        int bytes_sent = send(dst, buffer, bytes_read, 0);
        if (bytes_sent <= 0)
            break;
    }

// 무한대기 방지, 스레드 완벽 동기화 종료
#ifdef _WIN32
    shutdown(dst, SD_SEND);
#else
    shutdown(dst, SHUT_WR);
#endif
}

//
void handle_client(SocketHandle client_sock, const char *backend_ip, int backend_port) {
    SocketHandle backend_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // backend로의 socket 생성 실패 시, 기존의 client socket은 해제 해줘야겠지?
    if (backend_sock == kInvalidSocket) {
        std::cerr << "[-] Failed to create backend socket\n";
#ifdef _WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
        return;
    }

    // 검색할 호스트의 주소에 대한 조건을 설정한다.
    // [struct addrinfo] : IPv4/6 dual stack, Thread-Safe, L3/L4를 하나의 function으로 동시 해석 가능.
    struct addrinfo hints, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(backend_port);
    if (getaddrinfo(backend_ip, port_str.c_str(), &hints, &res) != 0 || res == nullptr) {
        std::cerr << "[-] DNS resolution failed for backend: " << backend_ip << "\n";
#ifdef _WIN32
        closesocket(client_sock);
        closesocket(backend_sock);
#else
        close(client_sock);
        close(backend_sock);
#endif
        return;
    }

    // ai -> AddrInfo struct
    // addr : socket ADDRess
    if (connect(backend_sock, res->ai_addr, res->ai_addrlen) != 0) {
        std::cerr << "[-] Failed to connect to backend server (" << backend_ip << ":" << backend_port << ")\n";
        freeaddrinfo(res);
#ifdef _WIN32
        closesocket(client_sock);
        closesocket(backend_sock);
#else
        close(client_sock);
        close(backend_sock);
#endif

        return;
    }

    freeaddrinfo(res);

    // 3. 이제 멀티 스레드로 양방향 동시 중계 시작하기
    std::thread client_to_server(forward_stream, client_sock, backend_sock);
    std::thread server_to_client(forward_stream, backend_sock, client_sock);

    // 스레드 join 가능 여부 안전 검사 (Best practice safety check for joinable threads)
    // 바로 위에서 thread 생성 후 즉시 대기하므로 안전하게 동기화 (Safely synchronize since threads were just created)
    if (client_to_server.joinable())
        client_to_server.join();
    if (server_to_client.joinable())
        server_to_client.join();

    // 4. 통신 끝나면 이제 양쪽 소켓은 정리
#ifdef _WIN32
    closesocket(client_sock);
    closesocket(backend_sock);
#else
    close(client_sock);
    close(backend_sock);
#endif
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[-] WSAStartup failed\n";
        return 1;
    }
#endif

    std::cout << std::unitbuf;

    std::cout << "[ReverseProxy] L7 Reverse Proxy Gateway started.\n";

    const int listen_port = 8080; // 리버스 프록시 인입 리스닝 포트 (Reverse proxy incoming listen port)
    // 백엔드 웹 애플리케이션 접속 주소 및 포트 (Backend web application host address and port)
    const char *env_backend = std::getenv("BACKEND_HOST");
    const char *backend_ip = env_backend ? env_backend : "127.0.0.1";

    const int backend_port = 9090;

    // 1. Create listening socket
    SocketHandle server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock == kInvalidSocket) {
        std::cerr << "[-] Failed to create server socket\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // 2. 포트 재사용 옵션 설정
    // 프로그램 재실행 시 포트 재사용에 대한 설정
    int opt = 1;
#ifdef _WIN32
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&opt), sizeof(opt));
#else
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // 3. 주소 구조체 설정 및 값 바인딩
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;                // IPv4를 사용하겠다!
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 모든 네트워크 인터페이스에 대한 수신을 허용한다.
    server_addr.sin_port = htons(listen_port);

    // 값 바인딩 및 에러 체크
    if (bind(server_sock, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) != 0) {
        std::cerr << "[-] Bind failed on port " << listen_port << "\n";
#ifdef _WIN32
        closesocket(server_sock);
        WSACleanup();
#else
        close(server_sock);
#endif
        return 1;
    }

    // 4. 리스닝(연결 수신 대기열) 활성화
    if (listen(server_sock, SOMAXCONN) != 0) {
        std::cerr << "[-] Listen failed on port " << listen_port << "\n";
#ifdef _WIN32
        closesocket(server_sock);
        WSACleanup();
#else
        close(server_sock);
#endif
        return 1;
    }

    std::cout << "[+] Reverse Proxy listening on 0.0.0.0:" << listen_port << "...\n";

    std::cout << "[*] Monitoring and proxying incoming client traffic in real-time...\n";

    // 5. 클라이언트 연결 수락 및 인입 트래픽 중계 루프 (Accept client connections and forward traffic loop)
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        SocketHandle client_sock = accept(server_sock, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
        if (client_sock == kInvalidSocket) {
            std::cerr << "[-] Accept failed\n";
            break;
        }

        // 소켓 주소 구조체에서 클라이언트 IP/Port 추출 (Extract client IP and Port from socket address structure)
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        std::cout << "[ReverseProxy] Source: " << client_ip << ":" << client_port << " -> Dest Port: " << listen_port << "\n";

        // 개별 클라이언트 요청을 비동기 스레드로 넘겨서 중계를 처리한다 (Forward each client request to a worker thread)
        std::thread(handle_client, client_sock, backend_ip, backend_port).detach();
    }

#ifdef _WIN32
    closesocket(server_sock);
#else
    close(server_sock);
#endif

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}