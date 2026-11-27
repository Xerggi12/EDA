#include "bloqueo_archivo.h"

namespace eda {

BloqueoArchivo::BloqueoArchivo(const std::filesystem::path& carpeta) {
    std::filesystem::create_directories(carpeta);
    std::filesystem::path ruta = carpeta / "registro.lock";
#ifdef _WIN32
    bloqueo = CreateFileW(ruta.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (bloqueo == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Otro motor usa esta carpeta o no hay permiso de escritura.");
    }
#else
    bloqueo = ::open(ruta.c_str(), O_CREAT | O_RDWR, 0600);
    if (bloqueo < 0 || flock(bloqueo, LOCK_EX | LOCK_NB) != 0) {
        throw std::runtime_error("Otro motor usa esta carpeta o no hay permiso de escritura.");
    }
#endif
}

BloqueoArchivo::~BloqueoArchivo() {
#ifdef _WIN32
    if (bloqueo != INVALID_HANDLE_VALUE) {
        CloseHandle(bloqueo);
    }
#else
    if (bloqueo >= 0) {
        flock(bloqueo, LOCK_UN);
        ::close(bloqueo);
    }
#endif
}

} // namespace eda
