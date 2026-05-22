#pragma once
#include <string>
#include <optional>
#include <vector>
#include <functional>
#include "../../models/Patient.h"
#include "../../models/Clinic.h"

class IPatient {
public:
	virtual ~IPatient() = default;
	virtual void save(Patient& patient) = 0;

	virtual std::optional<Patient> findById(int id) const = 0;

	virtual std::vector<Patient> findAll() = 0;
	virtual std::vector<Patient> findBy(const std::function<bool(const Patient&)>& pred) = 0;
	virtual bool removeBy(int id) = 0;
};
