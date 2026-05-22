#pragma once
#include "../../models/Clinic.h"
#include <optional>
#include <vector>
#include <functional>

class IClinic
{
public:
	virtual ~IClinic() = default;
	virtual void save(Clinic& c) = 0;
	virtual std::optional<Clinic> findById(int id) const = 0;
	virtual std::vector<Clinic> findAll() const = 0;
	virtual std::vector<Clinic> findBy(const std::function<bool(const Clinic&)>& pred) const = 0;
	virtual bool removeBy(int id) = 0;
};
