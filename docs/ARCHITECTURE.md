# Project Architecture & Security Patterns

이 문서는 AI가 프로젝트의 전체적인 폴더 구조의 논리적 의미, 적용된 아키텍처 패턴, 그리고 핵심적인 보안 아키텍처 구현 방식을 오해 없이 이해하도록 돕는 장기 메모리 파일입니다.

---

## 1. 디렉토리 구조 및 역할 (Directory Structure)

프로젝트 루트 아래의 각 폴더 및 주요 파일들의 역할 분담은 다음과 같습니다.

* **`src/`**: 백엔드 C++ 소스 코드가 위치하는 디렉토리
  * **[main.cpp](../src/main.cpp)**: 애플리케이션 진입점. 서버 생성, 라우팅 정의, 전역 시그널 처리 및 기본 보안 HTTP 헤더 탑재를 총괄합니다.
  * **`controllers/`**: HTTP 요청 분석 및 응답 처리 계층
    * [auth_controller.hpp](../src/controllers/auth_controller.hpp): 로그인, 로그아웃, 회원가입, 사용자 정보 조회 엔드포인트 제어 및 응답 JSON Escaping 처리.
    * [board_controller.hpp](../src/controllers/board_controller.hpp): 게시글 작성/조회/상세조회/삭제 엔드포인트 제어.
  * **`services/`**: 핵심 비즈니스 로직 및 외부 연동(DB, 세션 관리 등) 계층
    * [auth_service.hpp](../src/services/auth_service.hpp): 사용자 가입/로그인 로직, Argon2id 해싱 처리, MySQL Prepared Statement 쿼리 실행.
    * [board_service.hpp](../src/services/board_service.hpp): 게시글 CRUD 로직 및 MySQL Prepared Statement 쿼리 실행.
    * [db_manager.hpp](../src/services/db_manager.hpp): MySQL 커넥션 풀(Singleton) 관리자. 연결 획득/반환, 초기 연결 재시도, 핑 실패 시 재연결을 담당 (상세: 본 문서 3.12).
    * [session_manager.hpp](../src/services/session_manager.hpp): 난수 기반 세션 ID 발급 및 메모리 기반 화이트리스트 세션 테이블 검증.
    * [login_limiter.hpp](../src/services/login_limiter.hpp): 로그인 실패 횟수 누적 및 임시 계정 잠금 정책 관리.
  * **`db_queries.hpp`**: MySQL Prepared Statement 쿼리 문자열 상수 모음 (`Queries::` 네임스페이스).
  * **`middleware.hpp`**: `requireAuth`, `requireAuthAndCsrf`, `requireAdmin` 등 공통 보안 미들웨어 필터.
* **`public/`**: 웹 프론트엔드 정적 리소스 (HTML, JS, CSS)
  * [index.html](../public/index.html) / `admin.html` / `login.html`: 각각의 화면 템플릿.
  * [error.html](../public/error.html): 플레이스홀더 방식의 공용 에러 페이지 템플릿 (상세 정보 노출 차단).
  * `*.js` 및 `*.css` 정적 스크립트/스타일시트 파일 (보안 CSP 준수를 위해 인라인 코드 배제).
* **`docker/init.sql`**, **`docker-compose.yml`**, **`Dockerfile`**: MariaDB 및 앱 컨테이너 기반 인프라 정의 (기존 SQLite3 `server.db` 파일은 완전히 제거되고 이 구조로 대체됨, 상세: [CONTEXT.md](CONTEXT.md)).
* **`CMakeLists.txt`**: C++ 빌드 환경설정 정의.

---

## 2. 디자인 패턴 및 아키텍처 (Design Patterns & Architecture)

* **Layered Architecture (계층형 아키텍처)**:
  * 클라이언트의 HTTP 요청은 **Controller 계층**(`auth_controller`)에서 입력값 유효성 검증과 JSON 출력을 처리합니다.
  * 실제 비즈니스 로직 및 영속성 처리는 **Service 계층**(`auth_service`, `session_manager`)으로 철저하게 격리하여 비즈니스 논리와 통신 프로토콜을 분리하고 있습니다.
* **Graceful Shutdown (우아한 종료)**:
  * [main.cpp](../src/main.cpp) 내 전역 `httplib::Server` 주소를 매핑하여 `SIGINT` 시그널이 오면 소켓을 올바르게 종료합니다. MySQL 커넥션 풀(`DbManager`)은 이 시그널 핸들러와 무관하게, 프로그램 종료 시점의 Meyer's Singleton 정적 소멸자(`~DbManager`)에서 `destroyPool()`이 호출되어 모든 연결을 정리합니다.
* **Connection Pool (커넥션 풀 / Object Pool + Singleton)**:
  * [db_manager.hpp](../src/services/db_manager.hpp)의 `DbManager`는 Meyer's Singleton으로 구현되어 있으며, 내부적으로 `std::vector<MYSQL*>` 풀과 `std::mutex`+`std::condition_variable`을 사용해 스레드 안전하게 연결을 대여(`getConnection`)/반환(`releaseConnection`)합니다. 각 서비스는 요청 처리 시작 시 연결을 빌리고 끝에서 반드시 반환하는 수동 체크아웃 패턴을 사용합니다 (상세 및 알려진 한계: 본 문서 3.12).

---

## 3. 핵심 보안 구현 패턴 (Key Security Implementation Patterns)

이 프로젝트는 학습 목적에 맞추어 아래의 보안 패턴이 철저히 준수되어 구현되어 있습니다.

### 3.1 SQL Injection 방어
* **구현 방식**: 모든 사용자 입력 변수(`username` 등)는 문자열 접합(Concatenation) 없이 MySQL C API의 `mysql_stmt_init`/`mysql_stmt_prepare`로 준비된 구문에 `MYSQL_BIND` 구조체와 `mysql_stmt_bind_param`을 통해 바인딩됩니다 (**Prepared Statements**).
* **목적**: 쿼리의 제어 구조(구문 분석 트리)와 데이터를 사전에 명확하게 구분하여 임의의 악성 페이로드가 SQL 문법으로 해석되는 것을 근본적으로 원천 차단합니다.

### 3.2 세션 및 쿠키 조작 방어
* **구현 방식**: 
  1. 클라이언트 측에 단순 노출되던 회원 ID 쿠키를 폐기하고, **OpenSSL의 `RAND_bytes` API**를 사용하여 생성한 암호학적으로 안전한 256비트 엔트로피의 **무작위 난수 세션 ID**(64자 16진수 문자열)를 쿠키로 발급합니다.
  2. 동일하게 무작위 난수 생성 함수를 통해 생성된 **안전한 CSRF 토큰**을 세션에 바인딩합니다.
  3. 서버 측 `SessionManager` 메모리 내에서만 유효 세션 테이블을 관리(White-list 방식)합니다.
  4. 로그인 시 기존 세션을 지우고 새 세션을 할당하여 **Session Fixation** 공격을 방지하며, 세션 쿠키 발급 시 **`HttpOnly`** 및 **`Secure`** 플래그를 설정하여 스크립트 유출 및 평문 전송 경로를 차단합니다.

### 3.3 교차 사이트 스크립팅 (XSS) 방어
* **C++ JSON Escape & HTML Entity Encoding**: 
  1. JSON 구문 분석 트리 훼손을 방지하기 위해 응답 문자열 포맷팅 시 기본적으로 `escapeJson` 처리를 수행합니다.
  2. 이에 더해, 게시판 출력 API([board_controller.hpp](../src/controllers/board_controller.hpp))에서 글 제목, 내용, 작성자 등의 데이터를 출력하기 직전 C++ 레벨에서 HTML 엔티티 인코더인 `htmlEscape`를 경유하도록 구현했습니다. 이를 통해 `<`, `>`, `&`, `"`, `'`, `/` 등의 메타 문자가 안전한 엔티티(예: `&lt;`, `&gt;`)로 물리적 치환되어 브라우저 렌더링 방식(innerHTML 등)이나 CSP 우회 시나리오에 구애받지 않고 XSS 공격을 원천 무력화합니다.
* **기본 보안 헤더 및 CSP**:
  * [main.cpp](../src/main.cpp)에서 `Content-Security-Policy`(`default-src 'self'`)를 포함한 주요 브라우저 보안 헤더들을 응답 기본값으로 강제 설정합니다.
  * 웹 페이지 내의 인라인 스크립트를 허용하지 않고 오직 독립된 `.js` 파일의 코드만 해석하도록 구성하여 반사형/저장형 XSS의 공격면을 통제합니다.

### 3.4 자동화 공격 (무차별 대입) 방어
* **구현 방식**: `LoginLimiter` 클래스를 이용해 각 `username`별로 로그인 실패 횟수 및 잠금 만료 시각을 메모리 상에서 관리합니다. `AuthController`는 실제 DB 로그인 쿼리를 호출하기 전, `getRemainingLockoutTime` 함수를 사용하여 선제적으로 차단(Pre-Check)을 처리(HTTP Status `429` 반환)합니다.
* **목적**: 무차별적인 무차별 대입(Brute Force) 공격을 차단해 유저 계정을 지키고, 불필요한 DB 해시 검증 연산을 차단하여 서버 가용성(DoS 방지)을 확보합니다.

### 3.5 크로스사이트 요청 위조 (CSRF) 방어
* **구현 방식**:
  1. 유저 로그인 시 서버가 세션 ID와 함께 암호학적으로 강력한 난수 토큰(`csrf_token`)을 생성하여 서버 측 세션 저장소(`SessionManager`)에 바인딩합니다.
  2. 세션 정보 조회 API(`/api/me`) 응답을 통해 토큰을 안전하게 프론트엔드로 전달합니다.
  3. 프론트엔드 자바스크립트는 이 값을 메모리 변수에 보관하고, 상태 변경 POST 요청(`/logout`)을 보낼 때 HTTP 요청 헤더 `X-CSRF-Token`에 담아 보냅니다.
  4. 서버의 `AuthController`는 요청을 처리하기 전 세션에 저장된 토큰 값과 헤더로 전송된 토큰 값을 대조하여 일치하는 경우에만 처리를 허가합니다.
* **목적**: 타 도메인에서 브라우저의 자동 쿠키 전송 동작을 악용해 강제로 발생시키는 상태 변경 요청(세션 파괴, 로그아웃 등)을 무력화하여 요청의 정당한 출처와 유저의 명확한 의도를 강제로 확인합니다.

### 3.6 안전한 커스텀 에러 페이지 및 정보 유출 방어
* **구현 방식**:
  1. **정보 은닉 (Information Concealment)**: 서버 내부의 스택 트레이스나 구체적인 에러 메시지(예: 쿼리 구문 오류, 파일 경로 등)를 사용자 브라우저에 그대로 노출시키지 않습니다.
  2. **에러 추적 ID (Correlation ID)**: 서버 에러 발생 시 고유한 무작위 난수 코드(`ERR-xxxxxx` 형태)를 생성하여, 상세 로그는 서버 측 디스크(`error.log`) 및 콘솔(`std::cerr`)에 해당 ID와 함께 안전하게 기록(Dual Logging)하고, 사용자에게는 이 ID 값과 함께 최소한의 안내 메시지만을 표시합니다.
  3. **하이브리드 분기**: 클라이언트 요청 경로가 API(`/api/*`)인지 일반 정적 페이지인지 판별하여, API의 경우 표준화된 JSON 형식으로 에러 응답을 반환하고, 일반 웹 페이지의 경우 `public/error.html` 템플릿의 플레이스홀더(`{{TITLE}}`, `{{DESCRIPTION}}`, `{{TRACKING_ID}}`)를 C++단에서 치환하여 반환합니다.
* **목적**: 공격자가 에러 메시지 분석을 통해 시스템 내부 정보(OS, 웹 서버 세부 버전, DB 구조 등)를 유추(Information Leakage)하는 공격을 차단하면서도, 내부 관리자가 운영 중 발생한 문제를 발급된 Tracking ID를 통해 디버깅할 수 있는 추적성을 완벽하게 구축합니다.

### 3.7 안전한 쿠키 파싱
* **구현 방식**: [helpers.hpp](../src/helpers.hpp)에 작성된 `getCookieValue` 함수를 통해 쿠키를 안전하게 처리합니다. 수신된 쿠키 헤더를 세미콜론 `;` 기준으로 분할(split)하고, 각 조각의 공백을 제거(trim)한 다음, 타겟 접두사(`key=`)로 완벽하게 시작하는지 대조하여 값을 가져옵니다.
* **목적**: 쿠키 이름의 일부만 겹치는 더미 쿠키(예: `bad_auth_session=`)를 이용해 정상 사용자의 세션 파싱을 방해하는 DoS 및 변조 세션 강제 주입(Session Fixation 연계) 등의 보안 우회 경로를 원천 방지합니다.

### 3.8 공통 보안 미들웨어 인터셉터
* **구현 방식**: C++ 함수형 데코레이터 패턴을 기반으로 [middleware.hpp](../src/middleware.hpp)에 `requireAuth`, `requireAuthAndCsrf`, `requireAdmin` 필터를 구축하고 [main.cpp](../src/main.cpp) 라우팅 선언부에 체이닝 형태로 연동했습니다. 인증에 성공하면 사용자 정보가 담긴 `UserContext`를 컨트롤러에 안전하게 파라미터로 주입합니다. 특히 CSRF 검증 실패 시에는 **서버 세션을 파괴하고 브라우저 쿠키를 만료(Max-Age=0)**시키는 강력한 방어 메커니즘을 적용했습니다.
* **목적**: 중복되는 보안 로직(세션 체크, CSRF 검사, 역할 인가 등)을 공통 횡단 관심사(Cross-cutting Concerns)로 묶어 컨트롤러 결합도를 낮추고, 신규 개발 시 발생할 수 있는 보안 누락 실수를 방지하여 설계 결함을 근본적으로 예방합니다.
### 3.9 수평/수직적 권한 제어 및 IDOR 방어 (BOLA/IDOR & Authorization Control)
* **구현 방식**: 
  1. 게시글 삭제 등 민감한 리소스 제어 시, 요청자의 세션 사용자 ID(`username`)와 대상 리소스의 실제 작성자(`post.author`)가 일치하는지 철저하게 비교 검증합니다.
  2. 추가적으로 수직적 권한 제어를 위해, 요청자의 역할이 관리자(`ADMIN`)인 경우(`role == "ADMIN"`)에는 작성자 일치 여부와 관계없이 게시글을 삭제할 수 있도록 비즈니스 규칙(Option 1)을 적용했습니다.
  3. **서비스 계층 및 SQL 레벨 이중 검증 (Defense in Depth)**: 컨트롤러 단의 권한 체크 우회(IDOR) 가능성을 예방하기 위해, [BoardService::deletePost](../src/services/board_service.hpp#L268) 함수가 요청자 아이디와 역할을 필수 인자로 받도록 설계했습니다. 내부적으로 `Queries::SECURE_DELETE_POST` 쿼리(`DELETE FROM posts WHERE id = ? AND (author = ? OR ? = 'ADMIN')`)를 수행하고, MySQL `mysql_stmt_affected_rows` API를 사용하여 쿼리 결과에 의해 실제 레코드가 1개 이상 지워졌는지를 감지해 성공 여부를 리턴하는 안전한 이중 방어막을 구축했습니다.
### 3.10 패스워드 보안 강화 및 실시간 마이그레이션 (Argon2id & Lazy Migration)
* **구현 방식**:
  1. **Argon2id 도입**: 대규모 GPU 병렬 무차별 대입(Brute-forcing) 공격을 무력화하기 위해 메모리 하드(Memory-hard) 기반의 암호학적 키 유도 함수인 Argon2id를 도입했습니다 (설정값: m_cost = 32MB, t_cost = 3, parallelism = 1).
  2. **솔트 내장 포맷 활용**: Argon2id 고유의 표준 인코딩 포맷(`$argon2id$v=19$...`)을 사용하여 별도 DB `salt` 컬럼의 물리적 관리 부담을 제거하고 결합도를 낮추었습니다.
  3. **실시간 마이그레이션 (Lazy Migration)**: 로그인 요청 시 DB 해시 문자열 접두사를 감지하여, 레거시(SHA-256) 계정으로 인증 성공 시점에 평문 패스워드를 Argon2id로 신규 해싱하여 DB를 즉시 업데이트하는 횡단 관심사 로직을 추가했습니다.
* **목적**: 노후화된 해시 알고리즘을 최신 업계 표준으로 안전하게 격상시키며, 프로덕션 환경의 다운타임이나 강제 패스워드 리셋 없이 장기적이고 유연하게 보안 취약점을 완전히 도려냅니다.

### 3.11 메모리 소진 DoS 방어 및 백그라운드 가비지 컬렉터 (Memory DoS & Background GC)
* **구현 방식**: 
  1. **입력값 검증 (Input Validation)**: 로그인 진입로(`handleLogin`)에서 사용자명 크기를 3~32자로 제한하여 페이로드 비대화 공격(Payload Bloating)을 원천 차단합니다.
  2. **가비지 컬렉션 (Active GC)**: `SessionManager` 및 `LoginLimiter`에 만료 데이터 소거 함수(`cleanupExpiredSessions`, `cleanupExpiredAttempts`)를 추가하고, `main.cpp`에서 C++ `std::thread` 백그라운드 데몬 스레드를 구동하여 10초 주기로 메모리를 능동 소거(Active Reclamation)합니다.
  3. **스레드 안전 및 우아한 종료 (Thread-safety & Graceful Shutdown)**: `std::mutex` 락으로 멀티스레드 접근 경쟁 상태를 예방하고, `std::atomic<bool>` 플래그 분할 감시(1초 분할 수면) 구조로 서버 종료 시 즉각 스레드를 안전하게 조인(Join)합니다.
* **목적**: 무차별 대입 및 임시 세션 데이터가 힙(Heap) 영역에 영구 누적되어 프로세스 가용 메모리 임계치(15MB)를 초과해 발생하는 서비스 거부(DoS) 취약점을 근본적으로 해결합니다.

### 3.12 MySQL 커넥션 풀 재연결 복원성 (및 알려진 한계)
* **구현 방식**: [db_manager.hpp](../src/services/db_manager.hpp)의 `getConnection()`은 풀에서 꺼낸 연결에 대해 `mysql_ping()`으로 생존 여부를 확인하고, 실패 시 최대 5회(500ms 간격)까지 `createConnection()` 재시도를 수행합니다. 초기 풀 생성(`initializePool()`) 시에도 최대 10회(2초 간격) 재시도 로직이 별도로 존재합니다.
* **[한계 — 보안 관점의 정직한 평가]** 5회 재시도가 모두 실패하면 `getConnection()`은 `nullptr`을 반환하지만, 이미 `connection_pool.pop_back()`으로 슬롯을 꺼낸 상태이므로 그 슬롯은 풀에 반환되지 않고 **영구적으로 유실**됩니다. DB 장애가 2.5초(5×500ms)보다 길게 지속되면 요청마다 풀 크기가 하나씩 줄어들며, 결국 풀이 완전히 고갈되어 신규 요청 스레드가 `pool_cv.wait()`에서 영구 대기하는 서비스 전체 마비(THREAT-07)로 이어질 수 있습니다. 이는 완전히 해결된 상태가 아니라 장애 지속 시간을 2.5초만큼 유예시킨 것에 가깝습니다. 근본 해결(실패한 슬롯을 재시도 큐에 되돌리거나, `nullptr` 반환 시 호출자가 명확한 503 에러로 전환하는 방어 로직)은 [TODO.md](TODO.md)에 미완료 과제로 남아 있습니다.
* **목적**: DB가 자동 재시작되는 컨테이너 환경(MariaDB)에서 순간적인 연결 끊김에 대한 복원력을 확보하되, 이 방어가 "장애 지속 시간에 비례해 결국 무력화될 수 있는 임시방편"이라는 한계를 명확히 인지하고 있어야 합니다.

---

## 4. 향후 보안 및 아키텍처 개선 과제 (Future Improvements)

* **비로그인 사용자 보호 기능 보완**:
  * `/login` 및 `/signup` 엔드포인트에 대해 무차별 대입 공격(Brute Force) 제한 강화 및 패스워드 정책 고도화.
* **MySQL 커넥션 풀 슬롯 영구 유실 문제 근본 해결** (3.12 참고, THREAT-07): 현재는 재시도로 장애 지속 시간을 유예할 뿐, 재시도 소진 시 풀 슬롯이 영구 소실되는 구조적 결함이 남아있음.


