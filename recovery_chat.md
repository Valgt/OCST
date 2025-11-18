# Session Recovery Notes (2025-11-18)

## Context
- Repository: OCST (Optimal Communication Spanning Tree).
- Focus: Phase 2.0 warm starts and new `dijkstra_tree` warm-start generator.
- Recent work: Added `src/cpp/warm_start/dijkstra_tree.cpp` to build Dijkstra-based warm-start trees and wrote build/run instructions in the file.

## New Warm-Start Idea Implemented
- File: `src/cpp/warm_start/dijkstra_tree.cpp`.
- Behavior: For each instance, run Dijkstra from every possible root, build the shortest-path tree, evaluate OCST objective on that tree via LCA path lengths, and keep the best tree. Outputs `data/input/dijkstra_tree/<instance>.txt` (format: first line `n`, then `n-1` edges).
- Build example:
  ```bash
  g++ -std=c++17 -Ithird_party -o build/dijkstra_tree_warmstart src/cpp/warm_start/dijkstra_tree.cpp
  ```
- Run example:
  ```bash
  ./build/dijkstra_tree_warmstart data/input data/input/dijkstra_tree
  ```
- Output files exist: `data/input/dijkstra_tree/*.txt` (warm-start trees).

## Experiment Attempts and Failures
- Command used for warm-start experiment:
  ```bash
  python3 scripts/standardization/orchestrator.py --tag quick_check --formulation path_based --config config=/home/sergio/OCST/tmp_dijkstra_config.json
  ```
  - Config file created: `/home/sergio/OCST/tmp_dijkstra_config.json`
    ```json
    {
      "warm_starts": ["dijkstra_tree", "mst"],
      "mip_gap": 0.0,
      "time_limit": 3600,
      "threads": 1
    }
    ```
  - Results directory: `experiments/quick_check_20251118_131822/path_based/`
  - All 25 quick_check instances failed with `Gurobi error: Var::set` (see `results/detailed_results.json`).
  - Individual result files under `…/path_based/results/*.results.json` show status=ERROR.

- Manual run of the binary shows root cause remains license/HostID:
  ```bash
  LD_LIBRARY_PATH=/opt/gurobi/linux64/lib:$LD_LIBRARY_PATH ./build/executables/path_based_formulation --instance ocstpin0 --seed 42 --config /home/sergio/OCST/tmp_dijkstra_config.json
  ```
  Output:
  ```
  [path_based] Gurobi error: HostID mismatch (licensed to 5d4521f6, hostid is 0)
  ```
  => License is not valid for this host. The orchestrator error “Var::set” is masking the HostID mismatch.

- Baseline formulation also fails:
  ```bash
  python3 scripts/standardization/orchestrator.py --tag quick_check --formulation path_based_formulation_original --seed 42
  ```
  Results directory: `experiments/quick_check_20251118_132158/path_based_formulation_original/`
  All failed with `terminate called after throwing an instance of 'GRBException'` (also likely license-related).

## What to Fix First
1) Fix Gurobi license on this host:
   - Ensure `GRB_LICENSE_FILE` points to a valid license for this HostID, or rerun `grbgetkey` to generate a new license for hostid=0 machine.
   - Verify `LD_LIBRARY_PATH` includes `/opt/gurobi/linux64/lib` when running binaries.
2) After license is valid, rerun the experiment:
   ```bash
   python3 scripts/standardization/orchestrator.py --tag quick_check --formulation path_based --config config=/home/sergio/OCST/tmp_dijkstra_config.json
   ```
   (Warm-start ideas in config: `["dijkstra_tree","mst"]`.)
3) If needed, rerun baseline to confirm env:
   ```bash
   python3 scripts/standardization/orchestrator.py --tag quick_check --formulation path_based_formulation_original --seed 42
   ```

## Useful Paths for Logs
- Warm-start experiment logs: `experiments/quick_check_20251118_131822/path_based/`
  - `results/detailed_results.json` contains stderr per instance.
- Baseline attempt logs: `experiments/quick_check_20251118_132158/path_based_formulation_original/`
  - `results/detailed_results.json` with GRBException messages.
- Warm-start generator source: `src/cpp/warm_start/dijkstra_tree.cpp`.
- Warm-start outputs: `data/input/dijkstra_tree/*.txt`.

## Reminders
- Command template for warm-start runs (after license fix):
  ```bash
  python3 scripts/standardization/orchestrator.py --tag quick_check --formulation path_based --config config=/home/sergio/OCST/tmp_dijkstra_config.json
  ```
- LD_LIBRARY_PATH may be needed when running binaries directly:
  ```bash
  LD_LIBRARY_PATH=/opt/gurobi/linux64/lib:$LD_LIBRARY_PATH ./build/executables/path_based_formulation --instance ocstpin0 --seed 42 --config /home/sergio/OCST/tmp_dijkstra_config.json
  ```

