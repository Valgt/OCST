# Phase 2.0 - Warm Starts (Seguimiento)

## Estado actual
- Loader/validador común en `src/cpp/common/warm_start_loader.*` (fail-fast, O(n+m)).
- Integración en `path_based_formulation`: aplica warm starts del JSON (`warm_starts`), si falta/rompe → aborta.
- Telemetría: `warm_starts_tried`, `warm_start_used`, `warm_start_path` en payload, runtime/gap/estatus poblados.
- Logging: eventos `warm_start` (start/success/failure) vía `StructuredLogger` cuando está habilitado.
- Orquestador ajustado: propaga métricas (runtime, nodos, iteraciones, lazy/cuts) y warm starts a `experiment_summary` y `comparative_summary`, incluyendo la formulación control; compara contra best-known.
- Experimento `quick_check` 25×2 (path_based vs control) completado: objetivos idénticos, warm_start_used=`mst`, métricas paridad ± pequeño ruido.

## Flujo actual
1) Config JSON define `warm_starts` (orden fijo). Sin config → `["mst"]` por defecto.
2) Se buscan archivos `data/input/<idea>/<instancia>.txt` (n, n-1 aristas 0-based en árbol).
3) Loader valida tamaño, rango, duplicados y conectividad; mapea a índices del grafo.
4) Solver crea un start por idea: `x` en 1 para aristas del árbol, `y` en 1 sobre el camino único por requerimiento.
5) Telemetría/log: se registra idea, estado y duración de parseo; se vuelca a payload/metadata al final.

## Decisiones tomadas
- Centralizar loader en `common` para reuso futuro (otras formulaciones).
- Mantener siembra solo en `x` y flujos unitarios en `y` (consistente con Phase 2.0).
- Usar `solver_metadata` + `warm_start_path` para trazar qué warm starts se probaron/usaron.
- Describir estatus humano (`optimization_status_description`) y rellenar `runtime_stats`.

## Pendientes / siguientes pasos
- Agregar más ideas (ej. `dijkstra_tree`) cuando haya archivos en `data/input/<idea>/`.
- Opcional: logging más rico (stats finales) y digest real de config.
- Validar en smoke test una instancia pequeña con `mst` presente.
- Mostrar warm starts/nodos/cortes también en la tabla de consola del orquestador (si se desea).

## Preguntas abiertas
- ¿Formato preferido para warm_starts_tried (csv vs. key individual)? Actualmente csv en `solver_metadata`.
- ¿Queremos habilitar logging estructurado por defecto o solo bajo flag CLI?
