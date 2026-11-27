#include "registry.h"
#include <iostream>

// Node envia una linea por solicitud. Los campos estan separados por tabulaciones.
// Los textos usan escapes como %20; esta parte solo conecta Node con C++.
int valorHexadecimal(char caracter) {
    if (caracter >= '0' && caracter <= '9') {
        return caracter - '0';
    }
    if (caracter >= 'a' && caracter <= 'f') {
        return caracter - 'a' + 10;
    }
    if (caracter >= 'A' && caracter <= 'F') {
        return caracter - 'A' + 10;
    }
    return -1;
}

std::string decodificarTexto(const std::string& entrada) {
    std::string salida;
    for (size_t i = 0; i < entrada.size(); i++) {
        if (entrada[i] != '%') {
            salida.push_back(entrada[i]);
            continue;
        }
        if (i + 2 >= entrada.size()) {
            throw std::runtime_error("Escape invalido.");
        }
        int primerDigito = valorHexadecimal(entrada[i + 1]);
        int segundoDigito = valorHexadecimal(entrada[i + 2]);
        if (primerDigito < 0 || segundoDigito < 0) {
            throw std::runtime_error("Escape invalido.");
        }
        salida.push_back(static_cast<char>(primerDigito * 16 + segundoDigito));
        i = i + 2;
    }
    return salida;
}

std::vector<std::string> separarCampos(const std::string& linea) {
    std::vector<std::string> campos;
    size_t inicio = 0;
    while (true) {
        size_t fin = linea.find('\t', inicio);
        if (fin == std::string::npos) {
            campos.push_back(decodificarTexto(linea.substr(inicio)));
            break;
        }
        campos.push_back(decodificarTexto(linea.substr(inicio, fin - inicio)));
        inicio = fin + 1;
    }
    return campos;
}

void validarCantidad(const std::vector<std::string>& argumentos, size_t cantidadEsperada) {
    if (argumentos.size() != cantidadEsperada) {
        throw std::runtime_error("Cantidad de argumentos invalida.");
    }
}

eda::DatosRegistro leerDatosRegistro(const std::vector<std::string>& argumentos) {
    eda::DatosRegistro datos;
    datos.nombres = argumentos[2];
    datos.apellidoPaterno = argumentos[3];
    datos.apellidoMaterno = argumentos[4];
    datos.nacionalidad = argumentos[5];
    datos.ubigeoNacimiento = argumentos[6];
    datos.ubigeoDireccion = argumentos[7];
    datos.ciudad = argumentos[8];
    datos.calle = argumentos[9];
    datos.numeroDireccion = static_cast<uint32_t>(eda::leerEntero(argumentos[10], 999999));
    datos.detalleDireccion = argumentos[11];
    datos.telefono = argumentos[12];
    datos.correo = argumentos[13];
    datos.estadoCivil = static_cast<uint8_t>(eda::leerEntero(argumentos[14], 4));
    return datos;
}

void informarProgreso(size_t cantidad) {
    std::cout << "{\"event\":\"progress\",\"count\":" << cantidad << "}\n";
    std::cout.flush();
}

// Esta funcion selecciona la operacion; los algoritmos estan en sus propias clases.
std::string ejecutarComando(eda::RegistroCiudadanos& registro,
                            const std::vector<std::string>& argumentos, bool& terminar) {
    const std::string& comando = argumentos[1];
    if (comando == "status") {
        validarCantidad(argumentos, 2);
        return registro.obtenerEstado();
    } else if (comando == "verify") {
        validarCantidad(argumentos, 2);
        return registro.verificarIndice();
    } else if (comando == "find") {
        validarCantidad(argumentos, 3);
        uint32_t dni = eda::convertirDni(argumentos[2]);
        const eda::Ciudadano* ciudadano = registro.buscarCiudadano(dni);
        if (ciudadano == nullptr) {
            return "null";
        }
        return registro.ciudadanoAJson(*ciudadano);
    } else if (comando == "search" || comando == "suggest-field") {
        validarCantidad(argumentos, 6);
        return registro.buscarPorNombre(eda::leerEntero(argumentos[2], 2), argumentos[3],
            eda::leerEntero(argumentos[4], 100000000), eda::leerEntero(argumentos[5], 50),
            comando == "suggest-field");
    } else if (comando == "list") {
        validarCantidad(argumentos, 8);
        return registro.listarPersonas(argumentos[2], {argumentos[3], argumentos[4], argumentos[5]},
            eda::leerEntero(argumentos[6], 100000000), eda::leerEntero(argumentos[7], 50));
    } else if (comando == "sample") {
        validarCantidad(argumentos, 3);
        size_t limite = static_cast<size_t>(eda::leerEntero(argumentos[2], 100));
        return registro.obtenerMuestra(limite);
    } else if (comando == "suggest") {
        validarCantidad(argumentos, 4);
        size_t limite = static_cast<size_t>(eda::leerEntero(argumentos[3], 50));
        return registro.sugerirNombres(argumentos[2], limite);
    } else if (comando == "death") {
        validarCantidad(argumentos, 3);
        uint32_t dni = eda::convertirDni(argumentos[2]);
        registro.registrarFallecimiento(dni);
        return registro.ciudadanoAJson(*registro.buscarCiudadano(dni));
    } else if (comando == "register") {
        validarCantidad(argumentos, 16);
        if (!argumentos[15].empty()) {
            throw std::runtime_error("El DNI lo genera el sistema.");
        }
        eda::DatosRegistro datos = leerDatosRegistro(argumentos);
        return registro.registrarCiudadano(datos);
    } else if (comando == "update") {
        validarCantidad(argumentos, 18);
        eda::DatosRegistro datos = leerDatosRegistro(argumentos);
        return registro.actualizarPersona(eda::convertirDni(argumentos[15]), datos,
                                          argumentos[16], argumentos[17]);
    } else if (comando == "history") {
        validarCantidad(argumentos, 3);
        return registro.obtenerHistorial(eda::convertirDni(argumentos[2]));
    } else if (comando == "generate") {
        validarCantidad(argumentos, 4);
        uint32_t objetivo = static_cast<uint32_t>(eda::leerEntero(argumentos[2], 100000000));
        uint64_t semilla = eda::leerEntero(argumentos[3], UINT64_MAX);
        return registro.generarPoblacion(objetivo, semilla, informarProgreso);
    } else if (comando == "benchmark") {
        validarCantidad(argumentos, 3);
        uint32_t muestras = static_cast<uint32_t>(eda::leerEntero(argumentos[2], 100000));
        return registro.medirRendimiento(muestras);
    } else if (comando == "flush" || comando == "quit") {
        validarCantidad(argumentos, 2);
        registro.guardarCambios();
        terminar = comando == "quit";
        return "true";
    }
    throw std::runtime_error("Comando o argumentos invalidos.");
}

int main(int argc, char** argv) {
    try {
        std::filesystem::path carpetaDatos = "data";
        if (argc > 1) {
            carpetaDatos = std::filesystem::u8path(argv[1]);
        }
        eda::RegistroCiudadanos registro(carpetaDatos);
        std::cout << "{\"event\":\"ready\",\"status\":" << registro.obtenerEstado() << "}\n";
        std::cout.flush();

        std::string linea;
        bool terminar = false;
        while (!terminar && std::getline(std::cin, linea)) {
            std::string id = "0";
            try {
                if (linea.size() > 32768) {
                    throw std::runtime_error("Solicitud demasiado grande.");
                }
                std::vector<std::string> argumentos = separarCampos(linea);
                if (argumentos.size() < 2) {
                    throw std::runtime_error("Comando incompleto.");
                }
                // Validamos antes de usar el identificador dentro de la respuesta JSON.
                eda::leerEntero(argumentos[0], UINT32_MAX);
                id = argumentos[0];
                std::string resultado = ejecutarComando(registro, argumentos, terminar);
                std::cout << "{\"id\":" << id << ",\"ok\":true,\"result\":" << resultado << "}\n";
            } catch (const std::exception& error) {
                std::cout << "{\"id\":" << id
                          << ",\"ok\":false,\"error\":" << eda::escaparJson(error.what()) << "}\n";
            }
            std::cout.flush();
        }
        registro.guardarCambios();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error de inicio: " << error.what() << '\n';
        return 1;
    }
}
