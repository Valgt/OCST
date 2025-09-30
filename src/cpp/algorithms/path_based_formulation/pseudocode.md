# Algorithm Pseudocode


```pseudo
function resolver_instancia_pathbased(ruta_archivo_entrada, ruta_salida_csv, tiempo_limite, heuristica):
    flujo_entrada <- abrir_archivo(ruta_archivo_entrada)

    // 1. Parseo de instancia (estructura base del problema)
    instancia <- leer_instancia(flujo_entrada)
        leer n, m, probabilidad
        aristas <- recopilar m triples (u, v, costo)
        demandas <- recopilar r requerimientos (origen, destino, peso)
        instancia <- construir_OptimalStruct(n, aristas, demandas, probabilidad)
            instancia.arcs <- desdoblar aristas en direcciones
            instancia.indexes <- mapear (i,j) -> índice en arcs
            instancia.weights <- matriz de pesos acumulados por requerimiento
            instancia.min_dist <- distancias mínimas (Floyd–Warshall)
            instancia.lower_bound_ocstp <- cota base para informes

    cerrar(flujo_entrada)

    // 2. Preparación del solver y parámetros globales
    solver <- new PathBased(instancia)
        solver.env <- GRBEnv()
            env.setParam(OutputFlag, 0)            // sin ruido en consola
        solver.model <- GRBModel(env)
            model.setParam(LazyConstraints, 1)     // autoriza restricciones en callbacks
            model.setParam(PreCrush, 1)            // asegura que las lazy se apliquen sin presolve previo
            model.setParam(Presolve, 0)            // mantiene el modelo intacto para la separación
            model.setParam(Threads, 1)             // reproducibilidad
            model.setParam(Cuts, 0)                // prioriza cortes personalizados
            model.setParam(TimeLimit, tiempo_limite)
            model.setParam(Heuristics, heuristica)

    // 3. Chequeo rápido de conectividad; aborta si el grafo está fragmentado
    si no es_conexo(instancia.edges, instancia.n):
        respuesta_fallida -> Ans(..., answer=-1)
        escribir_csv(ruta_salida_csv, respuesta_fallida)
        retornar respuesta_fallida

    // 4. Definición de variables del MILP
    x <- model.addVars(m, BINARIO)                   // presencia de aristas
    para cada requerimiento k en [0, r):
        y[k] <- model.addVars(m, CONTINUO)           // flujo unitario entre origen y destino k

    // 5. Restricciones estructurales y objetivo
    imponer_suma_aristas(model, x, n-1)
    para cada requerimiento k:
        balancear_flujo(model, y[k], demandas[k], instancia.indexes)
    for arco (i,j):
        model.addConstr( y[:, (i,j)] <= x[indice(i,j)] ) // el flujo solo usa aristas activas
    definir_objetivo(model, x, instancia.weights)        // costo esperado del árbol

    // 6. Solución inicial opcional (mejor bound y arranque más rápido)
    set_initial_solution(model, x, y, heuristicas_disponibles(instancia))

    // 7. Registro de callbacks de separación
    callback_sec <- new SECSep(x, instancia)
        // En soluciones enteras (GRB_CB_MIPSOL):
        //   - Detecta ciclos con SECInteger::FindCycle y usa addLazy(∑ ciclo x ≤ |S|-1).
        //     => Lazy constraints solo se evalúan en candidatas 0/1 y garantizan árboles.
        // En relajaciones fraccionarias (GRB_CB_MIPNODE óptimo):
        //   - Construye grafo residual con valores x fraccionarios.
        //   - Ejecuta max-flow entre pares representativos; si min-cut < 1, añade addCut(∑ δ(S) x ≥ 1).
        //     => Los cortes fortalecen el LP global, mejoran cotas y reducen el árbol B&B.
        //   - Repite con red normalizada para SEC fraccionarias.
    model.setCallback(callback_sec)

    // 8. Optimización (branch-and-cut interno de Gurobi)
    model.optimize()

    // 9. Extracción de métricas
    estado <- model.status
    valor_objetivo <- (estado == OPTIMO) ? model.objVal : model.bestObjBound
    gap <- calcular_gap(model)
    respuesta <- Ans(instancia.n, instancia.arcs.tamaño, r,
                     instancia.prob, valor_objetivo,
                     model.getNodeCount(), model.getRuntime(), gap,
                     callback_sec.lazy(), callback_sec.cuts(),
                     resolver_relajacion(model),
                     Solver.GetLower(model), Solver.GetUpper(model))

    // 10. Persistencia del resultado
    escribir_csv(ruta_salida_csv, respuesta)
    retornar respuesta
```