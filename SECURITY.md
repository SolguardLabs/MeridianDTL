# Politica de seguridad

La seguridad de Meridian DTL se apoya en autorizaciones firmadas, dominios de
firma separados, validaciones de ledger y conservacion determinista de
suministro. Este documento resume el alcance esperado de revision y los
controles de seguridad del proyecto.

## Alcance

Se consideran dentro de alcance:

- Construccion y verificacion de intenciones firmadas.
- Reservas de fondos y nonces de pagador.
- Compactacion de rutas multi-hop.
- Receipts por epoch y settlements.
- Validacion de bovedas, lanes y activos.
- Calculo de cargos operativos y reservas internas.
- Conservacion de suministro.
- Salida JSON de escenarios reproducibles.
- Scripts, CI y tests JavaScript incluidos en el repositorio.

Quedan fuera de alcance:

- Custodia real de claves privadas.
- Persistencia externa o bases de datos.
- Integracion con redes publicas o contratos desplegados.
- Control de acceso de sistema operativo.
- Riesgos derivados de modificar localmente escenarios o fixtures.

## Modelo de confianza

El protocolo asume que:

- Las claves privadas se generan y custodian fuera del ledger.
- Cada cuenta registrada corresponde a una identidad coherente.
- Los operadores presentan planes y receipts dentro de los dominios esperados.
- Los integradores ejecutan la version exacta validada por CI.
- Los reportes JSON son salida de diagnostico y no una fuente externa de verdad.

## Controles implementados

### Dominios de autorizacion

Las intenciones y receipts usan dominios distintos. Esta separacion reduce el
riesgo de reutilizacion de autorizaciones entre acciones con significado
operativo diferente.

### Nonces

Cada cuenta mantiene nonces independientes para intenciones y receipts. El
ledger valida el nonce esperado antes de aceptar una reserva o settlement.

### Digests canonicos

Los identificadores, digests de ruta y receipts se derivan desde datos canonicos
con dominios explicitos. Esto permite reproducir resultados entre ejecuciones y
comparar estados sin depender de ordenamientos externos.

### Conservacion de suministro

Las transiciones criticas se aplican sobre estructuras de estado controladas y
se rechazan si la suma de saldos liquidos, reservas pendientes y bovedas no
coincide con el suministro emitido por el ledger.

### Validacion de lanes

Cada lane declara activo, boveda origen, boveda destino, clase de settlement,
cargos y estado de disponibilidad. Las rutas se validan contra ese catalogo
antes de reservar o liquidar.

## Practicas de desarrollo

Todo cambio deberia pasar por:

```bash
bash scripts/ci.sh
```

Para revisiones rapidas:

```bash
bash scripts/tests.sh
```

## Gestion de dependencias

El motor C no usa dependencias externas. Los tests JavaScript se ejecutan con
`node:test`; Prettier es una dependencia de desarrollo opcional. Dependabot esta
configurado para revisar:

- GitHub Actions.
- npm.

## Reporte responsable

Los hallazgos de seguridad deben comunicarse de forma privada al equipo
mantenedor. Un reporte util debe incluir:

- Version o commit analizado.
- Sistema operativo y compilador utilizado.
- Comando exacto de reproduccion.
- Escenario afectado.
- Impacto tecnico y economico estimado.
- Evidencia minima para validar el hallazgo.
- Recomendacion de mitigacion, si aplica.

No se deben publicar pruebas de concepto completas ni trazas con impacto
economico sin coordinacion previa.

## Criterios de severidad

- Critica: perdida de fondos, liquidacion no autorizada, creacion de suministro
  o ruptura de conservacion.
- Alta: bypass de firmas, reuso de autorizaciones, corrupcion persistente de
  estado o duplicacion de receipts.
- Media: denegacion de servicio local, validaciones incompletas sin impacto
  economico directo o escenarios reproducibles que fallen bajo entradas validas.
- Baja: problemas de tooling, documentacion o hardening sin impacto sobre la
  liquidacion.

## Limitaciones

Meridian DTL no incluye consenso distribuido, almacenamiento tolerante a fallos,
gestion real de claves, monitorizacion ni integracion on-chain. Cualquier uso
fuera de entornos controlados requiere una revision de arquitectura y auditoria
independiente.
