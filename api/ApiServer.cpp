// httplib must come first: it pulls in <windows.h> on Windows, and some
// models later do `using namespace std;` which would otherwise cause
// `byte` ambiguity in the Windows SDK headers (std::byte vs ::byte).
#include "../requirements/httplib.h"

#include "ApiServer.h"

#include "../requirements/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using nlohmann::json;
using httplib::Request;
using httplib::Response;

struct ApiServer::Impl {
    int port;
    PatientService&        patients;
    DoctorService&         doctors;
    ScheduleService&       schedules;
    ClinicService&         clinics;
    RoomService&           rooms;
    SpecializationService& specializations;
    AppointmentService&    appointments;
    BackupService&         backup;

    httplib::Server svr;

    void ok(Response& res, const json& j, int status = 200) {
        res.status = status;
        res.set_content(j.dump(), "application/json");
    }

    void err(Response& res, int status, const std::string& msg) {
        res.status = status;
        res.set_content(json{{"error", msg}}.dump(), "application/json");
    }

    static int pathInt(const Request& req, size_t idx) {
        return std::stoi(req.matches[idx].str());
    }

    static json parseBody(const Request& req) {
        if (req.body.empty()) return json::object();
        return json::parse(req.body);
    }

    void setupCors() {
        svr.set_default_headers({
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type"}
        });
        svr.Options(R"(/.*)", [](const Request&, Response& res) {
            res.status = 204;
        });
    }

    void setupErrorHandler() {
        svr.set_exception_handler([this](const Request&, Response& res, std::exception_ptr ep) {
            try {
                std::rethrow_exception(ep);
            } catch (const std::invalid_argument& e) {
                err(res, 400, e.what());
            } catch (const std::out_of_range& e) {
                err(res, 400, e.what());
            } catch (const json::exception& e) {
                err(res, 400, std::string("JSON error: ") + e.what());
            } catch (const std::exception& e) {
                err(res, 500, e.what());
            } catch (...) {
                err(res, 500, "unknown error");
            }
        });
        svr.set_error_handler([this](const Request&, Response& res) {
            if (res.body.empty()) {
                err(res, res.status, "not found");
            }
        });
    }

    // ---------------- Patients ----------------
    void setupPatients() {
        svr.Get("/api/patients", [this](const Request&, Response& res) {
            json arr = json::array();
            for (const auto& p : patients.findAll()) arr.push_back(p);
            ok(res, arr);
        });
        svr.Get(R"(/api/patients/(\d+))", [this](const Request& req, Response& res) {
            auto p = patients.findByID(pathInt(req, 1));
            if (!p) return err(res, 404, "patient not found");
            ok(res, *p);
        });
        svr.Post("/api/patients", [this](const Request& req, Response& res) {
            ok(res, patients.newPatientJSON(parseBody(req)), 201);
        });
        svr.Post("/api/patients/search", [this](const Request& req, Response& res) {
            json arr = json::array();
            for (const auto& p : patients.findByJson(parseBody(req))) arr.push_back(p);
            ok(res, arr);
        });
        svr.Patch(R"(/api/patients/(\d+))", [this](const Request& req, Response& res) {
            ok(res, patients.update(pathInt(req, 1), parseBody(req)));
        });
        svr.Delete(R"(/api/patients/(\d+))", [this](const Request& req, Response& res) {
            ok(res, json{{"removed", patients.removeByID(pathInt(req, 1))}});
        });
    }

    // ---------------- Doctors ----------------
    void setupDoctors() {
        svr.Get("/api/doctors", [this](const Request&, Response& res) {
            json arr = json::array();
            for (const auto& d : doctors.findAll()) arr.push_back(d);
            ok(res, arr);
        });
        svr.Get(R"(/api/doctors/(\d+))", [this](const Request& req, Response& res) {
            auto d = doctors.findByID(pathInt(req, 1));
            if (!d) return err(res, 404, "doctor not found");
            ok(res, *d);
        });
        svr.Post("/api/doctors", [this](const Request& req, Response& res) {
            ok(res, doctors.newDoctorJSON(parseBody(req)), 201);
        });
        svr.Post("/api/doctors/search", [this](const Request& req, Response& res) {
            json arr = json::array();
            for (const auto& d : doctors.findByJson(parseBody(req))) arr.push_back(d);
            ok(res, arr);
        });
        svr.Patch(R"(/api/doctors/(\d+))", [this](const Request& req, Response& res) {
            ok(res, doctors.update(pathInt(req, 1), parseBody(req)));
        });
        svr.Delete(R"(/api/doctors/(\d+))", [this](const Request& req, Response& res) {
            ok(res, json{{"removed", doctors.removeByID(pathInt(req, 1))}});
        });
    }

    // ---------------- Clinics ----------------
    void setupClinics() {
        svr.Get("/api/clinics", [this](const Request&, Response& res) {
            json arr = json::array();
            for (const auto& c : clinics.findAll()) arr.push_back(c);
            ok(res, arr);
        });
        svr.Get(R"(/api/clinics/(\d+))", [this](const Request& req, Response& res) {
            auto c = clinics.findByID(pathInt(req, 1));
            if (!c) return err(res, 404, "clinic not found");
            ok(res, *c);
        });
        svr.Post("/api/clinics", [this](const Request& req, Response& res) {
            ok(res, clinics.newClinicJSON(parseBody(req)), 201);
        });
        svr.Patch(R"(/api/clinics/(\d+))", [this](const Request& req, Response& res) {
            ok(res, clinics.update(pathInt(req, 1), parseBody(req)));
        });
        svr.Delete(R"(/api/clinics/(\d+))", [this](const Request& req, Response& res) {
            ok(res, json{{"removed", clinics.removeByID(pathInt(req, 1))}});
        });
    }

    // ---------------- Rooms ----------------
    void setupRooms() {
        svr.Get("/api/rooms", [this](const Request&, Response& res) {
            json arr = json::array();
            for (const auto& r : rooms.findAll()) arr.push_back(r);
            ok(res, arr);
        });
        svr.Get(R"(/api/rooms/(\d+))", [this](const Request& req, Response& res) {
            auto r = rooms.findById(pathInt(req, 1));
            if (!r) return err(res, 404, "room not found");
            ok(res, *r);
        });
        svr.Get(R"(/api/rooms/by-clinic/(\d+))", [this](const Request& req, Response& res) {
            json arr = json::array();
            for (const auto& r : rooms.findByClinicId(pathInt(req, 1))) arr.push_back(r);
            ok(res, arr);
        });
        svr.Post("/api/rooms", [this](const Request& req, Response& res) {
            ok(res, rooms.addRoomJSON(parseBody(req)), 201);
        });
        svr.Post("/api/rooms/search", [this](const Request& req, Response& res) {
            json arr = json::array();
            for (const auto& r : rooms.findByJson(parseBody(req))) arr.push_back(r);
            ok(res, arr);
        });
        svr.Patch(R"(/api/rooms/(\d+))", [this](const Request& req, Response& res) {
            ok(res, rooms.updateRoom(pathInt(req, 1), parseBody(req)));
        });
        svr.Delete(R"(/api/rooms/(\d+))", [this](const Request& req, Response& res) {
            ok(res, json{{"removed", rooms.removeByID(pathInt(req, 1))}});
        });
    }

    // ---------------- Specializations ----------------
    void setupSpecializations() {
        svr.Get("/api/specializations", [this](const Request&, Response& res) {
            json arr = json::array();
            for (const auto& s : specializations.findAll()) arr.push_back(s);
            ok(res, arr);
        });
        svr.Get(R"(/api/specializations/(\d+))", [this](const Request& req, Response& res) {
            auto s = specializations.findByID(pathInt(req, 1));
            if (!s) return err(res, 404, "specialization not found");
            ok(res, *s);
        });
        svr.Get("/api/specializations/by-name", [this](const Request& req, Response& res) {
            if (!req.has_param("name")) return err(res, 400, "missing 'name' query param");
            auto s = specializations.findByName(req.get_param_value("name"));
            if (!s) return err(res, 404, "specialization not found");
            ok(res, *s);
        });
        svr.Post("/api/specializations", [this](const Request& req, Response& res) {
            ok(res, specializations.newSpecializationJSON(parseBody(req)), 201);
        });
        svr.Patch(R"(/api/specializations/(\d+))", [this](const Request& req, Response& res) {
            ok(res, specializations.update(pathInt(req, 1), parseBody(req)));
        });
        svr.Delete(R"(/api/specializations/(\d+))", [this](const Request& req, Response& res) {
            ok(res, json{{"removed", specializations.removeByID(pathInt(req, 1))}});
        });
    }

    // ---------------- Appointments ----------------
    void setupAppointments() {
        svr.Get("/api/appointments", [this](const Request&, Response& res) {
            json arr = json::array();
            for (const auto& a : appointments.findAll()) arr.push_back(a);
            ok(res, arr);
        });
        svr.Get(R"(/api/appointments/by-patient/(\d+))", [this](const Request& req, Response& res) {
            json arr = json::array();
            for (const auto& a : appointments.findByPatientId(pathInt(req, 1))) arr.push_back(a);
            ok(res, arr);
        });
        svr.Post("/api/appointments", [this](const Request& req, Response& res) {
            ok(res, appointments.newAppointmentJSON(parseBody(req)), 201);
        });
        svr.Post("/api/appointments/search", [this](const Request& req, Response& res) {
            json arr = json::array();
            for (const auto& a : appointments.findByJSON(parseBody(req))) arr.push_back(a);
            ok(res, arr);
        });
        svr.Patch(R"(/api/appointments/(\d+)/status)", [this](const Request& req, Response& res) {
            json body = parseBody(req);
            auto status = static_cast<AppointmentStatus>(body.at("status").get<int>());
            ok(res, appointments.updateStatus(pathInt(req, 1), status));
        });
        svr.Patch(R"(/api/appointments/(\d+)/notes)", [this](const Request& req, Response& res) {
            json body = parseBody(req);
            ok(res, appointments.updateNotes(pathInt(req, 1), body.at("notes").get<std::string>()));
        });
        svr.Delete(R"(/api/appointments/(\d+))", [this](const Request& req, Response& res) {
            ok(res, json{{"removed", appointments.removeByID(pathInt(req, 1))}});
        });
    }

    // ---------------- Schedules ----------------
    void setupSchedules() {
        svr.Get("/api/schedules", [this](const Request&, Response& res) {
            json arr = json::array();
            for (const auto& s : schedules.findAll()) arr.push_back(s);
            ok(res, arr);
        });
        svr.Get(R"(/api/schedules/by-doctor/(\d+))", [this](const Request& req, Response& res) {
            json arr = json::array();
            for (const auto& s : schedules.findByDocID(pathInt(req, 1))) arr.push_back(s);
            ok(res, arr);
        });
        svr.Post("/api/schedules", [this](const Request& req, Response& res) {
            ok(res, schedules.setNewScheduleJSON(parseBody(req)), 201);
        });
        svr.Delete(R"(/api/schedules/(\d+))", [this](const Request& req, Response& res) {
            ok(res, json{{"removed", schedules.removeByID(pathInt(req, 1))}});
        });
        svr.Get("/api/schedules/available-days", [this](const Request& req, Response& res) {
            if (!req.has_param("specId")) return err(res, 400, "missing 'specId' query param");
            int specId    = std::stoi(req.get_param_value("specId"));
            int clinicId  = req.has_param("clinicId")  ? std::stoi(req.get_param_value("clinicId"))  : -1;
            int daysAhead = req.has_param("daysAhead") ? std::stoi(req.get_param_value("daysAhead")) : 14;
            ok(res, schedules.getAvailableDays(specId, clinicId, daysAhead));
        });
        svr.Get("/api/schedules/doctors-by-day", [this](const Request& req, Response& res) {
            if (!req.has_param("specId") || !req.has_param("date"))
                return err(res, 400, "missing 'specId' or 'date' query param");
            int specId   = std::stoi(req.get_param_value("specId"));
            int clinicId = req.has_param("clinicId") ? std::stoi(req.get_param_value("clinicId")) : -1;
            ok(res, schedules.getDoctorIdByDaySlot(specId, req.get_param_value("date"), clinicId));
        });
        svr.Get("/api/schedules/time-slots", [this](const Request& req, Response& res) {
            if (!req.has_param("doctorId") || !req.has_param("date"))
                return err(res, 400, "missing 'doctorId' or 'date' query param");
            int doctorId = std::stoi(req.get_param_value("doctorId"));
            int clinicId = req.has_param("clinicId") ? std::stoi(req.get_param_value("clinicId")) : -1;
            json arr = json::array();
            for (const auto& ts : schedules.getAvailableTimeSlots(doctorId, req.get_param_value("date"), clinicId))
                arr.push_back(ts);
            ok(res, arr);
        });
        svr.Get("/api/schedules/today", [this](const Request&, Response& res) {
            ok(res, json{
                {"today", ScheduleService::getToday()},
                {"now",   ScheduleService::getCurrentTime()}
            });
        });
    }

    // ---------------- Backup ----------------
    static constexpr const char* kBackupDir = "backups";

    // Список .bck-файлов в backups/, отсортированный по имени.
    // Имена содержат ISO-таймстамп, поэтому лексикографическая сортировка
    // совпадает с хронологической — последний элемент = самый свежий.
    static std::vector<std::string> listBackupFiles() {
        namespace fs = std::filesystem;
        std::vector<std::string> out;
        std::error_code ec;
        if (!fs::exists(kBackupDir, ec)) return out;
        for (const auto& e : fs::directory_iterator(kBackupDir, ec)) {
            if (!e.is_regular_file()) continue;
            auto name = e.path().filename().string();
            if (name.size() >= 4 && name.substr(name.size() - 4) == ".bck") {
                out.push_back(std::move(name));
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    // Защита от path traversal: разрешаем только базовые имена без
    // разделителей и `..`.
    static bool isSafeBackupName(const std::string& name) {
        if (name.empty()) return false;
        if (name.find('/')  != std::string::npos) return false;
        if (name.find('\\') != std::string::npos) return false;
        if (name.find("..") != std::string::npos) return false;
        return true;
    }

    void setupBackup() {
        svr.Get("/api/backup/export", [this](const Request&, Response& res) {
            ok(res, backup.exportAll());
        });
        svr.Post("/api/backup/import", [this](const Request& req, Response& res) {
            backup.importAll(parseBody(req));
            ok(res, json{{"imported", true}});
        });
        svr.Post("/api/backup/save", [this](const Request& req, Response& res) {
            std::filesystem::create_directories(kBackupDir);
            json body = parseBody(req);
            std::string path = body.value("path", std::string(kBackupDir) + "/bck_manual.bck");
            backup.saveToFile(path);
            ok(res, json{{"saved", true}, {"path", path}});
        });
        // Без поля path — грузим последний снапшот из backups/.
        // С path — грузим именно его (полный или относительный путь).
        svr.Post("/api/backup/load", [this](const Request& req, Response& res) {
            json body = parseBody(req);
            std::string path = body.value("path", "");
            if (path.empty()) {
                auto files = listBackupFiles();
                if (files.empty()) return err(res, 404, "no backups found in " + std::string(kBackupDir) + "/");
                path = std::string(kBackupDir) + "/" + files.back();
            }
            backup.loadFromFile(path);
            ok(res, json{{"loaded", true}, {"path", path}});
        });

        // Примитивная HTML-страница со списком бэкапов: клик по имени —
        // скачать файл.
        svr.Get("/api/backup/list", [this](const Request&, Response& res) {
            auto files = listBackupFiles();
            std::stringstream html;
            html << "<!doctype html><meta charset=\"utf-8\"><title>MedFlow backups</title>"
                 << "<style>body{font-family:sans-serif;max-width:720px;margin:2em auto;padding:0 1em}"
                 << "li{margin:.25em 0}a{font-family:monospace}</style>"
                 << "<h1>Backups</h1>";
            if (files.empty()) {
                html << "<p>(пока пусто, " << kBackupDir << "/ не содержит .bck-файлов)</p>";
            } else {
                html << "<ul>";
                // свежие — сверху
                for (auto it = files.rbegin(); it != files.rend(); ++it) {
                    html << "<li><a href=\"/api/backup/download?file=" << *it
                         << "\" download>" << *it << "</a></li>";
                }
                html << "</ul>";
            }
            res.set_content(html.str(), "text/html; charset=utf-8");
        });

        // Отдача конкретного файла бэкапа. Имя — только базовое (без путей),
        // иначе возвращаем 400.
        svr.Get("/api/backup/download", [this](const Request& req, Response& res) {
            if (!req.has_param("file")) return err(res, 400, "missing 'file' query param");
            std::string fname = req.get_param_value("file");
            if (!isSafeBackupName(fname)) return err(res, 400, "invalid filename");
            std::ifstream f(std::string(kBackupDir) + "/" + fname, std::ios::binary);
            if (!f) return err(res, 404, "backup not found");
            std::stringstream ss;
            ss << f.rdbuf();
            res.set_header("Content-Disposition", "attachment; filename=\"" + fname + "\"");
            res.set_content(ss.str(), "application/octet-stream");
        });
    }

    // ---------------- Misc + static UI ----------------
    std::string webRoot = "./web";

    std::string readWebFile(const std::string& rel) const {
        std::ifstream f(webRoot + "/" + rel, std::ios::binary);
        if (!f) return {};
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    void setupMisc() {
        svr.Get("/api/health", [this](const Request&, Response& res) {
            ok(res, json{{"status", "ok"}, {"port", port}});
        });

        svr.set_file_extension_and_mimetype_mapping("jsx",  "application/javascript; charset=utf-8");
        svr.set_file_extension_and_mimetype_mapping("html", "text/html; charset=utf-8");
        svr.set_file_extension_and_mimetype_mapping("js",   "application/javascript; charset=utf-8");
        svr.set_file_extension_and_mimetype_mapping("css",  "text/css; charset=utf-8");

        const char* webDirs[] = { "./web", "../web", "../../web" };
        bool mounted = false;
        for (const char* d : webDirs) {
            if (svr.set_mount_point("/", d)) {
                std::cout << "Serving static UI from " << d << std::endl;
                webRoot = d;
                mounted = true;
                break;
            }
        }

        // pretty URLs: /clinic/:id → clinic admin dashboard
        svr.Get(R"(/clinic/(\d+))", [this](const Request&, Response& res) {
            std::string body = readWebFile("clinic.html");
            if (body.empty()) { err(res, 404, "clinic UI not found"); return; }
            res.set_content(body, "text/html; charset=utf-8");
        });
        svr.Get("/clinic", [](const Request&, Response& res) {
            res.status = 302;
            res.set_header("Location", "/");
        });

        // pretty URLs: /doctor/:id → doctor dashboard
        svr.Get(R"(/doctor/(\d+))", [this](const Request&, Response& res) {
            std::string body = readWebFile("doctor.html");
            if (body.empty()) { err(res, 404, "doctor UI not found"); return; }
            res.set_content(body, "text/html; charset=utf-8");
        });
        svr.Get("/doctor", [](const Request&, Response& res) {
            res.status = 302;
            res.set_header("Location", "/");
        });

        // pretty URLs: /patient/:id → patient dashboard
        svr.Get(R"(/patient/(\d+))", [this](const Request&, Response& res) {
            std::string body = readWebFile("patient.html");
            if (body.empty()) { err(res, 404, "patient UI not found"); return; }
            res.set_content(body, "text/html; charset=utf-8");
        });
        svr.Get("/patient", [](const Request&, Response& res) {
            res.status = 302;
            res.set_header("Location", "/");
        });

        if (!mounted) {
            std::cerr << "WARNING: could not find ./web directory. UI will not be served." << std::endl;
            svr.Get("/", [](const Request&, Response& res) {
                res.set_content(
                    "<!doctype html><meta charset=\"utf-8\"><title>MedFlow API</title>"
                    "<h1>MedFlow API</h1><p>Static UI not found. API is available at <code>/api/...</code>.</p>",
                    "text/html; charset=utf-8");
            });
        }
    }

    void setupAll() {
        setupCors();
        setupErrorHandler();
        setupPatients();
        setupDoctors();
        setupClinics();
        setupRooms();
        setupSpecializations();
        setupAppointments();
        setupSchedules();
        setupBackup();
        setupMisc();
    }
};

ApiServer::ApiServer(int port,
                     PatientService& patients,
                     DoctorService& doctors,
                     ScheduleService& schedules,
                     ClinicService& clinics,
                     RoomService& rooms,
                     SpecializationService& specializations,
                     AppointmentService& appointments,
                     BackupService& backup)
    : impl_(new Impl{
          port, patients, doctors, schedules, clinics, rooms,
          specializations, appointments, backup, {}
      }) {
    impl_->setupAll();
}

ApiServer::~ApiServer() = default;

void ApiServer::run() {
    std::cout << "MedFlow API listening on 0.0.0.0:" << impl_->port << std::endl;
    std::cout << "  Health: http://localhost:" << impl_->port << "/api/health" << std::endl;
    impl_->svr.listen("0.0.0.0", impl_->port);
}

void ApiServer::stop() {
    impl_->svr.stop();
}
