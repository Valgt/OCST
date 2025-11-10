# Legacy Scripts Directory

Esta carpeta contiene los scripts originales de ejecución de experimentos que existían antes de la implementación del orquestador unificado (Workstream 5).

## ⚠️ Estado: DEPRECATED

**Estos scripts están deprecados y serán removidos en futuras versiones.** Se mantienen aquí únicamente por compatibilidad temporal durante la transición.

## 📋 Scripts Disponibles

- `run_analysis.sh` - Análisis de resultados
- `run_comparative_experiment.sh` - Experimentos comparativos
- `run_experiment.sh` - Experimento genérico
- `run_flow_based_experiment.sh` - Formulación flow-based
- `run_flow_based_relaxed_experiment.sh` - Formulación flow-based relajada
- `run_path_based_experiment.sh` - Formulación path-based
- `run_visualizer.sh` - Visualización de resultados

## 🔄 Migración Recomendada

En lugar de usar estos scripts, utilice el nuevo orquestador unificado:

```bash
# En lugar de: ./run_path_based_experiment.sh
python scripts/standardization/orchestrator.py --formulation path_based --tag quick_check
```

## 📝 Notas Técnicas

- Los scripts legacy usan el formato de archivos legacy (.ocstpin, .sol, .csv)
- No soportan la nueva configuración JSON
- No generan metadatos de reproducibilidad
- No siguen la estructura de output estandarizada

## 🗑️ Plan de Eliminación

Estos scripts serán eliminados después de:
1. Validación completa del orquestador unificado
2. Migración de todos los workflows experimentales
3. Verificación de que no se usan en producción
