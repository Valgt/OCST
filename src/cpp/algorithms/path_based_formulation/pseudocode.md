# Algorithm Pseudocode - Path-Based Formulation (Corrected Version)

## Main Algorithm

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
            instancia.edge_lookup <- tabla O(1) para búsqueda de aristas

    cerrar(flujo_entrada)

    // 2. Preparación del solver y parámetros globales
    solver <- new PathBasedSolver(instancia)
        solver.env <- GRBEnv()
            env.setParam(OutputFlag, 1)            // logging habilitado para debugging
        solver.model <- GRBModel(env)
            model.setParam(LazyConstraints, 1)     // autoriza restricciones en callbacks
            model.setParam(PreCrush, 1)            // asegura que las lazy se apliquen sin presolve previo
            model.setParam(Presolve, 0)            // mantiene el modelo intacto para la separación
            model.setParam(Threads, 1)             // reproducibilidad
            model.setParam(Cuts, 0)                // prioriza cortes personalizados
            model.setParam(MIPFocus, 1)            // enfoque en encontrar soluciones factibles
            model.setParam(NumericFocus, 2)        // máxima precisión numérica
            model.setParam(MIPGap, 1e-06)          // gap absoluto muy pequeño
            model.setParam(MIPGapAbs, 1e-06)       // gap relativo muy pequeño
            model.setParam(TimeLimit, tiempo_limite)
            model.setParam(Heuristics, heuristica)

    // 3. Chequeo rápido de conectividad; aborta si el grafo está fragmentado
    si no es_conexo(instancia.edges, instancia.n):
        respuesta_fallida -> Ans(..., answer=-1)
        escribir_csv(ruta_salida_csv, respuesta_fallida)
        retornar respuesta_fallida

    // 4. Definición de variables del MILP
    x_vars_ <- model.addVars(m, BINARIO)            // presencia de aristas
    y_vars_ <- vector<vector<GRBVar>>(r)           // flujo unitario por requerimiento
    para cada requerimiento k en [0, r):
        y_vars_[k] <- model.addVars(2*m, CONTINUO) // flujo en ambas direcciones

    // 5. Restricciones estructurales y objetivo
    add_structural_constraints(model, x_vars_, y_vars_, instancia)
        // Restricción (4.18): ∑ x_ij = n-1
        model.addConstr(sum(x_vars_) == n-1)
        
        // Restricciones de flujo (4.19-4.21): flujo unitario por requerimiento
        para cada requerimiento k:
            req <- demandas[k]
            // Flujo entrante al destino = 1
            model.addConstr(sum(y_vars_[k][arcos_entrantes(req.destino)]) == 1)
            // Flujo saliente del origen = 1  
            model.addConstr(sum(y_vars_[k][arcos_salientes(req.origen)]) == 1)
            // Conservación de flujo en nodos intermedios
            para cada nodo j != req.origen y j != req.destino:
                model.addConstr(sum(y_vars_[k][arcos_entrantes(j)]) == sum(y_vars_[k][arcos_salientes(j)]))
        
        // Restricciones de acoplamiento (4.22): y_ij^r + y_ji^r <= x_ij
        para cada arista (i,j) en aristas:
            edge_idx <- instancia.get_edge_index(i, j)
            para cada requerimiento k:
                model.addConstr(y_vars_[k][2*edge_idx] + y_vars_[k][2*edge_idx+1] <= x_vars_[edge_idx])

    definir_objetivo(model, x_vars_, y_vars_, instancia.weights)

    // 6. Solución inicial (MST warm-start)
    set_initial_solution(model, x_vars_, y_vars_, instancia)

    // 7. Registro de callbacks de separación SEC
    callback_sec <- new SECCallback(instancia, x_vars_)
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
                     callback_sec.lazy_constraints_count_,
                     callback_sec.cutting_planes_count_,
                     resolver_relajacion(model),
                     Solver.GetLower(model), Solver.GetUpper(model))

    // 10. Persistencia del resultado
    escribir_csv(ruta_salida_csv, respuesta)
    escribir_solucion_completa(ruta_salida_csv, x_vars_, y_vars_, instancia)
    retornar respuesta
```

## SEC Callback Implementation

```pseudo
class SECCallback extends GRBCallback:
    variables:
        instance_: OptimalStruct
        x_vars_: vector<GRBVar>
        lazy_constraints_count_: int
        cutting_planes_count_: int

    function callback():
        donde <- get(GRB_CB_WHERE)
        
        si donde == GRB_CB_MIPSOL:
            // Solución entera: detectar ciclos y componentes desconectadas
            x_sol <- getSolution(x_vars_)
            add_lazy_constraints(x_sol)
            
        si donde == GRB_CB_MIPNODE:
            // Nodo fraccionario: separación fraccionaria
            estado_nodo <- getIntInfo(GRB_CB_MIPNODE_STATUS)
            si estado_nodo == GRB_OPTIMAL:
                add_fractional_cuts()

    function add_lazy_constraints(x_sol):
        // 1. Detectar ciclos usando DFS
        ciclo <- find_cycle(x_sol)
        si ciclo.size() >= 3:
            si verify_cycle_exists(ciclo, x_sol):
                add_cycle_constraint(ciclo)
                lazy_constraints_count_++
                retornar
        
        // 2. Detectar componentes desconectadas
        componentes <- detectar_componentes_conexas(x_sol)
        para cada componente en componentes:
            si componente.size() < instance_.num_nodes:
                add_connectivity_constraint(componente)
                lazy_constraints_count_++

    function find_cycle(x_sol):
        // Construir lista de adyacencia con aristas activas
        adj <- vector<vector<int>>(instance_.num_nodes)
        para cada arista e en [0, m):
            si x_sol[e] > 0.5:
                edge <- instance_.edges[e]
                adj[edge.source].push_back(edge.destination)
                adj[edge.destination].push_back(edge.source)
        
        // DFS con stack para detectar back edges
        visited <- vector<bool>(instance_.num_nodes, false)
        parent <- vector<int>(instance_.num_nodes, -1)
        
        para cada nodo start en [0, n):
            si not visited[start]:
                stack <- Stack<int>()
                stack.push(start)
                visited[start] <- true
                
                mientras stack no esté vacío:
                    u <- stack.pop()
                    para cada v en adj[u]:
                        si not visited[v]:
                            visited[v] <- true
                            parent[v] <- u
                            stack.push(v)
                        sino si parent[u] != v:
                            // Back edge detectado: reconstruir ciclo usando LCA
                            ciclo <- reconstruct_cycle(u, v, parent)
                            si ciclo.size() >= 3:
                                retornar ciclo
        
        retornar vector<int>() // vacío si no hay ciclo

    function reconstruct_cycle(u, v, parent):
        // Trazar ancestros de u y v hasta la raíz
        pathU <- vector<int>()
        pathV <- vector<int>()
        
        // Construir camino de u a raíz
        cur <- u
        mientras cur != -1:
            pathU.push_back(cur)
            cur <- parent[cur]
        
        // Construir camino de v a raíz
        cur <- v
        mientras cur != -1:
            pathV.push_back(cur)
            cur <- parent[cur]
        
        // Encontrar ancestro común más bajo (LCA)
        pathUSet <- Set<int>(pathU.begin(), pathU.end())
        lca <- -1
        para cada nodo en pathV:
            si nodo en pathUSet:
                lca <- nodo
                break
        
        si lca == -1:
            retornar vector<int>() // sin ancestro común
        
        // Reconstruir ciclo: u → ... → LCA → ... → v → u
        ciclo <- vector<int>()
        
        // Camino de u a LCA
        cur <- u
        mientras cur != -1:
            ciclo.push_back(cur)
            si cur == lca:
                break
            cur <- parent[cur]
        
        // Camino de v a LCA (en reversa, excluyendo LCA)
        pathVToLCA <- vector<int>()
        cur <- v
        mientras cur != -1:
            pathVToLCA.push_back(cur)
            si cur == lca:
                break
            cur <- parent[cur]
        
        // Agregar camino de v a LCA en reversa (excluyendo LCA)
        para i desde pathVToLCA.size()-2 hasta 0:
            ciclo.push_back(pathVToLCA[i])
        
        retornar ciclo

    function add_cycle_constraint(ciclo):
        cut_expr <- GRBLinExpr()
        para i desde 0 hasta ciclo.size()-1:
            u <- ciclo[i]
            v <- ciclo[(i+1) % ciclo.size()]
            edge_idx <- instance_.get_edge_index(u, v)
            si edge_idx >= 0:
                cut_expr += x_vars_[edge_idx]
        
        si cut_expr.size() > 0:
            addLazy(cut_expr <= ciclo.size() - 1.0)

    function add_connectivity_constraint(componente):
        cut_expr <- GRBLinExpr()
        in_component <- vector<bool>(instance_.num_nodes, false)
        para cada nodo en componente:
            in_component[nodo] <- true
        
        // Sumar aristas que cruzan la frontera del componente
        para cada u en componente:
            para cada v en [0, n):
                si not in_component[v] y instance_.has_edge(u, v):
                    edge_idx <- instance_.get_edge_index(u, v)
                    si edge_idx >= 0:
                        cut_expr += x_vars_[edge_idx]
        
        si cut_expr.size() > 0:
            addLazy(cut_expr >= 1.0)

    function add_fractional_cuts():
        // Obtener solución fraccionaria
        x_sol <- getNodeRel(x_vars_)
        
        // Construir red residual con capacidades fraccionarias
        maxflow <- MaxFlowMinCut(instance_.num_nodes)
        para cada arista e en [0, m):
            si x_sol[e] > 1e-9:
                edge <- instance_.edges[e]
                maxflow.add_edge(edge.source, edge.destination, x_sol[e])
                maxflow.add_edge(edge.destination, edge.source, x_sol[e])
        
        // Buscar cortes violados usando max-flow min-cut
        para cada s en [0, n):
            para cada t en [s+1, n):
                max_flow_value <- maxflow.max_flow(s, t)
                si max_flow_value < 1.0 - 1e-9:
                    // Corte violado encontrado
                    cut <- maxflow.get_min_cut(s)
                    add_connectivity_constraint(cut)
                    cutting_planes_count_++
                    break // Solo un corte por nodo para eficiencia
```

## MaxFlowMinCut Implementation

```pseudo
class MaxFlowMinCut:
    variables:
        num_nodes_: int
        capacity_: vector<vector<double>>
        residual_: vector<vector<double>>

    function add_edge(u, v, cap):
        capacity_[u][v] <- cap
        residual_[u][v] <- cap

    function max_flow(source, sink):
        max_flow_value <- 0.0
        
        mientras true:
            parent <- vector<int>(num_nodes_, -1)
            queue <- Queue<int>()
            queue.push(source)
            parent[source] <- source
            
            // BFS para encontrar camino aumentante
            mientras queue no esté vacío y parent[sink] == -1:
                u <- queue.front()
                queue.pop()
                
                para cada v en [0, num_nodes_):
                    si parent[v] == -1 y residual_[u][v] > 1e-9:
                        parent[v] <- u
                        queue.push(v)
            
            si parent[sink] == -1:
                break // No hay más caminos aumentantes
            
            // Encontrar capacidad mínima del camino
            bottleneck <- INFINITY
            v <- sink
            mientras v != source:
                u <- parent[v]
                bottleneck <- min(bottleneck, residual_[u][v])
                v <- u
            
            // Actualizar capacidades residuales
            v <- sink
            mientras v != source:
                u <- parent[v]
                residual_[u][v] -= bottleneck
                residual_[v][u] += bottleneck
                v <- u
            
            max_flow_value += bottleneck
        
        retornar max_flow_value

    function get_min_cut(source):
        visited <- vector<bool>(num_nodes_, false)
        queue <- Queue<int>()
        queue.push(source)
        visited[source] <- true
        
        mientras queue no esté vacío:
            u <- queue.front()
            queue.pop()
            
            para cada v en [0, num_nodes_):
                si not visited[v] y residual_[u][v] > 1e-9:
                    visited[v] <- true
                    queue.push(v)
        
        cut <- vector<int>()
        para cada i en [0, num_nodes_):
            si visited[i]:
                cut.push_back(i)
        
        retornar cut
```

## Warm-start Implementation

```pseudo
function set_initial_solution(model, x_vars_, y_vars_, instancia):
    // Generar MST usando algoritmo de Kruskal
    mst_edges <- find_mst(instancia)
    
    // Establecer variables x basadas en MST
    para cada edge_idx en [0, instancia.num_edges):
        edge <- instancia.edges[edge_idx]
        si edge en mst_edges:
            x_vars_[edge_idx].set(GRB_DoubleAttr_Start, 1.0)
        sino:
            x_vars_[edge_idx].set(GRB_DoubleAttr_Start, 0.0)
    
    // Establecer variables y basadas en caminos MST
    para cada requerimiento k en [0, instancia.num_requirements):
        req <- instancia.requirements[k]
        path <- find_path_in_mst(req.origin, req.destination, mst_edges, instancia)
        
        // Marcar flujo unitario a lo largo del camino
        para cada edge en path:
            edge_idx <- instancia.get_edge_index(edge.source, edge.destination)
            si edge.source == edge.source: // dirección original
                y_vars_[k][2*edge_idx].set(GRB_DoubleAttr_Start, 1.0)
            sino: // dirección reversa
                y_vars_[k][2*edge_idx+1].set(GRB_DoubleAttr_Start, 1.0)

function find_mst(instancia):
    // Algoritmo de Kruskal con DSU
    edges_sorted <- ordenar_aristas_por_costo(instancia.edges)
    dsu <- DisjointSetUnion(instancia.num_nodes)
    mst_edges <- vector<Edge>()
    
    para cada edge en edges_sorted:
        si dsu.find(edge.source) != dsu.find(edge.destination):
            dsu.union(edge.source, edge.destination)
            mst_edges.push_back(edge)
            si mst_edges.size() == instancia.num_nodes - 1:
                break
    
    retornar mst_edges
```