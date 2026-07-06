# Bartimaeus App - Security Hardening TODO List

이 파일은 프로젝트의 본래 디자인(투박함)과 실무 기능을 점진적으로 개발해 나가며, 식별된 핵심 보안 위협(Threats)을 차례대로 해결해 나가기 위한 TO-DO 관리 목록입니다.

---

## 📌 보안 강화 작업 목록 (Checklist)

- [ ] **[TODO-01] 암호학적으로 안전한 세션 ID 및 토큰 생성 (THREAT-05)**
  - [ ] [session_manager.hpp](file:///c:/Projects/Bartimaeus_app/src/services/session_manager.hpp)의 `generateSessionId`에서 암호학적으로 취약한 `std::mt19937` 제거
  - [ ] OpenSSL의 `RAND_bytes` API를 C++ 코드로 연동
  - [ ] 64글자의 암호학적으로 안전한 무작위 난수(16진수 변환)를 발급하는 CSPRNG 설계
  - [ ] 서버 컴파일 빌드 및 런타임 로그인 테스트 검증

- [ ] **[TODO-02] 메모리 소진 DoS 방어를 위한 백그라운드 가비지 컬렉터 스레드 구현 (THREAT-03)**
  - [ ] [session_manager.hpp](file:///c:/Projects/Bartimaeus_app/src/services/session_manager.hpp) 및 [login_limiter.hpp](file:///c:/Projects/Bartimaeus_app/src/services/login_limiter.hpp)에 전체 순회 및 만료 엔트리 소거 인터페이스(예: `cleanupExpiredSessions`) 추가
  - [ ] C++ `std::thread`를 사용해 백그라운드에서 주기적으로(예: 1분 또는 5분마다) 소거 로직을 루프로 실행하는 스레드 생성 (서버 종료 시 안전하게 `join`되도록 설계)
  - [ ] 동시성 경합 방지를 위해 적절한 뮤텍스 락(Mutex Lock) 범위 검토 및 런타임 검증

- [ ] **[TODO-03] SQLite3 동시성 제어 및 트랜잭션 경합 방지 (THREAT-04)**
  - [ ] `AuthService` 및 `BoardService` 생성 시 SQLite DB 연결 직후 `sqlite3_busy_timeout` 설정 주입 (예: 5000ms 대기 설정)
  - [ ] 트랜잭션 충돌 시의 무조건적인 HTTP 500 장애 발생률을 0%로 줄어들게 하는 지연 응답 전략 런타임 검증

- [ ] **[TODO-04] 비밀번호 복잡도 유효성 검증 규칙 도입 (THREAT-01)**
  - [ ] `AuthService::signUp` 시 비밀번호 최소 길이(예: 8자 이상) 및 구성 조건(대소문자, 숫자, 특수문자 조합) 정규식 검사 함수 추가
  - [ ] 검증 실패 시 오류 메시지 프론트엔드로 전달 및 사용자 테스트용 옵션 유지 정책 수립 (개발 편의를 위해 최종 단계 진행)
