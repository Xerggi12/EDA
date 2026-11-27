#pragma once
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <vector>

namespace eda {

// Arbol B+ para el catalogo de nombres y apellidos.
// Los nodos se identifican por su posicion: no usamos new ni delete.
class ArbolBMas {
private:
    static constexpr size_t SIN_NODO = std::numeric_limits<size_t>::max();
    static constexpr size_t MAXIMO_CLAVES = 31;

    struct Nodo {
        bool esHoja;
        std::vector<std::string> claves;
        std::vector<uint32_t> codigosTexto;
        std::vector<size_t> hijos;
        size_t siguienteHoja = SIN_NODO;

        explicit Nodo(bool hoja);
    };

    struct ResultadoInsercion {
        bool agregado = false;
        bool dividido = false;
        std::string separador;
        size_t nodoDerecho = SIN_NODO;
    };

    // deque conserva las referencias cuando agregamos un nodo al final.
    std::deque<Nodo> nodos;
    size_t raiz = 0;
    size_t cantidadTerminos = 0;

    size_t crearNodo(bool esHoja);

    size_t posicionEnHoja(const Nodo& nodo, const std::string& clave) const;

    size_t elegirHijo(const Nodo& nodo, const std::string& clave) const;

    ResultadoInsercion dividirHoja(size_t numeroNodo);

    ResultadoInsercion dividirNodoInterno(size_t numeroNodo);

    ResultadoInsercion insertarEnNodo(size_t numeroNodo, const std::string& clave,
                                      uint32_t codigoTexto);

public:
    ArbolBMas();

    void insertar(const std::string& clave, uint32_t codigoTexto);

    std::vector<uint32_t> buscarPrefijo(const std::string& prefijo, size_t limite) const;

    size_t cantidad() const;
};
} // namespace eda
