#include "utilidades.h"

namespace eda {

std::string escaparJson(const std::string& valor) {
    std::ostringstream salida;
    salida << '"';
    for (unsigned char c : valor) {
        switch (c) {
        case '"':
            salida << "\\\"";
            break;
        case '\\':
            salida << "\\\\";
            break;
        case '\n':
            salida << "\\n";
            break;
        case '\r':
            salida << "\\r";
            break;
        case '\t':
            salida << "\\t";
            break;
        default:
            if (c < 32) {
                salida << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned>(c) << std::dec;
            } else {
                salida << static_cast<char>(c);
            }
        }
    }
    salida << '"';
    return salida.str();
}

uint64_t leerEntero(const std::string& texto, uint64_t maximo) {
    if (texto.empty()) {
        throw std::runtime_error("Se esperaba un entero no negativo.");
    }
    uint64_t valor = 0;
    for (char c : texto) {
        if (c < '0' || c > '9') {
            throw std::runtime_error("Numero invalido.");
        }
        uint64_t digito = static_cast<uint64_t>(c - '0');
        if (digito > maximo || valor > (maximo - digito) / 10) {
            throw std::runtime_error("Numero fuera de rango.");
        }
        valor = valor * 10 + digito;
    }
    return valor;
}

uint32_t convertirDni(const std::string& texto) {
    if (texto.size() != 8) {
        throw std::runtime_error("El DNI debe contener exactamente ocho digitos.");
    }
    return static_cast<uint32_t>(leerEntero(texto, 99999999));
}

std::string formatearDni(uint32_t dni) {
    std::ostringstream salida;
    salida << std::setw(8) << std::setfill('0') << dni;
    return salida.str();
}

std::string normalizarTexto(std::string texto) {
    // Convertimos a minusculas y quitamos tildes para comparar los nombres.
    // Las letras con tilde ocupan dos bytes en UTF-8.
    std::string resultado;
    for (size_t i = 0; i < texto.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(texto[i]);
        if (c >= 'A' && c <= 'Z') {
            resultado.push_back(static_cast<char>(c + 32));
        } else if (c == 0xc3 && i + 1 < texto.size()) {
            unsigned char siguiente = static_cast<unsigned char>(texto[++i]);
            switch (siguiente) {
            case 0x81:
            case 0xa1:
                resultado.push_back('a');
                break;
            case 0x89:
            case 0xa9:
                resultado.push_back('e');
                break;
            case 0x8d:
            case 0xad:
                resultado.push_back('i');
                break;
            case 0x93:
            case 0xb3:
                resultado.push_back('o');
                break;
            case 0x9a:
            case 0xba:
            case 0x9c:
            case 0xbc:
                resultado.push_back('u');
                break;
            case 0x91:
            case 0xb1:
                resultado += "\xc3\xb1";
                break;
            default:
                resultado.push_back(static_cast<char>(c));
                resultado.push_back(static_cast<char>(siguiente));
            }
        } else {
            resultado.push_back(static_cast<char>(c));
        }
    }
    return resultado;
}

} // namespace eda
