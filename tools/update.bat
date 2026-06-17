@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

REM ============================================================
REM  MedFlow — обновление с GitHub и пересборка Docker-контейнера
REM  Использование:
REM    update.bat          обновить (пересобрать только если есть изменения)
REM    update.bat --force  пересобрать и перезапустить в любом случае
REM ============================================================

REM Параметры (см. tools/README.md)
set "IMAGE=medflow"
set "CONTAINER=medflow"
set "PORT=8420"

REM Скрипт лежит в tools/; все операции (git, docker build, backups) идут из
REM корня репозитория — на уровень выше.
cd /d "%~dp0.." || (echo [ERROR] Не удалось перейти в корень репозитория & exit /b 1)
set "DATA_DIR=%CD%\backups"

set "FORCE=0"
if /i "%~1"=="--force" set "FORCE=1"

echo.
echo === 1/4 Получение новой версии из GitHub ===
for /f "delims=" %%i in ('git rev-parse HEAD 2^>nul') do set "OLD_REV=%%i"

git pull --ff-only
if errorlevel 1 (
    echo [ERROR] git pull завершился с ошибкой. Обновление прервано.
    exit /b 1
)

for /f "delims=" %%i in ('git rev-parse HEAD 2^>nul') do set "NEW_REV=%%i"

if "%FORCE%"=="0" if "%OLD_REV%"=="%NEW_REV%" (
    echo Новых коммитов нет ^(HEAD = %NEW_REV%^).
    echo Пересборка не требуется. Запустите с --force, чтобы пересобрать принудительно.
    exit /b 0
)

echo.
echo === 2/4 Сборка Docker-образа "%IMAGE%" ===
docker build -t "%IMAGE%" .
if errorlevel 1 (
    echo [ERROR] Сборка образа не удалась. Старый контейнер не тронут.
    exit /b 1
)

echo.
echo === 3/4 Перезапуск контейнера "%CONTAINER%" ===

REM Каталог backups/ (БД medflow.db + .bck-снапшоты) монтируем с хоста внутрь
REM контейнера. Без этого приложение пишет в эфемерный /app/backups и теряет
REM данные при каждом пересоздании контейнера.
if not exist "%DATA_DIR%" (
    echo Создаю каталог данных: "%DATA_DIR%"
    mkdir "%DATA_DIR%"
)

REM Снести старый контейнер, если он есть (ошибку игнорируем — его может не быть)
docker rm -f "%CONTAINER%" >nul 2>&1

docker run -d ^
  --name "%CONTAINER%" ^
  --restart unless-stopped ^
  -p %PORT%:%PORT% ^
  -v "%DATA_DIR%:/app/backups" ^
  "%IMAGE%"
if errorlevel 1 (
    echo [ERROR] Не удалось запустить контейнер.
    exit /b 1
)

echo.
echo === 4/4 Проверка ===
docker ps --filter "name=%CONTAINER%"
echo.
echo Готово. HEAD = %NEW_REV%
echo Проверка здоровья: curl http://localhost:%PORT%/api/health

endlocal
