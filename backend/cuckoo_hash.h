#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace eda {

// Tabla hash Cuckoo: cada DNI tiene dos grupos posibles, de cuatro espacios.
// Guardamos DNI -> posicion; los datos completos estan en otro arreglo.
class TablaCuckoo {
private:
    static constexpr uint32_t VACIO = std::numeric_limits<uint32_t>::max();
    static constexpr size_t ESPACIOS_POR_GRUPO = 4;
    static constexpr size_t MAXIMO_DESPLAZAMIENTOS = 512;
    static constexpr double OCUPACION_MAXIMA = 0.85;

    struct Entrada {
        uint32_t dni = VACIO;
        uint32_t posicion = 0;
    };

    std::vector<std::array<Entrada, ESPACIOS_POR_GRUPO>> grupos;
    size_t cantidadEntradas = 0;
    uint64_t semilla = 0x243f6a8885a308d3ULL;

    // Mezcla numerica para repartir los DNI por toda la tabla.
    // Las constantes pertenecen a la funcion de mezcla SplitMix64.
    static uint64_t mezclar(uint64_t numero);

    size_t primeraPosicion(uint32_t dni) const;

    size_t segundaPosicion(uint32_t dni) const;

    bool guardarSiHayEspacio(size_t numeroGrupo, const Entrada& entrada);

    bool colocar(Entrada& pendiente);

    void reconstruir(size_t nuevaCantidadGrupos, Entrada pendiente);

public:
    TablaCuckoo();

    void reservar(size_t registrosEsperados);

    bool buscar(uint32_t dni, uint32_t& posicion) const;

    bool insertar(uint32_t dni, uint32_t posicion);

    size_t cantidad() const;

    size_t memoriaBytes() const;
};
} // namespace eda
