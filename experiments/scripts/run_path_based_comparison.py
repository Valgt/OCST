#!/usr/bin/env python3
"""
Run path_based_formulation (JSON) and path_based_formulation_original (legacy)
on the same instance set and compare their outputs.

Configuration file: experiments/config/path_based_comparison.yaml
"""

import csv
import fnmatch
import os
import subprocess
import sys
import time
from collections import namedtuple
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

import yaml

ComparisonResult = namedtuple(
    "ComparisonResult",
    [
        "instance",
        "json_status",
        "legacy_status",
        "json_objective",
        "legacy_objective",
        "objective_diff",
        "objectives_match",
        "json_edges",
        "legacy_edges",
        "edges_match",
        "notes",
    ],
)


@dataclass
class SolverOutput:
    success: bool
    stdout: str
    stderr: str
    objective: Optional[float]
    edge_list: Optional[List[Tuple[int, int]]]
    solution_path: Optional[Path]
    status: str


def load_config(config_path: Path) -> Dict:
    with open(config_path, "r", encoding="utf-8") as handle:
        return yaml.safe_load(handle)


def ensure_directory(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def collect_json_instances(json_dir: Path, include_patterns: Sequence[str], exclude_patterns: Sequence[str]) -> List[Path]:
    json_dir = json_dir.resolve()
    instances: List[Path] = []
    for pattern in include_patterns:
        instances.extend(sorted(json_dir.glob(pattern)))

    # Remove duplicates while preserving order
    seen = set()
    filtered: List[Path] = []
    for path in instances:
        if path in seen:
            continue
        seen.add(path)
        filtered.append(path)

    if not exclude_patterns:
        return filtered

    def is_excluded(path_obj: Path) -> bool:
        name = path_obj.name
        relative = path_obj.relative_to(json_dir)
        for pattern in exclude_patterns:
            if fnmatch.fnmatch(name, pattern) or fnmatch.fnmatch(str(relative), pattern):
                return True
        return False

    return [path for path in filtered if not is_excluded(path)]


def resolve_legacy_path(legacy_dir: Path, base_name: str) -> Optional[Path]:
    candidate = legacy_dir / base_name
    return candidate if candidate.exists() else None


def remove_if_exists(path: Path) -> None:
    if path.exists():
        path.unlink()


def run_solver(executable: Path, instance_path: Path, output_csv: Path, time_limit: float, heuristics: float, env: Dict[str, str]) -> Tuple[int, str, str]:
    command = [
        str(executable),
        str(instance_path),
        str(output_csv),
        str(time_limit),
        str(heuristics),
    ]
    result = subprocess.run(
        command,
        capture_output=True,
        text=True,
        timeout=time_limit + 30,
        env=env,
    )
    return result.returncode, result.stdout, result.stderr


def parse_solution_file(path: Path) -> Tuple[Optional[float], Optional[List[Tuple[int, int]]], str]:
    if not path.exists():
        return None, None, "solution file missing"

    try:
        with open(path, "r", encoding="utf-8") as handle:
            lines = [line.strip() for line in handle.readlines() if line.strip()]
    except OSError as exc:
        return None, None, f"failed to read solution file: {exc}"

    if not lines:
        return None, None, "empty solution file"

    try:
        objective = float(lines[0])
    except ValueError:
        return None, None, "invalid objective in solution file"

    edge_lines = []
    for line in lines[2:]:
        parts = line.split()
        if len(parts) != 2:
            return objective, None, "malformed edge line"
        try:
            u, v = int(parts[0]), int(parts[1])
        except ValueError:
            return objective, None, "non-integer edge endpoint"
        edge_lines.append(tuple(sorted((u, v))))

    return objective, edge_lines, "ok"


def summarize_solver_run(executable: Path, instance_path: Path, output_csv: Path, solution_path: Path, time_limit: float, heuristics: float, env: Dict[str, str]) -> SolverOutput:
    remove_if_exists(output_csv)
    remove_if_exists(solution_path)

    try:
        returncode, stdout, stderr = run_solver(executable, instance_path, output_csv, time_limit, heuristics, env)
    except subprocess.TimeoutExpired:
        return SolverOutput(
            success=False,
            stdout="",
            stderr="timeout",
            objective=None,
            edge_list=None,
            solution_path=None,
            status="timeout",
        )
    except FileNotFoundError:
        return SolverOutput(
            success=False,
            stdout="",
            stderr="executable not found",
            objective=None,
            edge_list=None,
            solution_path=None,
            status="missing_executable",
        )

    objective, edges, solution_status = parse_solution_file(solution_path)
    success = returncode == 0 and solution_status == "ok" and objective is not None
    status = "ok" if success else f"solver_exit_{returncode}" if returncode != 0 else solution_status

    return SolverOutput(
        success=success,
        stdout=stdout,
        stderr=stderr,
        objective=objective,
        edge_list=edges,
        solution_path=solution_path if success else None,
        status=status,
    )


def compare_edges(edges_a: Optional[List[Tuple[int, int]]], edges_b: Optional[List[Tuple[int, int]]]) -> Optional[bool]:
    if edges_a is None or edges_b is None:
        return None
    return set(edges_a) == set(edges_b)


def main() -> None:
    config_path = Path("experiments/config/path_based_comparison.yaml")
    if not config_path.exists():
        print(f"❌ Configuration file not found: {config_path}", file=sys.stderr)
        sys.exit(1)

    config = load_config(config_path)

    json_exec = Path(config["execution"]["json_executable"])
    legacy_exec = Path(config["execution"]["legacy_executable"])
    time_limit = float(config["execution"]["time_limit"])
    heuristics = float(config["execution"].get("heuristics_level", 0.5))
    tolerance = float(config["execution"].get("tolerance", 1e-4))

    env = os.environ.copy()
    for key, value in config["execution"].get("environment", {}).items():
        current = env.get(key, "")
        if key == "LD_LIBRARY_PATH" and current:
            env[key] = f"{value}:{current}"
        else:
            env[key] = value

    json_dir = Path(config["instances"]["json_directory"])
    legacy_dir = Path(config["instances"]["legacy_directory"])
    include_patterns = config["instances"].get("include_patterns", ["*.json"])
    exclude_patterns = config["instances"].get("exclude_patterns", [])

    json_instances = collect_json_instances(json_dir, include_patterns, exclude_patterns)
    if not json_instances:
        print("❌ No JSON instances found with provided patterns.", file=sys.stderr)
        sys.exit(1)

    comparisons: List[ComparisonResult] = []
    start_time = time.time()

    print("============================================================")
    print("Path-Based vs Path-Based Original Comparison")
    print("============================================================")
    print(f"JSON executable   : {json_exec}")
    print(f"Legacy executable : {legacy_exec}")
    print(f"Instances (JSON)  : {len(json_instances)}")
    print(f"Tolerance         : {tolerance}")
    print("============================================================\n")

    if not json_exec.exists():
        print(f"❌ JSON solver executable not found: {json_exec}", file=sys.stderr)
        sys.exit(1)
    if not legacy_exec.exists():
        print(f"❌ Legacy solver executable not found: {legacy_exec}", file=sys.stderr)
        sys.exit(1)

    for index, json_path in enumerate(json_instances, 1):
        base_name = json_path.stem
        legacy_path = resolve_legacy_path(legacy_dir, base_name)

        print(f"[{index}/{len(json_instances)}] Instance: {base_name}")

        if legacy_path is None:
            print(f"   ⚠ Legacy instance not found for {base_name}; skipping.\n")
            comparisons.append(
                ComparisonResult(
                    instance=base_name,
                    json_status="missing_legacy",
                    legacy_status="missing_legacy",
                    json_objective=None,
                    legacy_objective=None,
                    objective_diff=None,
                    objectives_match=False,
                    json_edges=None,
                    legacy_edges=None,
                    edges_match=None,
                    notes="Legacy instance missing",
                )
            )
            continue

        output_dir = Path(config["output"].get("results_directory", "experiments/results"))
        ensure_directory(output_dir)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        csv_json = output_dir / f"{base_name}_json_{timestamp}.csv"
        csv_legacy = output_dir / f"{base_name}_legacy_{timestamp}.csv"

        solution_dir = Path("data/output/test_instances")
        ensure_directory(solution_dir)

        json_solution = solution_dir / f"complete_{json_path.name}.sol"
        legacy_solution = solution_dir / f"complete_{legacy_path.name}.sol"

        json_output = summarize_solver_run(
            json_exec, json_path, csv_json, json_solution, time_limit, heuristics, env
        )
        print(f"   JSON solver status  : {json_output.status}")

        legacy_output = summarize_solver_run(
            legacy_exec, legacy_path, csv_legacy, legacy_solution, time_limit, heuristics, env
        )
        print(f"   Legacy solver status: {legacy_output.status}")

        objective_diff = None
        objectives_match = False
        if json_output.objective is not None and legacy_output.objective is not None:
            objective_diff = abs(json_output.objective - legacy_output.objective)
            objectives_match = objective_diff <= tolerance

        edges_match = compare_edges(json_output.edge_list, legacy_output.edge_list)

        note_parts = []
        if not json_output.success:
            note_parts.append(f"json={json_output.status}")
        if not legacy_output.success:
            note_parts.append(f"legacy={legacy_output.status}")
        if objectives_match:
            note_parts.append("objectives match")
        elif objective_diff is not None:
            note_parts.append(f"objective diff={objective_diff:.6f}")

        comparisons.append(
            ComparisonResult(
                instance=base_name,
                json_status=json_output.status,
                legacy_status=legacy_output.status,
                json_objective=json_output.objective,
                legacy_objective=legacy_output.objective,
                objective_diff=objective_diff,
                objectives_match=objectives_match,
                json_edges=len(json_output.edge_list) if json_output.edge_list is not None else None,
                legacy_edges=len(legacy_output.edge_list) if legacy_output.edge_list is not None else None,
                edges_match=edges_match,
                notes="; ".join(note_parts) if note_parts else "ok",
            )
        )

        print(
            f"   Objectives: JSON={json_output.objective} | Legacy={legacy_output.objective} | Match={objectives_match}"
        )
        if edges_match is not None:
            print(f"   Edge sets match   : {edges_match}")
        else:
            print("   Edge sets match   : n/a")
        print()

    duration = time.time() - start_time

    csv_prefix = config["output"].get("csv_prefix", "path_based_comparison")
    summary_path = Path(config["output"].get("results_directory", "experiments/results"))
    ensure_directory(summary_path)
    summary_file = summary_path / f"{csv_prefix}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

    with open(summary_file, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "instance",
                "json_status",
                "legacy_status",
                "json_objective",
                "legacy_objective",
                "objective_diff",
                "objectives_match",
                "json_edges",
                "legacy_edges",
                "edges_match",
                "notes",
            ]
        )
        for row in comparisons:
            writer.writerow(row)

    matches = sum(1 for row in comparisons if row.objectives_match)
    total = sum(1 for row in comparisons if row.json_objective is not None and row.legacy_objective is not None)

    print("============================================================")
    print("Comparison Summary")
    print("============================================================")
    print(f"Total instances processed : {len(comparisons)}")
    print(f"Valid comparisons         : {total}")
    print(f"Matching objectives       : {matches}")
    print(f"Elapsed time              : {duration:.2f} seconds")
    print(f"Results CSV               : {summary_file}")
    print("============================================================")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nExecution interrupted by user.", file=sys.stderr)
        sys.exit(130)
