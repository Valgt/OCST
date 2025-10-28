# Pseudocódigo End-to-End FlowBasedRelaxed (Optimized)

## Overview

Este pseudocódigo describe la implementación **completa y optimizada** de la formulación Flow-Based Relaxed (RFB) para el problema OCST, incluyendo todas las correcciones de bugs y optimizaciones algorítmicas aplicadas.

---

## Main Algorithm

```pseudo
function resolver_instancia_flowbased_relaxed(ruta_archivo_entrada, ruta_salida_csv, tiempo_limite, heuristica):
    flujo_entrada <- abrir_archivo(ruta_archivo_entrada)

    // =========================================================================
    // 1. PARSEO DE INSTANCIA
    // =========================================================================
    instancia <- leer_instancia(flujo_entrada)
        leer n, m, probabilidad
        aristas <- recopilar m triples (u, v, costo)
        demandas <- recopilar r requerimientos (origen, destino, peso)
        
        // Construir estructura de datos optimizada
        instancia <- construir_OCSTInstance(n, aristas, demandas, probabilidad)
            instancia.edges <- aristas no dirigidas
            instancia.adjacency_matrix[i][j] <- índice de arista o -1 si no existe
            instancia.requirements <- lista de demandas
    
    cerrar(flujo_entrada)

    // =========================================================================
    // 2. INICIALIZACIÓN DEL SOLVER
    // =========================================================================
    env <- GRBEnv()
    env.setParam(OutputFlag, 0)
    
    model <- GRBModel(env)
    model.setParam(LazyConstraints, 1)    // Habilita lazy constraints
    model.setParam(PreCrush, 1)           // Mejora preprocessing
    model.setParam(Presolve, 0)           // Desactiva presolve para control
    model.setParam(Threads, 1)            // Single-threaded para reproducibilidad
    model.setParam(Cuts, 0)               // Desactiva cortes automáticos de Gurobi
    model.setParam(TimeLimit, tiempo_limite)
    model.setParam(Heuristics, heuristica)

    // =========================================================================
    // 3. CHEQUEO DE CONECTIVIDAD
    // =========================================================================
    si no es_conexo(instancia.edges, instancia.n):
        respuesta_fallida <- Ans(..., objective=-1)
        escribir_csv(ruta_salida_csv, respuesta_fallida)
        retornar respuesta_fallida

    // =========================================================================
    // 4. DEFINICIÓN DE VARIABLES
    // =========================================================================
    // CLAVE: Solo x y f (NO hay variables y de arborescencia)
    
    x <- vector de m variables binarias              // x[e] ∈ {0,1} para cada arista
    f <- matriz de n × 2m variables continuas        // f[o][arc] ≥ 0 para cada origen y arco
    
    // Compartir x entre ambas orientaciones de cada arista
    para cada arista e = {i,j}:
        x[índice(i,j)] <- misma variable que x[índice(j,i)]

    // =========================================================================
    // 5. RESTRICCIONES ESTRUCTURALES
    // =========================================================================
    
    // (4.37) Restricción de árbol: exactamente n-1 aristas
    model.addConstr(∑[e ∈ E] x[e] == n-1)
    
    // =========================================================================
    // 6. RESTRICCIONES DE FLUJO
    // =========================================================================
    
    // OPTIMIZACIÓN: Precalcular matriz de demandas una sola vez
    demand_matrix <- matriz n × n inicializada en 0
    para cada req en requirements:
        demand_matrix[req.origin][req.destination] += req.weight
    
    para cada origen o en [0, n):
        
        // (4.39) Conservación de flujo en cada nodo j ≠ o
        para cada nodo j en [0, n) \ {o}:
            flow_in <- ∑[arc: dest(arc)=j] f[o][arc]
            flow_out <- ∑[arc: source(arc)=j] f[o][arc]
            model.addConstr(flow_in - flow_out == demand_matrix[o][j])
        
        // (4.40) Flujo inicial desde el origen
        initial_flow <- ∑[arc: source(arc)=o] f[o][arc]
        total_demand <- ∑[d] demand_matrix[o][d]
        model.addConstr(initial_flow == total_demand)
        
        // CRÍTICO: Restricción adicional de flujo entrante al origen = 0
        // Esto previene ciclos en el origen
        inflow_to_root <- ∑[arc: dest(arc)=o] f[o][arc]
        model.addConstr(inflow_to_root == 0)

    // =========================================================================
    // 7. RESTRICCIONES DE ACOPLAMIENTO
    // =========================================================================
    
    // (4.41) Coupling: flujo acotado por selección de arista
    // CRÍTICO: Aplicar a TODOS los orígenes (incluso con demanda = 0)
    para cada origen o en [0, n):
        total_demand_o <- ∑[d] demand_matrix[o][d]
        
        para cada arista e = {i,j}:
            // Flujo en ambas direcciones acotado por x[e]
            model.addConstr(f[o][arc(i,j)] + f[o][arc(j,i)] ≤ total_demand_o * x[e])
            // Nota: Cuando total_demand_o = 0, esto fuerza f[o][...] = 0 (correcto!)

    // =========================================================================
    // 8. FUNCIÓN OBJETIVO
    // =========================================================================
    
    // (4.36) Minimizar costo esperado de comunicación
    objective <- 0
    para cada origen o en [0, n):
        para cada arco directed_arc en arcs:
            cost <- costo de la arista base
            objective += cost * f[o][directed_arc]
    
    model.setObjective(objective, GRB_MINIMIZE)

    // =========================================================================
    // 9. SOLUCIÓN INICIAL (WARM-START OPTIMIZADO)
    // =========================================================================
    
    // 9.1 Calcular MST usando Kruskal
    mst_edges <- calcular_MST_Kruskal(instancia)
    
    // 9.2 Configurar variables x
    para cada arista e:
        x[e].set(Start, 1.0 si e en mst_edges, 0.0 si no)
    
    // 9.3 Construir estructura de árbol por BFS desde cada raíz
    parent <- matriz n × n inicializada en -1
    parent_edge <- matriz n × n inicializada en -1
    
    para cada root en [0, n):
        visited <- array booleano de tamaño n, inicializado en false
        queue <- cola vacía
        
        queue.push(root)
        visited[root] <- true
        parent[root][root] <- root
        
        mientras queue no esté vacía:
            u <- queue.pop()
            para cada vecino v de u en el MST:
                si no visited[v]:
                    visited[v] <- true
                    parent[root][v] <- u
                    parent_edge[root][v] <- índice de arista(u,v)
                    queue.push(v)
    
    // 9.4 OPTIMIZACIÓN: Calcular flujos de subtree eficientemente
    // Complejidad: O(n²) en vez de O(n³)
    
    para cada root en [0, n):
        // Construir lista de hijos UNA VEZ por root
        children <- vector de n listas vacías
        para cada i en [0, n):
            si i ≠ root y parent[root][i] ≠ -1:
                children[parent[root][i]].append(i)
        
        // Calcular TODAS las demandas de subtree en UN SOLO DFS con caché
        subtree_demand <- array de n, inicializado en -1
        compute_all_subtree_demands(root, root, children, demand_matrix[root], subtree_demand)
        
        // Configurar variables de flujo usando valores cacheados
        para cada nodo j en [0, n):
            si j ≠ root y parent[root][j] ≠ -1:
                parent_j <- parent[root][j]
                edge_idx <- parent_edge[root][j]
                
                // Determinar orientación correcta del arco
                arc_index <- calcular_arc_index(edge_idx, parent_j, j)
                
                // Usar valor cacheado (O(1) en vez de O(n²))
                flow_value <- subtree_demand[j]
                f[root][arc_index].set(Start, flow_value)

    // =========================================================================
    // 10. REGISTRO DE CALLBACK
    // =========================================================================
    
    callback <- new SECCallback(x, instancia)
    model.setCallback(callback)

    // =========================================================================
    // 11. OPTIMIZACIÓN DEL MODELO
    // =========================================================================
    
    model.optimize()

    // =========================================================================
    // 12. EXTRACCIÓN DE RESULTADOS (CON MANEJO ROBUSTO)
    // =========================================================================
    
    gurobi_status <- model.get(Status)
    runtime <- model.get(Runtime)
    nodes_explored <- model.get(NodeCount)
    
    // CRÍTICO: Chequear SolCount antes de leer ObjVal
    si gurobi_status == GRB_OPTIMAL:
        is_optimal <- true
        objective_value <- model.get(ObjVal)
        mip_gap <- 0.0
    sino si gurobi_status == GRB_TIME_LIMIT:
        is_optimal <- false
        si model.get(SolCount) > 0:
            // Hay solución factible
            objective_value <- model.get(ObjVal)
            mip_gap <- calcular_gap(model)
        sino:
            // No hay solución factible
            objective_value <- -1.0
            mip_gap <- 100.0
    sino:
        is_optimal <- false
        objective_value <- -1.0
        mip_gap <- 100.0
    
    // Extraer solución si existe
    selected_edges <- []
    si objective_value > 0:
        para cada arista e:
            si x[e].get(X) > 0.5:
                selected_edges.append(e)
    
    // =========================================================================
    // 13. CONSTRUCCIÓN DE RESPUESTA
    // =========================================================================
    
    respuesta <- Ans(
        n = instancia.n,
        m = instancia.edges.size,
        r = instancia.requirements.size,
        prob = instancia.probability,
        objective = objective_value,
        nodes = nodes_explored,
        runtime = runtime,
        gap = mip_gap,
        lazy = callback.lazy_count(),
        cuts = callback.cuts_count(),
        relaxation = -1,  // No calculado
        lower_bound = model.get(ObjBound),
        upper_bound = objective_value,
        selected_edges = selected_edges
    )

    // =========================================================================
    // 14. SALIDA
    // =========================================================================
    
    escribir_csv(ruta_salida_csv, respuesta)
    retornar respuesta
```

---

## Función Auxiliar: compute_all_subtree_demands (OPTIMIZADA)

```pseudo
// OPTIMIZACIÓN CLAVE: Calcula TODAS las demandas de subtree en un solo DFS
// Complejidad: O(n) en vez de O(n²)

function compute_all_subtree_demands(root, node, children, demand_row, cache):
    // Usar memoización para evitar recálculos
    si cache[node] ≥ 0:
        retornar cache[node]
    
    total_demand <- 0.0
    
    // Demanda directa desde root a este nodo (lookup O(1))
    total_demand += demand_row[node]
    
    // DFS recursivo: acumular demandas de todos los descendientes
    para cada child en children[node]:
        compute_all_subtree_demands(root, child, children, demand_row, cache)
        total_demand += cache[child]
    
    // Cachear resultado
    cache[node] <- total_demand
    retornar total_demand
```

---

## Callback: SECCallback

```pseudo
class SECCallback extends GRBCallback:
    variables:
        x_vars          // Referencias a variables x
        instance        // Instancia OCST
        lazy_count      // Contador de lazy constraints
        cuts_count      // Contador de cutting planes

    // =========================================================================
    // CALLBACK PRINCIPAL
    // =========================================================================
    
    function callback():
        si where == GRB_CB_MIPSOL:
            // Solución entera encontrada
            add_lazy_constraints_integer()
        
        sino si where == GRB_CB_MIPNODE:
            // Nodo B&B con LP óptimo
            si model.get(NodeCount) == 0:  // Solo en nodo raíz
                add_fractional_cuts()

    // =========================================================================
    // LAZY CONSTRAINTS (MIPSOL)
    // =========================================================================
    
    function add_lazy_constraints_integer():
        // Extraer solución entera
        x_vals <- array de tamaño m
        para cada arista e:
            x_vals[e] <- getSolution(x_vars[e])
        
        // Encontrar componentes conexas usando DFS OPTIMIZADO
        component <- array de tamaño n, inicializado en -1
        num_components <- 0
        
        para cada nodo v:
            si component[v] == -1:
                dfs_component_optimized(v, num_components, component, x_vals)
                num_components++
        
        // Agregar SEC para cada componente con |S| > 1
        para cada comp_id en [0, num_components):
            component_nodes <- nodos con component[v] == comp_id
            
            si |component_nodes| > 1:
                add_sec_lazy(component_nodes)

    // =========================================================================
    // DFS OPTIMIZADO (O(m) en vez de O(n·m))
    // =========================================================================
    
    function dfs_component_optimized(v, comp_id, component, x_vals):
        component[v] <- comp_id
        
        // OPTIMIZACIÓN: Usar adjacency_matrix para lookup directo
        // En vez de iterar TODAS las aristas, solo checkear vecinos
        para cada neighbor en [0, n):
            si neighbor == v:
                continuar
            
            edge_idx <- instance.adjacency_matrix[v][neighbor]
            si edge_idx ≠ -1 y component[neighbor] == -1 y x_vals[edge_idx] > 0.5:
                dfs_component_optimized(neighbor, comp_id, component, x_vals)

    // =========================================================================
    // SEC LAZY CONSTRAINT OPTIMIZADA (O(m) en vez de O(n·m))
    // =========================================================================
    
    function add_sec_lazy(component_nodes):
        cut_expr <- 0
        
        // OPTIMIZACIÓN: Bool vector para chequeo O(1)
        in_component <- array booleano de tamaño n, inicializado en false
        para cada v en component_nodes:
            in_component[v] <- true
        
        // OPTIMIZACIÓN: Iterar solo aristas incidentes a componente
        para cada u en component_nodes:
            para cada v en [0, n):
                si u ≥ v:
                    continuar  // Evitar contar dos veces
                
                edge_idx <- instance.adjacency_matrix[u][v]
                si edge_idx ≠ -1 y in_component[v]:
                    cut_expr += x_vars[edge_idx]
        
        // (4.38) SEC: ∑(x en S) ≤ |S|-1
        addLazy(cut_expr ≤ |component_nodes| - 1)
        lazy_count++

    // =========================================================================
    // FRACTIONAL CUTS (MIPNODE)
    // =========================================================================
    
    function add_fractional_cuts():
        intentar:
            // Extraer solución fraccionaria del LP
            sol <- matriz n × n de solución LP actual
            
            para cada arista e con source=i, dest=j:
                x_val <- getNodeRel(x_vars[e])
                sol[i][j] <- x_val
                sol[j][i] <- x_val
            
            // Construir redes de flujo para CSI y SEC
            csi_capacities <- capacidades basadas en sol[i][j]
            csi_network <- Network(n, csi_capacities)
            
            cuts_this_round <- 0  // Contador local (no límite global!)
            
            // CRÍTICO: Evaluar TODOS los pares (s,t) sin early return
            // Procesamos 2(n-1) pares: con raíces fijas en 0 y n-1
            para cada rConfig en [0, 1]:
                rFixed <- si rConfig==0 entonces 0 sino n-1
                
                para cada t en [0, n):
                    si t == rFixed:
                        continuar
                    
                    // === CORTES CSI (Conectividad) ===
                    maxFlowCSI <- csi_network.MaxFlow(rFixed, t)
                    si maxFlowCSI < 0.99:  // Violación de conectividad
                        cut_arcs <- csi_network.GetMinCutArcs(rFixed, t)
                        add_csi_cut(cut_arcs)
                        cuts_this_round++
                    
                    // === CORTES SEC (Subtour) ===
                    sec_capacities <- construir_red_SEC_con_arcos_auxiliares(sol, n)
                    sec_network <- Network(n, sec_capacities)
                    minCutSEC <- sec_network.MaxFlow(rFixed, t)
                    
                    si minCutSEC < n - 0.01:  // Violación SEC
                        S <- sec_network.GetMinCutVertices(rFixed, t)
                        add_sec_cut(S)
                        cuts_this_round++
                    
                    // NO HAY EARLY RETURN!
                    // Procesar TODOS los pares para mantener consistencia LP
            
        capturar GRBException:
            // getNodeRel puede fallar en algunos contextos, ignorar
            retornar

    // =========================================================================
    // CSI CUT
    // =========================================================================
    
    function add_csi_cut(cut_arcs):
        cut_expr <- 0
        para cada arc_idx en cut_arcs:
            edge_idx <- mapear arc a edge
            cut_expr += x_vars[edge_idx]
        
        si cut_expr.size > 0:
            addCut(cut_expr ≥ 1.0)
            cuts_count++

    // =========================================================================
    // SEC CUT (para solución fraccionaria)
    // =========================================================================
    
    function add_sec_cut(S):
        cut_expr <- 0
        
        // Similar a SEC lazy pero para solución fraccionaria
        in_S <- bool array
        para cada v en S:
            in_S[v] <- true
        
        para cada u en S:
            para cada v en [0, n):
                si u ≥ v:
                    continuar
                edge_idx <- instance.adjacency_matrix[u][v]
                si edge_idx ≠ -1 y in_S[v]:
                    cut_expr += x_vars[edge_idx]
        
        si cut_expr.size > 0:
            addCut(cut_expr ≤ |S| - 1)
            cuts_count++
```

---

## Roles Simplificados de Clases

### OCSTInstance
- Almacena n (nodos), edges (aristas), requirements (demandas)
- **Optimización:** `adjacency_matrix[i][j]` para lookup O(1) de aristas

### SECCallback
- Hereda de `GRBCallback`
- Intercepta eventos MIPSOL (solución entera) y MIPNODE (LP óptimo)
- **Lazy constraints:** Separa SEC para soluciones enteras
- **Cutting planes:** Separa CSI + SEC para soluciones fraccionarias
- **Optimización:** DFS O(m), lazy O(m), sin early return en fractional cuts

### FlowBasedRelaxedSolver
- Maneja el ciclo completo de optimización
- Crea variables, agrega restricciones, configura objetivo
- **Warm-start optimizado:** O(n) por root con caching
- **Extracción robusta:** Chequea SolCount antes de ObjVal

### Network (Max-Flow/Min-Cut)
- Implementa Edmonds-Karp para max-flow
- Método `GetMinCutArcs()` para CSI
- Método `GetMinCutVertices()` para SEC
- Usado en callback fraccionario

---

## Complejidades Finales

| Operación | Complejidad | Notas |
|-----------|-------------|-------|
| Parseo | O(m + r) | Lineal en entrada |
| Crear variables | O(nm) | n orígenes × 2m arcos |
| Crear restricciones | O(nm + m) | Flujo + coupling |
| Warm-start (total) | O(r + n²) | Con optimización |
| DFS component | O(m) | Con adjacency matrix |
| Lazy SEC | O(m) | Por componente |
| Fractional cuts | O(n² · flow) | Por invocación, sin límite |

---

## Cambios vs Pseudocódigo Inicial

1. ✅ **Agregado chequeo SolCount** antes de leer ObjVal
2. ✅ **Removido early return** en fractional cuts (MAX_CUTS_PER_ROUND)
3. ✅ **Optimizado DFS** component con adjacency_matrix
4. ✅ **Optimizado lazy constraints** con bool vector
5. ✅ **Optimizado warm-start** con compute_all_subtree_demands cacheado
6. ✅ **Agregado constraint** inflow_to_root == 0
7. ✅ **Aclarado** que (4.41) aplica a TODOS los orígenes

---

## Notas de Implementación

### Variables Compartidas
Las variables x deben compartirse entre ambas orientaciones de cada arista no dirigida. En Gurobi esto se hace compartiendo la misma referencia de variable.

### Arcos Dirigidos
Cada arista no dirigida {i,j} genera dos arcos dirigidos:
- arc(i→j) con índice 2*e
- arc(j→i) con índice 2*e+1

### Constraint (4.41) para Demanda Cero
Cuando total_demand_o = 0, la restricción (4.41) se convierte en:
```
f[o][ij] + f[o][ji] ≤ 0
```
Como f ≥ 0, esto fuerza f[o][ij] = f[o][ji] = 0, que es correcto.

### Callback sin Límite de Cortes
Es **crucial** procesar todos los pares (s,t) en fractional cuts sin early return. Dejar pares sin evaluar crea inconsistencias en la relajación LP que pueden llevar a reportar soluciones subóptimas como óptimas.

---

*Pseudocódigo actualizado: October 28, 2025*  
*Versión: 1.0 - Optimized and Validated*
