# Análisis de Formulaciones OCST

Esta carpeta contiene herramientas y resultados de análisis avanzado para comparar las diferentes formulaciones del problema OCST.

## Estructura

```
experiments/analysis/
├── README.md                           # Este archivo
├── advanced_analysis.py                # Script principal de análisis
├── formulation_comparison_analysis.png # Gráficos de comparación
├── formulation_analysis_report.md      # Reporte detallado
└── analysis_env/                       # Entorno virtual con librerías
```

## Herramientas Disponibles

### Script de Análisis Avanzado

El script `advanced_analysis.py` utiliza pandas, numpy, matplotlib y seaborn para realizar análisis estadísticos detallados de los resultados de experimentos comparativos.

**Características:**
- Análisis estadístico completo
- Visualizaciones automáticas
- Generación de reportes
- Comparación de correctitud
- Tests estadísticos

### Uso

```bash
# Activar entorno virtual
source analysis_env/bin/activate

# Ejecutar análisis
python advanced_analysis.py
```

## Resultados Generados

### Gráficos
- `formulation_comparison_analysis.png`: Visualizaciones comparativas
  - Scatter plot de objetivos
  - Histograma de diferencias
  - Box plot de nodos explorados
  - Análisis tiempo vs diferencia

### Reportes
- `formulation_analysis_report.md`: Reporte ejecutivo con:
  - Métricas principales
  - Análisis de correctitud
  - Conclusiones y recomendaciones

## Dependencias

El entorno virtual `analysis_env` incluye:
- pandas: Manipulación de datos
- numpy: Cálculos numéricos
- matplotlib: Visualizaciones básicas
- seaborn: Visualizaciones avanzadas
- scipy: Tests estadísticos

## Análisis Disponibles

1. **Comparación de Objetivos**: Diferencia entre formulaciones
2. **Análisis de Correctitud**: Verificación contra óptimos conocidos
3. **Rendimiento**: Nodos explorados y tiempo de ejecución
4. **Correlaciones**: Relaciones entre métricas
5. **Tests Estadísticos**: Significancia de diferencias

## Notas

- El script busca automáticamente el archivo de resultados más reciente
- Los resultados se guardan en la misma carpeta
- Compatible con archivos CSV de experimentos comparativos
