#pragma once
#include <string>
#include <optional>
#include <vector>
#include "../../models/Specialization.h"

class ISpecialization {
public:

	virtual ~ISpecialization() = default;
	virtual void save(Specialization& specialization) = 0;
	
	virtual std::optional<Specialization> findBy(int id) const = 0;
	virtual std::optional<Specialization> findBy(std::string name) const = 0;

	virtual std::vector<Specialization> findAll() const = 0;

	virtual bool removeBy(int id) = 0;
};