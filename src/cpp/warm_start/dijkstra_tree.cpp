#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

struct EdgeRow {
    int u;
    int v;
    double cost;
};

struct RequirementRow {
    int origin;
    int destination;
    double weight;
};

struct InstanceData {
    int num_nodes = 0;
    std::vector<EdgeRow> edges;
    std::vector<RequirementRow> requirements;
};

double safe_double(const nlohmann::json& value, const std::string& context) {
    if (!value.is_number()) {
        throw std::runtime_error("Expected numeric value for " + context);
    }
    return value.get<double>();
}

InstanceData parse_instance(const fs::path& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Cannot open input file: " + path.string());
    }

    nlohmann::json json;
    in >> json;

    if (!json.contains("graph") || !json["graph"].is_object()) {
        throw std::runtime_error("Missing graph object in " + path.string());
    }
    const auto& g = json["graph"];
    if (!g.contains("nodes") || !g.contains("edges")) {
        throw std::runtime_error("Missing nodes/edges in graph for " + path.string());
    }

    InstanceData data;
    data.num_nodes = g["nodes"].get<int>();
    if (data.num_nodes <= 0) {
        throw std::runtime_error("Invalid node count in " + path.string());
    }

    const auto& edges_json = g["edges"];
    if (!edges_json.is_array()) {
        throw std::runtime_error("graph.edges is not an array in " + path.string());
    }
    data.edges.reserve(edges_json.size());
    for (const auto& e : edges_json) {
        if (!e.contains("source") || !e.contains("destination") || !e.contains("cost")) {
            throw std::runtime_error("Edge missing fields in " + path.string());
        }
        int u = e["source"].get<int>();
        int v = e["destination"].get<int>();
        double c = safe_double(e["cost"], "edge cost in " + path.string());

        if (u < 0 || u >= data.num_nodes || v < 0 || v >= data.num_nodes) {
            throw std::runtime_error("Edge endpoints out of range in " + path.string());
        }
        if (c < 0) {
            throw std::runtime_error("Negative edge cost in " + path.string());
        }
        data.edges.push_back({u, v, c});
    }

    if (!json.contains("requirements") || !json["requirements"].is_array()) {
        throw std::runtime_error("Missing requirements array in " + path.string());
    }
    const auto& reqs_json = json["requirements"];
    data.requirements.reserve(reqs_json.size());
    for (const auto& r : reqs_json) {
        if (!r.contains("origin") || !r.contains("destination") || !r.contains("weight")) {
            throw std::runtime_error("Requirement missing fields in " + path.string());
        }
        int o = r["origin"].get<int>();
        int d = r["destination"].get<int>();
        double w = safe_double(r["weight"], "requirement weight in " + path.string());

        if (o < 0 || o >= data.num_nodes || d < 0 || d >= data.num_nodes) {
            throw std::runtime_error("Requirement endpoints out of range in " + path.string());
        }
        if (w < 0) {
            throw std::runtime_error("Negative requirement weight in " + path.string());
        }
        data.requirements.push_back({o, d, w});
    }

    return data;
}

std::vector<std::vector<std::pair<int, double>>> build_adj_list(int n, const std::vector<EdgeRow>& edges) {
    std::vector<std::vector<std::pair<int, double>>> adj(n);
    for (const auto& e : edges) {
        adj[e.u].push_back({e.v, e.cost});
        adj[e.v].push_back({e.u, e.cost});
    }
    return adj;
}

std::vector<std::vector<double>> build_cost_matrix(int n, const std::vector<EdgeRow>& edges) {
    const double INF = std::numeric_limits<double>::infinity();
    std::vector<std::vector<double>> cost(n, std::vector<double>(n, INF));
    for (const auto& e : edges) {
        double current = cost[e.u][e.v];
        if (e.cost < current) {
            cost[e.u][e.v] = e.cost;
            cost[e.v][e.u] = e.cost;
        }
    }
    return cost;
}

std::vector<int> run_dijkstra(int source,
                             const std::vector<std::vector<std::pair<int, double>>>& adj,
                             std::vector<double>& dist_out) {
    const double INF = std::numeric_limits<double>::infinity();
    const int n = static_cast<int>(adj.size());

    dist_out.assign(n, INF);
    std::vector<int> parent(n, -1);
    std::priority_queue<std::pair<double, int>,
                        std::vector<std::pair<double, int>>,
                        std::greater<>> pq;

    dist_out[source] = 0.0;
    pq.push({0.0, source});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d != dist_out[u]) continue;

        for (const auto& [v, w] : adj[u]) {
            double nd = d + w;
            if (nd < dist_out[v]) {
                dist_out[v] = nd;
                parent[v] = u;
                pq.push({nd, v});
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        if (dist_out[i] == INF) {
            throw std::runtime_error("Graph is disconnected; Dijkstra tree impossible");
        }
    }
    return parent;
}

std::vector<std::pair<int, int>> build_tree_edges_from_parent(int root, const std::vector<int>& parent) {
    const int n = static_cast<int>(parent.size());
    std::vector<std::pair<int, int>> edges;
    edges.reserve(std::max(0, n - 1));
    for (int v = 0; v < n; ++v) {
        if (v == root) continue;
        if (parent[v] < 0) {
            throw std::runtime_error("Parent array missing entry for node " + std::to_string(v));
        }
        edges.emplace_back(v, parent[v]);
    }
    return edges;
}

struct LcaData {
    std::vector<std::vector<int>> up;
    std::vector<int> depth;
    std::vector<double> dist_root;
};

LcaData build_lca(int root,
                  const std::vector<std::vector<std::pair<int, double>>>& tree_adj) {
    const int n = static_cast<int>(tree_adj.size());
    int lg = 1;
    while ((1 << lg) <= n) ++lg;

    std::vector<std::vector<int>> up(lg, std::vector<int>(n, -1));
    std::vector<int> depth(n, 0);
    std::vector<double> dist_root(n, 0.0);

    std::vector<int> stack;
    stack.reserve(n);
    stack.push_back(root);
    up[0][root] = -1;
    depth[root] = 0;
    dist_root[root] = 0.0;

    while (!stack.empty()) {
        int u = stack.back();
        stack.pop_back();
        for (const auto& [v, w] : tree_adj[u]) {
            if (v == up[0][u]) continue;
            up[0][v] = u;
            depth[v] = depth[u] + 1;
            dist_root[v] = dist_root[u] + w;
            stack.push_back(v);
        }
    }

    for (int k = 1; k < lg; ++k) {
        for (int v = 0; v < n; ++v) {
            int mid = up[k - 1][v];
            up[k][v] = (mid == -1) ? -1 : up[k - 1][mid];
        }
    }

    return {std::move(up), std::move(depth), std::move(dist_root)};
}

int lca(int u, int v, const LcaData& lca_data) {
    int u_depth = lca_data.depth[u];
    int v_depth = lca_data.depth[v];
    int diff;

    if (u_depth < v_depth) {
        diff = v_depth - u_depth;
        for (int k = static_cast<int>(lca_data.up.size()) - 1; k >= 0; --k) {
            if (diff & (1 << k)) {
                v = lca_data.up[k][v];
            }
        }
    } else if (v_depth < u_depth) {
        diff = u_depth - v_depth;
        for (int k = static_cast<int>(lca_data.up.size()) - 1; k >= 0; --k) {
            if (diff & (1 << k)) {
                u = lca_data.up[k][u];
            }
        }
    }

    if (u == v) return u;

    for (int k = static_cast<int>(lca_data.up.size()) - 1; k >= 0; --k) {
        if (lca_data.up[k][u] != lca_data.up[k][v]) {
            u = lca_data.up[k][u];
            v = lca_data.up[k][v];
        }
    }
    return lca_data.up[0][u];
}

double compute_objective(int root,
                         const std::vector<std::pair<int, int>>& tree_edges,
                         const std::vector<RequirementRow>& requirements,
                         const std::vector<std::vector<double>>& cost_matrix) {
    const int n = static_cast<int>(cost_matrix.size());
    std::vector<std::vector<std::pair<int, double>>> tree_adj(n);
    tree_adj.reserve(n);
    for (const auto& [u, v] : tree_edges) {
        double c = cost_matrix[u][v];
        if (!std::isfinite(c)) {
            throw std::runtime_error("Tree edge not present in original graph");
        }
        tree_adj[u].push_back({v, c});
        tree_adj[v].push_back({u, c});
    }

    LcaData lca_data = build_lca(root, tree_adj);
    double objective = 0.0;
    for (const auto& r : requirements) {
        int ancestor = lca(r.origin, r.destination, lca_data);
        double path_cost = lca_data.dist_root[r.origin] +
                           lca_data.dist_root[r.destination] -
                           2.0 * lca_data.dist_root[ancestor];
        objective += r.weight * path_cost;
    }
    return objective;
}

void write_warm_start(const std::string& instance_name,
                      int num_nodes,
                      const std::vector<std::pair<int, int>>& edges,
                      const fs::path& output_dir) {
    fs::create_directories(output_dir);
    fs::path out_file = output_dir / (instance_name + ".txt");
    std::ofstream out(out_file);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open output file: " + out_file.string());
    }
    out << num_nodes << "\n";
    for (const auto& [u, v] : edges) {
        out << u << " " << v << "\n";
    }
}

void process_instance_file(const fs::path& path, const fs::path& output_dir) {
    InstanceData data = parse_instance(path);
    auto adj = build_adj_list(data.num_nodes, data.edges);
    auto cost_matrix = build_cost_matrix(data.num_nodes, data.edges);

    double best_objective = std::numeric_limits<double>::infinity();
    std::vector<std::pair<int, int>> best_tree;
    int best_root = -1;

    for (int root = 0; root < data.num_nodes; ++root) {
        std::vector<double> dist;
        std::vector<int> parent = run_dijkstra(root, adj, dist);
        auto tree_edges = build_tree_edges_from_parent(root, parent);
        double objective = compute_objective(root, tree_edges, data.requirements, cost_matrix);
        if (objective < best_objective) {
            best_objective = objective;
            best_tree = std::move(tree_edges);
            best_root = root;
        }
    }

    if (best_root == -1 || static_cast<int>(best_tree.size()) != data.num_nodes - 1) {
        throw std::runtime_error("Failed to build a feasible Dijkstra tree for " + path.string());
    }

    write_warm_start(path.stem().string(), data.num_nodes, best_tree, output_dir);
}

int main(int argc, char* argv[]) {
    // Usage: ./dijkstra_tree_warmstart [input_dir_json] [output_dir]
    // Defaults: input_dir_json="data/input", output_dir="data/input/dijkstra_tree"
    fs::path input_dir = "data/input";
    fs::path output_dir = "data/input/dijkstra_tree";
    if (argc >= 2) input_dir = argv[1];
    if (argc >= 3) output_dir = argv[2];

    if (!fs::exists(input_dir) || !fs::is_directory(input_dir)) {
        std::cerr << "Input directory not found: " << input_dir << std::endl;
        return 1;
    }

    int processed = 0;
    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        try {
            std::cout << "Processing " << entry.path().stem().string() << "..." << std::endl;
            process_instance_file(entry.path(), output_dir);
            processed++;
        } catch (const std::exception& e) {
            std::cerr << "Error processing " << entry.path() << ": " << e.what() << std::endl;
            return 1;  // strict abort on any error
        }
    }

    std::cout << "Completed. Instances processed: " << processed << std::endl;
    return 0;
}

/*
Build (manual example):
  g++ -std=c++17 -Ithird_party -o build/dijkstra_tree_warmstart src/cpp/warm_start/dijkstra_tree.cpp

Run (manual example):
  ./build/dijkstra_tree_warmstart data/input data/input/dijkstra_tree
*/
