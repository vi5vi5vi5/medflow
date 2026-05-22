#pragma once
#include "../Abstract/IRoom.h"
#include <unordered_map>

class InMemoryRoom : public IRoom{
public:
	void save(Room& r) override { // Сохранение комнаты, если id == -1, то присвоить новый id, иначе перезаписать существующую комнату
		if (r.id == -1) r.id = nextId_++;
		else if (r.id >= nextId_) nextId_ = r.id + 1;
		rooms_[r.id] = r;
	};

	std::vector<Room> findAll() const override { // Поиск всех комнат, возвращает список vector<Room>, может быть пустым, ничего страшного
		std::vector<Room> result;
		for (const auto& [id, room] : rooms_) {
			result.push_back(room);
		}
		return result;
	};

	std::optional<Room> findById(int id) const override { // Поиск комнаты по id комнаты
		auto wantedRoom = rooms_.find(id);
		if (wantedRoom != rooms_.end()) {
			return wantedRoom->second;
		}
		return std::nullopt;
	};

	bool removeBy(int id) override { // Удаление комнаты по id, возвращает true, если удаление прошло успешно, false, если комната не найдена
		return rooms_.erase(id) > 0;
	}

	std::vector<Room> findBy(const std::function<bool(const Room&)>& pred) const override { // Поиск комнат по предикату (произвольная C++-лямбда)
		std::vector<Room> result;
		for (const auto& [id, room] : rooms_) {
			if (pred(room)) result.push_back(room);
		}
		return result;
	}


private:
	std::unordered_map<int, Room> rooms_;
	int nextId_ = 0;
};