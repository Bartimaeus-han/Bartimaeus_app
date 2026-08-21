#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <thread> // 다중 클라이언트 동시 중계

// OS별
#ifdef _WIN32
#include <WS2tcpip.h>
#include <WinSock2.h>

#else
#include <arpa/inet.h>
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

// Screening Router는 client(external internet)과 server사이에서 작동하기 때문에 그에 따른 중계 파이프인 스트림
void forward_stream(SocketHandle src, SocketHandle dst) {
    // x86/64 architecture에서 CPU의 MMU와 OS 커널이 메모리를 관리하는 기본 단위가 정확하게 4KB이다.
    // Ethernet의 MTU는 1500byte, header제외 MSS는 약 1460byte -> 4096은 TCP packet 2~3개분량
    // char buffer[4096]은 malloc/new를 사용하지 않고도 즉시 함수이 stack 영역으로 할당된다.
    // 실무에서의 프록시 및 웹 서버들도 I/O buffer syze로 4 or 8 KB를 기본으로 채택 중이다.
    char buffer[4096];

    while (true) {
        // external -> screening router
        int bytes_read = recv(src, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0)
            break;

        // screening router -> server
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

    // 아래 안전 검사를 해주는게 Best Practice
    // 바로 위에서 thread 생성 후 바로 확인하기 때문에 99.99% joinable() == true
    // 예외가 있을 수 있지만, 현재 스크리닝 라우터의 목적은 중계 세션에 대한 로그 기록이므로 넘어간다
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

    std::cout << "[ScreeningRouter] L3/L4 Packet Logging Gateway started.\n";

    const int listen_port = 8080; // 이건 단순 스크리닝 라우터 작동 시에만 listen 하는 port
    // 여기는 이제 실제 스크리닝 라우터로써 뒷단에 있는 서버에 대한 주소
    const char *backend_ip = "127.0.0.1";
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

    std::cout << "[+] Screening Router listening on 0.0.0.0:" << listen_port << "...\n";

    std::cout << "[*] Monitoring incoming L3/L4 traffic in real-time...\n";

    // 5. 클라이언트의 연결 수락 및 L3/L4 logging loop
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        SocketHandle client_sock = accept(server_sock, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);
        if (client_sock == kInvalidSocket) {
            std::cerr << "[-] Accept failed\n";
            break;
        }

        // OSI 3,4 계층에서 데이터(IP, Port) 추출 및 문자열로의 변환
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        std::cout << "[Screening Router] Source: " << client_ip << ":" << client_port << " -> Dest Port: " << listen_port << "\n";

        // 개별 클라이언트 요청을 비동기 스레드로 넘겨서 중계를 처리한다.
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