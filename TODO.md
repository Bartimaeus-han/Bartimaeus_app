# Bartimaeus App - Security Hardening TODO List

이 파일은 프로젝트의 본래 디자인(투박함)과 실무 기능을 점진적으로 개발해 나가며, 식별된 핵심 보안 위협(Threats)을 차례대로 해결해 나가기 위한 현재/미래의 백로그 관리 목록입니다. 완료된 항목은 제거되며, 현재 진행 중인 항목은 `[/]`로 마킹됩니다.

---

## 📌 보안 강화 작업 목록 (Checklist)

- [/] **[CRITICAL] DB 커넥션 풀 고갈로 인한 전체 서비스 마비 버그 수정 (THREAT-07)**
    - [x] `db_manager.hpp`의 `getConnection()`에서 `mysql_ping()` 실패 후 `createConnection()` 재연결이 실패(nullptr 반환)하면 해당 풀 슬롯이 영구 소실되는 버그 발견 및 실제 재현 (Docker 환경에서 MariaDB 컨테이너 재시작 후 전체 API 응답 불가 상태 확인, exit code 137로 간접 확인) — 코드 흐름(pop_back → ping 실패 → createConnection 재실패 → nullptr → releaseConnection의 guard에서 소실) 전체 메커니즘 학습 및 이해 완료
    - [ ] 재연결 실패 시 풀 슬롯을 잃지 않고 재시도하거나, 최소한 `nullptr` 반환 시 호출자가 이를 감지해 사용자에게 503 등 명확한 에러를 반환하도록 방어 로직 설계 (현재는 워커 스레드가 `pool_cv.wait()`에서 영구 대기하며 cpp-httplib 공용 스레드 풀 전체를 고갈시킴)
    - [ ] 임시 복구 조치: `docker compose restart app`으로 pool 재초기화 확인

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

- [ ] **Docker 전용 빌드 전환에 따른 `main.cpp` OS 조건부 레거시 코드 정리 여부 결정 (THREAT-06-Followup)**
    - [ ] `_WIN32`/`__APPLE__` 분기(Job Object 메모리 제한, Mach 커널 API 기반 `printMemoryUsage` 등)가 Docker-only 전환 이후 실제 빌드 경로에서는 전혀 컴파일되지 않는 죽은 코드가 됨 — 유지(향후 네이티브 빌드 대비) vs 완전 제거(Linux 전용 단순화) 결정 필요
    - [ ] 유지하기로 할 경우, 로컬 clangd/IntelliSense가 실제 빌드 타겟(Linux)이 아닌 Windows 분기를 분석 대상으로 삼고 있어 버그가 에디터에서 안 걸리고 숨을 수 있다는 리스크를 인지하고 있을 것 (Linux 전용 `mach/mach.h` 컴파일 실패로 처음 발견된 사례 참고)

- [ ] **`RLIMIT_AS` 상향(128MB→512MB)에 따른 THREAT-05(DoS) 실습 조건 재설계 (THREAT-05-Followup)**
    - [ ] 기존 128MB 한도가 idle 상태 스레드 스택 오버헤드만으로 거의 소진되어 신규 연결 스레드의 스택 할당이 실패, TLS 핸드셰이크가 응답 없이 무한 행(hang)되는 실제 장애로 이어짐을 확인(2026-07-31) → 임시 조치로 512MB 상향하여 정상화됨(상세: `CONTEXT.md` 3.1-8)
    - [ ] 512MB는 정상 가동 확보를 위한 응급 조치이며 DoS 방어선으로는 느슨함 — Ochlos 공격 실습(`memory_exhaustion.py`) 재설계 시 이 상향된 한도 기준으로 공격 강도/시나리오 재산정 필요
    - [ ] 근본 대안 검토: ① 스레드 스택 크기를 `pthread_attr_setstacksize` 등으로 명시적으로 축소해 스레드당 오버헤드 자체를 줄이는 방향, ② Docker `cgroups` 기반 물리 메모리 제한(`mem_limit`)으로 전환하고 `RLIMIT_AS`는 보조 수단으로 격하 — 512MB 고정 상향 대신 근본 해결책 채택 여부 논의 필요

