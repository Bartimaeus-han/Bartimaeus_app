# run.ps1 (직접 생성하실 파일 내용)
if (Test-Path ".\build_win\Debug\SecureWebServer.exe") {
    Write-Host "서버를 실행합니다..." -ForegroundColor Green
    .\build_win\Debug\SecureWebServer.exe
} else {
    Write-Host "빌드된 실행 파일이 없습니다. 먼저 빌드를 진행해주세요." -ForegroundColor Red
}
