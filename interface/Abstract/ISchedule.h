#pragma once
#include "../../models/Schedule.h"
#include "../../models/Doctor.h"
#include "../../models/Room.h"
#include <optional>
#include <vector>
#include <functional>

class ISchedule {
public:
	virtual ~ISchedule() = default;
	virtual void save(Schedule& schedule) = 0;

	virtual std::optional<Schedule> findById(int id) const = 0;

	virtual std::vector<Schedule> findAll() const = 0;
	virtual std::vector<Schedule> findBy(const std::function<bool(const Schedule&)>& pred) const = 0;

	virtual bool removeBy(int id) = 0;
};
