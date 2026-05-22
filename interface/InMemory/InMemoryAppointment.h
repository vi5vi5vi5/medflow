#pragma once
#include <unordered_map>
#include "../Abstract/IAppointment.h"

class InMemoryAppointment : public IAppointment
{
public:
	void save(Appointment& appointment) override {
		if (appointment.id == -1) appointment.id = nextId_++;
		else if (appointment.id >= nextId_) nextId_ = appointment.id + 1;
		appointments_[appointment.id] = appointment;
	};
	bool remove(int appointmentId) override {
		return appointments_.erase(appointmentId) > 0;
	}

	// one param
	std::optional<Appointment> findById(int appointmentId) const override { // по id возвращаем один приём, а не вектор
		auto wantedAppointment = appointments_.find(appointmentId);
		if (wantedAppointment != appointments_.end()) {
			return wantedAppointment->second;
		}
		return std::nullopt;
	};

	std::vector<Appointment> findAll() const override {
		std::vector<Appointment> result;
		for (const auto& [id, appointment] : appointments_) {
			result.push_back(appointment);
		}
		return result;
	};

	std::vector<Appointment> findBy(const std::function<bool(const Appointment&)>& pred) const override { // Поиск приёмов по предикату (произвольная C++-лямбда)
		std::vector<Appointment> result;
		for (const auto& [id, appointment] : appointments_) {
			if (pred(appointment)) result.push_back(appointment);
		}
		return result;
	}

private:
	std::unordered_map<int, Appointment> appointments_;
	int nextId_ = 0;
};