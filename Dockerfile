# syntax=docker/dockerfile:1.6

FROM gcc:13-bookworm AS build
WORKDIR /src
COPY . .
RUN mkdir -p /out \
 && gcc -O2 -DSQLITE_THREADSAFE=1 -c requirements/sqlite3.c -o /out/sqlite3.o \
 && g++ -std=c++20 -O2 -pthread \
        -static-libstdc++ -static-libgcc \
        MedFlow_BY_LLl-23.cpp api/ApiServer.cpp services/InsuranceValidator.cpp \
        /out/sqlite3.o -ldl \
        -o /out/medflow

FROM debian:bookworm-slim AS runtime
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates locales \
 && sed -i 's/^# *\(en_US.UTF-8\)/\1/' /etc/locale.gen \
 && locale-gen \
 && rm -rf /var/lib/apt/lists/*
ENV LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8

WORKDIR /app
COPY --from=build /out/medflow /app/medflow
COPY web /app/web

EXPOSE 8420
CMD ["/app/medflow"]
