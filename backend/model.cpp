#include "model.h"

namespace eda {

void AlmacenCiudadanos::prepararEspacio() {
    size_t numeroBloque = cantidadRegistros / TAMANO_BLOQUE;
    if (numeroBloque == bloques.size()) {
        bloques.push_back(std::vector<Ciudadano>(TAMANO_BLOQUE));
    }
}

void AlmacenCiudadanos::agregar(const Ciudadano& ciudadano) {
    prepararEspacio();
    obtener(cantidadRegistros) = ciudadano;
    cantidadRegistros++;
}

Ciudadano& AlmacenCiudadanos::obtener(size_t posicion) {
    size_t numeroBloque = posicion / TAMANO_BLOQUE;
    size_t posicionEnBloque = posicion % TAMANO_BLOQUE;
    return bloques.at(numeroBloque)[posicionEnBloque];
}

const Ciudadano& AlmacenCiudadanos::obtener(size_t posicion) const {
    size_t numeroBloque = posicion / TAMANO_BLOQUE;
    size_t posicionEnBloque = posicion % TAMANO_BLOQUE;
    return bloques.at(numeroBloque)[posicionEnBloque];
}

size_t AlmacenCiudadanos::cantidad() const {
    return cantidadRegistros;
}

size_t AlmacenCiudadanos::memoriaBytes() const {
    return bloques.size() * TAMANO_BLOQUE * sizeof(Ciudadano);
}

} // namespace eda
