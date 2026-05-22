#pragma once
#include "../Abstract/ISpecialization.h"
#include <unordered_map>

class InMemorySpecialization : public ISpecialization {
public:
	void save(Specialization& spec) override { // Сохранение информации о специализации, если id == -1, то создать новую специализацию, иначе обновить существующую
		if (spec.id == -1) spec.id = nextId_++;
		else if (spec.id >= nextId_) nextId_ = spec.id + 1;
		specializations_[spec.id] = spec;
	};

	std::optional<Specialization> findBy(int id) const override { // Поиск специализации по id, возвращает структуру Specialization, или nullptr, если специализация не найдена
		auto wantedSpecialization = specializations_.find(id);
		if (wantedSpecialization != specializations_.end()) {
			return wantedSpecialization->second;
		}
		return std::nullopt;
	};

	std::optional<Specialization> findBy(std::string name) const override { // Поиск специализации по имени, возвращает структуру Specialization, или nullptr, если специализация не найдена
		for (const auto& [id, specialization] : specializations_) {
			if (specialization.name.find(name) != std::string::npos) {
				return specialization;
			}
		}
		return std::nullopt;
	};

	std::vector<Specialization> findAll() const override { // Получение списка всех специализаций, возвращает список vector<Specialization>, может быть пустым, ничего страшного
		std::vector<Specialization> result;
		for (const auto& [id, specialization] : specializations_) {
			result.push_back(specialization);
		}
		return result;
	};

	bool removeBy(int id) override { // Удаление специализации по id, возвращает true, если удаление прошло успешно, false, если специализация не найдена
		return specializations_.erase(id) > 0;
	}

private:
	std::unordered_map<int, Specialization> specializations_;
	int nextId_ = 0;
};