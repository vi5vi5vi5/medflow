#include <iostream>
#include <memory>
#include <clocale>
#include <csignal>
#include <atomic>
#include <chrono>
#include <ctime>
#include <filesystem>

#include "interface/sqlite/SqliteDb.h"
#include "interface/sqlite/SqliteClinic.h"
#include "interface/sqlite/SqliteRoom.h"
#include "interface/sqlite/SqliteDoctor.h"
#include "interface/sqlite/SqlitePatient.h"
#include "interface/sqlite/SqliteAppointment.h"
#include "interface/sqlite/SqliteSchedule.h"
#include "interface/sqlite/SqliteSpecialization.h"

#include "services/PatientService.h"
#include "services/DoctorService.h"
#include "services/ScheduleService.h"
#include "services/ClinicService.h"
#include "services/RoomService.h"
#include "services/SpecializationService.h"
#include "services/AppointmentService.h"
#include "services/BackupService.h"

#include "api/ApiServer.h"

#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

using namespace std;

constexpr int kPort = 8420;
constexpr const char* kDataDir = "backups";
constexpr const char* kDbPath  = "backups/medflow.db";

// Используется обработчиком сигналов для graceful-остановки httplib-сервера
// (нужно Docker'у: SIGTERM приходит при `docker stop`).
static ApiServer* g_server = nullptr;
static std::atomic<bool> g_shuttingDown{false};

static void handleSignal(int /*sig*/) {
    if (g_shuttingDown.exchange(true)) return;
    if (g_server) g_server->stop();
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#else
    // На Linux/macOS берём UTF-8 из переменных окружения (LANG/LC_ALL)
    std::setlocale(LC_ALL, "");
#endif

    std::signal(SIGINT,  handleSignal);
    std::signal(SIGTERM, handleSignal);
#ifdef SIGPIPE
    // httplib сам обрабатывает обрыв соединения; глушим SIGPIPE, чтобы не падать
    std::signal(SIGPIPE, SIG_IGN);
#endif

    // Папка для данных (БД и бэкапы) — единственное место, которое
    // монтируется снаружи. Создаём заранее, иначе sqlite_open упадёт.
    std::filesystem::create_directories(kDataDir);

    // Единое соединение со SQLite — все репозитории работают через него.
    // Файл создаётся автоматически, схема накатывается при старте.
    auto db = make_shared<SqliteDb>(kDbPath);

    auto clinicRepo  = make_shared<SqliteClinic>(db);
    auto roomRepo    = make_shared<SqliteRoom>(db);
    auto doctorRepo  = make_shared<SqliteDoctor>(db);
    auto patientRepo = make_shared<SqlitePatient>(db);
    auto apptRepo    = make_shared<SqliteAppointment>(db);
    auto schedRepo   = make_shared<SqliteSchedule>(db);
    auto specRepo    = make_shared<SqliteSpecialization>(db);

    ClinicService          clinicService(clinicRepo, roomRepo, schedRepo, patientRepo);
    PatientService         patientService(patientRepo, apptRepo, clinicRepo);
    DoctorService          doctorService(doctorRepo, specRepo, schedRepo, apptRepo);
    ScheduleService        schedService(schedRepo, apptRepo, doctorRepo, roomRepo);
    RoomService            roomService(roomRepo, clinicRepo, schedRepo);
    SpecializationService  specService(specRepo, doctorRepo);
    AppointmentService     apptService(apptRepo, patientRepo, doctorRepo, schedRepo);
    BackupService          bkup(clinicRepo, roomRepo, specRepo, doctorRepo, patientRepo, schedRepo, apptRepo);

    // Состояние теперь живёт в medflow.db. JSON-бэкап на старте не грузим —
    // если нужно восстановиться, делается это явно через /api/backup/load.

    ApiServer server(kPort,
                     patientService,
                     doctorService,
                     schedService,
                     clinicService,
                     roomService,
                     specService,
                     apptService,
                     bkup);
    g_server = &server;
    server.run();
    g_server = nullptr;

    // На выходе делаем JSON-снапшот с меткой времени в имени.
    // Сама БД (medflow.db) уже консистентна — sqlite пишет на каждый commit.
    try {
        namespace fs = std::filesystem;
        fs::create_directories("backups");

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d_%H-%M-%S", &tm);

        std::string path = std::string("backups/bck_") + ts + ".bck";
        bkup.saveToFile(path);
        cout << "Saved exit snapshot to " << path << endl;
    } catch (const std::exception& e) {
        cerr << "Failed to save exit snapshot: " << e.what() << endl;
    }

    return 0;
}
