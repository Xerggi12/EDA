#pragma once
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace eda {
// Detalle del sistema operativo, separado de los algoritmos del curso.
// El bloqueo dura mientras este objeto exista; se libera al cerrar el motor.
class BloqueoArchivo {
private:
#ifdef _WIN32
    HANDLE bloqueo = INVALID_HANDLE_VALUE;
#else
    int bloqueo = -1;
#endif

public:
    explicit BloqueoArchivo(const std::filesystem::path& carpeta);

    ~BloqueoArchivo();
};
} // namespace eda
