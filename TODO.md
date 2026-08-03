# Bartimaeus App - Security Hardening TODO List

이 파일은 프로젝트의 본래 디자인(투박함)과 실무 기능을 점진적으로 개발해 나가며, 식별된 핵심 보안 위협(Threats)을 차례대로 해결해 나가기 위한 현재/미래의 백로그 관리 목록입니다. 완료된 항목은 제거되며, 현재 진행 중인 항목은 `[/]`로 마킹됩니다.

---

## 📌 보안 강화 작업 목록 (Checklist)

- [x] **[CRITICAL] DB 커넥션 풀 고갈로 인한 전체 서비스 마비 버그 수정 (THREAT-07)** — 코드 반영 및 런타임 검증까지 전부 완료, **커밋만 아직 안 됨** (커밋 후 `CONTEXT.md` 기록 및 이 항목 pruning 예정)
    - [x] `db_manager.hpp`의 `getConnection()`에서 `mysql_ping()` 실패 후 `createConnection()` 재연결이 실패(nullptr 반환)하면 해당 풀 슬롯이 영구 소실되는 버그 발견 및 실제 재현 (Docker 환경에서 MariaDB 컨테이너 재시작 후 전체 API 응답 불가 상태 확인, exit code 137로 간접 확인) — 코드 흐름(pop_back → ping 실패 → createConnection 재실패 → nullptr → releaseConnection의 guard에서 소실) 전체 메커니즘 학습 및 이해 완료
    - [x] 재연결 실패 시 풀 슬롯을 잃지 않고 재시도하거나, 최소한 `nullptr` 반환 시 호출자가 이를 감지해 사용자에게 503 등 명확한 에러를 반환하도록 방어 로직 설계 (현재는 워커 스레드가 `pool_cv.wait()`에서 영구 대기하며 cpp-httplib 공용 스레드 풀 전체를 고갈시킴)
        - [x] 설계 방향 확정: **B안(HikariCP 스타일)** — 요청 스레드는 `pool_cv.wait_for()`로 최대 대기시간만 기다리고 재연결도 1회만 빠르게 시도(fast-fail)하며, 별도 `std::jthread` 백그라운드 힐러가 주기적으로 소실된 슬롯을 채운다. (A안: 요청 스레드가 직접 온디맨드로 커넥션을 재생성하는 방식은 기각)
        - [x] `db_manager.hpp`의 `getConnection()`에 bounded wait + fast-fail 재연결 + `cur_conns` 차감 로직 적용 (docker compose up --build로 컴파일/동작 확인 완료)
        - [x] `db_manager.hpp`에 백그라운드 힐러 스레드(`pool_healer`) 추가 — 생성자에서 시작. `docker compose stop/start mariadb`로 실제 deficit(0/5) 발생 후 힐러가 한 틱 안에 5개까지 연달아 복구하고, 이후 연결 누수 없이(정상 상태에선 재생성 안 함) 도는 것까지 런타임 중 동작은 로그로 검증 완료
        - [x] `~DbManager()` 소멸자에 `pool_healer.request_stop()` + `join()`을 `destroyPool()`보다 먼저 명시적으로 호출하는 로직 추가 (코드 확인 결과 커밋 `a147d15`에서 이미 적용 완료됨 — `db_manager.hpp:99-105`. 로그인 요청으로 `DbManager` 싱글턴을 강제 초기화해 `pool_healer`를 살려둔 상태에서 `docker compose stop app`(SIGTERM)으로 종료 시나리오 실증 검증 완료 — 종료 직전까지 찍히던 `Healer tick.` 로그가 `Web Server stopped safely.` 이후로는 더 이상 찍히지 않고, 크래시/에러 없이 `ExitCode 0`으로 정상 종료됨을 확인. 단, `pool_healer.join()`이 `mysql_real_connect()` 블로킹 호출 도중이면 grace period 내 종료를 보장 못 하는 구조적 약점은 미해결 상태로 남아있음)
        - [x] 호출부에서 "DB 연결 실패"와 "비즈니스 로직 실패(아이디 중복 등)"를 구분해 503 vs 409 응답 분리 — `auth_service.hpp`의 `signUp()`/`login()`을 `bool` 대신 `AuthResult{Success,Fail,DbError}` enum 반환으로 전환하고, `auth_controller.hpp`의 `handleSignUp`/`handleLogin`을 `switch`문으로 분기(409/401 vs 503)하도록 수정. `docker compose stop mariadb` 상태에서 실제로 로그인 시도 시 503 + `"Service temporarily unavailable"` 응답을 curl로 실증 검증 완료
        - [x] **(부수 발견 및 수정)** [main.cpp:53-113](src/main.cpp)의 `svr.set_error_handler`가 상태 코드 400 이상이면 컨트롤러/미들웨어가 이미 채워놓은 `res.body`를 무조건 덮어써서, 421/409/503은 물론 `middleware.hpp`의 401/403 메시지까지 전부 범용 "An unexpected error occurred." 문구로 뭉개지고 있던 기존 버그를 발견. `if (!res.body.empty()) return;` 가드 추가로 해결하고, 겸사겸사 `is_api_request` 판별 조건에 `/login`,`/signup`,`/logout`(비-`/api` 접두사 JSON 라우트) 추가해 이 세 라우트도 JSON 에러 응답을 받도록 수정

- [ ] **API 레벨 자동 테스트/스모크 테스트 구축 (Infra)**
    - [ ] 회원가입/로그인/게시글 CRUD 등 핵심 API를 대상으로 한 자동화된 스모크 테스트 스크립트 도입 검토 (매번 수동 curl 호출 대신, Docker 환경 기동 후 자동으로 헬스체크 + 핵심 플로우 검증)
    - [ ] 도구 후보(예: 간단한 Python/curl 스크립트 vs pytest 기반) 및 실행 시점(로컬 vs CI) 논의 필요

- [ ] **비밀번호 복잡도 유효성 검증 규칙 도입 (THREAT-01)**
    - [ ] `AuthService::signUp` 시 비밀번호 최소 길이 및 구성 조건 정규식 검사 함수 추가 (Add password length and pattern validation in AuthService::signUp)
    - [ ] 검증 실패 시 오류 메시지 프론트엔드로 전달 및 예외 정책 수립 (Pass error messages to frontend and establish policy exceptions)

- [/] **Docker 기반 인프라 확장 및 MySQL 데이터베이스 마이그레이션 (THREAT-06)**
    - [x] CMakeLists.txt 내 mysqlclient 라이브러리 연동 (Link mysqlclient library in CMakeLists.txt)
    - [x] db_queries.hpp 내 SQL 스키마 및 쿼리 MySQL 호환성 튜닝 (Tune SQL schemas and queries for MySQL compatibility in db_queries.hpp)
    - [x] MySQL Connection Pool (db_manager.hpp) 구현 (Implement MySQL Connection Pool in db_manager.hpp)
    - [x] AuthService 및 BoardService 내 sqlite3 API를 MySQL C API로 포팅 (Port sqlite3 APIs to MySQL C APIs in AuthService and BoardService)
        - [x] BoardService MySQL C API 포팅 완료 (Completed BoardService MySQL C API porting)
        - [x] AuthService MySQL C API 포팅 완료 (Completed AuthService MySQL C API porting)
    - [x] CMake POST_BUILD 단계로 런타임 DLL(libssl, libcrypto, libmysql) 자동 배포 처리 (Auto-deploy runtime DLLs via CMake POST_BUILD step, fixes STATUS_DLL_NOT_FOUND on exe launch)
    - [x] Docker Compose를 활용한 App 및 MySQL 컨테이너 기반 인프라 구축 (Build App and MySQL container-based infrastructure using Docker Compose)
        - [x] **버그 수정 완료**: Dockerfile 런타임 스테이지에서 `certs/` 디렉토리 COPY 누락 문제 발견 및 수정 (Runtime stage was missing `COPY --from=builder /app/certs ./certs`)
        - [x] `docker compose up --build` 재검증 완료 — app(https://localhost:9090), mariadb 컨테이너 모두 정상 기동 및 상호 연결 확인 (Re-verified: both containers start cleanly and connect successfully)
    - [ ] 멀티 프로세스 로드 밸런싱 환경에서 데이터베이스 동시성 격리 수준 검증 (Verify database concurrency isolation levels in a multi-process load-balanced environment)

- [ ] **실무 베스트 프랙티스 기반의 비밀번호/설정 보안 고도화 및 외부 커넥션 풀 라이브러리 검토 (THREAT-06-Followup)**
    - [ ] .env 파일 또는 외부 Vault 서버를 이용한 비밀번호/데이터베이스 계정 정보 보호 및 주입 기법 설계
    - [ ] HikariCP 방식의 C++ 외부 커넥션 풀 또는 서드파티 DB 인터페이스 도입 가능성 기술 검토
    - [ ] TLS 개인키(`certs/key.pem`)를 Docker 이미지에 직접 COPY하는 방식의 보안 리스크 검토 (이미지 유출 시 개인키 노출) — 볼륨 마운트 또는 Docker Secret 방식 전환 여부 결정

- [ ] **벤더 MySQL 헤더(`3rdparty/mysql/include`) 수정사항 관리 방식 재검토 (THREAT-06-Followup)**
    - [ ] Windows 포팅을 위해 직접 수정한 `mysql.h`/`mysql_com.h`가 원본과 구분 없이 커밋되는 문제 논의 필요 (ABI 불일치 리스크, upstream diff 추적 불가)
    - [ ] 후보안: 파일 상단 수정 이력 주석 명시 / 별도 `.patch` 파일 분리 관리(vcpkg 방식 참고) 중 택1

- [/] **Docker 전용 빌드 전환에 따른 `main.cpp` OS 조건부 레거시 코드 정리 (THREAT-06-Followup)**
    - [x] 결정 확정(2026-08-01): **로컬 개발도 Docker 전용으로 완전 전환**. 이후 `run.ps1` 네이티브 빌드는 더 이상 사용하지 않고, 컴파일/실행/검증은 항상 `docker compose up --build` 경로로만 진행한다.
    - [ ] `main.cpp`의 `_WIN32`/`__APPLE__` 조건부 레거시 코드(Job Object 메모리 제한, Mach 커널 API 기반 `printMemoryUsage` 등) 제거 — Linux 전용으로 단순화
    - [ ] `run.ps1` 스크립트 자체의 보관/삭제 여부는 학습자가 직접 결정 (AI가 임의로 삭제하지 않음)

- [/] **`RLIMIT_AS` 상향(128MB→512MB)에 따른 THREAT-05(DoS) 실습 조건 재설계 (THREAT-05-Followup)**
    - [x] 기존 128MB 한도가 idle 상태 스레드 스택 오버헤드만으로 거의 소진되어 신규 연결 스레드의 스택 할당이 실패, TLS 핸드셰이크가 응답 없이 무한 행(hang)되는 실제 장애로 이어짐을 확인(2026-07-31) → 임시 조치로 512MB 상향하여 정상화됨(상세: `CONTEXT.md` 3.1-8)
    - [x] 512MB는 정상 가동 확보를 위한 응급 조치이며 DoS 방어선으로는 느슨함 — Ochlos 공격 실습(`memory_exhaustion.py`) 재설계 시 이 상향된 한도 기준으로 공격 강도/시나리오 재산정 필요
    - [x] 결정 확정(2026-08-01): `RLIMIT_AS`는 가상 주소공간(VSS) 기준이라 실제 물리 메모리 압박(RSS)과 상관관계가 약해 1차 방어 수단으로 부적절함을 확인 → **1차 방어선을 Docker `cgroups` 기반 물리 메모리 제한(`docker-compose.yml`의 `mem_limit`)으로 전환**하고, 코드 레벨 `RLIMIT_AS`(`main.cpp`의 `limitProcessMemory()`)는 일단 비활성화(호출 제거)하기로 결정
    - [ ] `docker-compose.yml`의 `app` 서비스에 `mem_limit` 설정 추가
    - [ ] `main.cpp`의 `limitProcessMemory(512)` 호출 제거/비활성화 (OS 레벨 제한 중단)
    - [ ] (나중) OS 레벨 `RLIMIT_AS`를 소프트 리밋(2단 방어의 보조 수단)으로 재도입할지 검토 — 재도입 시 스레드 스택 크기(`pthread_attr_setstacksize`)를 함께 축소해 가상주소 오버헤드 자체를 줄이는 방향도 함께 고려

