#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

// -------------------------- Data structures -------------------------------

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

// -------------------------- Parsing helpers -------------------------------

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

// -------------------------- Dinic max-flow (double) -----------------------

struct Dinic {
    struct Edge {
        int to;
        int rev;
        double cap;
    };

    int n;
    std::vector<std::vector<Edge>> g;
    std::vector<int> level, it;

    explicit Dinic(int n_) : n(n_), g(n_), level(n_), it(n_) {}

    void add_edge(int u, int v, double cap) {
        Edge a{v, static_cast<int>(g[v].size()), cap};
        Edge b{u, static_cast<int>(g[u].size()), 0.0};
        g[u].push_back(a);
        g[v].push_back(b);
    }

    bool bfs(int s, int t) {
        std::fill(level.begin(), level.end(), -1);
        std::queue<int> q;
        level[s] = 0;
        q.push(s);
        while (!q.empty()) {
            int v = q.front();
            q.pop();
            for (const auto& e : g[v]) {
                if (e.cap > 1e-12 && level[e.to] == -1) {
                    level[e.to] = level[v] + 1;
                    q.push(e.to);
                }
            }
        }
        return level[t] != -1;
    }

    double dfs(int v, int t, double f) {
        if (v == t) return f;
        for (int& i = it[v]; i < static_cast<int>(g[v].size()); ++i) {
            Edge& e = g[v][i];
            if (e.cap > 1e-12 && level[e.to] == level[v] + 1) {
                double ret = dfs(e.to, t, std::min(f, e.cap));
                if (ret > 1e-12) {
                    e.cap -= ret;
                    g[e.to][e.rev].cap += ret;
                    return ret;
                }
            }
        }
        return 0.0;
    }

    double max_flow(int s, int t) {
        double flow = 0.0;
        while (bfs(s, t)) {
            std::fill(it.begin(), it.end(), 0);
            while (true) {
                double pushed = dfs(s, t, std::numeric_limits<double>::infinity());
                if (pushed <= 1e-12) break;
                flow += pushed;
            }
        }
        return flow;
    }

    std::vector<int> min_cut_reachable(int s) {
        std::vector<int> vis(n, 0);
        std::queue<int> q;
        q.push(s);
        vis[s] = 1;
        while (!q.empty()) {
            int v = q.front();
            q.pop();
            for (const auto& e : g[v]) {
                if (e.cap > 1e-12 && !vis[e.to]) {
                    vis[e.to] = 1;
                    q.push(e.to);
                }
            }
        }
        return vis;
    }
};

// -------------------------- Gomory-Hu tree -------------------------------

struct GHResult {
    std::vector<int> parent;
    std::vector<double> cut_value;
    std::vector<std::vector<int>> cut_side;  // cut_side[v][i] == 1 if i is on v's side of the min-cut
};

GHResult gomory_hu(const std::vector<std::vector<double>>& cost_matrix) {
    const int n = static_cast<int>(cost_matrix.size());
    if (n == 0) return {{}, {}, {}};

    std::vector<int> parent(n, 0);
    std::vector<double> cut_value(n, 0.0);
    std::vector<std::vector<int>> cut_side(n);

    for (int s = 1; s < n; ++s) {
        int t = parent[s];

        Dinic dinic(n);
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                double c = cost_matrix[i][j];
                if (std::isfinite(c) && c > 0.0) {
                    dinic.add_edge(i, j, c);
                    dinic.add_edge(j, i, c);
                }
            }
        }

        double flow = dinic.max_flow(s, t);
        cut_value[s] = flow;
        std::vector<int> reach = dinic.min_cut_reachable(s);

        for (int v = s + 1; v < n; ++v) {
            if (parent[v] == t && reach[v]) {
                parent[v] = s;
            }
        }

        if (reach[parent[s]]) {
            int old_parent_t = parent[t];
            auto old_side_t = std::move(cut_side[t]);
            double old_cut_t = cut_value[t];

            parent[s] = old_parent_t;
            parent[t] = s;

            cut_value[s] = old_cut_t;
            cut_value[t] = flow;

            cut_side[t] = std::move(reach);
            cut_side[s] = std::move(old_side_t);
        } else {
            cut_side[s] = std::move(reach);
        }
    }

    return {std::move(parent), std::move(cut_value), std::move(cut_side)};
}

// -------------------------- LCA / objective ------------------------------

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
    if (u == v) return u;
    int du = lca_data.depth[u];
    int dv = lca_data.depth[v];

    if (du < dv) {
        int diff = dv - du;
        for (int k = static_cast<int>(lca_data.up.size()) - 1; k >= 0; --k) {
            if (diff & (1 << k)) v = lca_data.up[k][v];
        }
    } else if (dv < du) {
        int diff = du - dv;
        for (int k = static_cast<int>(lca_data.up.size()) - 1; k >= 0; --k) {
            if (diff & (1 << k)) u = lca_data.up[k][u];
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

// -------------------------- Warm-start writer ----------------------------

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

// -------------------------- Pipeline per instance ------------------------

void process_instance_file(const fs::path& path, const fs::path& output_dir) {
    InstanceData data = parse_instance(path);
    auto cost_matrix = build_cost_matrix(data.num_nodes, data.edges);

    GHResult gh = gomory_hu(cost_matrix);
    if (static_cast<int>(gh.parent.size()) != data.num_nodes) {
        throw std::runtime_error("Gomory-Hu construction failed for " + path.string());
    }

    struct CandidateEdge {
        double cost;
        int u;
        int v;
    };
    std::vector<CandidateEdge> candidates;
    candidates.reserve(std::max(0, data.num_nodes - 1));
    for (int v = 1; v < data.num_nodes; ++v) {
        int p = gh.parent[v];
        if (p < 0 || p >= data.num_nodes) {
            throw std::runtime_error("Invalid parent index in Gomory-Hu tree");
        }
        if (std::isfinite(cost_matrix[v][p])) {
            candidates.push_back({cost_matrix[v][p], v, p});
            continue;
        }

        if (gh.cut_side.size() != static_cast<size_t>(data.num_nodes) ||
            gh.cut_side[v].size() != static_cast<size_t>(data.num_nodes)) {
            throw std::runtime_error("Missing cut partition for node " + std::to_string(v));
        }

        const auto& side = gh.cut_side[v];
        const double INF = std::numeric_limits<double>::infinity();
        double best_cost = INF;
        std::pair<int, int> best_edge{-1, -1};
        for (const auto& e : data.edges) {
            if (side[e.u] == side[e.v]) continue;
            if (e.cost < best_cost) {
                best_cost = e.cost;
                best_edge = {e.u, e.v};
            }
        }

        if (!std::isfinite(best_cost)) {
            throw std::runtime_error("No feasible edge bridging Gomory-Hu cut for node " + std::to_string(v));
        }
        candidates.push_back({best_cost, best_edge.first, best_edge.second});
    }

    // Kruskal over candidate edges to enforce a simple spanning tree without duplicates.
    struct DSU {
        std::vector<int> p, r;
        explicit DSU(int n) : p(n), r(n, 0) { std::iota(p.begin(), p.end(), 0); }
        int find(int x) { return p[x] == x ? x : p[x] = find(p[x]); }
        bool unite(int a, int b) {
            a = find(a); b = find(b);
            if (a == b) return false;
            if (r[a] < r[b]) std::swap(a, b);
            p[b] = a;
            if (r[a] == r[b]) r[a]++;
            return true;
        }
    };

    std::sort(candidates.begin(), candidates.end(),
              [](const CandidateEdge& a, const CandidateEdge& b) { return a.cost < b.cost; });

    DSU dsu(data.num_nodes);
    std::vector<std::pair<int, int>> tree_edges;
    for (const auto& e : candidates) {
        if (dsu.unite(e.u, e.v)) {
            tree_edges.emplace_back(e.u, e.v);
            if (static_cast<int>(tree_edges.size()) == data.num_nodes - 1) break;
        }
    }

    if (static_cast<int>(tree_edges.size()) < data.num_nodes - 1) {
        // Fallback: complete the spanning tree with cheapest original edges.
        std::vector<EdgeRow> sorted_edges = data.edges;
        std::sort(sorted_edges.begin(), sorted_edges.end(),
                  [](const EdgeRow& a, const EdgeRow& b) { return a.cost < b.cost; });
        for (const auto& e : sorted_edges) {
            if (dsu.unite(e.u, e.v)) {
                tree_edges.emplace_back(e.u, e.v);
                if (static_cast<int>(tree_edges.size()) == data.num_nodes - 1) break;
            }
        }
    }

    if (static_cast<int>(tree_edges.size()) != data.num_nodes - 1) {
        throw std::runtime_error("Gomory-Hu tree does not have n-1 edges");
    }

    // Evaluate objective to ensure the tree is feasible; use node 0 as root.
    double objective = compute_objective(0, tree_edges, data.requirements, cost_matrix);
    (void)objective;  // objective not used for selection; single GH tree per instance

    write_warm_start(path.stem().string(), data.num_nodes, tree_edges, output_dir);
}

// -------------------------- Main -----------------------------------------

int main(int argc, char* argv[]) {
    // Usage: ./gomory_hu_tree_warmstart [input_dir_json] [output_dir]
    // Defaults: input_dir_json="data/input", output_dir="data/input/gomory_hu_tree"
    fs::path input_dir = "data/input";
    fs::path output_dir = "data/input/gomory_hu_tree";
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
  g++ -std=c++17 -Ithird_party -o build/gomory_hu_tree_warmstart src/cpp/warm_start/gomory_hu_tree.cpp

Run (manual example):
  ./build/gomory_hu_tree_warmstart data/input data/input/gomory_hu_tree
*/
