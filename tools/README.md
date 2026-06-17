# tools/ — эксплуатация и вспомогательные скрипты

Здесь живёт всё, что не участвует в основной сборке приложения: деплой и
обновление на сервере, плюс дев-утилиты. Запускается руками, когда надо.

| Файл | Назначение |
|---|---|
| `update.sh` | Обновление развёрнутого сервера (Linux): pull + пересборка + перезапуск контейнера |
| `update.bat` | То же для Windows / Docker Desktop |
| `gen_seed.py` | Генератор реалистичного тестового бэкапа |
| этот `README.md` | Инструкция по деплою и эксплуатации |

> **Главное про данные.** Состояние приложения целиком лежит в каталоге
> **`backups/`** в корне репозитория: боевая БД `backups/medflow.db` (SQLite) и
> JSON-снапшоты `backups/bck_*.bck`. Этот каталог — **единственное**, что надо
> монтировать наружу из контейнера (`-v "<repo>/backups:/app/backups"`) и бэкапить.
> Сервис слушает порт **8420**.

---

## Деплой на Linux-сервер

Сервис собирается из [`../Dockerfile`](../Dockerfile) (multi-stage:
`gcc:13` → `debian:bookworm-slim`, бинарь слинкован статически с libstdc++).
`.dockerignore` отсекает MSVC-мусор (`x64/`, `.vs/` и пр.). Образ и контейнер
называются `medflow`.

### TL;DR — минимум, чтобы взлетело

```bash
# 1. Docker (один раз)
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER && newgrp docker

# 2. Исходники на сервер
git clone <your-repo-url> medflow && cd medflow

# 3. Сборка + запуск (из корня репозитория)
docker build -t medflow .
docker run -d \
  --name medflow \
  --restart unless-stopped \
  -p 8420:8420 \
  -v "$PWD/backups:/app/backups" \
  medflow

# 4. Проверка
docker ps
curl http://localhost:8420/api/health
# → {"status":"ok","port":8420}
```

Каталог `backups/` на хосте создаётся автоматически при первом запуске (Docker
создаст его как пустую папку, приложение положит туда `medflow.db`). Если внутри
уже лежит `medflow.db` от прошлого запуска — он подхватится как есть.

### Вариант A: собирать образ на сервере

Когда: на сервере есть интернет и хотя бы 1–2 CPU. Самый простой путь.

```bash
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
newgrp docker                      # либо перелогиниться

git clone <your-repo-url> medflow
cd medflow

docker build -t medflow .

docker run -d \
  --name medflow \
  --restart unless-stopped \
  -p 8420:8420 \
  -v "$PWD/backups:/app/backups" \
  medflow

docker logs -f medflow
curl http://localhost:8420/api/health
```

### Вариант B: собрать локально, перенести образ

Когда: на сервере нет интернета, он слабый, или хочется детерминированности.

**Локально:**
```bash
# --platform важен, если у тебя Mac на ARM, а сервер x86_64
docker build --platform linux/amd64 -t medflow .
docker save medflow | gzip > medflow.tar.gz
scp medflow.tar.gz user@server:/tmp/
```

**На сервере:**
```bash
gunzip -c /tmp/medflow.tar.gz | docker load
mkdir -p /opt/medflow && cd /opt/medflow

docker run -d \
  --name medflow \
  --restart unless-stopped \
  -p 8420:8420 \
  -v /opt/medflow/backups:/app/backups \
  medflow
```

### Открыть порт наружу

По умолчанию `-p 8420:8420` биндится на все интерфейсы, но фаервол ОС может рубить.

```bash
# ufw (Ubuntu)
sudo ufw allow 8420/tcp

# firewalld (RHEL/Fedora/CentOS)
sudo firewall-cmd --permanent --add-port=8420/tcp
sudo firewall-cmd --reload
```

Если сервер за облачным провайдером (AWS / Hetzner / DO) — не забудь про security
group / cloud firewall в их панели. Чтобы слушать только локально (например, за
nginx): `-p 127.0.0.1:8420:8420`.

### Эксплуатация

```bash
docker logs -f medflow              # хвост логов
docker restart medflow              # рестарт
docker stop medflow                 # остановка: SIGTERM → graceful + exit-снапшот в backups/
docker start medflow                # поднять обратно
docker rm -f medflow                # снести контейнер (каталог backups/ на хосте остаётся)

# Ручной бэкап всего каталога состояния
cp -r backups "backups-$(date +%F)"
```

> При штатной остановке (`docker stop` шлёт `SIGTERM`) приложение пишет снапшот
> `backups/bck_<timestamp>.bck` и закрывает БД. Сама `medflow.db` консистентна на
> каждый commit, так что и `docker kill` данные не теряет — потеряется только
> финальный JSON-снапшот.

> Валидация страховых полисов ходит на внешний сервис `82.40.57.187:8123` — для
> создания/обновления пациентов с `insuranceNum` контейнеру нужен исходящий доступ
> в сеть.

### (Опционально) HTTPS через nginx + Let's Encrypt

`/etc/nginx/sites-available/medflow`:
```nginx
server {
    listen 80;
    server_name medflow.example.com;

    location / {
        proxy_pass http://127.0.0.1:8420;
        proxy_set_header Host              $host;
        proxy_set_header X-Real-IP         $remote_addr;
        proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

```bash
sudo ln -s /etc/nginx/sites-available/medflow /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
sudo apt install -y certbot python3-certbot-nginx
sudo certbot --nginx -d medflow.example.com
```

При такой схеме контейнер запускай с биндом только на localhost:
`-p 127.0.0.1:8420:8420`.

### Если что-то сломалось

| Симптом | Причина / что делать |
|---|---|
| Приложение «не видит» БД и бэкапы после пересоздания контейнера | Не смонтирован каталог `backups/`. Контейнер должен запускаться с `-v "<repo>/backups:/app/backups"`, иначе он пишет в эфемерный `/app/backups` внутри себя и теряет данные. Данные на хосте при этом целы — поправь `-v` и пересоздай контейнер. |
| `docker logs medflow` пустой, контейнер `Exited (1)` | Бинарь что-то не нашёл. Смотри `docker logs medflow` целиком. |
| `GLIBCXX_3.4.xx not found` | Старый рантайм-образ. В Dockerfile уже `-static-libstdc++ -static-libgcc`, это лечит — не убирай флаги. |
| `curl: (7) Failed to connect ... 8420` снаружи | Фаервол ОС / cloud security group. См. «Открыть порт наружу». |
| Конфликт порта (`address already in use`) | Кто-то занял 8420: `sudo ss -lntp \| grep 8420`. Либо смени проброс: `-p 9000:8420`. |
| Хочется зайти внутрь | `docker exec -it medflow bash` (там debian-slim; `curl` нет — `apt-get update && apt-get install -y curl`). |

Куда ткнуться, чтобы убедиться, что живо:
- `GET http://<host>:8420/api/health` → `{"status":"ok","port":8420}`
- `GET http://<host>:8420/api/schedules/today` → текущие «сегодня»/«сейчас» сервиса
- `GET http://<host>:8420/` → статика из `web/` (UI)

Полный справочник API — в [`../api/README.md`](../api/README.md).

---

## Обновление развёрнутого сервера — `update.sh` / `update.bat`

Скрипты автоматизируют типовой цикл «подтянуть свежий код и перекатить контейнер».
Логика обоих одинакова:

1. `git pull --ff-only` и сравнение HEAD до/после;
2. если новых коммитов нет — выходят, не пересобирая (форс через `--force`);
3. `docker build -t medflow .`;
4. сносят старый контейнер и поднимают новый с `--restart unless-stopped`,
   `-p 8420:8420` и `-v <repo>/backups:/app/backups`;
5. показывают `docker ps` и подсказку для health-check.

> Скрипты лежат в `tools/`, но все операции (`git`, `docker build`, монтирование
> `backups/`) выполняются из **корня репозитория** — они сами поднимаются на
> уровень выше. Запускать можно из любого места.

**Linux / сервер:**
```bash
./tools/update.sh           # обновить (пересборка только при новых коммитах)
./tools/update.sh --force   # пересобрать и перезапустить принудительно
```

**Windows / Docker Desktop:**
```bat
tools\update.bat
tools\update.bat --force
```

Замечания:
- `git pull --ff-only` намеренно не делает merge-коммитов: если локальная копия
  разошлась с веткой, скрипт честно упадёт на шаге 1, а не намешает историю.
- При падении `docker build` старый контейнер остаётся нетронутым.
- На сервере Docker может требовать `sudo` — тогда `sudo ./tools/update.sh` либо
  добавь пользователя в группу `docker`.

---

## `gen_seed.py` — тестовые данные

Генератор реалистичного тестового бэкапа. Кладёт результат в `../seed.bck` (по
умолчанию) — JSON в формате [`BackupService`](../services/BackupService.h),
который можно залить через `POST /api/backup/import` (см.
[`../api/README.md`](../api/README.md)).

Соблюдает доменные инварианты, которые проверяют сервисы — иначе импорт упадёт:

- `Patient.clinic_id` указывает на существующую клинику.
- У врача есть `Schedule` в `Room`, привязанной к нужной клинике.
- Каждый `Appointment` попадает в один из слотов расписания
  (`timeFrom + k*(slotDurationMin + breakAfterMin)`).
- Нет пересечений приёмов по одному врачу и по одному пациенту.
- У врача не больше одного `Schedule` на день недели.
- `createdAt < scheduledAt`; статус согласован с датой.

Запуск:

```bash
cd tools
python gen_seed.py
# → ../seed.bck

# залить в работающее приложение:
curl -X POST -H "Content-Type: application/json" \
     --data-binary @../seed.bck \
     http://localhost:8420/api/backup/import
```

Сид детерминирован — `SEED = 42` в начале файла. Меняешь — получаешь другой
набор данных.

---

## Когда добавлять сюда новое

- Скрипты деплоя/эксплуатации.
- Одноразовые миграции данных.
- Локальные дев-утилиты (генераторы, дампилки).
- Скрипты для CI, если такой появится.

Что **не сюда**: рантайм-код приложения (это [`../services`](../services)) и
сборочные конфиги (это корень репы и [`../Dockerfile`](../Dockerfile)).
