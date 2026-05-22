#pragma once
#include <string>
#include <optional>
#include <vector>
#include <functional>
#include "../../models/Doctor.h"
#include "../../models/Specialization.h"

class IDoctor {
public:
	virtual ~IDoctor() = default;
	virtual void save(Doctor& doctor) = 0;

	virtual std::optional<Doctor> findById(int id) const = 0;

	virtual std::vector<Doctor> findAll() const = 0;
	virtual std::vector<Doctor> findBy(const std::function<bool(const Doctor&)>& pred) const = 0;

	virtual bool removeBy(int id) = 0;
};
