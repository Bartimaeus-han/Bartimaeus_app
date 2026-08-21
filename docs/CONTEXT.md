# Project Context & Development History

이 문서는 AI가 프로젝트의 기술 사양 및 그동안의 빌드/보안 패치 변경 이력을 정확히 파악하여, 개발 진행 과정에서 혼동 없이 개발 컨텍스트를 유지할 수 있도록 돕는 장기 메모리 파일입니다.

---

## 1. 프로젝트 기술 사양 (Project Tech Stack)

* **기반 언어**: C++20 이상 (MSVC 18 최신 표준 라이브러리 지원 및 clangd 파싱 호환성을 위해 C++20으로 상향)
* **서버 라이브러리**: `cpp-httplib` (헤더 온리 라이브러리)
* **데이터베이스**: MySQL (MySQL C API 직접 연동, Connection Pool 구현) — 기존 SQLite3(`server.db`)에서 마이그레이션 완료
* **빌드 시스템**: CMake (macOS/Unix 및 Windows 크로스 플랫폼 지원 구조)
* **동작 포트**: `9090` 포트 (`0.0.0.0:9090` 리스닝)
* **동작 제어**: (2026-08-01부로 변경) 로컬 개발도 Docker 전용으로 전환, `docker compose up --build`로 빌드/실행 자동화. 기존 `run.ps1`/`run.sh` 네이티브 빌드 스크립트는 더 이상 사용하지 않음(파일 보관 여부는 별도 결정 사항, 상세: `TODO.md`)

---

## 2. Git 커밋 히스토리 및 보안 패치 타임라인 (Git History & Security Timeline)

| 커밋 해시 | 관련 영역 / 파일 | 작업 구분 | 주요 변경 사항 및 보안 방어 맥락 (Security Context) |
| :--- | :--- | :--- | :--- |
| **`a28e24b`** | [main.cpp](../src/main.cpp) | **Feat (최초 생성)** | `cpp-httplib`를 이용한 웹 서버 기초 뼈대 구축 및 `/signup`, `/login`, `/api/users` 라우트 최초 정의 |
| **`5fb82bc`** | `public/login.html` | **Feat** | 정적 리소스 마운트 폴더 내 로그인 페이지 신설 |
| **`b074924`** | `src/*`, `public/` | **Bug (취약성)** | 회원가입/로그인 기능 및 관리자 페이지 추가. 당시는 패스워드 평문 전송/저장, 사용자 ID 기반 무방비 쿠키 검증 등 대다수 취약점이 방치된 초기 버전 |
| **`d5182f8`** | [main.cpp](../src/main.cpp) | **Refactor** | `SIGINT(Ctrl+C)` 시그널 처리를 도입하여 안전하게 서버 루프를 중지시키는 **Graceful Shutdown** 구현 |
| **`d6614db`** | `src/db_queries.hpp` | **Feat** | SQLite3 연동 시작 및 애플리케이션 기동 시 `users` 테이블 자동 생성 로직 설계 |
| **`20e6938`** | `src/db_queries.hpp` | **Refactor** | 기존 인메모리 방식 데이터베이스(`user_db`)를 완전히 걷어내고 실물 파일 기반 SQLite3 연동 완성 |
| **`984a0e7`** | `public/index.html` | **Feat** | 대시보드 내 관리자 페이지(admin) 진입 버튼을 로그인 유저 중 `admin` 역할을 가진 계정에만 동적 표시하도록 제어 |
| **`e1fee2b`** | [auth_service.hpp](../src/services/auth_service.hpp) | **Security (SQLi)** | 사용자 입력값 우회 취약점 방어. `sqlite3_prepare_v2` 및 `sqlite3_bind_text`를 적용해 **Prepared Statements** 구현하여 **SQL Injection 원천 차단** |
| **`4b70cc2`** | `EXCEPTIONS.md` | **Docs** | 예외 처리 표준 및 에러 대응을 위한 가이드 문서 추가 |
| **`727f967`** | `PROJECT_GUIDELINES.md`| **Docs** | AI 개발 협업 가이드라인 최초 작성 |
| **`a081dc4`** | [main.cpp](../src/main.cpp) | **Security (Crypto)** | 패스워드를 DB 저장/비교 시 **SHA256 해시 암호화** 적용. 로그인 여부에 따라 `/index.html` 또는 `/login.html`로 루트 `/` 분기 및 리다이렉트 구조 도입 |
| **`cf43543`** | [session_manager.hpp](../src/services/session_manager.hpp) | **Security (Session)** | 단순 쿠키 검증에 의한 권한 탈취 취약점 해결. `SessionManager` 설계 후 난수 토큰 기반의 화이트리스트 세션 검증 도입, 세션 쿠키에 `HttpOnly`를 심어 **Session Hijacking/Fixation 방어** |
| **`bf00409`** | [auth_service.hpp](../src/services/auth_service.hpp) | **Security (Info)** | `/api/users` 데이터 조회 시 사용자 비밀번호 해시 필드(`password`)가 외부로 흘러나가지 않도록 조회 쿼리 및 구조체 바인딩에서 패스워드 제외 조치 |
| **`8592d76`** | [main.cpp](../src/main.cpp) | **Chore (Build)** | macOS/Unix 지원을 위해 하드코딩된 Windows 툴체인 경로 제거 및 LF 개행 문자 강제. 서버 실행 기본 포트를 `8080`에서 **`9090`**으로 변경 |
| **`5b122ed`** | [auth_controller.hpp](../src/controllers/auth_controller.hpp) | **Security (XSS)** | 사용자 입력 정보 및 JSON 응답 데이터 유출 제어를 위해 C++ 레벨에서 **`jsonEscape`** 필터링 적용 (Stored XSS 1차 대응) |
| **`d9ccc64`** | `CMakeLists.txt` | **Chore (Build)** | CMake 빌드 구성 및 크로스플랫폼 타겟 결과물 빌드 구조 최적화 |
| **`7f3e9d2`** | [main.cpp](../src/main.cpp) | **Security (Headers)** | HTTP 응답 시 **보안 헤더 일괄 탑재** (`Content-Security-Policy`, `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `X-XSS-Protection: 1; mode=block`) |
| **`5b737d4`** | `public/*.js`, `public/*.html` | **Security (CSP)** | 인라인 스크립트를 완전히 걷어내고 별도 `.js` 정적 파일로 스크립트를 분리하여 **CSP(콘텐츠 보안 정책)와의 호환성 확보 및 XSS 공격 차단** |
| **`4a2b212`** | `src/*` | **Refactor** | 기존에 남아있던 취약한 쿼리 관련 잔재 파일 정리 및 `src/controllers`, `src/services` 형태의 패키지 아키텍처 재정립 |
| **`5e759c5`** | `CMakeLists.txt` | **Refactor** | 크로스 컴파일 호환성 강화를 위해 빌드 시스템 내부 스크립트 수정 및 링커 설정 보강 |
| **`80a23eb`** | `src/*`, `CMakeLists.txt` | **Security (RateLimit)** | 자동화 무차별 대입 공격(Brute Force) 방어를 위해 ID 기반의 로그인 시도 차단 정책(`LoginLimiter`) 도입 및 `AuthController` / `main.cpp` 연동 |
| **`c2fa066`** | `src/*`, `public/js/*` | **Security (CSRF)** | 동적 세션 토큰 방식의 Anti-CSRF 방어를 도입하여 세션 정보 반환(`/api/me`) 시 토큰을 발급하고, 로그아웃(`/logout`) POST 요청 시 헤더 검증 수행 |
| **`463e4ee`** | `src/*`, `public/error.html` | **Security (ErrorPage)** | 상세 에러 정보 유출 방지를 위한 무작위 Error Tracking ID 매핑, 외부 HTML 템플릿 연동, API(/api/*) 경로 JSON 응답 및 이중 로그(콘솔/파일) 기록 적용 |
| **`87a9d31`** | [helpers.hpp](../src/helpers.hpp), [auth_controller.hpp](../src/controllers/auth_controller.hpp) | **Security (Cookie)** | 쿠키 키 이름 부분 일치 우회 방지를 위해 `;` 구분자 기반 분할 및 공백 trim을 적용한 안전한 쿠키 파싱 구현 및 중복 코드 리팩토링 |
| **`53e3c7f`** | `src/*`, `CMakeLists.txt` | **Refactor** | 중복되는 세션/CSRF 검증 코드를 공통 횡단 관심사로 통합하기 위해 C++ 함수형 스타일의 보안 미들웨어([middleware.hpp](../src/middleware.hpp)) 도입 및 컨트롤러 핸들러 슬림화 리팩토링 수행 |
| **`825a689`** | `src/*`, `public/js/*` | **Refactor (AdminDelete)** | 관리자가 다른 사용자의 글을 삭제할 수 있도록 권한 정책 변경(Option 1) 및 클라이언트 측 confirm() 삭제 재확인 로직 추가 |
| **`7d4dffc`** | [main.cpp](../src/main.cpp) | **Security (DoS)** | Payload 자원 고갈 공격 방어를 위한 최초의 프로세스 메모리 제한(`setrlimit` 기반 process memory restriction) 로직 도입 — 이후 RLIMIT_AS VSS 소진 장애(3.1-8 참고)의 근본 원인이 된 코드의 최초 도입 지점 |
| **`39449a1`** | `src/*` | **Security (IN)** | 서비스 계층 및 SQL 수준에서 게시글 삭제 권한 검증 이중화 적용 (Defense in Depth) |
| **`df88187`** | `src/*`, `CMakeLists.txt` | **Security (Argon2id)** | 패스워드 해싱 알고리즘을 SHA-256에서 Argon2id(32MB, 3-pass)로 업그레이드 완료하고, 로그인 시 레거시 사용자의 해시를 실시간 자동 전환(Lazy Migration)하는 과도기 대응 로직 구현 |
| **`ce7256a`** | `src/*` | **Security (XSS)** | 게시글 제목, 본문, 작성자 출력 시점에 HTML Entity Encoding(`htmlEscape`)을 적용하여 세션/브라우저 상태와 관계없이 Stored XSS 공격면을 원천 차단하는 이중 방어 패치 적용 |
| **`a4c6464`** | [session_manager.hpp](../src/services/session_manager.hpp), [helpers.hpp](../src/helpers.hpp) | **Security (CSPRNG)** | 세션 ID 및 CSRF 토큰 생성 엔진을 취약한 `std::mt19937`에서 OpenSSL `RAND_bytes` 기반 CSPRNG로 패치하여 예측 가능성을 완전히 배제하고 엔트로피를 256비트로 확장. 또한 `helpers.hpp` 내의 한글 주석을 영문으로 치환하여 MSVC C4819 컴파일 경고 해결 |
| **`c06d93e`** | [main.cpp](../src/main.cpp), [memory_exhaustion.py](../Ochlos/scripts/memory_exhaustion.py) | **Security (DoS)** | macOS/Linux 호환을 위한 `setrlimit` 기반 메모리 할당 제한(RLIMIT_AS) 및 `mach_task_self()` Resident/Footprint 메모리 측정 구현. 공격 실습용 Python DoS 스크립트 작성 및 서버 메모리 제한을 128MB에서 35MB로 조정하여 DoS 취약점 증명 환경 마련 |
| **`b17da27`** | [auth_service.hpp](../src/services/auth_service.hpp), [board_service.hpp](../src/services/board_service.hpp), [sqlite_concurrency.py](../Ochlos/scripts/sqlite_concurrency.py) | **Security (Hardening)** | SQLite3 동시성 경합 취약점(THREAT-04)을 다중 서비스 동시 타격(가입 & 글쓰기 루프)으로 자연적 유도 검증 성공 후, 각 서비스 생성자에 `sqlite3_busy_timeout(db, 1000)` 대기 설정을 주입해 락 충돌 장애율을 0%로 완벽 방어 |

| **`b8ade32`** | `src/*` | **Security (Hardening)** | C++ 백그라운드 GC 스레드(std::thread)를 도입하여 만료된 세션 및 로그인 실패 이력을 10초 주기로 청소하고, 로그인 진입로(handleLogin)의 사용자명 최대 길이(32자) 선제 입력값 검증 추가. 이진 탐색 기법을 이용한 공격 페이로드 최적화 및 방어 검증 완료 |
| **`d1422c9`** | [main.cpp](../src/main.cpp) | **Refactor (Shutdown)** | C++20 `std::jthread` 및 `std::stop_token`을 적용하여 백그라운드 GC 스레드의 수명 주기를 RAII로 보장하고, `main` 함수 전역에 try-catch 예외 처리 블록을 씌워 예외 상황에서의 안전한 서버 종료 및 리소스 자동 해제 구조 구축 완료 |
| **`d1422c9`** | [main.cpp](../src/main.cpp) | **Security (Hardening)** | HTTP 라우팅 핸들러 내부 예외 격리를 위해 `set_exception_handler`를 등록하고, 예외 발생 시 개별 요청 수준에서 500 에러와 임의 에러 추적 ID(Tracking ID)를 반환하도록 예외 격리(Isolating Exceptions) 처리 완료 |
| **`157990d`** | [db_manager.hpp](../src/services/db_manager.hpp), `CMakeLists.txt` | **Feat (MySQL 마이그레이션 시작)** | CMake 빌드에 MySQL 클라이언트 라이브러리 최초 연동 및 `DbManager` 커넥션 풀 클래스(Singleton + `std::vector<MYSQL*>` 풀 + `condition_variable` 대기) 최초 구현 |
| **`0fb08d6`** | [auth_service.hpp](../src/services/auth_service.hpp) | **Refactor (마이그레이션 과도기)** | `AuthService`가 SQLite3와 MySQL을 과도기적으로 병행 지원하도록 리팩토링 |
| **`eec4bc3`** | [auth_service.hpp](../src/services/auth_service.hpp), `db_queries.hpp` | **Refactor (MySQL)** | Argon2id 해시가 자체 salt를 내장하므로 별도 DB `salt` 컬럼 삭제, 로그인/회원가입 로직을 MySQL C API(`mysql_stmt_*` Prepared Statement)로 전환 |
| **`8da547b`** | [board_service.hpp](../src/services/board_service.hpp), `docker/init.sql` | **Refactor (MySQL)** | `BoardService`를 MySQL C API로 전환하고 Docker용 초기 스키마(`init.sql`) 최초 작성 |
| **`8f1c062`** | `3rdparty/mysql/include/*` | **Chore (Build)** | Windows 포팅을 위한 MySQL C API 벤더 헤더(`mysql.h`, `mysql_com.h` 등)를 프로젝트 내에 직접 포함 |
| **`88a2be0`** | `CLAUDE.md`, `.agents/AGENTS.md` | **Chore (Tooling)** | Antigravity IDE 전용 `AGENTS.md` 제거 및 `CLAUDE.md` 체제로 AI 협업 규칙 통합 시작 |
| **`5a5c426`** | `docker-compose.yml`, `docker/init.sql` | **Feat (Infra)** | MySQL(MariaDB) 컨테이너 기동을 위한 `docker-compose.yml` 신설 및 초기 스키마 DB 이름 수정 |
| **`ce387d1`** | `CMakeLists.txt`, [auth_service.hpp](../src/services/auth_service.hpp) | **Chore (Build)** | MySQL C API 연동 마무리 — CMake `POST_BUILD` 단계로 `libmysql.dll` 등 런타임 DLL 자동 배포, Windows 포팅용 `mysql.h`/`mysql_com.h` 수정 |
| **`88a3329`** | `docker-compose.yml`, [main.cpp](../src/main.cpp) | **Refactor (Infra)** | SQLite3(`server.db`) 완전 제거 및 MariaDB Docker 컨테이너 기반 인프라로 전환 |
| **`645f02f`** | [db_manager.hpp](../src/services/db_manager.hpp) | **Bug/Security (THREAT-07)** | DB 컨테이너 재시작 시 커넥션 풀의 기존 연결이 무효화되는 문제 발견, `getConnection()`에 재연결 재시도(5회, 500ms 간격) 로직 추가 — 단, 전체 재시도 실패 시 풀 슬롯이 영구 소실되는 근본 문제는 미해결 (상세: [TODO.md](TODO.md)) |
| **`c5bacdf`** | [main.cpp](../src/main.cpp) | **Bug/Security (THREAT-05-Followup)** | `RLIMIT_AS` 128MB 한도가 유휴 상태 스레드 스택만으로 소진되어 HTTPS 연결이 무한 대기(hang)되는 장애 발생 확인, 512MB로 임시 상향 (상세: 3.1-8) |
| **`a147d15`** | [main.cpp](../src/main.cpp), `docker-compose.yml` | **Refactor (THREAT-05-Followup 완료)** | OS 레벨(`RLIMIT_AS`) 물리 메모리 제한의 한계(VSS 기준이라 실제 RSS 압박과 상관관계 약함, 3.1-8 참고)를 확인하고 결정(2026-08-01)한 대로, `main.cpp`의 `limitProcessMemory()`/`printMemoryUsage()` 함수와 그 호출부를 완전히 제거하고 `docker-compose.yml`의 `app` 서비스에 `mem_limit: 2g` cgroups 제한을 추가하여 물리 메모리 제한 책임을 Docker 레벨로 전환 |
| **`6e65963`** | [auth_service.hpp](../src/services/auth_service.hpp) | **Bug/Security (THREAT-08)** | `login()`의 파라미터 바인딩용 `MYSQL_BIND bind[1]`에 `memset` 초기화가 누락되어 `is_null` 등 미초기화 포인터 필드가 스택 쓰레기 값으로 남아있었고, `mysql_stmt_bind_param()` 호출 시 `libmariadb.so.3` 내부에서 이를 역참조하며 General Protection Fault(SIGSEGV) 발생 — 유저 존재 여부와 무관하게 `/login` 요청마다 100% 서버 크래시로 이어지는 치명적 가용성 버그였음(2026-08-01/02). `docker inspect`의 ExitCode 139 및 `dmesg` 커널 트랩 로그(`libmariadb.so.3+0x2bbef`, 매 크래시 동일 오프셋)로 재현성을 실증 확인한 뒤 `memset(bind, 0, sizeof(bind));` 한 줄 추가로 수정 (상세: `EXCEPTIONS.md` 2.1) |
| **`4625237`** | [auth_service.hpp](../src/services/auth_service.hpp), [auth_controller.hpp](../src/controllers/auth_controller.hpp), [main.cpp](../src/main.cpp) | **Security/Bug (THREAT-07 완료)** | DB 커넥션 풀 고갈 버그(THREAT-07)의 마지막 잔여 항목 마무리. ① `~DbManager()` 소멸자의 `pool_healer.request_stop()+join()` → `destroyPool()` 순서를 실제 종료 시나리오(`docker compose stop app`, SIGTERM)로 실증 검증 — 크래시 없이 ExitCode 0 확인. ② `auth_service.hpp`의 `signUp()`/`login()`을 `bool` 대신 `AuthResult{Success,Fail,DbError}` enum 반환으로 전환하고 `auth_controller.hpp`를 `switch`문으로 분기시켜, DB 연결 실패(503)와 비즈니스 로직 실패(아이디 중복→409, 인증 실패→401)를 최초로 구분함 — `docker compose stop mariadb`로 실제 DB 다운 상태를 만들어 503 응답과 이후 `pool_healer`의 자동 복구까지 curl로 실증 검증. ③ 이 과정에서 [main.cpp](../src/main.cpp)의 `svr.set_error_handler`가 상태 코드 400 이상이면 컨트롤러/미들웨어가 이미 채운 `res.body`를 무조건 덮어써서(`httplib.h`의 `400 <= res.status && error_handler_` 조건이 본문 존재 여부를 안 가림), 방금 나눈 401/409/503 메시지는 물론 `middleware.hpp`의 401/403 메시지까지 전부 범용 문구로 뭉개지고 있던 기존 구조적 버그를 발견 — `if (!res.body.empty()) return;` 가드로 해결. 겸사겸사 `is_api_request` 판별 조건에 `/login`,`/signup`,`/logout`(비-`/api` 접두사 JSON 라우트)도 추가해 이 세 라우트가 JSON 에러 응답을 받도록 수정 |
| **`Pending`** | `ScreeningRouter/*`, `Ochlos/*`, `Dockerfile`, `docker-compose.yml`, `CMakeLists.txt` | **Feat/Security (New-Firewall Phase 1 완료)** | ① `ScreeningRouter` L3/L4 실시간 로깅 게이트웨이 구축 및 Docker 멀티 스테이지 통합(`bartimaeus-screening-router`:8080 ➔ `bartimaeus-app`:9090) 완료 (`std::unitbuf` 실시간 로깅 검증). ② `Ochlos`를 서버 빌드 트리에서 완전히 독립 분리(`project(Ochlos)`)하고 단일 명령어 `crun` 파이프라인 구축. ③ `tcp_syn_flooding`으로 50개 동시 연결 점유 타격 시 스크리닝 라우터의 L3/L4 실시간 인입 로깅 실증 완료. |

---

## 3. CMake & 빌드 환경 트러블슈팅 히스토리 (C++20, Ninja, MSVC 연동)

### 3.1 AI 실책 및 교훈 (Mistakes & Lessons Learned)
향후 개발 세션 진행 시 동일한 혼선과 시간 낭비를 방지하기 위해 AI가 저지른 판단 오류와 교훈을 명확히 기록합니다.

1. **에디터 설정 선제 검증 누락**:
   * 현상: 사용자가 에디터 내에서 `CMake: Select a Kit` 명령어 자체가 아예 뜨지 않는다고 호소함.
   * 원인: `.vscode/settings.json` 파일에 `"cmake.useCMakePresets": "always"`로 설정되어 프리셋 모드가 강제 구동 중이었던 점을 사전에 탐색하지 않아 불필요한 해결책을 반복 제시함.
2. **Ninja 제너레이터의 아키텍처 지원 오해**:
   * 현상: `CMakePresets.json`에 무턱대고 `"architecture": "x64"`를 기입했다가 `Generator Ninja does not support platform specification` 에러를 유발함.
   * 원인: 이 옵션은 Visual Studio 제너레이터 전용 옵션이며, Ninja 빌드 도구는 직접 아키텍처 명시를 해석하지 못하고 환경 변수에 전적으로 의존한다는 기본 제약을 망각함.
3. **캐시 오염 파악 지연**:
   * 현상: 설정 파일 롤백 후 빌드를 돌렸으나 계속 컴파일러를 찾지 못하는 동일 에러 반복 발생.
   * 원인: `build_win/CMakeCache.txt`에 기록된 오염된 이전 빌드 설정을 강제로 날리지 않으면 동일한 값으로 계속 시도된다는 점을 간과하여 한 템포 늦게 캐시 클린 명령(`CMake: Delete Cache and Reconfigure`)을 제공함.
4. **UI 화면 정보 환각 (Hallucination)**:
   * 현상: 스크린샷 내 깃 동기화 화살표(`↻`)를 CMake 버튼으로 우기고, 있지도 않은 `[No Active Target]` 텍스트가 상태바에 표시되어 있다고 강변함.
   * 원인: 에디터의 최신 상태바 디자인 기본값 및 Git 플러그인 레이아웃을 확실히 확인하지 않고 지레짐작하여 설명하여 사용자에게 극심한 혼란을 안김.
5. **clangd MSVC 드라이버 모드 인자 해석 및 헤더 누출 에러**:
   * 현상: `.cpp` 파일에서만 C++ 표준 라이브러리(`std::ofstream` 등)에 대한 자동완성 드롭다운이 작동하지 않고 `0 results from Sema`가 반환됨.
   * 원인: 
     * **표준 버전 불일치**: MSVC 18.0 (v19.50)의 최신 표준 라이브러리 헤더들은 C++20 기반으로 작성되어 있어, clangd가 C++17 규격으로 해석할 때 파싱 오류(`incomplete due to errors`)가 누적되어 표준 라이브러리 인덱스가 대거 유실됨.
     * **인자 해석 오류**: `.clangd` 파일에서 `-isystem` 플래그와 경로명을 줄바꿈하여 리스트 요소로 분리해 적었더니, MSVC 드라이버 모드(`--driver-mode=cl`)로 실행 중이던 clangd가 경로명 문자열을 플래그의 파라미터가 아니라 **"빌드 대상 C++ 소스 파일"**로 잘못 인식하여 폴더 자체를 빌드 타겟으로 삼아 구문 분석에 실패함.
   * 해결책:
     * `CMake` 표준 및 `.clangd` 표준을 모두 C++20으로 상향 일치시킴.
     * `.clangd` 파일의 인클루드 경로 지정을 공백이나 줄바꿈 없이 `-imsvc<경로>` 및 `-I<경로>` 형식의 단일 문자열 토큰으로 작성하여 전달함.
6. **Winsock2 및 MySQL 헤더 순서 충돌 (`ws2tcpip.h` 컴파일 오류)**:
   * 현상: `.\run.ps1` 빌드 시 `ws2tcpip.h`에서 `SourceList`, `MULTICAST_MODE_TYPE`, `PIP_MSFILTER` 미선언 구문 에러 발생.
   * 원인: Windows 환경에서 `mysql.h`가 `WIN32_LEAN_AND_MEAN` 매크로 없이 `<windows.h>`를 먼저 끌어오면서 레거시 Winsock v1(`winsock.h`)이 인클루드되었고, 이로 인해 이후 `cpp-httplib`의 `<ws2tcpip.h>`와 네트워크 심볼 충돌 유발.
   * 해결책: [main.cpp](../src/main.cpp) 및 [db_manager.hpp](../src/services/db_manager.hpp) 최상단에 `WIN32_LEAN_AND_MEAN` 정의 및 `<winsock2.h>`, `<ws2tcpip.h>` 선제 선언을 추가하여 충돌 원천 차단.
7. **MySQL C API 전환 직후 실행 파일 즉시 종료 (`STATUS_DLL_NOT_FOUND`, 0xC0000135)**:
   * 현상: `.\run.ps1` 빌드는 에러 없이 성공하지만, 빌드된 `SecureWebServer.exe`를 실행하면 아무 로그도 없이 즉시 종료됨.
   * 원인: `dumpbin /dependents`로 확인한 결과, MySQL C API 링크로 새로 추가된 `libmysql.dll`과 conda 환경의 `libssl-3-x64.dll`/`libcrypto-3-x64.dll`이 exe 옆에도, 시스템 PATH에도 없어 Windows 로더가 프로세스를 기동 즉시 강제 종료시킴. 컴파일/링크 단계는 정상이라 빌드 로그만으로는 원인이 드러나지 않았음.
   * 해결책: [CMakeLists.txt](../CMakeLists.txt)에 `WIN32` 조건부 `POST_BUILD` 커스텀 커맨드를 추가하여, 빌드 완료 시마다 위 3개 DLL을 `${OPENSSL_ROOT_DIR}/bin` 및 `MYSQL_LIBRARIES`가 위치한 디렉터리에서 exe 출력 디렉터리로 자동 복사하도록 구성. PATH를 영구로 건드리지 않는 xcopy 배포 방식.

8. **`RLIMIT_AS` 128MB 소진으로 인한 HTTPS 연결 무한 행(Hang) 장애 (2026-07-31)**:
   * 현상: `docker compose up`으로 컨테이너(app, mariadb) 모두 정상 `Up` 상태였고 포트 매핑(9090)과 TCP 3-way handshake(`Test-NetConnection`)까지는 성공했으나, 실제 HTTPS 요청(`curl`)은 매번 응답 없이 타임아웃됨. 컨테이너 로그상 서버 기동 자체는 정상 출력됨.
   * 원인: TODO.md에 사전 기록되어 있던 `RLIMIT_AS`(128MB) VSS 소진 우려([TODO.md](TODO.md) 참고)가 실제 장애로 발현. Idle 상태에서 이미 128MB 한도의 97%가 스레드 스택 예약분으로 소진된 상태였기 때문에, 신규 연결을 처리할 스레드의 스택 할당이 조용히 실패 — TCP accept 자체는 OS 레벨에서 성공하지만 TLS 핸드셰이크를 처리할 스레드가 뜨지 못해 클라이언트 입장에서는 명확한 에러(Connection Refused 등) 없이 무한 대기(hang)로 관측됨.
   * 해결책(임시): [main.cpp](../src/main.cpp) `limitProcessMemory()` 호출 인자를 128MB에서 **512MB로 상향**하여 스레드 스택 오버헤드 여유분을 확보, 정상 연결 재개 확인. 근본적인 스레드 스택 크기 축소나 cgroups 기반 물리 메모리 제한 전환 등은 후속 과제로 [TODO.md](TODO.md)에 재기재됨.

### 3.2 현재 프로젝트 빌드 설정 값
* **빌드 제너레이터**: `Ninja` (최종 타겟 실행 파일: `SecureWebServer.exe`)
  * `CMAKE_EXPORT_COMPILE_COMMANDS`를 통해 `compile_commands.json`을 강제 활성화하여 `clangd` 언어 서버가 정상 동작하도록 보장함.
* **컴파일러**: `MSVC 19.50` (64비트 x64) - C++20 표준 활성화
* **IDE (Antigravity IDE) 연동 방식**:
  * `.vscode/settings.json` 내 `"cmake.useCMakePresets"`는 `"never"`로 비활성화하여 **Kit 모드**를 활성화.
  * `"cmake.generator"`는 `"Ninja"`로 고정.
  * `CMake: Select a Kit` 메뉴에서 **`Visual Studio Community Release - amd64`**를 수동 선택하여 빌드 환경을 구축함.
* **명령줄(Terminal)에서의 직접 빌드**:
  * `x64 Native Tools Command Prompt for VS` terminal을 열고 `cmake --preset windows-default`를 통해 64비트 MSVC를 타겟으로 1회 구성한 뒤, 이후에는 일반 terminal에서도 `cmake --build build_win` 명령어로 빌드 가능.

