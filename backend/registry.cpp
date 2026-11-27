#include "registry.h"

namespace eda {

double tiempoTranscurridoNs(Reloj::time_point inicio) {
    return std::chrono::duration<double, std::nano>(Reloj::now() - inicio).count();
}

void RegistroCiudadanos::validarTextoObligatorio(const std::string& valor) const {
    if (valor.empty() || valor.size() > 512) {
        throw std::runtime_error("Faltan datos obligatorios o un texto supera 512 bytes.");
    }
}

void RegistroCiudadanos::contarEstado(uint8_t estado, bool agregar) {
    if (estado == 1) {
        if (agregar) {
            cantidadActivos++;
        } else {
            cantidadActivos--;
        }
    } else if (estado == 2) {
        if (agregar) {
            cantidadFallecidos++;
        } else {
            cantidadFallecidos--;
        }
    }
}

const std::string& RegistroCiudadanos::texto(uint32_t id) const {
    return textos.at(id);
}

void RegistroCiudadanos::indexarNombre(uint32_t id) {
    if (!nombreIndexado.at(id) && !texto(id).empty()) {
        std::string clave = normalizarTexto(texto(id));
        auto grupo = grupoPorNombre.emplace(clave, static_cast<uint32_t>(gruposNombre.size()));
        if (grupo.second) {
            gruposNombre.emplace_back();
            indiceNombres.insertar(clave, id);
        }
        if (grupoPorTexto.size() <= id) grupoPorTexto.resize(id + 1);
        grupoPorTexto[id] = grupo.first->second;
        nombreIndexado[id] = 1;
    }
}

std::array<uint32_t, 3> RegistroCiudadanos::camposNombre(const Ciudadano& persona) {
    return {persona.nombres, persona.apellidoPaterno, persona.apellidoMaterno};
}

void RegistroCiudadanos::indexarPersona(const Ciudadano& persona, uint32_t posicion, const Ciudadano* anterior) {
    const auto nuevos = camposNombre(persona);
    for (size_t campo = 0; campo < nuevos.size(); ++campo) {
        indexarNombre(nuevos[campo]);
        auto& lista = gruposNombre[grupoPorTexto[nuevos[campo]]].listas[campo];
        if (!anterior) {
            lista.push_back(posicion);
            continue;
        }
        uint32_t viejo = camposNombre(*anterior)[campo];
        if (grupoPorTexto[viejo] == grupoPorTexto[nuevos[campo]]) continue;
        auto& previa = gruposNombre[grupoPorTexto[viejo]].listas[campo];
        previa.erase(std::lower_bound(previa.begin(), previa.end(), posicion));
        lista.insert(std::lower_bound(lista.begin(), lista.end(), posicion), posicion);
    }
}

uint32_t RegistroCiudadanos::guardarTexto(const std::string& valor, bool esNombre) {
    auto encontrado = codigosTexto.find(valor);
    uint32_t id;
    if (encontrado != codigosTexto.end()) {
        id = encontrado->second;
    } else {
        if (valor.size() > 512) {
            throw std::runtime_error("Texto demasiado largo (maximo 512 bytes UTF-8).");
        }
        if (textos.size() >= UINT32_MAX) {
            throw std::runtime_error("Catalogo agotado.");
        }
        id = static_cast<uint32_t>(textos.size());
        archivo.agregarEvento(EVENTO_TEXTO, valor);
        textos.push_back(valor);
        nombreIndexado.push_back(0);
        codigosTexto.emplace(valor, id);
    }
    if (esNombre) {
        indexarNombre(id);
    }
    return id;
}

uint32_t RegistroCiudadanos::aleatorioMenorQue(uint32_t limite) {
    std::uniform_int_distribution<uint32_t> distribucion(0, limite - 1);
    return distribucion(generador);
}

uint32_t RegistroCiudadanos::generarDniUnico() {
    if (ciudadanos.cantidad() >= MAXIMO_CIUDADANOS) {
        throw std::runtime_error("Se agoto el espacio de DNI de ocho digitos.");
    }
    uint32_t posicion = 0;
    for (size_t intento = 0; intento < 10000; ++intento) {
        const uint32_t candidato = aleatorioMenorQue(MAXIMO_CIUDADANOS);
        if (!indiceDni.buscar(candidato, posicion)) {
            return candidato;
        }
    }
    // Si casi todos los DNI estan usados, buscamos uno libre desde un punto aleatorio.
    const uint32_t inicio = aleatorioMenorQue(MAXIMO_CIUDADANOS);
    for (uint32_t desplazamiento = 0; desplazamiento < MAXIMO_CIUDADANOS; ++desplazamiento) {
        const uint32_t candidato = (inicio + desplazamiento) % MAXIMO_CIUDADANOS;
        if (!indiceDni.buscar(candidato, posicion)) {
            return candidato;
        }
    }
    throw std::runtime_error("No hay DNI disponibles.");
}

void RegistroCiudadanos::validarCodigosTexto(const Ciudadano& ciudadano) const {
    for (uint32_t id : {ciudadano.nombres, ciudadano.apellidoPaterno, ciudadano.apellidoMaterno,
                        ciudadano.nacionalidad, ciudadano.ciudad, ciudadano.calle,
                        ciudadano.detalleDireccion}) {
        if (id >= textos.size()) {
            throw std::runtime_error("Referencia a catalogo invalida.");
        }
    }
    if (ciudadano.correo != UINT32_MAX && ciudadano.correo >= textos.size()) {
        throw std::runtime_error("Referencia de correo invalida.");
    }
}

void RegistroCiudadanos::aplicarEvento(uint32_t tipo, const std::string& datos) {
    if (tipo == EVENTO_TEXTO) {
        if (datos.size() > 512) {
            throw std::runtime_error("Texto invalido en archivo.");
        }
        const uint32_t id = static_cast<uint32_t>(textos.size());
        if (!codigosTexto.emplace(datos, id).second) {
            throw std::runtime_error("Catalogo duplicado en archivo.");
        }
        textos.push_back(datos);
        nombreIndexado.push_back(0);
    } else if (tipo == EVENTO_ALTA) {
        Ciudadano ciudadano = deserializarCiudadano(datos);
        validarCodigosTexto(ciudadano);
        if (ciudadanos.cantidad() >= MAXIMO_CIUDADANOS ||
            !indiceDni.insertar(ciudadano.dni, static_cast<uint32_t>(ciudadanos.cantidad()))) {
            throw std::runtime_error("DNI duplicado o capacidad invalida en archivo.");
        }
        ciudadanos.agregar(ciudadano);
        contarEstado(ciudadano.estado, true);
        indexarPersona(ciudadano, static_cast<uint32_t>(ciudadanos.cantidad() - 1));
    } else if (tipo == EVENTO_ACTUALIZACION) {
        if (datos.size() < 56) {
            throw std::runtime_error("Actualizacion incompleta.");
        }
        Ciudadano nuevo = deserializarCiudadano(datos.substr(0, 56));
        validarCodigosTexto(nuevo);
        uint32_t posicion = 0;
        if (!indiceDni.buscar(nuevo.dni, posicion)) {
            throw std::runtime_error("Actualizacion de un DNI inexistente.");
        }
        Ciudadano& anterior = ciudadanos.obtener(posicion);
        if (anterior.estado != nuevo.estado) {
            throw std::runtime_error("La edicion no puede cambiar el estado vital.");
        }
        size_t lectura = 56;
        std::string fecha = leerTextoEvento(datos, lectura);
        std::string motivo = leerTextoEvento(datos, lectura);
        std::string referencia = leerTextoEvento(datos, lectura);
        if (lectura != datos.size() || fecha.empty() || motivo.empty()) {
            throw std::runtime_error("Metadatos de actualizacion invalidos.");
        }
        historial[nuevo.dni].push_back({anterior, nuevo, fecha, motivo, referencia});
        indexarPersona(nuevo, posicion, &anterior);
        anterior = nuevo;
    } else {
        if (datos.size() != 5) {
            throw std::runtime_error("Evento de estado invalido.");
        }
        size_t posicionLectura = 0;
        const uint32_t dni = leerEntero32(datos, posicionLectura);
        uint8_t estado = static_cast<uint8_t>(datos[posicionLectura]);
        uint32_t posicion = 0;
        if (estado > 2 || !indiceDni.buscar(dni, posicion)) {
            throw std::runtime_error("Estado o DNI inexistente en archivo.");
        }
        Ciudadano& ciudadano = ciudadanos.obtener(posicion);
        if (ciudadano.estado != estado) {
            contarEstado(ciudadano.estado, false);
            ciudadano.estado = estado;
            contarEstado(ciudadano.estado, true);
        }
    }
}

void RegistroCiudadanos::prepararDatosSimulados() {
    if (!nombresSimulados.empty()) {
        return;
    }
    codigoTextoVacio = guardarTexto("");
    codigoPeruana = guardarTexto("Peruana");
    for (const char* valor : nombres) {
        nombresSimulados.push_back(guardarTexto(valor, true));
    }
    for (const char* valor : apellidos) {
        apellidosSimulados.push_back(guardarTexto(valor, true));
    }
    for (const char* valor : {"Lima", "Lima", "Arequipa", "Cusco", "Trujillo"}) {
        ciudadesSimuladas.push_back(guardarTexto(valor));
    }
    for (const char* valor : {"Av. Los Alamos", "Calle Las Flores", "Jr. Los Olivos",
                              "Av. El Sol", "Calle Los Pinos"}) {
        callesSimuladas.push_back(guardarTexto(valor));
    }
}

Ciudadano RegistroCiudadanos::crearCiudadanoSimulado() {
    Ciudadano ciudadano;
    ciudadano.dni = generarDniUnico();
    ciudadano.nombres =
        nombresSimulados[aleatorioMenorQue(static_cast<uint32_t>(nombresSimulados.size()))];
    ciudadano.apellidoPaterno =
        apellidosSimulados[aleatorioMenorQue(static_cast<uint32_t>(apellidosSimulados.size()))];
    ciudadano.apellidoMaterno =
        apellidosSimulados[aleatorioMenorQue(static_cast<uint32_t>(apellidosSimulados.size()))];
    ciudadano.nacionalidad = codigoPeruana;
    ciudadano.ubigeoNacimiento =
        ubigeos[aleatorioMenorQue(static_cast<uint32_t>(ubigeos.size()))];
    const uint32_t direccion = aleatorioMenorQue(static_cast<uint32_t>(ubigeos.size()));
    ciudadano.ubigeoDireccion = ubigeos[direccion];
    ciudadano.ciudad = ciudadesSimuladas[direccion];
    ciudadano.calle =
        callesSimuladas[aleatorioMenorQue(static_cast<uint32_t>(callesSimuladas.size()))];
    ciudadano.numeroDireccion = 1 + aleatorioMenorQue(9999);
    ciudadano.telefono = 900000000 + aleatorioMenorQue(100000000);
    ciudadano.detalleDireccion = codigoTextoVacio;
    ciudadano.estadoCivil = static_cast<uint8_t>(aleatorioMenorQue(5));
    return ciudadano;
}

void RegistroCiudadanos::agregarCiudadano(const Ciudadano& ciudadano) {
    ciudadanos.prepararEspacio();
    indiceDni.reservar(ciudadanos.cantidad() + 1);
    archivo.agregarEvento(EVENTO_ALTA, serializarCiudadano(ciudadano));
    if (!indiceDni.insertar(ciudadano.dni, static_cast<uint32_t>(ciudadanos.cantidad()))) {
        throw std::runtime_error("DNI duplicado.");
    }
    ciudadanos.agregar(ciudadano);
    indexarPersona(ciudadano, static_cast<uint32_t>(ciudadanos.cantidad() - 1));
    contarEstado(ciudadano.estado, true);
}

std::string RegistroCiudadanos::listarPersonas(const std::string& dni, const std::array<std::string, 3>& filtros,
                           size_t inicio, size_t limite) const {
    std::array<std::vector<bool>, 3> aceptados;
    std::array<std::vector<uint32_t>, 3> terminos;
    size_t campoBase = 3, menorCantidad = ciudadanos.cantidad() + 1;
    for (size_t campo = 0; campo < 3; ++campo) {
        if (filtros[campo].empty()) continue;
        aceptados[campo].resize(gruposNombre.size());
        terminos[campo] = indiceNombres.buscarPrefijo(normalizarTexto(filtros[campo]), indiceNombres.cantidad());
        size_t cantidad = 0;
        for (uint32_t id : terminos[campo]) {
            uint32_t grupo = grupoPorTexto[id];
            aceptados[campo][grupo] = true;
            cantidad += gruposNombre[grupo].listas[campo].size();
        }
        if (cantidad < menorCantidad) { campoBase = campo; menorCantidad = cantidad; }
    }
    size_t total = 0, emitidos = 0;
    std::string items = "[";
    auto agregar = [&](const Ciudadano& persona) {
        if (emitidos++) items += ',';
        items += ciudadanoAJson(persona);
    };
    auto evaluar = [&](const Ciudadano& persona) {
        const auto nombres = camposNombre(persona);
        for (size_t campo = 0; campo < 3; ++campo) {
            if (!filtros[campo].empty() && !aceptados[campo][grupoPorTexto[nombres[campo]]]) return;
        }
        if (total++ >= inicio && emitidos < limite) agregar(persona);
    };
    if (!dni.empty()) {
        const Ciudadano* persona = buscarCiudadano(convertirDni(dni));
        if (persona) evaluar(*persona);
    } else if (campoBase == 3) {
        total = ciudadanos.cantidad();
        for (size_t i = inicio; i < total && emitidos < limite; ++i) agregar(ciudadanos.obtener(i));
    } else if (std::count(filtros.begin(), filtros.end(), std::string()) == 2) {
        return buscarPorNombre(campoBase, filtros[campoBase], inicio, limite, false);
    } else {
        // Para cruzar criterios, visitar solo las posiciones del campo menos frecuente.
        for (uint32_t id : terminos[campoBase])
            for (uint32_t posicion : gruposNombre[grupoPorTexto[id]].listas[campoBase])
                evaluar(ciudadanos.obtener(posicion));
    }
    return "{\"total\":" + std::to_string(total) + ",\"items\":" + items + "]}";
}

std::string RegistroCiudadanos::buscarPorNombre(size_t campo, const std::string& prefijo, size_t inicio,
                            size_t limite, bool sugerencias) const {
    if (campo >= 3 || prefijo.empty()) throw std::runtime_error("Criterio de busqueda invalido.");
    auto terminos = indiceNombres.buscarPrefijo(normalizarTexto(prefijo), indiceNombres.cantidad());
    std::string items = "[";
    size_t emitidos = 0, total = 0;
    for (uint32_t id : terminos) {
        const auto& lista = gruposNombre[grupoPorTexto[id]].listas[campo];
        if (lista.empty()) continue;
        if (sugerencias) {
            if (emitidos++) items += ',';
            auto campos = camposNombre(ciudadanos.obtener(lista.front()));
            items += escaparJson(texto(campos[campo]));
            if (emitidos == limite) break;
        } else {
            size_t desde = inicio > total ? inicio - total : 0;
            for (size_t i = desde; i < lista.size() && emitidos < limite; ++i) {
                if (emitidos++) items += ',';
                items += ciudadanoAJson(ciudadanos.obtener(lista[i]));
            }
            total += lista.size();
        }
    }
    items += ']';
    if (sugerencias) return items;
    return "{\"total\":" + std::to_string(total) + ",\"items\":" + items + "}";
}

RegistroCiudadanos::RegistroCiudadanos(const std::filesystem::path& carpeta) : archivo(carpeta) {
    // Estimamos la capacidad del indice antes de cargar millones de registros.
    indiceDni.reservar(static_cast<size_t>(
        std::min<uint64_t>(archivo.tamanoArchivo() / 68, MAXIMO_CIUDADANOS)));
    archivo.abrirLectura();
    uint32_t tipo = 0;
    std::string datos;
    while (archivo.leerEvento(tipo, datos)) {
        aplicarEvento(tipo, datos);
    }
    archivo.abrirEscritura();
}

void RegistroCiudadanos::guardarCambios() {
    archivo.guardarCambios();
}

std::string RegistroCiudadanos::obtenerEstado() const {
    uint64_t memoriaTotal = 0, memoriaDisponible = 0;
#ifdef _WIN32
    MEMORYSTATUSEX memoria{};
    memoria.dwLength = sizeof(memoria);
    if (GlobalMemoryStatusEx(&memoria)) {
        memoriaTotal = memoria.ullTotalPhys;
        memoriaDisponible = memoria.ullAvailPhys;
    }
#endif
    size_t bytesTexto = 0;
    for (const std::string& valor : textos) {
        bytesTexto += sizeof(std::string) + valor.capacity();
    }
    std::ostringstream salida;
    size_t bytesPosiciones = 0;
    for (const auto& grupo : gruposNombre)
        for (const auto& lista : grupo.listas) bytesPosiciones += lista.capacity() * sizeof(uint32_t);
    salida << "{\"total\":" << ciudadanos.cantidad() << ",\"active\":" << cantidadActivos
           << ",\"inactive\":" << ciudadanos.cantidad() - cantidadActivos
           << ",\"living\":" << cantidadActivos << ",\"deceased\":" << cantidadFallecidos
           << ",\"unclassified\":"
           << ciudadanos.cantidad() - cantidadActivos - cantidadFallecidos
           << ",\"recordBytes\":" << sizeof(Ciudadano)
           << ",\"recordCapacityBytes\":" << ciudadanos.memoriaBytes()
           << ",\"indexBytes\":" << indiceDni.memoriaBytes() + bytesPosiciones
           << ",\"catalogTextBytesEstimate\":" << bytesTexto
           << ",\"catalogEntries\":" << textos.size()
           << ",\"nameTerms\":" << indiceNombres.cantidad()
           << ",\"fileBytes\":" << archivo.tamanoArchivo()
           << ",\"systemMemoryBytes\":" << memoriaTotal
           << ",\"availableMemoryBytes\":" << memoriaDisponible << '}';
    return salida.str();
}

const Ciudadano* RegistroCiudadanos::buscarCiudadano(uint32_t dni) const {
    uint32_t posicion = 0;
    if (indiceDni.buscar(dni, posicion)) {
        return &ciudadanos.obtener(posicion);
    }
    return nullptr;
}

std::string RegistroCiudadanos::ciudadanoAJson(const Ciudadano& ciudadano) const {
    static const std::array<const char*, 5> civil{"soltero", "casado", "divorciado", "viudo",
                                                  "conviviente"};
    std::ostringstream nacimiento, direccion;
    nacimiento << std::setw(6) << std::setfill('0') << ciudadano.ubigeoNacimiento;
    direccion << std::setw(6) << std::setfill('0') << ciudadano.ubigeoDireccion;
    std::string correo;
    if (ciudadano.correo == UINT32_MAX) {
        correo = "ciudadano" + formatearDni(ciudadano.dni) + "@example.test";
    } else {
        correo = texto(ciudadano.correo);
    }
    std::string estado = "sin_clasificar";
    if (ciudadano.estado == 1) {
        estado = "vivo";
    } else if (ciudadano.estado == 2) {
        estado = "fallecido";
    }
    std::ostringstream salida;
    salida << "{\"dni\":" << escaparJson(formatearDni(ciudadano.dni))
           << ",\"firstNames\":" << escaparJson(texto(ciudadano.nombres))
           << ",\"paternalSurname\":" << escaparJson(texto(ciudadano.apellidoPaterno))
           << ",\"maternalSurname\":" << escaparJson(texto(ciudadano.apellidoMaterno))
           << ",\"nationality\":" << escaparJson(texto(ciudadano.nacionalidad))
           << ",\"birthUbigeo\":" << escaparJson(nacimiento.str())
           << ",\"address\":{\"ubigeo\":" << escaparJson(direccion.str())
           << ",\"city\":" << escaparJson(texto(ciudadano.ciudad))
           << ",\"street\":" << escaparJson(texto(ciudadano.calle))
           << ",\"number\":" << ciudadano.numeroDireccion
           << ",\"detail\":" << escaparJson(texto(ciudadano.detalleDireccion))
           << "},\"phone\":" << escaparJson(std::to_string(ciudadano.telefono))
           << ",\"email\":" << escaparJson(correo)
           << ",\"civilStatus\":" << escaparJson(civil[ciudadano.estadoCivil])
           << ",\"state\":" << escaparJson(estado) << '}';
    return salida.str();
}

std::string RegistroCiudadanos::obtenerMuestra(size_t limite) const {
    std::string salida = "[";
    for (size_t i = 0; i < std::min(limite, ciudadanos.cantidad()); ++i) {
        if (i) {
            salida += ',';
        }
        salida += ciudadanoAJson(ciudadanos.obtener(i));
    }
    return salida + "]";
}

std::string RegistroCiudadanos::sugerirNombres(const std::string& consulta, size_t limite) const {
    if (consulta.empty()) {
        return "[]";
    }
    std::vector<uint32_t> codigos =
        indiceNombres.buscarPrefijo(normalizarTexto(consulta), limite);
    std::string salida = "[";
    for (size_t i = 0; i < codigos.size(); ++i) {
        if (i) {
            salida += ',';
        }
        salida += escaparJson(texto(codigos[i]));
    }
    return salida + "]";
}

void RegistroCiudadanos::registrarFallecimiento(uint32_t dni) {
    uint32_t posicion = 0;
    if (!indiceDni.buscar(dni, posicion)) {
        throw std::runtime_error("La persona no esta registrada.");
    }
    Ciudadano& persona = ciudadanos.obtener(posicion);
    if (persona.estado == 2) {
        return;
    }
    if (persona.estado != 1) {
        throw std::runtime_error("El registro no tiene un estado vital clasificado.");
    }
    std::string evento;
    escribirEntero32(evento, dni);
    evento.push_back(2);
    archivo.agregarEvento(EVENTO_ESTADO, evento);
    contarEstado(persona.estado, false);
    persona.estado = 2;
    contarEstado(persona.estado, true);
    archivo.guardarCambios();
}

Ciudadano RegistroCiudadanos::crearDesdeDatos(const DatosRegistro& datos, uint32_t dni) {
    validarTextoObligatorio(datos.nombres);
    validarTextoObligatorio(datos.apellidoPaterno);
    validarTextoObligatorio(datos.apellidoMaterno);
    validarTextoObligatorio(datos.nacionalidad);
    validarTextoObligatorio(datos.ciudad);
    validarTextoObligatorio(datos.calle);
    validarTextoObligatorio(datos.correo);

    if (datos.ubigeoNacimiento.size() != 6 || datos.ubigeoDireccion.size() != 6) {
        throw std::runtime_error("El ubigeo debe tener seis digitos.");
    }
    if (datos.telefono.size() != 9) {
        throw std::runtime_error("El telefono debe contener nueve digitos.");
    }
    if (datos.correo.find('@') == std::string::npos ||
        datos.correo.find(' ') != std::string::npos) {
        throw std::runtime_error("Correo invalido.");
    }
    if (datos.estadoCivil > 4 || datos.numeroDireccion > 999999) {
        throw std::runtime_error("Estado civil o numero de direccion invalido.");
    }

    Ciudadano ciudadano;
    ciudadano.ubigeoNacimiento =
        static_cast<uint32_t>(leerEntero(datos.ubigeoNacimiento, 999999));
    ciudadano.ubigeoDireccion =
        static_cast<uint32_t>(leerEntero(datos.ubigeoDireccion, 999999));
    ciudadano.telefono = static_cast<uint32_t>(leerEntero(datos.telefono, 999999999));
    if (ciudadano.ubigeoNacimiento == 0 || ciudadano.ubigeoDireccion == 0) {
        throw std::runtime_error("Ubigeo invalido.");
    }
    if (ciudadano.telefono < 100000000) {
        throw std::runtime_error("Telefono invalido.");
    }

    ciudadano.dni = dni;
    ciudadano.nombres = guardarTexto(datos.nombres, true);
    ciudadano.apellidoPaterno = guardarTexto(datos.apellidoPaterno, true);
    ciudadano.apellidoMaterno = guardarTexto(datos.apellidoMaterno, true);
    ciudadano.nacionalidad = guardarTexto(datos.nacionalidad);
    ciudadano.ciudad = guardarTexto(datos.ciudad);
    ciudadano.calle = guardarTexto(datos.calle);
    ciudadano.detalleDireccion = guardarTexto(datos.detalleDireccion);
    ciudadano.correo = datos.correo == "ciudadano" + formatearDni(dni) + "@example.test"
                           ? UINT32_MAX : guardarTexto(datos.correo);
    ciudadano.numeroDireccion = datos.numeroDireccion;
    ciudadano.estadoCivil = datos.estadoCivil;
    return ciudadano;
}

std::string RegistroCiudadanos::registrarCiudadano(const DatosRegistro& datos) {
    Ciudadano ciudadano = crearDesdeDatos(datos, generarDniUnico());
    agregarCiudadano(ciudadano);
    archivo.guardarCambios();
    return ciudadanoAJson(ciudadano);
}

std::string RegistroCiudadanos::actualizarPersona(uint32_t dni, const DatosRegistro& datos,
                              const std::string& motivo, const std::string& referencia) {
    uint32_t posicion = 0;
    if (!indiceDni.buscar(dni, posicion)) {
        throw std::runtime_error("La persona no esta registrada.");
    }
    validarTextoObligatorio(motivo);
    if (referencia.size() > 512) {
        throw std::runtime_error("La referencia supera 512 bytes.");
    }
    const Ciudadano anterior = ciudadanos.obtener(posicion);
    // Identidad, nacionalidad, nacimiento y estado civil necesitan sustento.
    bool documentado = datos.nombres != texto(anterior.nombres) ||
        datos.apellidoPaterno != texto(anterior.apellidoPaterno) ||
        datos.apellidoMaterno != texto(anterior.apellidoMaterno) ||
        datos.nacionalidad != texto(anterior.nacionalidad) ||
        leerEntero(datos.ubigeoNacimiento, 999999) != anterior.ubigeoNacimiento ||
        datos.estadoCivil != anterior.estadoCivil;
    if (documentado) {
        validarTextoObligatorio(referencia);
    }
    Ciudadano nuevo = crearDesdeDatos(datos, dni);
    nuevo.estado = anterior.estado;
    if (ciudadanoAJson(nuevo) == ciudadanoAJson(anterior)) {
        return ciudadanoAJson(anterior);
    }
    std::time_t ahora = std::time(nullptr);
    std::ostringstream fecha;
    fecha << std::put_time(std::gmtime(&ahora), "%Y-%m-%dT%H:%M:%SZ");
    std::string evento = serializarCiudadano(nuevo);
    escribirTextoEvento(evento, fecha.str());
    escribirTextoEvento(evento, motivo);
    escribirTextoEvento(evento, referencia);
    archivo.agregarEvento(EVENTO_ACTUALIZACION, evento);
    aplicarEvento(EVENTO_ACTUALIZACION, evento);
    archivo.guardarCambios();
    return ciudadanoAJson(nuevo);
}

std::string RegistroCiudadanos::obtenerHistorial(uint32_t dni) const {
    if (!buscarCiudadano(dni)) {
        throw std::runtime_error("La persona no esta registrada.");
    }
    auto encontrado = historial.find(dni);
    std::string salida = "[";
    if (encontrado != historial.end()) {
        const auto& cambios = encontrado->second;
        size_t inicio = cambios.size() > 50 ? cambios.size() - 50 : 0;
        for (size_t i = cambios.size(); i > inicio; --i) {
            const CambioDatos& cambio = cambios[i - 1];
            if (i != cambios.size()) { salida += ','; }
            salida += "{\"date\":" + escaparJson(cambio.fecha) +
                ",\"reason\":" + escaparJson(cambio.motivo) +
                ",\"reference\":" + escaparJson(cambio.referencia) +
                ",\"before\":" + ciudadanoAJson(cambio.anterior) +
                ",\"after\":" + ciudadanoAJson(cambio.actual) + "}";
        }
    }
    return salida + "]";
}

std::string RegistroCiudadanos::generarPoblacion(uint32_t objetivo, uint64_t semilla,
                             void informarProgreso(size_t)) {
    if (objetivo < ciudadanos.cantidad()) {
        throw std::runtime_error(
            "El objetivo es menor que la poblacion existente; no se borran registros.");
    }
    if (objetivo > MAXIMO_CIUDADANOS) {
        throw std::runtime_error("El objetivo supera los DNI disponibles.");
    }
    generador.seed(semilla);
    prepararDatosSimulados();
    indiceDni.reservar(objetivo);
    Reloj::time_point inicio = Reloj::now();
    const size_t cantidadInicial = ciudadanos.cantidad();
    while (ciudadanos.cantidad() < objetivo) {
        agregarCiudadano(crearCiudadanoSimulado());
        if (ciudadanos.cantidad() % 100000 == 0) {
            archivo.guardarCambios();
            informarProgreso(ciudadanos.cantidad());
        }
    }
    archivo.guardarCambios();
    informarProgreso(ciudadanos.cantidad());
    std::ostringstream salida;
    salida << "{\"generated\":" << ciudadanos.cantidad() - cantidadInicial
           << ",\"seconds\":" << tiempoTranscurridoNs(inicio) / 1e9
           << ",\"status\":" << obtenerEstado() << '}';
    return salida.str();
}

std::string RegistroCiudadanos::medirRendimiento(uint32_t muestras) {
    if (ciudadanos.cantidad() == 0) {
        throw std::runtime_error("Genere ciudadanos antes de medir.");
    }
    if (muestras == 0 || muestras > 100000) {
        throw std::runtime_error("Muestras entre 1 y 100000.");
    }
    if (ciudadanos.cantidad() + muestras > MAXIMO_CIUDADANOS) {
        throw std::runtime_error("No hay suficientes DNI para la prueba de insercion.");
    }
    prepararDatosSimulados();
    indiceDni.reservar(ciudadanos.cantidad() + muestras);
    const size_t cantidadInicial = ciudadanos.cantidad();
    std::vector<uint32_t> aciertos, fallos, insertados;
    aciertos.reserve(muestras);
    fallos.reserve(muestras);
    insertados.reserve(muestras);
    for (uint32_t i = 0; i < muestras; ++i) {
        aciertos.push_back(
            ciudadanos.obtener(aleatorioMenorQue(static_cast<uint32_t>(cantidadInicial))).dni);
        fallos.push_back(generarDniUnico());
    }
    uint64_t encontrado = 0;
    uint32_t posicion = 0;
    Reloj::time_point inicio = Reloj::now();
    for (uint32_t clave : aciertos) {
        if (indiceDni.buscar(clave, posicion)) {
            encontrado++;
        }
    }
    const double tiempoAciertosNs = tiempoTranscurridoNs(inicio) / muestras;
    inicio = Reloj::now();
    for (uint32_t clave : fallos) {
        if (indiceDni.buscar(clave, posicion)) {
            encontrado++;
        }
    }
    const double tiempoFallosNs = tiempoTranscurridoNs(inicio) / muestras;
    double tiempoInsercionesNs = 0;
    for (uint32_t i = 0; i < muestras; ++i) {
        Ciudadano ciudadano = crearCiudadanoSimulado();
        inicio = Reloj::now();
        agregarCiudadano(ciudadano);
        archivo.guardarCambios();
        tiempoInsercionesNs += tiempoTranscurridoNs(inicio);
        insertados.push_back(ciudadano.dni);
    }
    double tiempoBajasNs = 0;
    for (uint32_t clave : insertados) {
        inicio = Reloj::now();
        registrarFallecimiento(clave);
        tiempoBajasNs += tiempoTranscurridoNs(inicio);
    }
    archivo.guardarCambios();
    std::ostringstream salida;
    salida << "{\"populationBefore\":" << cantidadInicial
           << ",\"populationAfter\":" << ciudadanos.cantidad() << ",\"samples\":" << muestras
           << ",\"lookupHitMeanNs\":" << tiempoAciertosNs
           << ",\"lookupMissMeanNs\":" << tiempoFallosNs
           << ",\"insertMeanNs\":" << tiempoInsercionesNs / muestras
           << ",\"deathMeanNs\":" << tiempoBajasNs / muestras
           << ",\"verifiedHits\":" << encontrado
           << ",\"scope\":\"Core only. Lookup measures the DNI index. Insert and death "
              "include the journal flush; insert excludes generation of the test person.\"}";
    return salida.str();
}

std::string RegistroCiudadanos::verificarIndice() const {
    size_t activosContados = 0;
    size_t fallecidosContados = 0;
    for (size_t i = 0; i < ciudadanos.cantidad(); ++i) {
        const Ciudadano& ciudadano = ciudadanos.obtener(i);
        uint32_t posicion = 0;
        if (!indiceDni.buscar(ciudadano.dni, posicion) || posicion != i) {
            throw std::runtime_error("Indice inconsistente con los registros.");
        }
        validarCodigosTexto(ciudadano);
        if (ciudadano.estado == 1) {
            activosContados++;
        }
        if (ciudadano.estado == 2) {
            fallecidosContados++;
        }
    }
    if (activosContados != cantidadActivos || fallecidosContados != cantidadFallecidos ||
        indiceDni.cantidad() != ciudadanos.cantidad()) {
        throw std::runtime_error("Conteo inconsistente.");
    }
    return "{\"valid\":true,\"checked\":" + std::to_string(ciudadanos.cantidad()) + "}";
}

} // namespace eda
