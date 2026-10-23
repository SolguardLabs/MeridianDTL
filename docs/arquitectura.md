# Arquitectura

MeridianDTL separa serialización, identidad, rutas, ledger, runtime y riesgo. El core C no depende de red, sistema de archivos ni reloj; el epoch llega como dato explícito.

```mermaid
flowchart TB
    Main[main.c] --> Runtime[runtime.c]
    Runtime --> Intent[intent.c]
    Intent --> Crypto[crypto + codec]
    Intent --> Route[route.c]
    Intent --> Exposure[exposure.c]
    Exposure --> Ledger
    Intent --> Ledger[ledger.c]
    Ledger --> Amount[amount.c]
    Ledger --> Digest[digest.c]
    Ledger --> RiskView[exposure views]
    RiskView --> Risk[risk.c]
```

## Dominio

`MdLedger` contiene activos, cuentas, bóvedas, lanes, reservas, receipts y journal en arrays acotados. Los límites explícitos evitan asignación dinámica y hacen visible el coste máximo de una operación.

`MdSettlementPlan` describe hops, lane, beneficiario, operador, caps y epochs. `MdCompactedPlan` conserva la forma ejecutable. `MdReservation` inmoviliza el importe; `MdReceipt` materializa el resultado.

## Flujo

```mermaid
sequenceDiagram
    participant R as Runtime
    participant I as Intent
    participant L as Ledger
    participant P as Route
    R->>I: Reserve signed intent
    I->>L: Lock gross amount
    I->>P: Compact plan
    P-->>I: Effective route
    I->>L: Debit reservation and vaults
    I->>L: Credit net, fees and reserve
    I-->>R: Receipt
```

## Determinismo

- Arrays y recorridos tienen orden estable.
- Digests usan serialización canónica con dominios explícitos.
- No hay coma flotante ni fuentes aleatorias.
- La salida JSON conserva campos y orden.
- El riesgo recibe posiciones y devuelve un informe sin mutación.

## Extensión

Una regla económica nueva debe vivir en una función pura y devolver `MdStatus`. Debe validar punteros, límites, overflow y capacidad. Los adaptadores solo traducen entrada y salida; no deben duplicar cálculos del core.
