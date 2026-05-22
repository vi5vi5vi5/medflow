#pragma once
#include "../interface/Abstract/IAppointment.h"
#include "../interface/Abstract/IPatient.h"
#include "../interface/Abstract/IDoctor.h"
#include "../interface/Abstract/ISchedule.h"
#include "ScheduleService.h"
#include "../requirements/json.hpp"
#include <vector>
#include <chrono>


class AppointmentService {
public:
	AppointmentService(std::shared_ptr<IAppointment> apptRepo, std::shared_ptr<IPatient> patientRepo, std::shared_ptr<IDoctor> doctorRepo, std::shared_ptr<ISchedule> schedRepo)
		: apptRepo_(std::move(apptRepo)), patientRepo_(std::move(patientRepo)), doctorRepo_(std::move(doctorRepo)), schedRepo_(std::move(schedRepo)) {};

	Appointment newAppointmentJSON(const nlohmann::json& json) {
		return newAppointent(
			json.value("patientId", -1),
			json.value("doctorId", -1),
			json.value("scheduledAt", "")
		);
	}

	Appointment newAppointent(int patientId, int doctorId, std::string scheduledAt) {
		if (!patientRepo_->findById(patientId)) {
			throw std::invalid_argument("Пациент с таким ID не найден");
		}
		if (!doctorRepo_->findById(doctorId)) {
			throw std::invalid_argument("Доктор с таким ID не найден");
		}
		if (!validateDateTime(scheduledAt)) {
			throw std::invalid_argument("Неверный формат даты и времени, ожидается YYYY-MM-DD HH:MM");
		}

		const std::string newDate = scheduledAt.substr(0, 10);
		const int newStart = ScheduleService::toMinutes(scheduledAt.substr(11, 5));
		const int newDur = lookupSlotDuration(doctorId, newDate, newStart);
		const int newEnd = newStart + newDur;

		auto intersects = [&](const Appointment& a) {
			if (a.status == AppointmentStatus::Cancelled) return false;
			if (a.scheduledAt.size() < 16) return false;
			if (a.scheduledAt.substr(0, 10) != newDate) return false;
			int aStart = ScheduleService::toMinutes(a.scheduledAt.substr(11, 5));
			int aDur = lookupSlotDuration(a.doctorId, newDate, aStart);
			int aEnd = aStart + aDur;
			return aStart < newEnd && newStart < aEnd;
		};

		if (apptRepo_->findBy([doctorId, &intersects](const Appointment& a) {
			return a.doctorId == doctorId && intersects(a);
		}).size() > 0) {
			throw std::invalid_argument("Доктор уже занят в это время");
		}
		if (apptRepo_->findBy([patientId, &intersects](const Appointment& a) {
			return a.patientId == patientId && intersects(a);
		}).size() > 0) {
			throw std::invalid_argument("Пациент уже имеет приём в это время");
		}
		
		Appointment appt;
		appt.patientId = patientId;
		appt.doctorId = doctorId;
		appt.scheduledAt = scheduledAt;
		appt.status = AppointmentStatus::Scheduled;
		appt.notes = "";
		appt.createdAt = ScheduleService::getToday() + " " + ScheduleService::getCurrentTime();

		apptRepo_->save(appt);

		return appt;

	}

	bool removeByID(int apptId) {
		return apptRepo_->remove(apptId);
	}

	std::vector<Appointment> findAll() {
		return apptRepo_->findAll();
	}

	std::vector<Appointment> findByPatientId(int patientId) {
		return apptRepo_->findBy([patientId](const Appointment& a) { return a.patientId == patientId; });
	}

	std::vector<Appointment> findByJSON(const nlohmann::json& json) {
		int patientId = json.value("patientId", -1);
		int doctorId = json.value("doctorId", -1);
		std::string scheduledAt = json.value("scheduledAt", "");
		int status = json.value("status", -1);
		return apptRepo_->findBy([patientId, doctorId, scheduledAt, status](const Appointment& a) {
			bool match = true;
			if (patientId != -1) match = match && (a.patientId == patientId);
			if (doctorId != -1) match = match && (a.doctorId == doctorId);
			if (!scheduledAt.empty()) match = match && (a.scheduledAt == scheduledAt);
			if (status != -1) match = match && (a.status == static_cast<AppointmentStatus>(status));
			return match;
		});
	}




	Appointment updateStatus(int apptId, AppointmentStatus newStatus) {
		auto apptOpt = apptRepo_->findById(apptId);
		if (!apptOpt) {
			throw std::invalid_argument("Приём с таким ID не найден");
		}
		Appointment appt = apptOpt.value();
		appt.status = newStatus;
		apptRepo_->save(appt);
		return appt;
	}

	Appointment updateNotes(int apptId, const std::string& newNotes) {
		auto apptOpt = apptRepo_->findById(apptId);
		if (!apptOpt) {
			throw std::invalid_argument("Приём с таким ID не найден");
		}
		Appointment appt = apptOpt.value();
		appt.notes = newNotes;
		apptRepo_->save(appt);
		return appt;
	}

private:

	static bool validateDateTime(const std::string& dateTime) {
		if (dateTime.size() != 16 || dateTime[4] != '-' || dateTime[7] != '-' || dateTime[10] != ' ' || dateTime[13] != ':') {
			return false;
		}
		int year, month, day, hour, minute;
		try {
			year   = std::stoi(dateTime.substr(0, 4));
			month  = std::stoi(dateTime.substr(5, 2));
			day    = std::stoi(dateTime.substr(8, 2));
			hour   = std::stoi(dateTime.substr(11, 2));
			minute = std::stoi(dateTime.substr(14, 2));
		}
		catch (const std::exception&) {
			return false;
		}
		if (year < 1900 || year > 9999) return false;
		if (hour < 0 || hour > 23) return false;
		if (minute < 0 || minute > 59) return false;

		// проверяем корректность даты через chrono (учитывает високосные годы и длину месяца)
		std::chrono::year_month_day ymd{
			std::chrono::year{year},
			std::chrono::month{static_cast<unsigned>(month)},
			std::chrono::day{static_cast<unsigned>(day)}
		};
		return ymd.ok();
	}

	// Находит slotDurationMin расписания, в которое попадает (date, startMin).
	// Если расписания для приёма уже нет (например, его удалили) — fallback 1 минута,
	// чтобы пересечения проверялись хотя бы по точному совпадению старта.
	int lookupSlotDuration(int doctorId, const std::string& date, int startMin) const {
		using namespace std::chrono;
		year_month_day ymd{
			year{std::stoi(date.substr(0, 4))},
			month{static_cast<unsigned>(std::stoi(date.substr(5, 2)))},
			day{static_cast<unsigned>(std::stoi(date.substr(8, 2)))}
		};
		DayOfWeek dow = static_cast<DayOfWeek>(weekday{ ymd }.iso_encoding());
		auto schedules = schedRepo_->findBy([doctorId, dow](const Schedule& s) {
			return s.doctorId == doctorId && s.dayOfWeek == dow;
		});
		for (const auto& s : schedules) {
			int f = ScheduleService::toMinutes(s.timeFrom);
			int t = ScheduleService::toMinutes(s.timeTo);
			if (startMin >= f && startMin < t) {
				return s.slotDurationMin > 0 ? s.slotDurationMin : 1;
			}
		}
		return 1;
	}


	std::shared_ptr<IAppointment> apptRepo_;
	std::shared_ptr<IPatient> patientRepo_;
	std::shared_ptr<IDoctor> doctorRepo_;
	std::shared_ptr<ISchedule> schedRepo_;

};