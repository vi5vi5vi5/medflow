#pragma once
#include <memory>
#include <string>

#include "../services/PatientService.h"
#include "../services/DoctorService.h"
#include "../services/ScheduleService.h"
#include "../services/ClinicService.h"
#include "../services/RoomService.h"
#include "../services/SpecializationService.h"
#include "../services/AppointmentService.h"
#include "../services/BackupService.h"

class ApiServer {
public:
    ApiServer(int port,
              PatientService& patients,
              DoctorService& doctors,
              ScheduleService& schedules,
              ClinicService& clinics,
              RoomService& rooms,
              SpecializationService& specializations,
              AppointmentService& appointments,
              BackupService& backup);

    ~ApiServer();

    void run();
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
