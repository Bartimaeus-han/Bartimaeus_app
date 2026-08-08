# Bartimaeus App - Security Hardening TODO List

이 파일은 프로젝트의 본래 디자인(투박함)과 실무 기능을 점진적으로 개발해 나가며, 식별된 핵심 보안 위협(Threats)을 차례대로 해결해 나가기 위한 현재/미래의 백로그 관리 목록입니다. 완료된 항목은 제거되며, 현재 진행 중인 항목은 `[/]`로 마킹됩니다.

---

## 📌 보안 강화 작업 목록 (Checklist)

- [ ] **컨테이너 재생성 시 에러 로그 유실 문제 (Infra-Logging)**
    - [x] 발견 경위(2026-08-07): 루트 파일 정리 과정에서 발견 — `docker-compose.yml`의 `app` 서비스에 로그 관련 볼륨 마운트가 없어, `helpers.hpp:104`가 쓰는 `error.log`가 컨테이너의 임시 파일시스템(`/app/error.log`)에만 존재함을 확인. `docker exec`로 실행 중인 컨테이너 내부를 직접 열어 로그 파일 부재를 실증 확인. `docker compose down`이나 `--build` 재생성 시 로그가 통째로 유실되는 구조
    - [ ] 조치 방향(2026-08-07 사용자 확정): 임시 방편(바인드 마운트 등)으로 지금 당장 땜질하지 않고, 착수 자체를 `ROADMAP.md` Phase 2(웹 서버 앞단 Reverse Proxy & WAF 구축) 시작 시점까지 통째로 보류하기로 결정 — 그때 방화벽/프록시 레이어의 로그까지 함께 고려해서, 처음부터 stdout/stderr 기반 로깅 + 중앙 로그 저장소(Phase 3 아나블레포: ELK/Loki 등) 방식으로 한 번에 설계할 계획. 그 전까지는 로그 유실이 알려진 한계로 존재하는 상태를 그대로 감수

- [/] **특정 프로세스(게시글 등록)에 대한 자동화 공격 통제 미흡 (THREAT-10)**
    - [x] `Web_Application_Guide.md`(KISA 가이드) 20번 "자동화 공격" 항목 기준 점검 시작(2026-08-07). 로그인(`/login`)에는 이미 `LoginLimiter`로 무차별 대입 방어가 적용되어 있음(커밋 `80a23eb`)을 확인했으나, 게시글 등록(`POST /api/post`, [board_controller.hpp:20](../src/controllers/board_controller.hpp#L20)의 `handleCreatePost`)에는 동일한 통제가 전혀 없음을 코드로 확인 — `requireAuthAndCsrf`로 인증/CSRF만 걸려있을 뿐, 횟수 제한이나 최소 요청 간격 제어가 없어 로그인된 사용자(또는 탈취된 세션)가 스크립트로 무제한 반복 등록 가능
    - [ ] 공격 선행 학습 모델에 따라 Ochlos로 게시글 등록 스팸/DB 자원 고갈 공격 실제 시연 예정
    - [ ] 방어책 설계 및 조치 (예: `LoginLimiter`와 유사한 사용자별 rate limiter 도입 여부, 기존 클래스 재사용 vs 신규 범용 `RateLimiter`로 일반화 여부 결정)

- [ ] **정적 페이지 role 체크 방식의 확장성 개선 검토 (THREAT-09-Followup)**
    - [ ] 현재 `pre_routing_handler`(`main.cpp`)의 `admin.html` role 체크는 `if (req.path == "/admin.html") { ... }` 형태로 페이지 경로 하나하나에 개별 하드코딩된 방식 — role 체크가 필요한 페이지가 늘어날수록 동일한 조건문을 계속 추가해야 하는 구조라 확장성이 떨어짐(2026-08-07 논의)
    - [ ] 실무에서는 "경로 → 필요 role" 매핑 테이블(선언형 설정)을 두고 공통 로직에서 순회 검사하는 패턴(예: Spring Security의 `antMatchers().hasRole()` 유사 개념)을 사용 — 다만 현재는 role 체크 대상이 `admin.html` 하나뿐이라 지금 당장 리팩터링하는 것은 과설계로 판단, 대상 페이지가 2개 이상으로 늘어나는 시점에 착수 검토

- [ ] **API 레벨 자동 테스트/스모크 테스트 구축 (Infra)**
    - [ ] 회원가입/로그인/게시글 CRUD 등 핵심 API를 대상으로 한 자동화된 스모크 테스트 스크립트 도입 검토 (매번 수동 curl 호출 대신, Docker 환경 기동 후 자동으로 헬스체크 + 핵심 플로우 검증)
    - [ ] 도구 후보(예: 간단한 Python/curl 스크립트 vs pytest 기반) 및 실행 시점(로컬 vs CI) 논의 필요

- [ ] **비밀번호 복잡도 유효성 검증 규칙 도입 (THREAT-01)**
    - [ ] `AuthService::signUp` 시 비밀번호 최소 길이 및 구성 조건 정규식 검사 함수 추가 (Add password length and pattern validation in AuthService::signUp)
    - [ ] 검증 실패 시 오류 메시지 프론트엔드로 전달 및 예외 정책 수립 (Pass error messages to frontend and establish policy exceptions)

- [ ] **`AuthService`/`BoardService`의 `db_mutex` 동시성 설계 재검토 (THREAT-07-Followup)**
    - [ ] 지금 `db_mutex`는 각 서비스 클래스 전체를 호출마다 통째로 직렬화함 — SQLite3 시절엔 파일 락 회피/스레드-비안전 커넥션 핸들 보호 목적으로 필요했지만, MariaDB(InnoDB)는 애초에 행 단위로 락을 걸기 때문에 서로 다른 행(예: `alice` 가입과 `bob` 가입)까지 굳이 한 줄로 세울 필요가 없음 — 이게 MariaDB로 전환한 핵심 이점(행이 다르면 동시 처리 가능) 중 하나를 스스로 깎아먹고 있는 셈. 트래픽이 늘면 이 지점이 성능 병목의 원인이 될 수 있어 재검토 필요
    - [ ] `signUp()`의 INSERT 실패 시 `mysql_stmt_errno()`로 에러 코드를 확인해 `ER_DUP_ENTRY`(1062, 진짜 중복 아이디)와 그 외 진짜 DB 오류를 구분 — 현재는 원인 불문 전부 `AuthResult::DbError`(503)로 응답해서, 동시 가입 레이스에서 진 쪽이 "서버 장애"라는 잘못된 메시지를 받음 (실제로는 "이미 존재하는 아이디"이므로 409가 맞음). `race_test_manual_*` 계정으로 두 프로세스 동시 가입 시켜 재현한 로그(`Duplicate entry '...' for key 'PRIMARY'`)로 실증 확인됨 (2026-08-04)
    - [ ] 위 두 가지를 함께 고려해 `db_mutex`를 제거하거나 범위를 좁히고, 대신 DB의 행 단위 락/유니크 제약과 정확한 에러 코드 해석에 안전성을 위임하는 방향으로 재설계할지 결정

- [ ] **실무 베스트 프랙티스 기반의 비밀번호/설정 보안 고도화 및 외부 커넥션 풀 라이브러리 검토 (THREAT-06-Followup)**
    - [ ] .env 파일 또는 외부 Vault 서버를 이용한 비밀번호/데이터베이스 계정 정보 보호 및 주입 기법 설계
    - [ ] HikariCP 방식의 C++ 외부 커넥션 풀 또는 서드파티 DB 인터페이스 도입 가능성 기술 검토
    - [ ] TLS 개인키(`certs/key.pem`)를 Docker 이미지에 직접 COPY하는 방식의 보안 리스크 검토 (이미지 유출 시 개인키 노출) — 볼륨 마운트 또는 Docker Secret 방식 전환 여부 결정

- [ ] **벤더 MySQL 헤더(`3rdparty/mysql/include`) 수정사항 관리 방식 재검토 (THREAT-06-Followup)**
    - [ ] Windows 포팅을 위해 직접 수정한 `mysql.h`/`mysql_com.h`가 원본과 구분 없이 커밋되는 문제 논의 필요 (ABI 불일치 리스크, upstream diff 추적 불가)
    - [ ] 후보안: 파일 상단 수정 이력 주석 명시 / 별도 `.patch` 파일 분리 관리(vcpkg 방식 참고) 중 택1

- [/] **`RLIMIT_AS` 상향(128MB→512MB)에 따른 THREAT-05(DoS) 실습 조건 재설계 (THREAT-05-Followup)**
    - [x] 기존 128MB 한도가 idle 상태 스레드 스택 오버헤드만으로 거의 소진되어 신규 연결 스레드의 스택 할당이 실패, TLS 핸드셰이크가 응답 없이 무한 행(hang)되는 실제 장애로 이어짐을 확인(2026-07-31) → 임시 조치로 512MB 상향하여 정상화됨(상세: `CONTEXT.md` 3.1-8)
    - [x] 512MB는 정상 가동 확보를 위한 응급 조치이며 DoS 방어선으로는 느슨함 — Ochlos 공격 실습(`memory_exhaustion.py`) 재설계 시 이 상향된 한도 기준으로 공격 강도/시나리오 재산정 필요
    - [x] 결정 확정(2026-08-01): `RLIMIT_AS`는 가상 주소공간(VSS) 기준이라 실제 물리 메모리 압박(RSS)과 상관관계가 약해 1차 방어 수단으로 부적절함을 확인 → **1차 방어선을 Docker `cgroups` 기반 물리 메모리 제한(`docker-compose.yml`의 `mem_limit`)으로 전환**하고, 코드 레벨 `RLIMIT_AS`(`main.cpp`의 `limitProcessMemory()`)는 일단 비활성화(호출 제거)하기로 결정
    - [x] `docker-compose.yml`의 `app` 서비스에 `mem_limit: 2g` 설정 확인(2026-08-04) — 커밋 `a147d15`에서 이미 반영됨
    - [x] `main.cpp`의 `limitProcessMemory()` 호출 및 함수 자체가 커밋 `a147d15`에서 이미 완전히 제거됨을 코드 확인(2026-08-04)
    - [ ] (나중) OS 레벨 `RLIMIT_AS`를 소프트 리밋(2단 방어의 보조 수단)으로 재도입할지 검토 — 재도입 시 스레드 스택 크기(`pthread_attr_setstacksize`)를 함께 축소해 가상주소 오버헤드 자체를 줄이는 방향도 함께 고려

- [ ] **(우선순위 하향, 멀티 프로세스 인프라 구축 이후 착수) 멀티 프로세스 로드 밸런싱 환경 데이터베이스 동시성 격리 수준 검증 (THREAT-06-Followup)**
    - [ ] 현재는 단일 프로세스 기준으로만 검증 완료. 실제 멀티 프로세스/로드밸런싱 인프라 구축이 이루어진 이후에 재점검 예정

