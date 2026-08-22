// 기존의 tcp_syn_flooding.cpp의 경우 connect()를 사용하기 때문에 3-way handshake를 수행
// 그로 인한 스레드 폭증이나 socket&FD 고갈 문제 발생

// raw sock를 사용하는 방식으로 진행

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <WinSock2.h>
#else