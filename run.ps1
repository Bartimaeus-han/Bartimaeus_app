Write-Host "Stopping and removing containers..." -ForegroundColor Cyan
docker compose down

Write-Host "Rebuilding images from scratch (no cache)..." -ForegroundColor Cyan
docker compose build --no-cache

Write-Host "Starting containers..." -ForegroundColor Cyan
docker compose up