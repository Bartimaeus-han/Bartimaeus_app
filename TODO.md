# Bartimaeus App - Security Hardening TODO List

이 파일은 프로젝트의 본래 디자인(투박함)과 실무 기능을 점진적으로 개발해 나가며, 식별된 핵심 보안 위협(Threats)을 차례대로 해결해 나가기 위한 현재/미래의 백로그 관리 목록입니다. 완료된 항목은 제거되며, 현재 진행 중인 항목은 `[/]`로 마킹됩니다.

---

## 📌 보안 강화 작업 목록 (Checklist)

- [/] **메모리 소진 DoS 방어를 위한 백그라운드 가비지 컬렉터 스레드 구현 (THREAT-03)**
    - [/] **[공격 실습 (Offensive)]** Ochlos 또는 Python 스크립트를 통해 만료된 세션 및 로그인 실패 이력을 메모리 상에 대량 누적시켜, 128MB 프로세스 메모리 제한 도달로 인한 웹 서버 크래시(DoS) 상태를 강제로 유발하고 취약점 증명
    - [ ] [session_manager.hpp](file:///c:/Projects/Bartimaeus_app/src/services/session_manager.hpp) 및 [login_limiter.hpp](file:///c:/Projects/Bartimaeus_app/src/services/login_limiter.hpp)에 만료 엔트리 소거 인터페이스(예: `cleanupExpiredSessions`) 추가
    - [ ] C++ `std::thread` 백그라운드 GC 스레드 루프 구현 및 안전한 스레드 조인(Join) 설계
    - [ ] 패치 적용 후 동일한 공격을 수행했을 때 만료된 엔트리들이 가비지 컬렉터에 의해 실시간 소거되어 128MB 메모리 임계치를 초과하지 않고 안전하게 방어되는지 재검증

- [ ] **SQLite3 동시성 제어 및 트랜잭션 경합 방지 (THREAT-04)**
    - [ ] `AuthService` 및 `BoardService` 생성 시 SQLite DB 연결 직후 `sqlite3_busy_timeout` 설정 주입 (예: 5000ms 대기 설정)
    - [ ] 트랜잭션 충돌 시의 무조건적인 HTTP 500 장애 발생률을 0%로 줄어들게 하는 지연 응답 전략 런타임 검증

- [ ] **비밀번호 복잡도 유효성 검증 규칙 도입 (THREAT-01)**
    - [ ] `AuthService::signUp` 시 비밀번호 최소 길이(예: 8자 이상) 및 구성 조건(대소문자, 숫자, 특수문자 조합) 정규식 검사 함수 추가
    - [ ] 검증 실패 시 오류 메시지 프론트엔드로 전달 및 사용자 테스트용 옵션 유지 정책 수립 (개발 편의를 위해 최종 단계 진행)
