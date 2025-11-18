#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <string>
#include <vector>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

struct DSU {
    std::vector<int> p, r;
    explicit DSU(int n) : p(n), r(n, 0) {
        for (int i = 0; i < n; ++i) p[i] = i;
    }
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

struct EdgeRow {
    int u;
    int v;
    double cost;
};

std::vector<std::pair<int, int>> compute_mst(int n, const std::vector<EdgeRow>& input_edges) {
    std::vector<std::tuple<double, int, int>> edges;
    edges.reserve(input_edges.size());
    for (const auto& e : input_edges) edges.emplace_back(e.cost, e.u, e.v);
    std::sort(edges.begin(), edges.end(), [](const auto& a, const auto& b) {
        return std::get<0>(a) < std::get<0>(b);
    });

    DSU dsu(n);
    std::vector<std::pair<int, int>> mst;
    mst.reserve(n - 1);

    for (const auto& [cost, u, v] : edges) {
        (void)cost;
        if (dsu.unite(u, v)) {
            mst.emplace_back(u, v);
            if (static_cast<int>(mst.size()) == n - 1) break;
        }
    }

    if (static_cast<int>(mst.size()) != n - 1) {
        throw std::runtime_error("Instance appears disconnected, MST not possible");
    }
    return mst;
}

void write_warm_start(const std::string& instance_name,
                      int num_nodes,
                      const std::vector<std::pair<int, int>>& mst_edges,
                      const fs::path& output_dir) {
    fs::create_directories(output_dir);
    fs::path out_file = output_dir / (instance_name + ".txt");
    std::ofstream out(out_file);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open output file: " + out_file.string());
    }
    out << num_nodes << "\n";
    for (const auto& [u, v] : mst_edges) {
        out << u << " " << v << "\n";
    }
}

std::vector<EdgeRow> parse_json_edges(const fs::path& path, int& n_out) {
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
    n_out = g["nodes"].get<int>();
    if (n_out <= 0) {
        throw std::runtime_error("Invalid node count in " + path.string());
    }
    const auto& edges_json = g["edges"];
    if (!edges_json.is_array()) {
        throw std::runtime_error("graph.edges is not an array in " + path.string());
    }

    std::vector<EdgeRow> edges;
    edges.reserve(edges_json.size());
    for (const auto& e : edges_json) {
        if (!e.contains("source") || !e.contains("destination") || !e.contains("cost")) {
            throw std::runtime_error("Edge missing fields in " + path.string());
        }
        int u = e["source"].get<int>();
        int v = e["destination"].get<int>();
        double c = e["cost"].get<double>();
        if (u < 0 || u >= n_out || v < 0 || v >= n_out) {
            throw std::runtime_error("Edge endpoints out of range in " + path.string());
        }
        if (c < 0) {
            throw std::runtime_error("Negative edge cost in " + path.string());
        }
        edges.push_back({u, v, c});
    }
    return edges;
}

void process_instance_file(const fs::path& path, const fs::path& output_dir) {
    int n = 0;
    std::vector<EdgeRow> edges = parse_json_edges(path, n);
    auto mst_edges = compute_mst(n, edges);
    write_warm_start(path.stem().string(), n, mst_edges, output_dir);
}

int main(int argc, char* argv[]) {
    // Usage: ./mst_warmstart [input_dir_json] [output_dir]
    // Defaults: input_dir_json="data/input", output_dir="data/input/mst"
    fs::path input_dir = "data/input";
    fs::path output_dir = "data/input/mst";
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
            return 1; // abort on any error (strict requirement)
        }
    }

    std::cout << "Completed. Instances processed: " << processed << std::endl;
    return 0;
}

/*
Build (manual example):
  g++ -std=c++17 -Ithird_party -o build/mst_warmstart src/cpp/warm_start/mst.cpp

Run (manual example):
  ./build/mst_warmstart data/input data/input/mst
*/
