#pragma once
#include <string>
#include "../interface/Abstract/ISchedule.h"
#include "../interface/Abstract/IRoom.h"
#include "../interface/Abstract/IAppointment.h"
#include "../interface/Abstract/IDoctor.h"
#include "../interface/Abstract/IClinic.h"

#include <vector>
#include <chrono>
#include <set>

struct TimeSlot {
	std::string date; // "YYYY-MM-DD"
	std::string time; // "HH:MM"
	int durationMin = 0;
	int roomId = -1;
};

inline void to_json(nlohmann::json& j, const TimeSlot& t) {
	j = nlohmann::json{
		{"date", t.date},
		{"time", t.time},
		{"durationMin", t.durationMin},
		{"roomId", t.roomId}
	};
}


class ScheduleService {
public:
	ScheduleService(std::shared_ptr<ISchedule> schedRepo, std::shared_ptr<IAppointment> apptRepo, std::shared_ptr<IDoctor> doctorRepo, std::shared_ptr<IRoom> roomRepo)
		: schedRepo_(std::move(schedRepo)), apptRepo_(std::move(apptRepo)), doctorRepo_(std::move(doctorRepo)), roomRepo_(std::move(roomRepo)) {};

	Schedule setNewScheduleJSON(const nlohmann::json& json) {
		DayOfWeek dow = static_cast<DayOfWeek>(json.value("dayOfWeek", 0));
		return setNewSchedule(
			json.value("doctorId", -1),
			json.value("roomId", -1),
			dow,
			json.value("timeFrom", ""),
			json.value("timeTo", ""),
			json.value("slotDurationMin", -1),
			json.value("breakAfterMin", 0)
		);
	}

	Schedule setNewSchedule(int doctorId, int roomId, DayOfWeek dow, std::string timeFrom, std::string timeTo, int slotDurationMin, int breakAfterMin = 0) {
		if (!doctorRepo_->findById(doctorId).has_value()) {
			throw std::invalid_argument("Нет доктора с таким id");
		}
		if (!roomRepo_->findById(roomId).has_value()) {
			throw std::invalid_argument("Нет кабинета с таким id");
		}
		int tempF, tempT;
		try {
			tempF = toMinutes(timeFrom);
			tempT = toMinutes(timeTo);
		}
		catch (const std::exception&) { throw std::invalid_argument("Ошибка обработки времени работы"); }

		if (tempT <= tempF) { throw std::invalid_argument("Врач не может работать меньше 1 минуты"); }

		auto shedCheck = schedRepo_->findBy([doctorId, dow](const Schedule& s) { return s.doctorId == doctorId and s.dayOfWeek == dow; });
		for (auto s : shedCheck) {
			if (tempT >= toMinutes(s.timeFrom) and tempF <= toMinutes(s.timeTo) ) {
				throw std::invalid_argument("Наложение времени: врач уже занят в выбранном диапазоне");
			}
		}

		if (slotDurationMin <= 0) { throw std::invalid_argument("Приём не может быть меньше 1 минуты"); }
		if (breakAfterMin < 0) { throw std::invalid_argument("Перерыв не может быть отрицательным"); }

		Schedule s;
		s.doctorId = doctorId;
		s.roomId = roomId;
		s.dayOfWeek = dow;
		s.timeFrom = timeFrom;
		s.timeTo = timeTo;
		s.slotDurationMin = slotDurationMin;
		s.breakAfterMin = breakAfterMin;
		schedRepo_->save(s);

		return s;
	}

	std::vector<Schedule> findByDocID(int docId) {
		return schedRepo_->findBy([docId](const Schedule& s) {return s.doctorId == docId; });
	}

	std::vector<Schedule> findAll() {
		return schedRepo_->findAll();
	}

	bool removeByID(int schId) {
		return schedRepo_->removeBy(schId);
	}



	std::vector<std::string> getAvailableDays(int specId, const int clinicId = -1, int daysAhead = 14 ) const {

		auto docs = doctorRepo_->findBy([specId](const Doctor& d) { return d.specialization_id == specId; });
		std::vector<std::string> result;
		std::string today = getToday();
		for (int i = 0; i < daysAhead; i++)
		{
			std::string date = getDateAfter(i, today);
			for (const auto& doc : docs) {
				if (!getAvailableTimeSlots(doc.id, date, clinicId).empty()) {
					result.push_back(date);
					break;
				}
			}
		}
		return result;
	}

	std::vector<int> getDoctorIdByDaySlot(int specId, const std::string& date, const int clinicId = -1) const {
		std::vector<int> result;
		for (auto doc : doctorRepo_->findBy([specId](const Doctor& doc) { return doc.specialization_id == specId; })) {
			if (!getAvailableTimeSlots(doc.id, date, clinicId).empty()) {
				result.push_back(doc.id);
			}
		}
		return result;
	}

	std::vector<TimeSlot> getAvailableTimeSlots(int doctorId, const std::string& date, const int clinicId = -1) const {

		auto doctorOpt = doctorRepo_->findById(doctorId);
		if (!doctorOpt) {
			throw std::invalid_argument("Врач с таким ID не найден");
		}

		DayOfWeek dow = getDayOfWeek(date);
		std::vector<TimeSlot> result;

		// Занятость по приёмам считается один раз на (doctorId, date) — не зависит от расписания.
		auto busySlots = getBusyMinutes(doctorId, date);
		const bool isToday = (date == getToday());
		const int currentTime = isToday ? toMinutes(getCurrentTime()) : 0;

		for (const auto& sched : schedRepo_->findBy([doctorId, dow](const Schedule& s) { return s.doctorId == doctorId && s.dayOfWeek == dow; }))
		{
			// Фильтр по клинике зависит только от расписания, поэтому проверяем
			// его один раз до перебора слотов и сразу пропускаем всё расписание.
			if (clinicId != -1) {
				auto room = roomRepo_->findById(sched.roomId);
				if (!room.has_value() || room->clinicId != clinicId) continue;
			}

			int total = sched.slotDurationMin + sched.breakAfterMin;
			int schedTo = toMinutes(sched.timeTo);

			for (int t = toMinutes(sched.timeFrom); t + sched.slotDurationMin <= schedTo; t += total)
			{
				if (isToday && t < currentTime) continue; // пропускаем уже прошедшие слоты

				// Проверяем перекрытие всего диапазона слота с занятыми минутами,
				// а не только его начала — иначе приём другой длительности на той же
				// дате может «спрятаться» внутри слота.
				bool conflict = false;
				for (int m = t; m < t + sched.slotDurationMin; ++m) {
					if (busySlots.count(m)) { conflict = true; break; }
				}
				if (conflict) continue;

				TimeSlot ts;
				ts.date = date;
				ts.time = fromMinutes(t);
				ts.durationMin = sched.slotDurationMin;
				ts.roomId = sched.roomId;
				result.push_back(ts);
			}
		}

		return result;


	};

	std::set<int> getBusyMinutes(int doctorId, const std::string& date) const {
		std::set<int> busy;
		DayOfWeek dow = getDayOfWeek(date);
		auto schedules = schedRepo_->findBy([doctorId, dow](const Schedule& s) {
			return s.doctorId == doctorId && s.dayOfWeek == dow;
		});
		for (const auto& appt : apptRepo_->findBy([doctorId, date](const Appointment& a) { return a.doctorId == doctorId && a.scheduledAt.substr(0, 10) == date && a.status != AppointmentStatus::Cancelled; }))
		{
			int start = toMinutes(appt.scheduledAt.substr(11, 5));
			// Берём длительность из того расписания, в чей рабочий диапазон попадает старт.
			// Если расписания нет (например, удалено) — минимальная защита: одна минута.
			int duration = 1;
			for (const auto& s : schedules) {
				int f = toMinutes(s.timeFrom);
				int t = toMinutes(s.timeTo);
				if (start >= f && start < t) {
					duration = s.slotDurationMin > 0 ? s.slotDurationMin : 1;
					break;
				}
			}
			for (int m = start; m < start + duration; ++m) {
				busy.insert(m);
			}
		}
		return busy;
	}

	static std::string getToday() {
		using namespace std::chrono;

		auto today = floor<days>(system_clock::now());
		year_month_day resultYmd(today);

		return std::format("{:04d}-{:02d}-{:02d}",
			static_cast<int>(resultYmd.year()),
			static_cast<unsigned>(resultYmd.month()),
			static_cast<unsigned>(resultYmd.day()));
		return "2026-04-21";

	}

	

	static int toMinutes(const std::string& hhmm) {

		return std::stoi(hhmm.substr(0, 2)) * 60 + std::stoi(hhmm.substr(3, 2));

	}

	static std::string fromMinutes(int m) {
		char buf[6];
		snprintf(buf, sizeof(buf), "%02d:%02d", m / 60, m % 60);
		return buf;
	}

	static std::string getCurrentTime() {
		namespace ch = std::chrono;

		auto now = ch::system_clock::now();
		auto local = ch::current_zone()->to_local(now);   // учитываем локальный часовой пояс
		auto tod = ch::floor<ch::minutes>(local);        // обрезаем до минут

		auto hms = ch::hh_mm_ss{ tod - ch::floor<ch::days>(tod) };

		return std::format("{:02d}:{:02d}",
			static_cast<int>(hms.hours().count()),
			static_cast<int>(hms.minutes().count()));

		return "05:00";
	}

private:

	DayOfWeek getDayOfWeek(const std::string& date) const {
		using namespace std::chrono;
		int _year = std::stoi(date.substr(0, 4));
		int _month = std::stoi(date.substr(5, 2));
		int _day = std::stoi(date.substr(8, 2));
		year_month_day ymd = { year(_year), month(_month), day(_day) };
		weekday wd = weekday{ ymd };
		return static_cast<DayOfWeek>(wd.iso_encoding());
	}

	

	static std::string getDateAfter(int daysAhead = 1, std::string thisDay = getToday()) {
		// Парсим строку "YYYY-MM-DD"
		if (thisDay.size() != 10 || thisDay[4] != '-' || thisDay[7] != '-')
			throw std::invalid_argument("Неверный формат даты, ожидается YYYY-MM-DD");

		int y = std::stoi(thisDay.substr(0, 4));
		int m = std::stoi(thisDay.substr(5, 2));
		int d = std::stoi(thisDay.substr(8, 2));

		// Собираем chrono::year_month_day и прибавляем дни
		namespace ch = std::chrono;

		ch::year_month_day ymd{ ch::year{y}, ch::month{static_cast<unsigned>(m)}, ch::day{static_cast<unsigned>(d)} };

		// Переводим в sys_days, прибавляем, переводим обратно
		auto result = ch::sys_days{ ymd } + ch::days{ daysAhead };
		ch::year_month_day resultYmd{ result };

		return std::format("{:04d}-{:02d}-{:02d}",
			static_cast<int>(resultYmd.year()),
			static_cast<unsigned>(resultYmd.month()),
			static_cast<unsigned>(resultYmd.day()));
	}

	std::shared_ptr<ISchedule> schedRepo_;
	std::shared_ptr<IAppointment> apptRepo_;
	std::shared_ptr<IRoom> roomRepo_;
	std::shared_ptr<IDoctor> doctorRepo_;

};