#pragma once
#include "../Abstract/IDoctor.h"
#include <unordered_map>

class InMemoryDoctor : public IDoctor {
public:
	void save(Doctor& doc) override { // Сохранение информации о докторе, если id == -1, то создать нового доктора, иначе обновить существующего
		if (doc.id == -1) doc.id = nextId_++;
		else if (doc.id >= nextId_) nextId_ = doc.id + 1;
		doctors_[doc.id] = doc;
	};

	bool removeBy(int id) override { // Удаление доктора по id, возвращает true, если удаление прошло успешно, false, если доктор не найден
		return doctors_.erase(id) > 0;
	}

	std::optional<Doctor> findById(int id) const override { // Поиск доктора по id, возвращает структуру Doctor, или nullptr, если доктор не найден
		auto wantedDoctor = doctors_.find(id);
		if (wantedDoctor != doctors_.end()) {
			return wantedDoctor->second;
		}
		return std::nullopt;
	};

	std::vector<Doctor> findAll() const override { // Получение списка всех докторов, возвращает список vector<Doctor>, может быть пустым, ничего страшного
		std::vector<Doctor> result;
		for (const auto& [id, doctor] : doctors_) {
			result.push_back(doctor);
		}
		return result;
	};

	std::vector<Doctor> findBy(const std::function<bool(const Doctor&)>& pred) const override { // Поиск докторов по предикату (произвольная C++-лямбда)
		std::vector<Doctor> result;
		for (const auto& [id, doctor] : doctors_) {
			if (pred(doctor)) result.push_back(doctor);
		}
		return result;
	}


private:
	std::unordered_map<int, Doctor> doctors_;
	int nextId_ = 0;
};