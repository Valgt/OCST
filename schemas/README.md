# JSON Schemas for OCST

This directory contains JSON schemas for standardized data formats in the OCST project.

## Schemas

### Instance Schema (`instance.schema.v1.json`)
Schema for OCST problem instances.

**Key characteristics:**
- `graph` and `requirements` are **immutable** (never change)
- Only `metadata` can evolve without affecting instance identity
- Field `name` is required and must be unique
- Tag `quick_check` is reserved for fast regression subset

**Example:** See `examples/instance_example.json`

### Result Schema (`result.schema.v1.json`)
Schema for solver execution results.

**Key characteristics:**
- Field `optimization_status.code` indicates solver termination state
- Field `solution` is only present if `optimization_status.has_solution == true`
- Field `run_uuid` (UUIDv4) enables correlation with log files
- Field `reproducibility` captures git commit, seed, and timestamp

**Status codes:**
- `OPTIMAL`: Solution found and proven optimal
- `TIME_LIMIT`: Time limit reached (may have feasible solution)
- `INFEASIBLE`: Problem is infeasible
- `UNBOUNDED`: Problem is unbounded
- `SUBOPTIMAL`: Feasible solution but not proven optimal
- `INTERRUPTED`: Interrupted by user/system
- `ERROR`: Error during optimization

**Example:** See `examples/result_example.json`

### Config Schema (`config.schema.v1.json`)
Schema for solver configuration parameters.

**Key characteristics:**
- All fields have default values
- Missing optional fields are filled with defaults
- Configuration is versioned and hashed for reproducibility

**Example:** See `examples/config_example.json`

## Validation

### Python
```python
import jsonschema
import json

schema = json.load(open('instance.schema.v1.json'))
data = json.load(open('instance.json'))
jsonschema.validate(data, schema)
```

### C++
Validation helpers are provided in `src/cpp/common/` (when implemented):
- `InstanceValidator` - Validates instance JSON
- `ResultValidator` - Validates result JSON

## Versioning

Schemas are versioned with `v1`, `v2`, etc. in the filename.
The `schema_version` field in JSON files indicates which schema version they conform to.

Migration scripts (when needed) will be documented separately.

