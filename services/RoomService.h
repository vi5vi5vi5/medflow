#pragma once
#include "../interface/Abstract/IRoom.h"
#include "../interface/Abstract/IClinic.h"
#include "../interface/Abstract/ISchedule.h"
#include "../requirements/json.hpp"

class RoomService {
public:
	RoomService(std::shared_ptr<IRoom> roomRepo,
	            std::shared_ptr<IClinic> clinicRepo,
	            std::shared_ptr<ISchedule> schedRepo)
		: roomRepo_(std::move(roomRepo)),
		  clinicRepo_(std::move(clinicRepo)),
		  schedRepo_(std::move(schedRepo)) {}

	Room addRoom(const std::string& number, int clinicId, int floor = 1) {
		if (!clinicRepo_->findById(clinicId).has_value()) {
			throw std::invalid_argument("Клиника с таким ID не найдена");
		}
		Room r;
		r.number = number;
		r.floor = floor; // по умолчанию 1 этаж, можно указать другой этаж
		r.clinicId = clinicId;
		roomRepo_->save(r);
		return r;
	}

	Room addRoomJSON(const nlohmann::json& json) { //json: number, clinicId, floor
		std::string number = json.value("number", "БезНомера");
		int clinicId = json.value("clinicId", -1);
		int floor = json.value("floor", 1);
		return addRoom(number, clinicId, floor);
	}

	std::optional<Room> findById(int id) { return roomRepo_->findById(id); } // Получение комнаты по id, возвращает std::optional<Room>, который может быть пустым, если комната не найдена

	std::vector<Room> findByClinicId(int clinicId) { return roomRepo_->findBy([clinicId](const Room& r) { return r.clinicId == clinicId; }); } // Получение всех комнат по id клиники

	std::vector<Room> findByJson(const nlohmann::json& json) { // Получение всех комнат, которые соответствуют фильтру в json: number, floor, clinicId
		std::vector<std::function<bool(const Room&)>> filters;
		if (json.contains("number")) {
			std::string number = json["number"];
			filters.push_back([number](const Room& r) { return r.number == number; });
		}
		if (json.contains("floor")) {
			int floor = json["floor"];
			filters.push_back([floor](const Room& r) { return r.floor == floor; });
		}
		if (json.contains("clinicId")) {
			int clinicId = json["clinicId"];
			filters.push_back([clinicId](const Room& r) { return r.clinicId == clinicId; });
		}
		return roomRepo_->findBy([filters](const Room& r) {
			for (const auto& filter : filters) {
				if (!filter(r)) return false;
			}
			return true;
		});
	}

	std::vector<Room> findAll() { return roomRepo_->findAll(); } // Получение всех комнат

	Room updateRoom(int id, const nlohmann::json& json) { // обновление данных комнаты по id и json с полями для обновления
		auto roomOpt = roomRepo_->findById(id);
		if (!roomOpt) {
			throw std::invalid_argument("Комната с таким ID не найдена");
		}
		Room r = roomOpt.value();
		if (json.contains("number")) {
			r.number = json["number"];
		}
		if (json.contains("floor")) {
			r.floor = json["floor"];
		}
		if (json.contains("clinicId")) {
			int clinicId = json["clinicId"];
			if (!clinicRepo_->findById(clinicId).has_value()) {
				throw std::invalid_argument("Клиника с таким ID не найдена");
			}
			r.clinicId = clinicId;
		}
		roomRepo_->save(r);
		return r;
	}

	// Каскадное удаление кабинета: удаляются все расписания, привязанные к нему.
	bool removeByID(int id) {
		if (!roomRepo_->findById(id).has_value()) return false;
		for (const auto& s : schedRepo_->findBy([id](const Schedule& s) { return s.roomId == id; })) {
			schedRepo_->removeBy(s.id);
		}
		return roomRepo_->removeBy(id);
	}

private:
	std::shared_ptr<IRoom>     roomRepo_;
	std::shared_ptr<IClinic>   clinicRepo_;
	std::shared_ptr<ISchedule> schedRepo_;
};