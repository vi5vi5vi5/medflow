#pragma once
#include <unordered_map>
#include "../Abstract/IClinic.h"

class InMemoryClinic : public IClinic {
public:
	void save(Clinic& c) override { //Сохранение клиники в репозитории
		if (c.id == -1) c.id = nextId_++ ;
		else if (c.id >= nextId_) nextId_ = c.id + 1;
		clinics_[c.id] = c;
	};

	std::optional <Clinic> findById(int id) const override { //Поиск клиники по id
		auto wantedClinic = clinics_.find(id);
		if (wantedClinic != clinics_.end()) {
			return (*wantedClinic).second;
		}
		return std::nullopt;
	};

	std::vector<Clinic> findAll() const override { //Получение всех клиник из репозитория
		std::vector<Clinic> result;
		for (const auto& [id, clinic] : clinics_) {
			result.push_back(clinic);
		}
		return result;
	};

	bool removeBy(int id) override { // Удаление клиники по id, возвращает true, если удаление прошло успешно, false, если клиника не найдена
		return clinics_.erase(id) > 0;
	}

	std::vector<Clinic> findBy(const std::function<bool(const Clinic&)>& pred) const override { // Поиск клиник по предикату (произвольная C++-лямбда)
		std::vector<Clinic> result;
		for (const auto& [id, c] : clinics_) {
			if (pred(c)) result.push_back(c);
		}
		return result;
	}

private:
	std::unordered_map<int, Clinic> clinics_;
	int nextId_ = 0;
};