#pragma once
#include "../Abstract/IPatient.h"
#include <unordered_map>

class InMemoryPatient : public IPatient {
public:
	void save(Patient& pat) override { // Сохранение информации о пациенте, если id == -1, то создать нового пациента, иначе обновить существующего
		if (pat.id == -1) pat.id = nextId_++;
		else if (pat.id >= nextId_) nextId_ = pat.id + 1;
		patients_[pat.id] = pat;
	};

	bool removeBy(int id) override { // Удаление пациента по id, возвращает true, если удаление прошло успешно, false, если пациент не найден
		return patients_.erase(id) > 0;
	}

	std::optional<Patient> findById(int id) const override { // Поиск пациента по id, возвращает структуру Patient, или nullptr, если пациент не найден
		auto wantedPatient = patients_.find(id);
		if (wantedPatient != patients_.end()) {
			return wantedPatient->second;
		}
		return std::nullopt;
	};


	std::vector<Patient> findAll() override { // Получение списка всех пациентов, возвращает список vector<Patient>, может быть пустым, ничего страшного
		std::vector<Patient> result;
		for (const auto& [id, patient] : patients_) {
			result.push_back(patient);
		}
		return result;
	};

	std::vector<Patient> findBy(const std::function<bool(const Patient&)>& pred) override { // Поиск пациентов по предикату (произвольная C++-лямбда)
		std::vector<Patient> result;
		for (const auto& [id, patient] : patients_) {
			if (pred(patient)) result.push_back(patient);
		}
		return result;
	}


private:
	std::unordered_map<int, Patient> patients_;
	int nextId_ = 0;
};