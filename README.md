# Optimal Communication Spanning Tree (OCST) - Tesis

Este repositorio contiene el código y documentación para la tesis sobre el problema de Optimal Communication Spanning Tree.

## Estructura del Proyecto

- `src/cpp/` - Implementaciones en C++ con Gurobi
- `thesis/` - Archivos LaTeX de la tesis
- `docs/papers/` - Papers de referencia
- `data/` - Instancias de prueba y resultados
- `experiments/` - Configuración y resultados de experimentos

## Configuración Rápida

### Verificación automática del entorno
```bash
# Ejecutar verificación completa (recomendado)
./scripts/test_setup.sh

# O verificación paso a paso
make check_gurobi    # Verificar configuración de Gurobi
make run_test        # Ejecutar test de permisos académicos
python3 src/python/test_environment.py  # Verificación completa
```

### Requisitos
- Gurobi Optimizer (versión 12.0 o superior) con licencia académica
- Compilador C++ (g++)
- Python 3.7+ con paquetes opcionales
- LaTeX (para la tesis)

### Instalación de dependencias Python
```bash
pip install -r requirements.txt
```

## Compilación

### Código C++
```bash
# Compilar todo
make all

# Compilar solo test de Gurobi
make test_gurobi

# Ejecutar test de permisos académicos
make run_test

# Limpiar archivos compilados
make clean
```

### Tesis LaTeX
```bash
cd thesis/
pdflatex main.tex
bibtex main
pdflatex main.tex
pdflatex main.tex
```

## Uso

1. **Configurar entorno**: `./scripts/test_setup.sh`
2. **Desarrollar algoritmos**: Editar archivos en `src/cpp/algorithms/`
3. **Ejecutar experimentos**: Usar scripts en `experiments/`
4. **Generar tesis**: Compilar archivos LaTeX en `thesis/`

## Documentación

Ver `struct.md` para la estructura completa del repositorio.
