/**
 * @file warm_start_loader.h
 * @brief Loader y validador de warm starts precomputados (Phase 2.0)
 *
 * Formato esperado: data/input/<idea>/<instancia>.txt
 * - Línea 1: n (número de nodos, debe coincidir con la instancia)
 * - Líneas 2..n: n-1 aristas como pares 0-based que forman un árbol
 *
 * Complejidad: O(n + m) en tiempo, O(n + m) en memoria.
 */

#ifndef OCST_COMMON_WARM_START_LOADER_H
#define OCST_COMMON_WARM_START_LOADER_H

#include <string>
#include <vector>

#include "common_types.h"

namespace ocst::common {

struct WarmStartTree {
    std::string idea;                                 // Nombre de la idea (ej. "mst")
    std::string path;                                 // Ruta al archivo cargado
    std::vector<std::pair<int, int>> tree_edges;      // Aristas del árbol (no dirigidas)
    std::vector<int> edge_indices;                    // Índices en instance.edges
};

/**
 * @brief Carga y valida un warm start desde disco.
 * @param instance Instancia OCST cargada.
 * @param idea Nombre de la idea (subdirectorio en data/input).
 * @param instance_name Nombre base de la instancia (sin extensión).
 * @throws std::runtime_error si falta el archivo o la estructura no es árbol válido.
 */
WarmStartTree load_warm_start_tree(const OCSTInstance& instance,
                                   const std::string& idea,
                                   const std::string& instance_name);

} // namespace ocst::common

#endif // OCST_COMMON_WARM_START_LOADER_H

