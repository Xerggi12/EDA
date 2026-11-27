// Interfaz con JavaScript del navegador. No se necesitan librerias.
const formatoNumero = new Intl.NumberFormat('es-PE');
let estadoSistema = null;
let accionPendiente = false;
let consultandoEstado = false;
let ciudadanosMuestra = [];
let busquedaTexto = null;
const porPagina = 20;
let ciudadanoSeleccionado = null;
let personaEnEdicion = null;
const camposIdentidad = ['firstNames', 'paternalSurname', 'maternalSurname', 'nationality', 'birthUbigeo'];
let catalogoUbigeos = [];

function elemento(id) {
  return document.getElementById(id);
}

async function solicitar(ruta, metodo = 'GET', datos) {
  const opciones = { method: metodo };
  if (datos !== undefined) {
    opciones.headers = { 'Content-Type': 'application/json' };
    opciones.body = JSON.stringify(datos);
  }
  const respuesta = await fetch('/api' + ruta, opciones);
  const contenido = await respuesta.json();
  if (!respuesta.ok) {
    throw new Error(contenido.error || 'No se pudo completar la solicitud.');
  }
  return contenido;
}

function mostrarMensaje(texto, esError = false) {
  const aviso = elemento('mensaje');
  aviso.textContent = texto;
  aviso.className = 'aviso ' + (esError ? 'aviso-error' : 'aviso-exito');
  aviso.hidden = !texto;
}

function cambiarPestana(nombre) {
  document.querySelectorAll('[data-panel]').forEach(function (boton) {
    const seleccionada = boton.dataset.panel === nombre;
    boton.setAttribute('aria-selected', String(seleccionada));
    boton.tabIndex = seleccionada ? 0 : -1;
    elemento('panel-' + boton.dataset.panel).hidden = !seleccionada;
  });
}

function tareaEnCurso() {
  return estadoSistema && estadoSistema.busy;
}

function actualizarControles() {
  const deshabilitar = !estadoSistema || !estadoSistema.ready || tareaEnCurso() || accionPendiente;
  document.querySelectorAll('[data-requiere-listo], [data-operacion]').forEach(function (control) {
    control.disabled = Boolean(deshabilitar);
  });
  elemento('registrar-fallecimiento').disabled = Boolean(deshabilitar || !ciudadanoSeleccionado);
  elemento('editar-persona').disabled = Boolean(deshabilitar || !ciudadanoSeleccionado);
  elemento('ver-historial').disabled = Boolean(deshabilitar || !ciudadanoSeleccionado);
  elemento('pagina-anterior').disabled = Boolean(deshabilitar || !busquedaTexto || busquedaTexto.inicio === 0);
  elemento('pagina-siguiente').disabled = Boolean(deshabilitar || !busquedaTexto || busquedaTexto.inicio + porPagina >= busquedaTexto.total);
}

async function ejecutarAccion(operacion) {
  if (accionPendiente) return;
  accionPendiente = true;
  mostrarMensaje('');
  actualizarControles();
  try {
    await operacion();
  } catch (error) {
    mostrarMensaje(error.message, true);
  } finally {
    accionPendiente = false;
    actualizarControles();
    await actualizarEstado();
  }
}

async function actualizarEstado() {
  if (consultandoEstado) return;
  consultandoEstado = true;
  try {
    estadoSistema = await solicitar('/disponibilidad');
    const conexion = elemento('conexion');
    let texto;
    let clase = 'aviso-info';
    if (estadoSistema.error) {
      texto = 'El sistema no está disponible: ' + estadoSistema.error;
      clase = 'aviso-error';
    } else if (!estadoSistema.ready) {
      texto = 'Cargando registros…';
    } else if (tareaEnCurso()) {
      texto = 'Procesando tarea…';
    } else {
      texto = '';
    }
    if (conexion.textContent !== texto) conexion.textContent = texto;
    conexion.className = 'aviso ' + clase;
    conexion.hidden = !texto;
    if (!busquedaTexto && estadoSistema.ready && !tareaEnCurso() && !accionPendiente) {
      await cargarListado({}, 0);
    }
    actualizarControles();
  } catch {
    estadoSistema = null;
    elemento('conexion').hidden = false;
    elemento('conexion').textContent = 'No se pudo conectar con el servidor. Comprueba que Iniciar backend.cmd siga abierto.';
    elemento('conexion').className = 'aviso aviso-error';
    actualizarControles();
  } finally {
    consultandoEstado = false;
  }
}

function crearInsignia(estado) {
  const etiqueta = document.createElement('span');
  etiqueta.className = 'insignia insignia-' + estado;
  const nombresEstado = { vivo: 'Vivo/a', fallecido: 'Fallecido/a', sin_clasificar: 'Sin clasificar' };
  etiqueta.textContent = nombresEstado[estado] || 'Sin clasificar';
  return etiqueta;
}

function dibujarMuestra() {
  if (!busquedaTexto) return;
  const cuerpo = elemento('tabla-muestra');
  cuerpo.replaceChildren();
  ciudadanosMuestra.forEach(function (ciudadano) {
    const fila = document.createElement('tr');
    const dni = document.createElement('td');
    dni.textContent = ciudadano.dni;
    const nombre = document.createElement('td');
    nombre.textContent = ciudadano.firstNames + ' ' + ciudadano.paternalSurname + ' ' + ciudadano.maternalSurname;
    const estado = document.createElement('td');
    estado.append(crearInsignia(ciudadano.state));
    const accion = document.createElement('td');
    const boton = document.createElement('button');
    boton.type = 'button';
    boton.className = 'boton boton-secundario';
    boton.textContent = 'Ver';
    boton.setAttribute('aria-label', 'Ver persona ' + ciudadano.dni);
    boton.dataset.operacion = 'consulta';
    boton.addEventListener('click', function () {
      ejecutarAccion(function () { return buscarCiudadano(ciudadano.dni); });
    });
    accion.append(boton);
    fila.append(dni, nombre, estado, accion);
    cuerpo.append(fila);
  });
  const { filtros, inicio, total } = busquedaTexto;
  const filtrado = Object.values(filtros).some(Boolean);
  elemento('titulo-resultados').textContent = filtrado ? 'Coincidencias' : 'Personas registradas';
  elemento('muestra-vacia').hidden = ciudadanosMuestra.length > 0;
  elemento('muestra-vacia').textContent = filtrado ? 'No se encontraron coincidencias.' : 'No hay personas registradas.';
  const rango = total ? formatoNumero.format(inicio + 1) + '–' + formatoNumero.format(inicio + ciudadanosMuestra.length) : '';
  elemento('cantidad-muestra').textContent = filtrado && total ? rango + ' de ' + formatoNumero.format(total) : rango;
  elemento('paginas-busqueda').hidden = total <= porPagina;
  actualizarControles();
}

function descripcionUbigeo(codigo) {
  const lugar = catalogoUbigeos.find(function (ubigeo) { return ubigeo.code === codigo; });
  if (!lugar) return 'Ubigeo ' + codigo;
  return lugar.department + ' / ' + lugar.province + ' / ' + lugar.district + ' (' + codigo + ')';
}

function mostrarCiudadano(ciudadano) {
  ciudadanoSeleccionado = ciudadano;
  elemento('historial-persona').hidden = true;
  elemento('lista-historial').replaceChildren();
  elemento('detalle-ciudadano').hidden = false;
  const etiqueta = crearInsignia(ciudadano.state);
  elemento('estado-ciudadano').className = etiqueta.className;
  elemento('estado-ciudadano').textContent = etiqueta.textContent;
  const civil = { soltero: 'Soltero/a', casado: 'Casado/a', divorciado: 'Divorciado/a', viudo: 'Viudo/a', conviviente: 'Conviviente' };
  let direccion = ciudadano.address.street + ' ' + ciudadano.address.number;
  if (ciudadano.address.detail) direccion += ', ' + ciudadano.address.detail;
  const campos = [
    ['DNI', ciudadano.dni], ['Nombres', ciudadano.firstNames],
    ['Apellido paterno', ciudadano.paternalSurname], ['Apellido materno', ciudadano.maternalSurname],
    ['Nacionalidad', ciudadano.nationality], ['Estado civil', civil[ciudadano.civilStatus]],
    ['Lugar de nacimiento', descripcionUbigeo(ciudadano.birthUbigeo)],
    ['Departamento / provincia / distrito', descripcionUbigeo(ciudadano.address.ubigeo)],
    ['Ciudad', ciudadano.address.city], ['Dirección', direccion],
    ['Teléfono de referencia', ciudadano.phone], ['Correo electrónico', ciudadano.email],
  ];
  const lista = elemento('datos-ciudadano');
  lista.replaceChildren();
  campos.forEach(function (campo) {
    const grupo = document.createElement('div');
    const titulo = document.createElement('dt');
    const valor = document.createElement('dd');
    titulo.textContent = campo[0];
    valor.textContent = campo[1];
    grupo.append(titulo, valor);
    lista.append(grupo);
  });
  const boton = elemento('registrar-fallecimiento');
  boton.textContent = 'Registrar fallecimiento';
  boton.hidden = ciudadano.state !== 'vivo';
  boton.className = 'boton boton-fallecimiento';
  actualizarControles();
}

async function buscarCiudadano(dni) {
  ciudadanoSeleccionado = null;
  elemento('detalle-ciudadano').hidden = true;
  const ciudadano = await solicitar('/citizens/' + dni);
  mostrarCiudadano(ciudadano);
  elemento('detalle-ciudadano').scrollIntoView({ block: 'nearest' });
  mostrarMensaje('Persona con DNI ' + dni + ' encontrada.');
}

async function cargarUbigeos() {
  try {
    const respuesta = await solicitar('/ubigeos');
    catalogoUbigeos = respuesta.items;
    catalogoUbigeos.forEach(function (lugar) {
      const opcion = document.createElement('option');
      opcion.value = lugar.code;
      opcion.label = lugar.department + ' / ' + lugar.province + ' / ' + lugar.district;
      elemento('lista-ubigeos').append(opcion);
    });
  } catch {
    mostrarMensaje('No se pudo cargar el catálogo de ubigeos. Puedes escribir los códigos manualmente.', true);
  }
}

function actualizarDescripcionUbigeo(inputId, descripcionId) {
  const codigo = elemento(inputId).value;
  let descripcion = 'Departamento / provincia / distrito';
  if (/^[0-9]{6}$/.test(codigo)) {
    const lugar = catalogoUbigeos.find(function (item) { return item.code === codigo; });
    descripcion = lugar ? descripcionUbigeo(codigo) : 'Ubigeo ' + codigo;
  }
  elemento(descripcionId).textContent = descripcion;
}

// Eventos de navegacion.
document.querySelectorAll('[data-panel]').forEach(function (boton) {
  boton.addEventListener('click', function () {
    mostrarMensaje('');
    cambiarPestana(boton.dataset.panel);
  });
  boton.addEventListener('keydown', function (evento) {
    const botones = Array.from(document.querySelectorAll('[data-panel]'));
    let posicion = botones.indexOf(boton);
    if (evento.key === 'ArrowRight') posicion = (posicion + 1) % botones.length;
    else if (evento.key === 'ArrowLeft') posicion = (posicion + botones.length - 1) % botones.length;
    else if (evento.key === 'Home') posicion = 0;
    else if (evento.key === 'End') posicion = botones.length - 1;
    else return;
    evento.preventDefault();
    cambiarPestana(botones[posicion].dataset.panel);
    botones[posicion].focus();
  });
});

const formularioBusqueda = elemento('form-buscar');
async function cargarListado(filtros, inicio) {
  const parametros = new URLSearchParams({ ...filtros, offset: inicio, limit: porPagina });
  const respuesta = await solicitar('/citizens?' + parametros);
  ciudadanoSeleccionado = null;
  elemento('detalle-ciudadano').hidden = true;
  busquedaTexto = { filtros, inicio, total: respuesta.total };
  ciudadanosMuestra = respuesta.items;
  dibujarMuestra();
}

formularioBusqueda.addEventListener('submit', function (evento) {
  evento.preventDefault();
  const filtros = {};
  for (const [campo, valor] of new FormData(formularioBusqueda)) {
    if (valor.trim()) filtros[campo] = valor.trim();
  }
  ejecutarAccion(async function () {
    await cargarListado(filtros, 0);
    if (filtros.dni && ciudadanosMuestra.length === 1) mostrarCiudadano(ciudadanosMuestra[0]);
  });
});
function cambiarPagina(paso) {
  if (!busquedaTexto) return;
  const { filtros, inicio } = busquedaTexto;
  ejecutarAccion(() => cargarListado(filtros, inicio + paso * porPagina));
}
elemento('pagina-anterior').addEventListener('click', () => cambiarPagina(-1));
elemento('pagina-siguiente').addEventListener('click', () => cambiarPagina(1));

formularioBusqueda.querySelectorAll('[data-filtro-nombre]').forEach(function (campo) {
  let temporizador;
  let version = 0;
  campo.addEventListener('input', function () {
    clearTimeout(temporizador);
    const solicitud = ++version;
    const lista = campo.list;
    lista.replaceChildren();
    const q = campo.value.trim();
    if (q.length < 2) return;
    temporizador = setTimeout(async function () {
      try {
        const parametros = new URLSearchParams({ field: campo.name, q, limit: 8 });
        const respuesta = await solicitar('/suggestions?' + parametros);
        if (version !== solicitud || campo.value.trim() !== q) return;
        for (const nombre of respuesta.items) lista.append(new Option(nombre, nombre));
      } catch { /* Se puede seguir buscando por texto. */ }
    }, 200);
  });
});
function limpiarFiltros() {
  formularioBusqueda.reset();
  formularioBusqueda.querySelectorAll('datalist').forEach(lista => lista.replaceChildren());
}
elemento('nueva-consulta').addEventListener('click', function () {
  limpiarFiltros();
  ejecutarAccion(() => cargarListado({}, 0));
});
elemento('imprimir-ficha').addEventListener('click', function () {
  if (ciudadanoSeleccionado) window.print();
});

function actualizarModoFormulario() {
  const editando = Boolean(personaEnEdicion);
  elemento('titulo-formulario').textContent = editando ? 'Editar persona' : 'Registrar persona';
  elemento('guardar-persona').textContent = editando ? 'Guardar cambios' : 'Registrar persona';
  elemento('opciones-edicion').hidden = !editando;
  elemento('sustento-edicion').hidden = !editando;
  elemento('cancelar-edicion').hidden = !editando;
  elemento('motivo-edicion').required = editando;
  const formulario = elemento('form-registrar');
  camposIdentidad.forEach(function (campo) {
    formulario.elements.namedItem(campo).readOnly = editando && !elemento('rectificar-identidad').checked;
  });
  let necesitaDocumento = false;
  if (editando) {
    necesitaDocumento = camposIdentidad.concat('civilStatus').some(function (campo) {
      return formulario.elements.namedItem(campo).value !== personaEnEdicion[campo];
    });
  }
  elemento('referencia-edicion').required = necesitaDocumento;
}

elemento('editar-persona').addEventListener('click', function () {
  if (!ciudadanoSeleccionado) return;
  personaEnEdicion = ciudadanoSeleccionado;
  const persona = personaEnEdicion;
  const valores = { ...persona, addressUbigeo: persona.address.ubigeo, ...persona.address };
  const formulario = elemento('form-registrar');
  for (const nombre of camposIdentidad.concat('civilStatus', 'phone', 'email', 'addressUbigeo', 'city', 'street', 'number', 'detail')) {
    formulario.elements.namedItem(nombre).value = valores[nombre];
  }
  elemento('dni-edicion').textContent = persona.dni;
  elemento('rectificar-identidad').checked = false;
  elemento('motivo-edicion').value = '';
  elemento('referencia-edicion').value = '';
  actualizarModoFormulario();
  actualizarDescripcionUbigeo('ubigeo-nacimiento', 'descripcion-nacimiento');
  actualizarDescripcionUbigeo('ubigeo-direccion', 'descripcion-direccion');
  cambiarPestana('registrar');
  elemento('titulo-formulario').scrollIntoView({ block: 'nearest' });
});
elemento('rectificar-identidad').addEventListener('change', actualizarModoFormulario);
elemento('form-registrar').addEventListener('input', actualizarModoFormulario);
elemento('cancelar-edicion').addEventListener('click', function () {
  personaEnEdicion = null;
  elemento('form-registrar').reset();
  actualizarModoFormulario();
  cambiarPestana('buscar');
});

function camposHistorial(persona) {
  return {
    Nombres: persona.firstNames, 'Apellido paterno': persona.paternalSurname,
    'Apellido materno': persona.maternalSurname, Nacionalidad: persona.nationality,
    'Ubigeo de nacimiento': persona.birthUbigeo, 'Estado civil': persona.civilStatus,
    'Ubigeo de domicilio': persona.address.ubigeo, Ciudad: persona.address.city,
    Calle: persona.address.street, Número: persona.address.number, Detalle: persona.address.detail,
    Teléfono: persona.phone, Correo: persona.email,
  };
}
elemento('ver-historial').addEventListener('click', function () {
  if (!ciudadanoSeleccionado) return;
  const dni = ciudadanoSeleccionado.dni;
  ejecutarAccion(async function () {
    const respuesta = await solicitar('/citizens/' + dni + '/history');
    const lista = elemento('lista-historial');
    lista.replaceChildren();
    if (!respuesta.items.length) lista.textContent = 'No se han realizado ediciones.';
    respuesta.items.forEach(function (cambio) {
      const bloque = document.createElement('div');
      bloque.className = 'cambio-persona';
      const titulo = document.createElement('strong');
      titulo.textContent = new Date(cambio.date).toLocaleString('es-PE') + ' — ' + cambio.reason;
      bloque.append(titulo);
      if (cambio.reference) {
        const referencia = document.createElement('p');
        referencia.textContent = 'Documento / expediente: ' + cambio.reference;
        bloque.append(referencia);
      }
      const anterior = camposHistorial(cambio.before);
      const actual = camposHistorial(cambio.after);
      for (const campo of Object.keys(anterior)) {
        if (anterior[campo] === actual[campo]) continue;
        const detalle = document.createElement('p');
        detalle.textContent = campo + ': ' + (anterior[campo] === '' ? '(vacío)' : anterior[campo]) + ' → ' + (actual[campo] === '' ? '(vacío)' : actual[campo]);
        bloque.append(detalle);
      }
      lista.append(bloque);
    });
    elemento('historial-persona').hidden = false;
  });
});

elemento('registrar-fallecimiento').addEventListener('click', function () {
  if (!ciudadanoSeleccionado || ciudadanoSeleccionado.state !== 'vivo') return;
  const dni = ciudadanoSeleccionado.dni;
  if (!window.confirm('¿Registrar el fallecimiento de la persona con DNI ' + dni + '?')) return;
  ejecutarAccion(async function () {
    const actualizado = await solicitar('/citizens/' + dni + '/death', 'POST', {});
    mostrarCiudadano(actualizado);
    ciudadanosMuestra = ciudadanosMuestra.map(function (persona) {
      return persona.dni === actualizado.dni ? actualizado : persona;
    });
    dibujarMuestra();
    mostrarMensaje('Fallecimiento registrado para el DNI ' + dni + '.');
  });
});

elemento('form-registrar').addEventListener('submit', function (evento) {
  evento.preventDefault();
  // Leemos los campos antes de deshabilitar el formulario durante el envio.
  const formulario = new FormData(elemento('form-registrar'));
  const dniEdicion = personaEnEdicion ? personaEnEdicion.dni : null;
  ejecutarAccion(async function () {
    const datos = {
      firstNames: formulario.get('firstNames'), paternalSurname: formulario.get('paternalSurname'),
      maternalSurname: formulario.get('maternalSurname'), nationality: formulario.get('nationality'),
      birthUbigeo: formulario.get('birthUbigeo'), civilStatus: formulario.get('civilStatus'),
      phone: formulario.get('phone'), email: formulario.get('email'),
      address: { ubigeo: formulario.get('addressUbigeo'), city: formulario.get('city'), street: formulario.get('street'), number: Number(formulario.get('number')), detail: formulario.get('detail') },
    };
    if (dniEdicion) {
      datos.reason = formulario.get('reason');
      datos.reference = formulario.get('reference');
    }
    const ciudadano = await solicitar(dniEdicion ? '/citizens/' + dniEdicion : '/citizens', dniEdicion ? 'PUT' : 'POST', datos);
    limpiarFiltros();
    await cargarListado({}, 0);
    personaEnEdicion = null;
    elemento('form-registrar').reset();
    actualizarModoFormulario();
    cambiarPestana('buscar');
    mostrarCiudadano(ciudadano);
    mostrarMensaje(dniEdicion ? 'Datos actualizados. DNI: ' + ciudadano.dni + '.' : 'Persona registrada. DNI generado: ' + ciudadano.dni + '.');
    elemento('detalle-ciudadano').scrollIntoView({ block: 'nearest' });
  });
});

[['ubigeo-nacimiento', 'descripcion-nacimiento'], ['ubigeo-direccion', 'descripcion-direccion']].forEach(function (par) {
  elemento(par[0]).addEventListener('input', function () { actualizarDescripcionUbigeo(par[0], par[1]); });
});
elemento('form-registrar').addEventListener('reset', function () {
  personaEnEdicion = null;
  setTimeout(function () {
    actualizarModoFormulario();
    actualizarDescripcionUbigeo('ubigeo-nacimiento', 'descripcion-nacimiento');
    actualizarDescripcionUbigeo('ubigeo-direccion', 'descripcion-direccion');
  }, 0);
});

let temporizadorSugerencias;
let solicitudSugerencias;
document.querySelectorAll('[data-autocompletar]').forEach(function (campo) {
  campo.addEventListener('input', function () {
    clearTimeout(temporizadorSugerencias);
    if (solicitudSugerencias) solicitudSugerencias.abort();
    elemento('lista-nombres').replaceChildren();
    const consulta = campo.value.trim();
    if (consulta.length < 2 || tareaEnCurso()) return;
    temporizadorSugerencias = setTimeout(async function () {
      solicitudSugerencias = new AbortController();
      try {
        const respuesta = await fetch('/api/suggestions?q=' + encodeURIComponent(consulta) + '&limit=8', { signal: solicitudSugerencias.signal });
        if (!respuesta.ok) return;
        const datos = await respuesta.json();
        if (campo.value.trim() !== consulta) return;
        datos.items.forEach(function (nombre) {
          const opcion = document.createElement('option');
          opcion.value = nombre;
          elemento('lista-nombres').append(opcion);
        });
      } catch { /* El formulario permite escribir aunque no lleguen sugerencias. */ }
    }, 250);
  });
});

cargarUbigeos();
actualizarEstado();
setInterval(actualizarEstado, 2000);
