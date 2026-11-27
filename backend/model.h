#pragma once
#include "utilidades.h"

namespace eda {

// uint32_t es un entero de 4 bytes; uint8_t ocupa 1 byte.
// Los campos de texto guardan un codigo del catalogo, no el texto repetido.
struct Ciudadano {
    uint32_t dni = 0;
    uint32_t nombres = 0;
    uint32_t apellidoPaterno = 0;
    uint32_t apellidoMaterno = 0;
    uint32_t nacionalidad = 0;
    uint32_t ubigeoNacimiento = 0;
    uint32_t ubigeoDireccion = 0;
    uint32_t ciudad = 0;
    uint32_t calle = 0;
    uint32_t numeroDireccion = 0;
    uint32_t telefono = 0;
    // Este valor indica que el correo es ciudadano<DNI>@example.test.
    uint32_t correo = UINT32_MAX;
    uint32_t detalleDireccion = 0;
    uint8_t estado = 1; // 0: sin clasificar, 1: vivo, 2: fallecido
    uint8_t estadoCivil = 0;
    uint16_t reservado = 0;
};
static_assert(sizeof(Ciudadano) == 56, "El registro debe ocupar 56 bytes.");

// Datos que recibimos al registrar una persona desde la API.
// Se usan strings aqui porque solo existe un formulario a la vez.
struct DatosRegistro {
    std::string nombres;
    std::string apellidoPaterno;
    std::string apellidoMaterno;
    std::string nacionalidad;
    std::string ubigeoNacimiento;
    std::string ubigeoDireccion;
    std::string ciudad;
    std::string calle;
    std::string detalleDireccion;
    std::string telefono;
    std::string correo;
    uint32_t numeroDireccion = 0;
    uint8_t estadoCivil = 0;
};

// Varios arreglos de tamano fijo, en vez de uno gigantesco que se copia al crecer.
class AlmacenCiudadanos {
private:
    static constexpr size_t TAMANO_BLOQUE = 65536;
    std::vector<std::vector<Ciudadano>> bloques;
    size_t cantidadRegistros = 0;

public:
    void prepararEspacio();

    void agregar(const Ciudadano& ciudadano);

    Ciudadano& obtener(size_t posicion);

    const Ciudadano& obtener(size_t posicion) const;

    size_t cantidad() const;

    size_t memoriaBytes() const;
};
} // namespace eda
