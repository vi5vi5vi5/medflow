#pragma once
#include <string>
#include <optional>
#include <vector>
#include <functional>
#include "../../models/Appointment.h"
#include "../../models/Patient.h"
#include "../../models/Doctor.h"


class IAppointment
{
public:
	virtual ~IAppointment() = default;

	virtual void save(Appointment& appointment) = 0;
	virtual bool remove(int appointmentId) = 0;

	virtual std::optional<Appointment> findById(int appointmentId) const = 0;

	virtual std::vector<Appointment> findAll() const = 0;
	virtual std::vector<Appointment> findBy(const std::function<bool(const Appointment&)>& pred) const = 0;
};
