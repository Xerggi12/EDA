param(
    [ValidateRange(1, 100000000)]
    [int]$Total = 33000000,
    [ValidateRange(0, 9007199254740991)]
    [long]$Semilla = 202602,
    [ValidateRange(1, 65535)]
    [int]$Puerto = 3000
)

$ErrorActionPreference = 'Stop'
$api = "http://127.0.0.1:$Puerto/api"

function ConsultarEstado {
    Invoke-RestMethod -Uri "$api/status" -TimeoutSec 15
}

function EsperarTarea($identificador) {
    do {
        Start-Sleep -Seconds 2
        $estado = ConsultarEstado
        if ($estado.error) { throw $estado.error }
        if ($estado.job.id -ne $identificador) {
            throw 'La tarea cambio. Consulte el estado antes de repetir la operacion.'
        }
        if ($estado.job.state -eq 'failed') { throw $estado.job.error }
        Write-Host ("{0}: {1:N0} registros comprobados o procesados." -f $estado.job.type, $estado.job.completed)
    } while ($estado.job.state -eq 'running')
    return $estado
}

try {
    Write-Host 'Conectando con el programa local. Mantenga abierta su ventana.'
    $estado = ConsultarEstado
    while (-not $estado.ready) {
        if ($estado.error) { throw $estado.error }
        Write-Host 'Esperando a que C++ termine de cargar los datos...'
        Start-Sleep -Seconds 2
        $estado = ConsultarEstado
    }
    if ($estado.job.state -eq 'running') {
        throw 'Ya hay una tarea masiva en curso. Espere a que termine antes de generar.'
    }
    Write-Host ("Personas guardadas: {0:N0}. Total solicitado: {1:N0}." -f $estado.status.total, $Total)
    if ($estado.status.total -lt $Total) {
        $datos = @{ target = $Total; seed = $Semilla } | ConvertTo-Json -Compress
        $tarea = Invoke-RestMethod -Uri "$api/generate" -Method Post -ContentType 'application/json' -Body $datos -TimeoutSec 15
        $estado = EsperarTarea $tarea.jobId
        if ($estado.status.total -ne $Total) { throw 'La generacion termino con un total inesperado.' }
    }
    Write-Host 'Verificando la correspondencia de todos los DNI con sus fichas...'
    $tarea = Invoke-RestMethod -Uri "$api/verify" -Method Post -ContentType 'application/json' -Body '{}' -TimeoutSec 15
    $estado = EsperarTarea $tarea.jobId
    if (-not $estado.job.result.valid -or $estado.job.result.checked -ne $estado.status.total) {
        throw 'La verificacion del padron no coincide con el total de personas.'
    }
    Write-Host ("Listo. {0:N0} personas guardadas y verificadas. Actualice la pagina web." -f $estado.status.total)
    exit 0
} catch {
    Write-Host ("No se pudo preparar el padron: " + $_.Exception.Message) -ForegroundColor Red
    Write-Host 'Abra primero Iniciar backend.cmd y revise que la consola del servidor siga abierta.'
    exit 1
}
