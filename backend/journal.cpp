#include "journal.h"

namespace eda {

void escribirEntero32(std::string& salida, uint32_t valor) {
    for (unsigned desplazamiento = 0; desplazamiento < 32; desplazamiento += 8) {
        salida.push_back(static_cast<char>((valor >> desplazamiento) & 255));
    }
}

uint32_t leerEntero32(const std::string& entrada, size_t& posicion) {
    if (posicion + 4 > entrada.size()) {
        throw std::runtime_error("Archivo incompleto.");
    }
    uint32_t resultado = 0;
    for (unsigned desplazamiento = 0; desplazamiento < 32; desplazamiento += 8) {
        resultado |= static_cast<uint32_t>(static_cast<unsigned char>(entrada[posicion++]))
                     << desplazamiento;
    }
    return resultado;
}

std::string serializarCiudadano(const Ciudadano& ciudadano) {
    std::string salida;
    salida.reserve(56);
    for (uint32_t valor :
         {ciudadano.dni, ciudadano.nombres, ciudadano.apellidoPaterno, ciudadano.apellidoMaterno,
          ciudadano.nacionalidad, ciudadano.ubigeoNacimiento, ciudadano.ubigeoDireccion,
          ciudadano.ciudad, ciudadano.calle, ciudadano.numeroDireccion, ciudadano.telefono,
          ciudadano.correo, ciudadano.detalleDireccion}) {
        escribirEntero32(salida, valor);
    }
    salida.push_back(static_cast<char>(ciudadano.estado));
    salida.push_back(static_cast<char>(ciudadano.estadoCivil));
    salida.push_back(0);
    salida.push_back(0);
    return salida;
}

Ciudadano deserializarCiudadano(const std::string& entrada) {
    if (entrada.size() != 56) {
        throw std::runtime_error("Longitud de ciudadano invalida.");
    }
    size_t posicion = 0;
    Ciudadano ciudadano;
    ciudadano.dni = leerEntero32(entrada, posicion);
    ciudadano.nombres = leerEntero32(entrada, posicion);
    ciudadano.apellidoPaterno = leerEntero32(entrada, posicion);
    ciudadano.apellidoMaterno = leerEntero32(entrada, posicion);
    ciudadano.nacionalidad = leerEntero32(entrada, posicion);
    ciudadano.ubigeoNacimiento = leerEntero32(entrada, posicion);
    ciudadano.ubigeoDireccion = leerEntero32(entrada, posicion);
    ciudadano.ciudad = leerEntero32(entrada, posicion);
    ciudadano.calle = leerEntero32(entrada, posicion);
    ciudadano.numeroDireccion = leerEntero32(entrada, posicion);
    ciudadano.telefono = leerEntero32(entrada, posicion);
    ciudadano.correo = leerEntero32(entrada, posicion);
    ciudadano.detalleDireccion = leerEntero32(entrada, posicion);
    ciudadano.estado = static_cast<uint8_t>(entrada[posicion++]);
    ciudadano.estadoCivil = static_cast<uint8_t>(entrada[posicion++]);
    if (ciudadano.estado > 2 || ciudadano.estadoCivil > 4 || ciudadano.dni > 99999999) {
        throw std::runtime_error("Registro invalido.");
    }
    return ciudadano;
}

void escribirTextoEvento(std::string& salida, const std::string& texto) {
    escribirEntero32(salida, static_cast<uint32_t>(texto.size()));
    salida += texto;
}

std::string leerTextoEvento(const std::string& entrada, size_t& posicion) {
    uint32_t longitud = leerEntero32(entrada, posicion);
    if (longitud > 512 || posicion + longitud > entrada.size()) {
        throw std::runtime_error("Texto de actualizacion invalido.");
    }
    std::string texto = entrada.substr(posicion, longitud);
    posicion += longitud;
    return texto;
}

uint32_t ArchivoCiudadanos::calcularChecksum(uint32_t tipo, const std::string& datos) {
    uint32_t resultado = 2166136261U ^ tipo;
    for (unsigned char byte : datos) {
        resultado = resultado ^ byte;
        resultado = resultado * 16777619U;
    }
    return resultado;
}

ArchivoCiudadanos::ArchivoCiudadanos(const std::filesystem::path& carpeta)
    : bloqueo(carpeta), rutaArchivo(carpeta / "ciudadanos.edaj") {
}

uint64_t ArchivoCiudadanos::tamanoArchivo() const {
    if (!std::filesystem::exists(rutaArchivo)) {
        return 0;
    }
    return std::filesystem::file_size(rutaArchivo);
}

void ArchivoCiudadanos::abrirLectura() {
    if (!std::filesystem::exists(rutaArchivo)) {
        std::ofstream archivoNuevo(rutaArchivo, std::ios::binary);
        archivoNuevo.write("EDACIT01", 8);
        archivoNuevo.close();
        if (!archivoNuevo) {
            throw std::runtime_error("No se pudo crear el archivo de datos.");
        }
    }
    entradaArchivo.open(rutaArchivo, std::ios::binary);
    char firma[8] = {};
    entradaArchivo.read(firma, 8);
    if (entradaArchivo.gcount() != 8 || std::memcmp(firma, "EDACIT01", 8) != 0) {
        throw std::runtime_error("Formato de datos no reconocido.");
    }
}

bool ArchivoCiudadanos::leerEvento(uint32_t& tipo, std::string& datos) {
    char bytesCabecera[12] = {};
    entradaArchivo.read(bytesCabecera, 12);
    if (entradaArchivo.gcount() == 0 && entradaArchivo.eof()) {
        return false;
    }
    if (entradaArchivo.gcount() != 12) {
        throw std::runtime_error(
            "Evento incompleto al final del archivo. Conserve el archivo para recuperacion.");
    }

    std::string cabecera(bytesCabecera, 12);
    size_t posicion = 0;
    uint32_t longitud = leerEntero32(cabecera, posicion);
    tipo = leerEntero32(cabecera, posicion);
    uint32_t checksumEsperado = leerEntero32(cabecera, posicion);
    if (longitud > 1048576 || tipo < EVENTO_TEXTO || tipo > EVENTO_ACTUALIZACION) {
        throw std::runtime_error("Cabecera de evento invalida.");
    }

    datos.assign(longitud, '\0');
    entradaArchivo.read(datos.data(), longitud);
    if (entradaArchivo.gcount() != static_cast<std::streamsize>(longitud) ||
        calcularChecksum(tipo, datos) != checksumEsperado) {
        throw std::runtime_error("Evento truncado o danado: no se modifico el archivo.");
    }
    return true;
}

void ArchivoCiudadanos::abrirEscritura() {
    entradaArchivo.close();
    salidaArchivo.open(rutaArchivo, std::ios::binary | std::ios::app);
    if (!salidaArchivo) {
        throw std::runtime_error("No se puede abrir el archivo para escritura.");
    }
}

void ArchivoCiudadanos::agregarEvento(uint32_t tipo, const std::string& datos) {
    std::string cabecera;
    escribirEntero32(cabecera, static_cast<uint32_t>(datos.size()));
    escribirEntero32(cabecera, tipo);
    escribirEntero32(cabecera, calcularChecksum(tipo, datos));
    salidaArchivo.write(cabecera.data(), static_cast<std::streamsize>(cabecera.size()));
    salidaArchivo.write(datos.data(), static_cast<std::streamsize>(datos.size()));
    if (!salidaArchivo) {
        throw std::runtime_error("Fallo al escribir datos. Revise el espacio en disco.");
    }
}

void ArchivoCiudadanos::guardarCambios() {
    salidaArchivo.flush();
    if (!salidaArchivo) {
        throw std::runtime_error("No se pudieron guardar los cambios.");
    }
}

} // namespace eda
