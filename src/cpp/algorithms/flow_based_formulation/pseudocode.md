# Pseudocódigo Flow-Based Formulation (Implementación Real)

```pseudo
function resolver_instancia_flowbased(ruta_archivo_entrada, ruta_salida_csv, tiempo_limite, heuristica):
    flujo_entrada <- abrir_archivo(ruta_archivo_entrada)

    // 1. Parseo de la instancia (datos y preprocesos comunes)
    instancia <- leer_instancia(flujo_entrada)
        leer n, m, probabilidad
        aristas <- recopilar m triples (u, v, costo)
        demandas <- recopilar r requerimientos (origen, destino, peso)
        instancia <- construir_OCSTInstance(n, aristas, demandas, probabilidad)
            instancia.edges <- aristas bidireccionales
            instancia.requirements <- demandas acumuladas por origen
            instancia.num_nodes <- n
            instancia.num_edges <- m

    cerrar(flujo_entrada)

    // 2. Preparación del solver y parámetros globales
    solver <- new FlowBasedSolver(instancia)
        solver.env <- GRBEnv()
            env.setParam(OutputFlag, 0)
        solver.model <- GRBModel(env)
            model.setParam(LazyConstraints, 1)
            model.setParam(PreCrush, 1)
            model.setParam(Presolve, 0)
            model.setParam(Threads, 1)
            model.setParam(Cuts, 0)
            model.setParam(TimeLimit, tiempo_limite)
            model.setParam(Heuristics, heuristica)
            model.setParam(MIPFocus, 1)
            model.setParam(NumericFocus, 2)

    // 3. Chequeo rápido de conectividad; aborta si el grafo está fragmentado
    si no es_conexo(instancia.edges, instancia.num_nodes):
        respuesta_fallida -> Ans(..., answer=-1)
        escribir_csv(ruta_salida_csv, respuesta_fallida)
        retornar respuesta_fallida

    // 4. Definición de variables del MILP (O(n³) variables)
    x_vars <- model.addVars(m, GRB_BINARY)                     // activa aristas
    f_vars <- matriz n x (2m) de model.addVars(GRB_CONTINUOUS)  // flujo por raíz y arco dirigido
    y_vars <- matriz n x (2m) de model.addVars(GRB_BINARY)      // arcos dirigidos elegidos por raíz

    // 5. Construcción de tabla de búsqueda O(1) para aristas
    edge_lookup <- matriz n x n inicializada con -1
    para cada arista e en [0, m):
        edge_lookup[aristas[e].source][aristas[e].destination] <- e
        edge_lookup[aristas[e].destination][aristas[e].source] <- e

    // 6. Restricciones estructurales y objetivo
    add_structural_constraints(solver, x_vars, f_vars, y_vars, instancia)
        // 6.1. Restricción de árbol: ∑ x = n-1
        // 6.2. Restricciones de arborescencia: ∑ y_o = n-1 para cada origen o
        // 6.3. Restricciones de flujo: conservación de demanda por origen
        // 6.4. Restricciones de acoplamiento: f_o_ij + f_o_ji ≤ sumW * x_ij
        // 6.5. Restricción de flujo entrante al origen: ∑ f_o[i→o] = 0

    // 7. Solución inicial (MST + warm-start completo)
    set_initial_solution(solver, x_vars, f_vars, y_vars, instancia)
        // 7.1. Calcular MST usando Kruskal con DSU
        // 7.2. Construir estructura padre para cada origen usando BFS
        // 7.3. Marcar variables x basadas en MST
        // 7.4. Marcar variables y con orientación correcta (2*edge_idx o 2*edge_idx+1)
        // 7.5. Calcular y marcar variables f acumulando demanda en subárboles
        // 7.6. CRÍTICO: Marcar arborescencia para TODOS los orígenes (incluso sin demanda)

    // 8. Registro de callbacks de separación
    sec_callback <- new SECCallback(instancia, x_vars, edge_lookup, lazy_count, cutting_count)
        // MIPSOL: detecta ciclos en solución binaria y añade lazy SEC
        // MIPNODE: ejecuta corte mínimo sobre x fraccionario y añade cutting planes
    model.setCallback(sec_callback)

    // 9. Optimización
    model.optimize()

    // 10. Métricas adicionales
    estado <- model.get(GRB_IntAttr_Status)
    valor_objetivo <- (estado == GRB_OPTIMAL) ? model.get(GRB_DoubleAttr_ObjVal) : model.get(GRB_DoubleAttr_ObjBound)
    gap <- model.get(GRB_DoubleAttr_MIPGap)
    respuesta <- Ans(instancia.num_nodes, instancia.num_edges, r,
                     instancia.prob, valor_objetivo,
                     model.get(GRB_DoubleAttr_NodeCount), model.get(GRB_DoubleAttr_Runtime), gap,
                     lazy_count, cutting_count,
                     model.get(GRB_DoubleAttr_ObjBound), model.get(GRB_DoubleAttr_ObjBound))

    // 11. Persistencia
    escribir_csv(ruta_salida_csv, respuesta)
    retornar respuesta
```

## Detalles Críticos de Implementación

### Variables y Dimensiones:
- **x_vars**: m variables binarias (una por arista no dirigida)
- **f_vars**: n × (2m) variables continuas (flujo por origen y arco dirigido)
- **y_vars**: n × (2m) variables binarias (arco dirigido por origen)
- **Complejidad total**: O(n³) variables

### Restricciones Clave:
1. **Árbol**: ∑ x = n-1
2. **Arborescencia**: ∑ y_o = n-1 para cada origen o
3. **Conservación de flujo**: ∑ f_o[in] - ∑ f_o[out] = W[o][j] para cada origen o y nodo j ≠ o
4. **Flujo entrante al origen**: ∑ f_o[i→o] = 0 para cada origen o
5. **Acoplamiento**: f_o_ij + f_o_ji ≤ sumW * x_ij donde sumW = ∑ W[o][d]

### Warm-start Crítico:
- **MST**: Usar Kruskal con DSU para eficiencia O(m log m)
- **Orientación correcta**: Verificar edge.source y edge.destination para determinar 2*edge_idx o 2*edge_idx+1
- **Acumulación de flujo**: Calcular demanda total en subárboles usando BFS
- **TODOS los orígenes**: Marcar arborescencia incluso para orígenes sin demanda explícita

### Callback SEC:
- **Lazy constraints**: Detectar componentes en soluciones enteras
- **Cutting planes**: Corte mínimo sobre soluciones fraccionarias usando Edmonds-Karp
- **Optimización**: Solo evaluar 2(n-1) pares (rFixed, t) en lugar de O(n²)

## Rol Simplificado de las Clases Principales

```pseudo
class FlowBased : Solver:
    crea variables x, f, y;
    impone restricciones de árboles vía suma x=y=n-1;
    modela flujos múltiples (uno por posible raíz) con balance y límites;
    registra SECSep y retorna Ans con métricas extendidas.

class UtilSolver::SetInitialSolutionFlow:
    obtiene MST sobre costos;
    reconstruye caminos por BFS desde cada raíz;
    fija valores Start para x, f, y coherentes con esos caminos.

class SECSep : SepCallBack:
    usa soluciones actuales de x para detectar ciclos;
    addLazy en enteros garantiza conectividad;
    addCut en fraccionarios refuerza la relajación con cortes de subtour.

class OptimalStruct:
    encapsula datos de instancia, índices bidireccionales y matriz W de demandas.

class Solver:
    inicializa el entorno Gurobi con parámetros comunes (LAZY=1, Presolve=0,
    Threads=1, Cuts=0) y provee utilidades para bounds y gap.

class Ans:
    agrega estadísticas (objetivo, tiempo, nodos B&B, cortes, relajación) y
    serializa resultados en CSV.

class InstanceSolver:
    orquesta la lectura con ReaderOCST y selecciona la formulación vía Formulations.

class ReaderOCST:
    parsea archivos de instancia y construye OptimalStruct con preprocesos.

class Formulations:
    factory que expone FlowBased/PathBased/etc. según el identificador solicitado.
```
