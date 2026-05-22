#pragma once
#include "../../models/Room.h"
#include "../../models/Clinic.h"
#include <optional>
#include <vector>
#include <functional>

class IRoom
{
public:
	virtual ~IRoom() = default;
	virtual void save(Room& room) = 0;
	virtual std::optional<Room> findById(int id) const = 0;
	virtual std::vector<Room> findAll() const = 0;
	virtual std::vector<Room> findBy(const std::function<bool(const Room&)>& pred) const = 0;
	virtual bool removeBy(int id) = 0;
};
