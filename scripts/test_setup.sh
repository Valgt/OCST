#!/bin/bash
# Script para probar el entorno de desarrollo OCST
# Verifica permisos académicos de Gurobi y configuración

set -e  # Salir si hay errores

echo "🚀 CONFIGURANDO ENTORNO OCST"
echo "=============================="

# Verificar que estamos en el directorio correcto
if [ ! -f "Makefile" ]; then
    echo "❌ Error: Ejecuta este script desde el directorio raíz del proyecto"
    exit 1
fi

# Verificar GUROBI_HOME
if [ -z "$GUROBI_HOME" ]; then
    echo "❌ Error: GUROBI_HOME no está configurado"
    echo "   Configúralo con: export GUROBI_HOME=/path/to/gurobi"
    exit 1
fi

echo "✓ GUROBI_HOME configurado: $GUROBI_HOME"

# Crear directorios necesarios
echo "📁 Creando directorios..."
make setup

# Verificar configuración de Gurobi
echo "🔍 Verificando configuración de Gurobi..."
make check_gurobi

# Compilar test de Gurobi
echo "🔨 Compilando test de Gurobi..."
make test_gurobi

# Ejecutar test de permisos académicos
echo "🧪 Ejecutando test de permisos académicos..."
make run_test

# Ejecutar verificación completa con Python
echo "🐍 Ejecutando verificación completa con Python..."
python3 src/python/test_environment.py

echo ""
echo "✅ ¡ENTORNO CONFIGURADO CORRECTAMENTE!"
echo "   Puedes comenzar a desarrollar tu tesis OCST"
echo ""
echo "📋 Comandos útiles:"
echo "   make run_test     - Ejecutar solo el test de Gurobi"
echo "   make clean        - Limpiar archivos compilados"
echo "   make help         - Ver todos los comandos disponibles"


