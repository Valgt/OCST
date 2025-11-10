#!/bin/bash
# Script para ejecutar análisis avanzado de formulaciones OCST

echo "🚀 ANÁLISIS AVANZADO DE FORMULACIONES OCST"
echo "=========================================="

# Verificar que estamos en el directorio correcto
if [ ! -d "experiments/analysis" ]; then
    echo "❌ Error: Ejecutar desde la raíz del proyecto OCST"
    exit 1
fi

# Cambiar al directorio de análisis
cd experiments/analysis

# Verificar que el entorno virtual existe
if [ ! -d "analysis_env" ]; then
    echo "❌ Error: Entorno virtual no encontrado"
    echo "   Ejecutar: python3 -m venv analysis_env"
    echo "   Luego: source analysis_env/bin/activate && pip install pandas numpy matplotlib seaborn scipy"
    exit 1
fi

# Activar entorno virtual y ejecutar análisis
echo "📊 Ejecutando análisis avanzado..."
source analysis_env/bin/activate
python3 advanced_analysis.py

echo ""
echo "✅ Análisis completado"
echo "📁 Resultados en: experiments/analysis/"
echo "📊 Archivos generados:"
echo "  - formulation_comparison_analysis.png"
echo "  - formulation_analysis_report.md"
