# Makefile para OCST - Tesis
# Configuración para C++ con Gurobi y Python

# Variables
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
GUROBI_FLAGS = -I$(GUROBI_HOME)/include -L$(GUROBI_HOME)/lib -lgurobi_c++ -lgurobi120
PYTHON = python3

# Directorios
SRC_DIR = src/cpp
TEST_DIR = $(SRC_DIR)/tests
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/executables

# Archivos fuente
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.cpp)
TEST_TARGETS = $(TEST_SOURCES:$(TEST_DIR)/%.cpp=$(BIN_DIR)/%)

# Objetivo principal
all: setup test_gurobi

# Crear directorios necesarios
setup:
	@mkdir -p $(BUILD_DIR)/objects
	@mkdir -p $(BIN_DIR)
	@echo "✓ Directorios creados"

# Compilar test de Gurobi
test_gurobi: $(BIN_DIR)/test_gurobi
	@echo "✓ Test de Gurobi compilado"

# Crear directorio de ejecutables
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(BIN_DIR)/test_gurobi: $(TEST_DIR)/test_gurobi.cpp | $(BIN_DIR)
	@echo "Compilando test de Gurobi..."
	$(CXX) $(CXXFLAGS) $< -o $@ $(GUROBI_FLAGS)
	@echo "✓ Compilación exitosa"

# Ejecutar test de Gurobi
run_test: test_gurobi
	@echo "Ejecutando test de permisos académicos de Gurobi..."
	@echo "================================================"
	$(BIN_DIR)/test_gurobi
	@echo "================================================"
	@echo "✓ Test completado"

# Limpiar archivos compilados
clean:
	@rm -rf $(BUILD_DIR)
	@echo "✓ Archivos de compilación eliminados"

# Verificar entorno Gurobi
check_gurobi:
	@echo "Verificando entorno Gurobi..."
	@echo "GUROBI_HOME: $(GUROBI_HOME)"
	@if [ -z "$(GUROBI_HOME)" ]; then \
		echo "❌ Error: GUROBI_HOME no está configurado"; \
		exit 1; \
	fi
	@if [ ! -d "$(GUROBI_HOME)/include" ]; then \
		echo "❌ Error: Directorio include no encontrado en $(GUROBI_HOME)"; \
		exit 1; \
	fi
	@if [ ! -d "$(GUROBI_HOME)/lib" ]; then \
		echo "❌ Error: Directorio lib no encontrado en $(GUROBI_HOME)"; \
		exit 1; \
	fi
	@echo "✓ Entorno Gurobi configurado correctamente"

# Ayuda
help:
	@echo "Comandos disponibles:"
	@echo "  make setup          - Crear directorios necesarios"
	@echo "  make test_gurobi    - Compilar test de Gurobi"
	@echo "  make run_test       - Ejecutar test de permisos académicos"
	@echo "  make check_gurobi   - Verificar configuración de Gurobi"
	@echo "  make clean          - Limpiar archivos compilados"
	@echo "  make help           - Mostrar esta ayuda"

# Objetivos que no son archivos
.PHONY: all setup test_gurobi run_test clean check_gurobi help


