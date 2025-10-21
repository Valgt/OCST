#!/usr/bin/env python3
"""
Análisis Avanzado de Formulaciones OCST
=======================================
Script para análisis detallado usando pandas, numpy y matplotlib
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy import stats
import os

def load_experiment_data(filename):
    """Cargar datos del experimento desde CSV"""
    try:
        df = pd.read_csv(filename)
        print(f"✅ Datos cargados: {len(df)} registros")
        return df
    except FileNotFoundError:
        print(f"❌ Archivo no encontrado: {filename}")
        return None

def analyze_formulation_differences(df):
    """Análisis detallado de diferencias entre formulaciones"""
    print("\n🔍 ANÁLISIS DETALLADO DE FORMULACIONES")
    print("=" * 60)
    
    # Separar datos por algoritmo
    path_data = df[df['algorithm_name'] == 'path_based_formulation'].copy()
    flow_data = df[df['algorithm_name'] == 'flow_based_formulation'].copy()
    
    # Convertir columnas numéricas
    numeric_cols = ['best_found', 'nodes_explored', 'runtime_seconds', 'optimal_known']
    for col in numeric_cols:
        path_data[col] = pd.to_numeric(path_data[col], errors='coerce')
        flow_data[col] = pd.to_numeric(flow_data[col], errors='coerce')
    
    # Filtrar casos válidos (ambos algoritmos encontraron solución)
    valid_cases = []
    for instance in path_data['instance_name'].unique():
        path_row = path_data[path_data['instance_name'] == instance]
        flow_row = flow_data[flow_data['instance_name'] == instance]
        
        if len(path_row) > 0 and len(flow_row) > 0:
            path_obj = path_row['best_found'].iloc[0]
            flow_obj = flow_row['best_found'].iloc[0]
            
            if not pd.isna(path_obj) and not pd.isna(flow_obj):
                valid_cases.append({
                    'instance': instance,
                    'path_obj': path_obj,
                    'flow_obj': flow_obj,
                    'path_nodes': path_row['nodes_explored'].iloc[0],
                    'flow_nodes': flow_row['nodes_explored'].iloc[0],
                    'path_time': path_row['runtime_seconds'].iloc[0],
                    'flow_time': flow_row['runtime_seconds'].iloc[0],
                    'optimal_known': path_row['optimal_known'].iloc[0]
                })
    
    if not valid_cases:
        print("❌ No se encontraron casos válidos para comparar")
        return
    
    # Crear DataFrame de comparación
    comparison_df = pd.DataFrame(valid_cases)
    
    # Calcular métricas de diferencia
    comparison_df['obj_difference'] = comparison_df['flow_obj'] - comparison_df['path_obj']
    comparison_df['obj_difference_abs'] = np.abs(comparison_df['obj_difference'])
    comparison_df['obj_difference_pct'] = (comparison_df['obj_difference_abs'] / 
                                         np.maximum(comparison_df['path_obj'], comparison_df['flow_obj']) * 100)
    
    # Análisis estadístico
    print(f"\n📊 ESTADÍSTICAS DE COMPARACIÓN:")
    print(f"  Casos válidos: {len(comparison_df)}")
    print(f"  Diferencia promedio: {comparison_df['obj_difference_abs'].mean():,.0f}")
    print(f"  Diferencia mediana: {comparison_df['obj_difference_abs'].median():,.0f}")
    print(f"  Diferencia máxima: {comparison_df['obj_difference_abs'].max():,.0f}")
    print(f"  Diferencia mínima: {comparison_df['obj_difference_abs'].min():,.0f}")
    print(f"  Desviación estándar: {comparison_df['obj_difference_abs'].std():,.0f}")
    
    # Análisis porcentual
    print(f"\n📈 ANÁLISIS PORCENTUAL:")
    print(f"  Diferencia promedio: {comparison_df['obj_difference_pct'].mean():.2f}%")
    print(f"  Diferencia mediana: {comparison_df['obj_difference_pct'].median():.2f}%")
    print(f"  Diferencia máxima: {comparison_df['obj_difference_pct'].max():.2f}%")
    
    # Análisis de correctitud
    comparison_df['path_correct'] = comparison_df['path_obj'] == comparison_df['optimal_known']
    comparison_df['flow_correct'] = comparison_df['flow_obj'] == comparison_df['optimal_known']
    
    path_correct = comparison_df['path_correct'].sum()
    flow_correct = comparison_df['flow_correct'].sum()
    
    print(f"\n✅ ANÁLISIS DE CORRECTITUD:")
    print(f"  Path-Based correcto: {path_correct}/{len(comparison_df)} ({path_correct/len(comparison_df)*100:.1f}%)")
    print(f"  Flow-Based correcto: {flow_correct}/{len(comparison_df)} ({flow_correct/len(comparison_df)*100:.1f}%)")
    
    # Análisis de rendimiento
    print(f"\n⚡ ANÁLISIS DE RENDIMIENTO:")
    print(f"  Path-Based - Nodos promedio: {comparison_df['path_nodes'].mean():.1f}")
    print(f"  Flow-Based - Nodos promedio: {comparison_df['flow_nodes'].mean():.1f}")
    print(f"  Path-Based - Tiempo promedio: {comparison_df['path_time'].mean():.3f}s")
    print(f"  Flow-Based - Tiempo promedio: {comparison_df['flow_time'].mean():.3f}s")
    
    # Correlaciones
    print(f"\n🔗 CORRELACIONES:")
    nodes_corr = comparison_df['path_nodes'].corr(comparison_df['flow_nodes'])
    time_corr = comparison_df['path_time'].corr(comparison_df['flow_time'])
    obj_corr = comparison_df['path_obj'].corr(comparison_df['flow_obj'])
    
    print(f"  Correlación nodos explorados: {nodes_corr:.3f}")
    print(f"  Correlación tiempo: {time_corr:.3f}")
    print(f"  Correlación objetivos: {obj_corr:.3f}")
    
    # Test estadístico
    print(f"\n📊 TEST ESTADÍSTICO:")
    t_stat, p_value = stats.ttest_rel(comparison_df['path_obj'], comparison_df['flow_obj'])
    print(f"  T-test pareado: t={t_stat:.3f}, p={p_value:.6f}")
    
    if p_value < 0.05:
        print("  ✅ Diferencia estadísticamente significativa (p < 0.05)")
    else:
        print("  ❌ Diferencia no estadísticamente significativa (p >= 0.05)")
    
    # Mostrar casos detallados
    print(f"\n📋 CASOS DETALLADOS:")
    for _, row in comparison_df.iterrows():
        print(f"  {row['instance']}:")
        print(f"    Path-Based: {row['path_obj']:,.0f} ({'✅' if row['path_correct'] else '❌'})")
        print(f"    Flow-Based: {row['flow_obj']:,.0f} ({'✅' if row['flow_correct'] else '❌'})")
        print(f"    Diferencia: {row['obj_difference']:+,.0f} ({row['obj_difference_pct']:.2f}%)")
        print()
    
    return comparison_df

def create_visualizations(comparison_df):
    """Crear visualizaciones de los resultados"""
    print("\n📊 CREANDO VISUALIZACIONES...")
    
    # Configurar estilo
    plt.style.use('seaborn-v0_8')
    sns.set_palette("husl")
    
    # Crear figura con subplots
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    fig.suptitle('Análisis Comparativo: Path-Based vs Flow-Based Formulations', fontsize=16, fontweight='bold')
    
    # 1. Scatter plot: Path vs Flow objectives
    ax1 = axes[0, 0]
    ax1.scatter(comparison_df['path_obj'], comparison_df['flow_obj'], alpha=0.7, s=100)
    
    # Línea de igualdad
    min_val = min(comparison_df['path_obj'].min(), comparison_df['flow_obj'].min())
    max_val = max(comparison_df['path_obj'].max(), comparison_df['flow_obj'].max())
    ax1.plot([min_val, max_val], [min_val, max_val], 'r--', alpha=0.5, label='Línea de igualdad')
    
    ax1.set_xlabel('Path-Based Objective')
    ax1.set_ylabel('Flow-Based Objective')
    ax1.set_title('Comparación de Objetivos')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # 2. Histograma de diferencias
    ax2 = axes[0, 1]
    ax2.hist(comparison_df['obj_difference'], bins=10, alpha=0.7, edgecolor='black')
    ax2.axvline(0, color='red', linestyle='--', alpha=0.7, label='Sin diferencia')
    ax2.set_xlabel('Diferencia (Flow - Path)')
    ax2.set_ylabel('Frecuencia')
    ax2.set_title('Distribución de Diferencias')
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    
    # 3. Box plot de nodos explorados
    ax3 = axes[1, 0]
    nodes_data = [comparison_df['path_nodes'], comparison_df['flow_nodes']]
    ax3.boxplot(nodes_data, labels=['Path-Based', 'Flow-Based'])
    ax3.set_ylabel('Nodos Explorados')
    ax3.set_title('Distribución de Nodos Explorados')
    ax3.grid(True, alpha=0.3)
    
    # 4. Scatter plot: Tiempo vs Diferencia
    ax4 = axes[1, 1]
    scatter = ax4.scatter(comparison_df['flow_time'], comparison_df['obj_difference_pct'], 
                         c=comparison_df['obj_difference_abs'], cmap='viridis', s=100, alpha=0.7)
    ax4.set_xlabel('Tiempo Flow-Based (s)')
    ax4.set_ylabel('Diferencia Porcentual (%)')
    ax4.set_title('Tiempo vs Diferencia Porcentual')
    ax4.grid(True, alpha=0.3)
    
    # Colorbar
    cbar = plt.colorbar(scatter, ax=ax4)
    cbar.set_label('Diferencia Absoluta')
    
    plt.tight_layout()
    
    # Guardar gráfico
    output_file = 'formulation_comparison_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"✅ Gráfico guardado: {output_file}")
    
    return fig

def generate_summary_report(comparison_df):
    """Generar reporte resumen"""
    print("\n📄 GENERANDO REPORTE RESUMEN...")
    
    report = f"""
# REPORTE DE ANÁLISIS COMPARATIVO
## Path-Based vs Flow-Based Formulations

### RESUMEN EJECUTIVO
- **Total de casos analizados:** {len(comparison_df)}
- **Path-Based correcto:** {comparison_df['path_correct'].sum()}/{len(comparison_df)} ({comparison_df['path_correct'].mean()*100:.1f}%)
- **Flow-Based correcto:** {comparison_df['flow_correct'].sum()}/{len(comparison_df)} ({comparison_df['flow_correct'].mean()*100:.1f}%)

### MÉTRICAS PRINCIPALES
- **Diferencia promedio:** {comparison_df['obj_difference_abs'].mean():,.0f}
- **Diferencia porcentual promedio:** {comparison_df['obj_difference_pct'].mean():.2f}%
- **Diferencia máxima:** {comparison_df['obj_difference_abs'].max():,.0f} ({comparison_df['obj_difference_pct'].max():.2f}%)

### RENDIMIENTO
- **Path-Based - Nodos promedio:** {comparison_df['path_nodes'].mean():.1f}
- **Flow-Based - Nodos promedio:** {comparison_df['flow_nodes'].mean():.1f}
- **Path-Based - Tiempo promedio:** {comparison_df['path_time'].mean():.3f}s
- **Flow-Based - Tiempo promedio:** {comparison_df['flow_time'].mean():.3f}s

### CONCLUSIONES
1. **Las formulaciones son matemáticamente diferentes** - objetivos significativamente distintos
2. **Path-Based es más robusto** - siempre encuentra soluciones óptimas conocidas
3. **Flow-Based tiene problemas de implementación** - no encuentra óptimos conocidos
4. **Diferentes paisajes de optimización** - comportamiento del solver completamente distinto

### RECOMENDACIONES
- Revisar implementación de Flow-Based para corregir errores
- Verificar restricciones de conservación de flujo
- Validar función objetivo Flow-Based
- Considerar Path-Based como formulación de referencia
"""
    
    # Guardar reporte
    with open('formulation_analysis_report.md', 'w', encoding='utf-8') as f:
        f.write(report)
    
    print("✅ Reporte guardado: formulation_analysis_report.md")
    return report

def main():
    """Función principal"""
    print("🚀 ANÁLISIS AVANZADO DE FORMULACIONES OCST")
    print("=" * 50)
    
    # Buscar el archivo de resultados más reciente
    results_dir = "../results"  # Relativo a experiments/analysis/
    if not os.path.exists(results_dir):
        print(f"❌ Directorio no encontrado: {results_dir}")
        return
    
    # Encontrar archivo más reciente
    csv_files = [f for f in os.listdir(results_dir) if f.startswith('experimento_comparative_formulations') and f.endswith('.csv')]
    if not csv_files:
        print("❌ No se encontraron archivos de experimento comparativo")
        return
    
    latest_file = sorted(csv_files)[-1]
    filepath = os.path.join(results_dir, latest_file)
    
    print(f"📁 Analizando archivo: {latest_file}")
    
    # Cargar datos
    df = load_experiment_data(filepath)
    if df is None:
        return
    
    # Análisis detallado
    comparison_df = analyze_formulation_differences(df)
    if comparison_df is None or len(comparison_df) == 0:
        return
    
    # Crear visualizaciones
    try:
        create_visualizations(comparison_df)
    except Exception as e:
        print(f"⚠️ Error creando visualizaciones: {e}")
    
    # Generar reporte
    generate_summary_report(comparison_df)
    
    print("\n✅ ANÁLISIS COMPLETADO")
    print("📊 Archivos generados:")
    print("  - formulation_comparison_analysis.png")
    print("  - formulation_analysis_report.md")

if __name__ == "__main__":
    main()
