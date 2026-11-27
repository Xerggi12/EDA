#pragma once
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eda {
std::string escaparJson(const std::string& valor);
uint64_t leerEntero(const std::string& texto, uint64_t maximo);
uint32_t convertirDni(const std::string& texto);
std::string formatearDni(uint32_t dni);
std::string normalizarTexto(std::string texto);

} // namespace eda
