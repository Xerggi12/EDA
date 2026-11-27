#include "bplus_tree.h"

namespace eda {

ArbolBMas::Nodo::Nodo(bool hoja) : esHoja(hoja) {
}

size_t ArbolBMas::crearNodo(bool esHoja) {
    nodos.push_back(Nodo(esHoja));
    return nodos.size() - 1;
}

size_t ArbolBMas::posicionEnHoja(const Nodo& nodo, const std::string& clave) const {
    size_t posicion = 0;
    while (posicion < nodo.claves.size() && nodo.claves[posicion] < clave) {
        posicion++;
    }
    return posicion;
}

size_t ArbolBMas::elegirHijo(const Nodo& nodo, const std::string& clave) const {
    size_t posicion = 0;
    while (posicion < nodo.claves.size() && clave >= nodo.claves[posicion]) {
        posicion++;
    }
    return posicion;
}

ArbolBMas::ResultadoInsercion ArbolBMas::dividirHoja(size_t numeroNodo) {
    Nodo& izquierda = nodos[numeroNodo];
    size_t numeroDerecho = crearNodo(true);
    Nodo& derecha = nodos[numeroDerecho];
    size_t mitad = izquierda.claves.size() / 2;

    for (size_t i = mitad; i < izquierda.claves.size(); i++) {
        derecha.claves.push_back(izquierda.claves[i]);
        derecha.codigosTexto.push_back(izquierda.codigosTexto[i]);
    }
    izquierda.claves.resize(mitad);
    izquierda.codigosTexto.resize(mitad);

    // Enlazamos las hojas para continuar una busqueda por prefijo.
    derecha.siguienteHoja = izquierda.siguienteHoja;
    izquierda.siguienteHoja = numeroDerecho;

    ResultadoInsercion resultado;
    resultado.agregado = true;
    resultado.dividido = true;
    resultado.separador = derecha.claves[0];
    resultado.nodoDerecho = numeroDerecho;
    return resultado;
}

ArbolBMas::ResultadoInsercion ArbolBMas::dividirNodoInterno(size_t numeroNodo) {
    Nodo& izquierda = nodos[numeroNodo];
    size_t numeroDerecho = crearNodo(false);
    Nodo& derecha = nodos[numeroDerecho];
    size_t mitad = izquierda.claves.size() / 2;

    ResultadoInsercion resultado;
    resultado.agregado = true;
    resultado.dividido = true;
    resultado.separador = izquierda.claves[mitad];
    resultado.nodoDerecho = numeroDerecho;

    // La clave central sube al padre; las restantes se reparten.
    for (size_t i = mitad + 1; i < izquierda.claves.size(); i++) {
        derecha.claves.push_back(izquierda.claves[i]);
    }
    for (size_t i = mitad + 1; i < izquierda.hijos.size(); i++) {
        derecha.hijos.push_back(izquierda.hijos[i]);
    }
    izquierda.claves.resize(mitad);
    izquierda.hijos.resize(mitad + 1);
    return resultado;
}

ArbolBMas::ResultadoInsercion ArbolBMas::insertarEnNodo(size_t numeroNodo, const std::string& clave,
                                  uint32_t codigoTexto) {
    Nodo& nodo = nodos[numeroNodo];

    if (nodo.esHoja) {
        size_t posicion = posicionEnHoja(nodo, clave);
        if (posicion < nodo.claves.size() && nodo.claves[posicion] == clave) {
            return ResultadoInsercion();
        }

        nodo.claves.insert(nodo.claves.begin() + posicion, clave);
        nodo.codigosTexto.insert(nodo.codigosTexto.begin() + posicion, codigoTexto);
        if (nodo.claves.size() > MAXIMO_CLAVES) {
            return dividirHoja(numeroNodo);
        }

        ResultadoInsercion resultado;
        resultado.agregado = true;
        return resultado;
    }

    size_t posicionHijo = elegirHijo(nodo, clave);
    ResultadoInsercion resultado = insertarEnNodo(nodo.hijos[posicionHijo], clave, codigoTexto);
    if (!resultado.dividido) {
        return resultado;
    }

    nodo.claves.insert(nodo.claves.begin() + posicionHijo, resultado.separador);
    nodo.hijos.insert(nodo.hijos.begin() + posicionHijo + 1, resultado.nodoDerecho);
    if (nodo.claves.size() > MAXIMO_CLAVES) {
        return dividirNodoInterno(numeroNodo);
    }
    resultado.dividido = false;
    return resultado;
}

ArbolBMas::ArbolBMas() {
    crearNodo(true);
}

void ArbolBMas::insertar(const std::string& clave, uint32_t codigoTexto) {
    ResultadoInsercion resultado = insertarEnNodo(raiz, clave, codigoTexto);

    if (resultado.dividido) {
        size_t nuevaRaiz = crearNodo(false);
        nodos[nuevaRaiz].claves.push_back(resultado.separador);
        nodos[nuevaRaiz].hijos.push_back(raiz);
        nodos[nuevaRaiz].hijos.push_back(resultado.nodoDerecho);
        raiz = nuevaRaiz;
    }
    if (resultado.agregado) {
        cantidadTerminos++;
    }
}

std::vector<uint32_t> ArbolBMas::buscarPrefijo(const std::string& prefijo, size_t limite) const {
    std::vector<uint32_t> encontrados;
    size_t numeroHoja = raiz;

    // Primero bajamos desde la raiz hasta la hoja correspondiente.
    while (!nodos[numeroHoja].esHoja) {
        const Nodo& nodo = nodos[numeroHoja];
        size_t hijo = elegirHijo(nodo, prefijo);
        numeroHoja = nodo.hijos[hijo];
    }

    size_t posicion = posicionEnHoja(nodos[numeroHoja], prefijo);
    while (numeroHoja != SIN_NODO && encontrados.size() < limite) {
        const Nodo& hoja = nodos[numeroHoja];
        while (posicion < hoja.claves.size() && encontrados.size() < limite) {
            if (hoja.claves[posicion].compare(0, prefijo.size(), prefijo) != 0) {
                return encontrados;
            }
            encontrados.push_back(hoja.codigosTexto[posicion]);
            posicion++;
        }
        numeroHoja = hoja.siguienteHoja;
        posicion = 0;
    }
    return encontrados;
}

size_t ArbolBMas::cantidad() const {
    return cantidadTerminos;
}

} // namespace eda
