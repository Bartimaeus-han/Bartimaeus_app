# MSVC 환경 변수를 자동으로 가져오는 함수 (Function to automatically import MSVC environment variables)
function Import-Vcvars {
    # vswhere 경로 설정 (Set vswhere path)
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        return
    }
    
    # VS 설치 경로 찾기 (Find VS installation path)
    $vsPath = & $vswhere -latest -property installationPath
    if (-not $vsPath) {
        return
    }
    
    # vcvarsall.bat 경로 구성 (Configure path for vcvarsall.bat)
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvars)) {
        return
    }
    
    Write-Host "MSVC 빌드 환경 변수를 불러오는 중... (Loading MSVC build environment variables...)" -ForegroundColor Gray
    
    # cmd.exe를 실행하여 환경 변수 목록 추출 (Run cmd.exe to extract environment variables list)
    $cmdLine = "`"$vcvars`" amd64 && set"
    $envLines = cmd.exe /c $cmdLine
    
    # 추출된 환경 변수를 현재 PowerShell 세션에 할당 (Assign extracted environment variables to current PowerShell session)
    foreach ($line in $envLines) {
        if ($line -match "^([^=]+)=(.*)$") {
            $name = $Matches[1]
            $value = $Matches[2]
            # 중요 환경 변수 프로세스 레벨에 설정 (Set critical environment variables at process level)
            if ($name -in @("PATH", "INCLUDE", "LIB", "LIBPATH")) {
                [System.Environment]::SetEnvironmentVariable($name, $value, [System.EnvironmentVariableTarget]::Process)
            }
        }
    }
}
# LIB 환경 변수가 없으면 MSVC 환경 로드 (Load MSVC environment if LIB environment variable is missing)
if (-not $env:LIB) {
    Import-Vcvars
}

# Perform server build
Write-Host "Start Server build..." -ForegroundColor Cyan
cmake --build .\build_win

# Verify build success
if ($LASTEXITCODE -eq 0) {
    Write-Host "Build Success! Run Server..." -ForegroundColor Green
    .\build_win\SecureWebServer.exe
}
else {
    Write-Host "Build Failed, please check error" -ForegroundColor Red
}