#pragma once
#include <string>

// Проверка номера полиса через внешний сервис заказчика.
// По сути сервер проверяет, что полис состоит из 16 цифр, но по
// требованию заказчика валидация делается именно по HTTP.
bool validateInsuranceNumRemote(const std::string& insuranceNum);
