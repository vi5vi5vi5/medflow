#pragma once
#include "../interface/Abstract/ISpecialization.h"
#include "../interface/Abstract/IDoctor.h"
#include "../requirements/json.hpp"

class SpecializationService
{
public:
	SpecializationService(std::shared_ptr<ISpecialization> specializationRepo,
	                      std::shared_ptr<IDoctor> doctorRepo)
		: specRepo_(std::move(specializationRepo)),
		  doctorRepo_(std::move(doctorRepo)) {}
	Specialization newSpecializationJSON(const nlohmann::json& json) { // создание специализации по json: name
		return newSpecialization(
			json.value("name", "")
		);
	}

	Specialization newSpecialization( // создание специализации по параметрам
		const std::string& name // Название специализации
	)
	{
		if (name.empty()) {
			throw std::invalid_argument("Название специализации не может быть пустым");
		}

		Specialization s;
		s.name = name;
		specRepo_->save(s);

		return s;
		
	};

	Specialization update(int id, const nlohmann::json& json) { // обновление данных специализации по id и json с полями для обновления
		auto specOpt = specRepo_->findBy(id);
		if (!specOpt) {
			throw std::invalid_argument("Специализация с таким ID не найдена");
		}
		Specialization s = specOpt.value();
		if (json.contains("name")) {
			s.name = json["name"];
		}
		specRepo_->save(s);
		return s;
	}

	std::optional<Specialization> findByID(int id) { return specRepo_->findBy(id); } // поиск специализации по id
	std::optional<Specialization> findByName(const std::string& name) { return specRepo_->findBy(name); } // поиск специализации по названию
	std::vector<Specialization> findAll() { return specRepo_->findAll(); } // Получение всех специализаций

	// Каскадное удаление специализации: у всех докторов с этой специализацией
	// поле specialization_id сбрасывается в -1. Сами доктора, их расписания и приёмы остаются.
	bool removeByID(int id) {
		if (!specRepo_->findBy(id).has_value()) return false;
		for (auto d : doctorRepo_->findBy([id](const Doctor& d) { return d.specialization_id == id; })) {
			d.specialization_id = -1;
			doctorRepo_->save(d);
		}
		return specRepo_->removeBy(id);
	}

private:
	std::shared_ptr<ISpecialization> specRepo_;
	std::shared_ptr<IDoctor>         doctorRepo_;
};