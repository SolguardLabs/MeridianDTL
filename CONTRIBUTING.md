# Contribuir

```bash
git clone https://github.com/SolguardLabs/MeridianDTL.git
cd MeridianDTL
npm ci
npm run ci
```

Mantenga importes en `MdAmount`, porcentajes en puntos básicos y cálculos fuera de adaptadores. Cada rama matemática necesita prueba C; cada cambio observable necesita una prueba sobre el binario. Documente redondeo, estados y errores.

Un pull request debe declarar alcance, impacto económico, compatibilidad y comandos ejecutados. `npm run ci` y `npm run release:check` deben terminar correctamente.
