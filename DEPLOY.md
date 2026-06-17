# MedFlow — деплой на Linux-сервер

Шпаргалка трезвому себе. Сервис слушает порт **8420**, состояние пишет в `bck1.bck` рядом с бинарём.

В репе уже лежат:
- `Dockerfile` — multi-stage сборка (gcc:13 → debian:bookworm-slim, бинарь слинкован statically с libstdc++).
- `.dockerignore` — отсекает MSVC-мусор (`x64/`, `enc_temp_folder/`, `.vs/`).

Локально билд + запуск тестировались, образ называется `medflow`, контейнер тоже `medflow`.

---

## TL;DR — минимум, чтобы взлетело

```bash
# 1. Docker (один раз)
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER && newgrp docker

# 2. Исходники на сервер
git clone <your-repo-url> medflow && cd medflow
# или с локалки:  rsync -avz --exclude x64 --exclude .vs ./ user@server:/opt/medflow/

# 3. Сборка + запуск
docker build -t medflow .
touch bck1.bck                    # чтобы стейт пережил рестарт контейнера
docker run -d \
  --name medflow \
  --restart unless-stopped \
  -p 8420:8420 \
  -v "$PWD/bck1.bck:/app/bck1.bck" \
  medflow

# 4. Проверка
docker ps
curl http://localhost:8420/api/health
# → {"port":8420,"status":"ok"}
```

---

## Вариант A: собирать образ на сервере

Когда: на сервере есть интернет и хотя бы 1–2 CPU. Самый простой путь.

```bash
# Поставить Docker (Ubuntu/Debian)
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
newgrp docker     # либо перелогиниться

# Получить код
git clone <your-repo-url> medflow
cd medflow

# Собрать
docker build -t medflow .

# Подготовить файл бэкапа (важно: именно файл, не каталог,
# иначе Docker создаст directory вместо файла и приложение упадёт)
touch bck1.bck

# Запустить с автоперезапуском
docker run -d \
  --name medflow \
  --restart unless-stopped \
  -p 8420:8420 \
  -v "$PWD/bck1.bck:/app/bck1.bck" \
  medflow

# Проверить
docker logs -f medflow
curl http://localhost:8420/api/health
curl http://localhost:8420/api/schedules/today
```

---

## Вариант B: собрать локально, перенести образ

Когда: на сервере нет интернета, или он слабый, или хочется детерминированности.

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
touch bck1.bck

docker run -d \
  --name medflow \
  --restart unless-stopped \
  -p 8420:8420 \
  -v /opt/medflow/bck1.bck:/app/bck1.bck \
  medflow
```

---

## Открыть порт наружу

По умолчанию `-p 8420:8420` биндится на все интерфейсы, но фаервол ОС может рубить.

```bash
# ufw (Ubuntu)
sudo ufw allow 8420/tcp

# firewalld (RHEL/Fedora/CentOS)
sudo firewall-cmd --permanent --add-port=8420/tcp
sudo firewall-cmd --reload
```

Если сервер за облачным провайдером (AWS / Hetzner / DO) — не забудь про security group / cloud firewall в их панели.

Чтобы слушать только локально (например, за nginx): `-p 127.0.0.1:8420:8420`.

---

## Эксплуатация

```bash
docker logs -f medflow              # хвост логов
docker restart medflow              # рестарт
docker stop medflow                 # остановка (приложение сохранит bck1.bck)
docker start medflow                # поднять обратно
docker rm -f medflow                # снести контейнер (volume и образ остаются)

# Ручной бэкап файла стейта
cp bck1.bck "bck1-$(date +%F).bck"

# Обновление образа (после нового build/save/load):
docker rm -f medflow
docker run -d --name medflow --restart unless-stopped \
  -p 8420:8420 -v "$PWD/bck1.bck:/app/bck1.bck" medflow
```

---

## (Опционально) HTTPS через nginx + Let's Encrypt

Когда хочется отдавать наружу на 443 с нормальным доменом.

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

---

## Если что-то сломалось

| Симптом | Причина / что делать |
|---|---|
| `docker logs medflow` пустой, контейнер `Exited (1)` | Скорее всего бинарь не нашёл что-то. Смотри `docker logs medflow` целиком. |
| `GLIBCXX_3.4.xx not found` | Старый рантайм-образ. В нашем Dockerfile уже `-static-libstdc++ -static-libgcc`, это лечит. Если чинил — не убирай флаги. |
| `bck1.bck` стал каталогом, не файлом | Перед `docker run` забыл `touch bck1.bck`. Удали каталог, сделай `touch`, перезапусти. |
| `curl: (7) Failed to connect ... 8420` снаружи | Фаервол ОС / cloud security group. См. секцию «Открыть порт наружу». |
| Конфликт порта (`address already in use`) | Кто-то уже занял 8420: `sudo ss -lntp \| grep 8420`. Либо смени проброс: `-p 9000:8420`. |
| Хочется зайти внутрь | `docker exec -it medflow bash` (там debian-slim, есть `ls`, нет `curl` — ставь `apt-get update && apt-get install -y curl` если надо). |

---

## Куда ткнуться, чтобы убедиться, что живо

- `GET http://<host>:8420/api/health` → `{"port":8420,"status":"ok"}`
- `GET http://<host>:8420/api/schedules/today` → текущие «сегодня» и «сейчас» сервиса
- `GET http://<host>:8420/` → статика из `web/` (UI)

Полный справочник API — в `api/README.md`.
