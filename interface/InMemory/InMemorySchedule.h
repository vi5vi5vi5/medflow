#pragma once
#include "../Abstract/ISchedule.h"
#include <unordered_map>

class InMemorySchedule : public ISchedule {
public:
	void save(Schedule& sched) override { // Сохранение информации о расписании, если id == -1, то создать новое расписание, иначе обновить существующее
		if (sched.id == -1) sched.id = nextId_++;
		else if (sched.id >= nextId_) nextId_ = sched.id + 1;
		schedules_[sched.id] = sched;
	};

	std::optional<Schedule> findById(int id) const override { // Поиск расписания по id, возвращает структуру Schedule, или nullptr, если расписание не найдено
		auto it = schedules_.find(id);
		if (it != schedules_.end()) {
			return it->second;
		}
		return std::nullopt;
	};

	std::vector<Schedule> findAll() const override { // Получение списка всех расписаний, возвращает список vector<Schedule>, может быть пустым, ничего страшного
		std::vector<Schedule> result;
		for (const auto& [id, schedule] : schedules_) {
			result.push_back(schedule);
		}
		return result;
	};

	bool removeBy(int id) override { // Удаление расписания по id, возвращает true, если удаление прошло успешно, false, если расписание не найдено
		return schedules_.erase(id) > 0;
	};

	std::vector<Schedule> findBy(const std::function<bool(const Schedule&)>& pred) const override { // Поиск расписаний по предикату (произвольная C++-лямбда)
		std::vector<Schedule> result;
		for (const auto& [id, schedule] : schedules_) {
			if (pred(schedule)) result.push_back(schedule);
		}
		return result;
	}

private:
	std::unordered_map<int, Schedule> schedules_;
	int nextId_ = 0;
};