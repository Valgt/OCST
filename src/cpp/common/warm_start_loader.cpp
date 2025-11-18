#include "warm_start_loader.h"

#include <filesystem>
#include <fstream>
#include <queue>
#include <stdexcept>
#include <unordered_set>

namespace ocst::common {

WarmStartTree load_warm_start_tree(const OCSTInstance& instance,
                                   const std::string& idea,
                                   const std::string& instance_name) {
    std::filesystem::path path = std::filesystem::path("data") / "input" / idea / (instance_name + ".txt");
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Warm start file not found: " + path.string());
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open warm start file: " + path.string());
    }

    int n;
    if (!(file >> n)) {
        throw std::runtime_error("Invalid warm start file header (n) in " + path.string());
    }
    if (n != instance.num_nodes) {
        throw std::runtime_error("Warm start node count mismatch in " + path.string());
    }

    std::vector<std::pair<int, int>> edges_raw;
    edges_raw.reserve(std::max(0, n - 1));
    std::unordered_set<std::string> seen;

    for (int i = 0; i < n - 1; ++i) {
        int u, v;
        if (!(file >> u >> v)) {
            throw std::runtime_error("Invalid edge format at line " + std::to_string(i + 2) + " in " + path.string());
        }
        if (u < 0 || u >= n || v < 0 || v >= n) {
            throw std::runtime_error("Edge endpoints out of range in " + path.string());
        }
        int a = std::min(u, v);
        int b = std::max(u, v);
        std::string key = std::to_string(a) + "-" + std::to_string(b);
        if (seen.count(key)) {
            throw std::runtime_error("Duplicate edge in warm start file: " + path.string());
        }
        seen.insert(key);
        edges_raw.emplace_back(u, v);
    }

    // Validar conectividad (BFS)
    std::vector<std::vector<int>> adj(n);
    for (const auto& e : edges_raw) {
        adj[e.first].push_back(e.second);
        adj[e.second].push_back(e.first);
    }

    std::vector<bool> visited(n, false);
    std::queue<int> q;
    q.push(0);
    visited[0] = true;
    int visited_count = 1;
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (int v : adj[u]) {
            if (!visited[v]) {
                visited[v] = true;
                visited_count++;
                q.push(v);
            }
        }
    }
    if (visited_count != n) {
        throw std::runtime_error("Warm start is not connected: " + path.string());
    }

    WarmStartTree data;
    data.idea = idea;
    data.path = path.string();
    data.tree_edges = edges_raw;
    data.edge_indices.reserve(edges_raw.size());

    for (const auto& e : edges_raw) {
        int idx = instance.get_edge_index(e.first, e.second);
        if (idx < 0) {
            throw std::runtime_error("Warm start edge not in graph: " + std::to_string(e.first) + "," + std::to_string(e.second));
        }
        data.edge_indices.push_back(idx);
    }

    return data;
}

} // namespace ocst::common

