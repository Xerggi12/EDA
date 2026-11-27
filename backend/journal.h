#pragma once
#include "model.h"
#include "bloqueo_archivo.h"
#include <cstring>
#include <fstream>

namespace eda {

// Los enteros se guardan byte por byte para mantener un formato estable.
// Nunca guardamos directamente objetos string ni direcciones de memoria.
void escribirEntero32(std::string& salida, uint32_t valor);
uint32_t leerEntero32(const std::string& entrada, size_t& posicion);
std::string serializarCiudadano(const Ciudadano& ciudadano);
Ciudadano deserializarCiudadano(const std::string& entrada);

constexpr uint32_t EVENTO_TEXTO = 1;
constexpr uint32_t EVENTO_ALTA = 2;
constexpr uint32_t EVENTO_ESTADO = 3;
constexpr uint32_t EVENTO_ACTUALIZACION = 4;

// Longitud + texto: permite guardar el motivo sin depender de separadores.
void escribirTextoEvento(std::string& salida, const std::string& texto);
std::string leerTextoEvento(const std::string& entrada, size_t& posicion);

// Cada cambio se agrega al final del archivo: no sobrescribimos ni borramos personas.
class ArchivoCiudadanos {
private:
    // Se declara primero para que se destruya despues de cerrar los streams.
    BloqueoArchivo bloqueo;
    std::filesystem::path rutaArchivo;
    std::ifstream entradaArchivo;
    std::ofstream salidaArchivo;

    // FNV-1a: comprobacion sencilla para detectar cambios accidentales en los bytes.
    static uint32_t calcularChecksum(uint32_t tipo, const std::string& datos);

public:
    explicit ArchivoCiudadanos(const std::filesystem::path& carpeta);

    uint64_t tamanoArchivo() const;

    void abrirLectura();

    bool leerEvento(uint32_t& tipo, std::string& datos);

    void abrirEscritura();

    void agregarEvento(uint32_t tipo, const std::string& datos);

    void guardarCambios();
};
} // namespace eda
