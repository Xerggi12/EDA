#include "cuckoo_hash.h"

namespace eda {

uint64_t TablaCuckoo::mezclar(uint64_t numero) {
    numero = numero ^ (numero >> 30);
    numero = numero * 0xbf58476d1ce4e5b9ULL;
    numero = numero ^ (numero >> 27);
    numero = numero * 0x94d049bb133111ebULL;
    return numero ^ (numero >> 31);
}

size_t TablaCuckoo::primeraPosicion(uint32_t dni) const {
    uint64_t hash = mezclar(dni ^ semilla);
    return static_cast<size_t>(hash % grupos.size());
}

size_t TablaCuckoo::segundaPosicion(uint32_t dni) const {
    uint64_t otraSemilla = semilla + 0x9e3779b97f4a7c15ULL;
    uint64_t hash = mezclar(dni ^ otraSemilla);
    size_t posicion = static_cast<size_t>(hash % grupos.size());
    if (posicion == primeraPosicion(dni)) {
        posicion = posicion ^ 1;
    }
    return posicion;
}

bool TablaCuckoo::guardarSiHayEspacio(size_t numeroGrupo, const Entrada& entrada) {
    for (size_t i = 0; i < ESPACIOS_POR_GRUPO; i++) {
        if (grupos[numeroGrupo][i].dni == VACIO) {
            grupos[numeroGrupo][i] = entrada;
            return true;
        }
    }
    return false;
}

bool TablaCuckoo::colocar(Entrada& pendiente) {
    size_t grupoActual = primeraPosicion(pendiente.dni);

    for (size_t intento = 0; intento < MAXIMO_DESPLAZAMIENTOS; intento++) {
        size_t grupoAlternativo = primeraPosicion(pendiente.dni);
        if (grupoActual == grupoAlternativo) {
            grupoAlternativo = segundaPosicion(pendiente.dni);
        }
        if (guardarSiHayEspacio(grupoActual, pendiente)) {
            return true;
        }
        if (guardarSiHayEspacio(grupoAlternativo, pendiente)) {
            return true;
        }

        // Ambos grupos estan llenos. Desplazamos una entrada a su otra opcion.
        size_t espacio = static_cast<size_t>(mezclar(pendiente.dni + intento + semilla) %
                                             ESPACIOS_POR_GRUPO);
        std::swap(pendiente, grupos[grupoActual][espacio]);

        size_t primera = primeraPosicion(pendiente.dni);
        if (grupoActual == primera) {
            grupoActual = segundaPosicion(pendiente.dni);
        } else {
            grupoActual = primera;
        }
    }
    return false;
}

void TablaCuckoo::reconstruir(size_t nuevaCantidadGrupos, Entrada pendiente) {
    // swap conserva la tabla anterior sin copiar millones de entradas.
    std::vector<std::array<Entrada, ESPACIOS_POR_GRUPO>> anteriores;
    anteriores.swap(grupos);

    bool terminado = false;
    while (!terminado) {
        grupos.assign(nuevaCantidadGrupos, std::array<Entrada, ESPACIOS_POR_GRUPO>());
        semilla = mezclar(semilla + 1);
        bool todasColocadas = true;

        for (size_t grupo = 0; grupo < anteriores.size() && todasColocadas; grupo++) {
            for (size_t espacio = 0; espacio < ESPACIOS_POR_GRUPO; espacio++) {
                if (anteriores[grupo][espacio].dni == VACIO) {
                    continue;
                }
                Entrada entrada = anteriores[grupo][espacio];
                if (!colocar(entrada)) {
                    todasColocadas = false;
                    break;
                }
            }
        }
        if (todasColocadas && pendiente.dni != VACIO) {
            Entrada entrada = pendiente;
            todasColocadas = colocar(entrada);
        }
        if (todasColocadas) {
            terminado = true;
        } else {
            // anteriores no se modifico: podemos volver a intentar.
            nuevaCantidadGrupos = nuevaCantidadGrupos * 2;
        }
    }
}

TablaCuckoo::TablaCuckoo() : grupos(16) {
}

void TablaCuckoo::reservar(size_t registrosEsperados) {
    size_t gruposNecesarios = grupos.size();
    while (gruposNecesarios * ESPACIOS_POR_GRUPO * OCUPACION_MAXIMA < registrosEsperados) {
        gruposNecesarios = gruposNecesarios * 2;
    }
    if (gruposNecesarios > grupos.size()) {
        reconstruir(gruposNecesarios, Entrada());
    }
}

bool TablaCuckoo::buscar(uint32_t dni, uint32_t& posicion) const {
    size_t candidatos[2] = {primeraPosicion(dni), segundaPosicion(dni)};
    for (size_t i = 0; i < 2; i++) {
        size_t grupo = candidatos[i];
        for (size_t espacio = 0; espacio < ESPACIOS_POR_GRUPO; espacio++) {
            if (grupos[grupo][espacio].dni == dni) {
                posicion = grupos[grupo][espacio].posicion;
                return true;
            }
        }
    }
    return false;
}

bool TablaCuckoo::insertar(uint32_t dni, uint32_t posicion) {
    uint32_t posicionExistente = 0;
    if (buscar(dni, posicionExistente)) {
        return false;
    }
    reservar(cantidadEntradas + 1);
    Entrada nuevaEntrada;
    nuevaEntrada.dni = dni;
    nuevaEntrada.posicion = posicion;
    if (!colocar(nuevaEntrada)) {
        // nuevaEntrada puede ser una entrada desplazada; no se debe perder.
        reconstruir(grupos.size() * 2, nuevaEntrada);
    }
    cantidadEntradas++;
    return true;
}

size_t TablaCuckoo::cantidad() const {
    return cantidadEntradas;
}

size_t TablaCuckoo::memoriaBytes() const {
    return grupos.capacity() * sizeof(std::array<Entrada, ESPACIOS_POR_GRUPO>);
}

} // namespace eda
