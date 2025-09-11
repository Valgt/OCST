# Estructura del Repositorio - Tesis OCST

## Descripción General
Este repositorio está diseñado para crear código para la tesis sobre el Optimal Communication Spanning Tree Problem (OCST). Incluye manejo de papers de referencia y archivos LaTeX.

## Estructura Propuesta

```
OCST/
├── README.md                          # Descripción del proyecto y cómo compilar/ejecutar
├── LICENSE                            # Licencia del proyecto
├── .gitignore                         # Archivos a ignorar en git
├── requirements.txt                   # Dependencias de Python (si usas)
├── Makefile                          # Para compilar automáticamente
│
├── docs/                             # Documentación
│   ├── papers/                       # Papers de referencia
│   │   ├── paper1.pdf
│   │   ├── paper2.pdf
│   │   └── references.bib            # Base de datos bibliográfica
│   ├── notes/                        # Notas de investigación
│   │   ├── problem_analysis.md
│   │   ├── algorithm_notes.md
│   │   └── experimental_results.md
│   └── presentations/                # Presentaciones
│       └── thesis_defense.pptx
│
├── thesis/                           # Archivos LaTeX de la tesis
│   ├── main.tex                      # Archivo principal
│   ├── chapters/                     # Capítulos individuales
│   │   ├── introduction.tex
│   │   ├── literature_review.tex
│   │   ├── methodology.tex
│   │   ├── implementation.tex
│   │   ├── experiments.tex
│   │   └── conclusions.tex
│   ├── figures/                      # Figuras y diagramas
│   │   ├── problem_illustration.pdf
│   │   ├── algorithm_flowchart.pdf
│   │   └── results_charts/
│   ├── tables/                       # Tablas
│   │   └── experimental_data.tex
│   ├── bibliography/                 # Referencias bibliográficas
│   │   ├── references.bib
│   │   └── custom_commands.tex
│   └── config/                       # Configuración LaTeX
│       ├── packages.tex
│       └── thesis_style.tex
│
├── src/                              # Código fuente
│   ├── cpp/                          # Implementaciones en C++
│   │   ├── algorithms/               # Algoritmos principales
│   │   │   ├── ocst_solver.cpp
│   │   │   ├── exact_algorithm.cpp
│   │   │   └── heuristic_algorithm.cpp
│   │   ├── data_structures/          # Estructuras de datos
│   │   │   ├── graph.h
│   │   │   ├── tree.h
│   │   │   └── priority_queue.h
│   │   ├── utils/                    # Utilidades
│   │   │   ├── file_parser.cpp
│   │   │   ├── graph_generator.cpp
│   │   │   └── result_writer.cpp
│   │   └── tests/                    # Tests unitarios
│   │       ├── test_graph.cpp
│   │       └── test_algorithms.cpp
│   │
│   ├── python/                       # Scripts de Python (si los usas)
│   │   ├── data_analysis.py
│   │   ├── visualization.py
│   │   └── experiment_runner.py
│   │
│   └── scripts/                      # Scripts de automatización
│       ├── compile.sh
│       ├── run_experiments.sh
│       └── generate_plots.py
│
├── data/                             # Datos de entrada y salida
│   ├── input/                        # Instancias de prueba
│   │   ├── small_instances/
│   │   ├── medium_instances/
│   │   └── large_instances/
│   ├── output/                       # Resultados de experimentos
│   │   ├── results/
│   │   ├── logs/
│   │   └── plots/
│   └── benchmarks/                   # Instancias de referencia
│       └── standard_instances/
│
├── experiments/                      # Configuración de experimentos
│   ├── config/                       # Archivos de configuración
│   │   ├── small_experiment.json
│   │   └── large_experiment.json
│   ├── results/                      # Resultados organizados
│   │   ├── performance_analysis/
│   │   └── comparison_studies/
│   └── scripts/                      # Scripts de experimentación
│       ├── run_benchmarks.py
│       └── analyze_results.py
│
└── build/                            # Archivos de compilación (ignorado en git)
    ├── objects/
    ├── executables/
    └── temp/
```

## Ventajas de esta Estructura

1. **Separación clara** entre código, documentación y tesis
2. **Fácil navegación** para encontrar archivos específicos
3. **Escalable** para proyectos grandes
4. **Estándar académico** para tesis
5. **Versionado eficiente** con git

## Archivos Clave a Crear Primero

- `README.md` con instrucciones de compilación
- `thesis/main.tex` como punto de entrada
- `src/cpp/algorithms/ocst_solver.cpp` para tu algoritmo principal
- `docs/papers/references.bib` para las referencias

## Notas de Implementación

- El directorio `build/` debe estar en `.gitignore`
- Los archivos de datos grandes pueden usar Git LFS
- Mantener un registro de cambios en `CHANGELOG.md`
- Documentar cada algoritmo en `docs/notes/`


