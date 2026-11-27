#pragma once
#include "bplus_tree.h"
#include "cuckoo_hash.h"
#include "journal.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <random>
#include <unordered_map>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace eda {
using Reloj = std::chrono::steady_clock;
double tiempoTranscurridoNs(Reloj::time_point inicio);

// Coordina los datos, los indices y el archivo. Node solo envia las solicitudes.
class RegistroCiudadanos {
private:
    void validarTextoObligatorio(const std::string& valor) const;
    AlmacenCiudadanos ciudadanos;
    TablaCuckoo indiceDni;
    ArbolBMas indiceNombres;
    // Cada término normalizado apunta a tres listas ordenadas de posiciones.
    struct GrupoNombre { std::array<std::vector<uint32_t>, 3> listas; };
    std::vector<GrupoNombre> gruposNombre;
    std::unordered_map<std::string, uint32_t> grupoPorNombre;
    std::vector<uint32_t> grupoPorTexto;
    std::vector<std::string> textos;
    std::vector<uint8_t> nombreIndexado;
    // Diccionario auxiliar: texto -> codigo. Evita repetir el mismo texto en RAM.
    std::unordered_map<std::string, uint32_t> codigosTexto;
    ArchivoCiudadanos archivo;
    struct CambioDatos {
        Ciudadano anterior;
        Ciudadano actual;
        std::string fecha, motivo, referencia;
    };
    // Solo las personas editadas tienen historial adicional en memoria.
    std::unordered_map<uint32_t, std::vector<CambioDatos>> historial;
    size_t cantidadActivos = 0;
    size_t cantidadFallecidos = 0;

    void contarEstado(uint8_t estado, bool agregar);
    std::mt19937_64 generador{202602};
    static constexpr uint32_t MAXIMO_CIUDADANOS = 100000000;
    static inline const std::array<uint32_t, 5> ubigeos{150101, 150122, 40101, 80101, 130101};
    static inline const std::array<const char*, 40> nombres{
        "Alejandro", "Alonso",  "Andres",  "Angel",    "Antonio", "Bruno",    "Camila", "Carla",
        "Carlos",    "Carmen",  "Cesar",   "Claudia",  "Daniel",  "Daniela",  "Diego",  "Eduardo",
        "Elena",     "Esteban", "Fabiana", "Fernando", "Gabriel", "Gabriela", "Hector", "Isabel",
        "Javier",    "Jorge",   "Jose",    "Juan",     "Julia",   "Laura",    "Lucia",  "Luis",
        "Manuel",    "Maria",   "Miguel",  "Natalia",  "Paola",   "Pedro",    "Sofia",  "Valeria"};
    static inline const std::array<const char*, 40> apellidos{
        "Aguilar", "Alarcon",  "Alvarez",   "Arias",   "Campos",    "Castillo", "Castro",
        "Chavez",  "Condori",  "Cruz",      "Diaz",    "Fernandez", "Flores",   "Garcia",
        "Gomez",   "Gonzales", "Gutierrez", "Herrera", "Huaman",    "Jimenez",  "Lopez",
        "Mamani",  "Medina",   "Mendoza",   "Morales", "Navarro",   "Paredes",  "Perez",
        "Quispe",  "Ramirez",  "Ramos",     "Reyes",   "Rios",      "Rivera",   "Rodriguez",
        "Rojas",   "Ruiz",     "Sanchez",   "Torres",  "Vargas"};
    std::vector<uint32_t> nombresSimulados, apellidosSimulados, ciudadesSimuladas, callesSimuladas;
    uint32_t codigoTextoVacio = 0, codigoPeruana = 0;

    const std::string& texto(uint32_t id) const;
    void indexarNombre(uint32_t id);
    static std::array<uint32_t, 3> camposNombre(const Ciudadano& persona);
    void indexarPersona(const Ciudadano& persona, uint32_t posicion, const Ciudadano* anterior = nullptr);
    uint32_t guardarTexto(const std::string& valor, bool esNombre = false);
    uint32_t aleatorioMenorQue(uint32_t limite);
    uint32_t generarDniUnico();
    void validarCodigosTexto(const Ciudadano& ciudadano) const;
    void aplicarEvento(uint32_t tipo, const std::string& datos);
    void prepararDatosSimulados();
    Ciudadano crearCiudadanoSimulado();
    void agregarCiudadano(const Ciudadano& ciudadano);

public:
    std::string listarPersonas(const std::string& dni, const std::array<std::string, 3>& filtros,
                               size_t inicio, size_t limite) const;
    std::string buscarPorNombre(size_t campo, const std::string& prefijo, size_t inicio,
                                size_t limite, bool sugerencias) const;
    explicit RegistroCiudadanos(const std::filesystem::path& carpeta);
    void guardarCambios();
    std::string obtenerEstado() const;
    const Ciudadano* buscarCiudadano(uint32_t dni) const;
    std::string ciudadanoAJson(const Ciudadano& ciudadano) const;
    std::string obtenerMuestra(size_t limite) const;
    std::string sugerirNombres(const std::string& consulta, size_t limite) const;
    // Se registra un fallecimiento sin retirar el DNI ni los datos de la persona.
    void registrarFallecimiento(uint32_t dni);
    Ciudadano crearDesdeDatos(const DatosRegistro& datos, uint32_t dni);
    std::string registrarCiudadano(const DatosRegistro& datos);
    std::string actualizarPersona(uint32_t dni, const DatosRegistro& datos,
                                  const std::string& motivo, const std::string& referencia);
    std::string obtenerHistorial(uint32_t dni) const;
    std::string generarPoblacion(uint32_t objetivo, uint64_t semilla,
                                 void informarProgreso(size_t));
    std::string medirRendimiento(uint32_t muestras);
    std::string verificarIndice() const;
};
} // namespace eda
