#!/usr/bin/env bash
# ============================================================
#  MedFlow — обновление с GitHub и пересборка Docker-контейнера
#  Использование:
#    ./update.sh          обновить (пересобрать только если есть изменения)
#    ./update.sh --force  пересобрать и перезапустить в любом случае
# ============================================================
set -euo pipefail

# Параметры (совпадают с DEPLOY.md)
IMAGE="medflow"
CONTAINER="medflow"
PORT="8420"

# Работаем из каталога, где лежит скрипт (корень репозитория)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

STATE_FILE="$SCRIPT_DIR/bck1.bck"

FORCE=0
if [[ "${1:-}" == "--force" ]]; then
    FORCE=1
fi

echo
echo "=== 1/4 Получение новой версии из GitHub ==="
OLD_REV="$(git rev-parse HEAD 2>/dev/null || echo none)"

git pull --ff-only

NEW_REV="$(git rev-parse HEAD 2>/dev/null || echo none)"

if [[ "$FORCE" -eq 0 && "$OLD_REV" == "$NEW_REV" ]]; then
    echo "Новых коммитов нет (HEAD = $NEW_REV)."
    echo "Пересборка не требуется. Запустите с --force, чтобы пересобрать принудительно."
    exit 0
fi

echo
echo "=== 2/4 Сборка Docker-образа \"$IMAGE\" ==="
docker build -t "$IMAGE" .

echo
echo "=== 3/4 Перезапуск контейнера \"$CONTAINER\" ==="

# Файл состояния должен существовать именно как файл (иначе Docker создаст каталог)
if [[ ! -f "$STATE_FILE" ]]; then
    echo "Создаю пустой файл состояния: $STATE_FILE"
    touch "$STATE_FILE"
fi

# Снести старый контейнер, если он есть (ошибку игнорируем — его может не быть)
docker rm -f "$CONTAINER" >/dev/null 2>&1 || true

docker run -d \
  --name "$CONTAINER" \
  --restart unless-stopped \
  -p "$PORT:$PORT" \
  -v "$STATE_FILE:/app/bck1.bck" \
  "$IMAGE"

echo
echo "=== 4/4 Проверка ==="
docker ps --filter "name=$CONTAINER"
echo
echo "Готово. HEAD = $NEW_REV"
echo "Проверка здоровья: curl http://localhost:$PORT/api/health"
