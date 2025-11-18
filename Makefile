# Makefile para OCST - Tesis
# Configuración para C++ con Gurobi y Python

# Variables
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
PATH_BASED_INCLUDES = -Isrc/cpp/common -Ithird_party
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

# Algoritmos
PATH_BASED_SRC = $(SRC_DIR)/algorithms/path_based_formulation/algorithm.cpp
PATH_BASED_SOLVER_SRC = $(SRC_DIR)/common/formulation_solver.cpp $(SRC_DIR)/common/config.cpp $(SRC_DIR)/common/structured_logger.cpp $(SRC_DIR)/common/warm_start_loader.cpp
PATH_BASED_ORIGINAL_SRC = $(SRC_DIR)/algorithms/path_based_formulation_original/algorithm.cpp
FLOW_BASED_SRC = $(SRC_DIR)/algorithms/flow_based_formulation/algorithm.cpp
FLOW_BASED_RELAXED_SRC = $(SRC_DIR)/algorithms/flow_based_relaxed_formulation/algorithm.cpp
ROOTED_TREE_BASED_SRC = $(SRC_DIR)/algorithms/rooted_tree_based_formulation/algorithm.cpp

# Build info generado automáticamente
BUILD_INFO_H = $(SRC_DIR)/common/build_info.h

# Objetivo principal
all: setup build_info test_gurobi path_based path_based_original flow_based flow_based_relaxed rooted_tree_based

# Crear directorios necesarios
setup:
	@mkdir -p $(BUILD_DIR)/objects
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(SRC_DIR)/common
	@echo "✓ Directorios creados"

# Generar build_info.h con git commit hash
build_info: $(BUILD_INFO_H)

$(BUILD_INFO_H): FORCE
	@mkdir -p $(SRC_DIR)/common
	@echo "// Auto-generated build information" > $@
	@echo "// Generated at build time - DO NOT EDIT" >> $@
	@echo "#ifndef OCST_COMMON_BUILD_INFO_H" >> $@
	@echo "#define OCST_COMMON_BUILD_INFO_H" >> $@
	@echo "" >> $@
	@echo "#define GIT_COMMIT \"$(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)\"" >> $@
	@echo "#define GIT_DIRTY $(shell git diff-index --quiet HEAD -- 2>/dev/null && echo false || echo true)" >> $@
	@echo "#define BUILD_TIMESTAMP \"$(shell date -u +%Y-%m-%dT%H:%M:%SZ)\"" >> $@
	@echo "" >> $@
	@echo "#endif // OCST_COMMON_BUILD_INFO_H" >> $@
	@echo "✓ build_info.h generado (commit: $(shell git rev-parse --short HEAD 2>/dev/null || echo unknown))"

FORCE:

# Compilar algoritmos
path_based: $(BIN_DIR)/path_based_formulation
	@echo "✓ Path-Based Formulation compilado"

path_based_original: $(BIN_DIR)/path_based_formulation_original
	@echo "✓ Path-Based Formulation Original (Baseline) compilado"

flow_based: $(BIN_DIR)/flow_based_formulation
	@echo "✓ Flow-Based Formulation compilado"

flow_based_relaxed: $(BIN_DIR)/flow_based_relaxed_formulation
	@echo "✓ Flow-Based Relaxed Formulation compilado"

rooted_tree_based: $(BIN_DIR)/rooted_tree_based_formulation
	@echo "✓ Rooted Tree-Based Formulation compilado"

$(BIN_DIR)/path_based_formulation: $(PATH_BASED_SRC) $(PATH_BASED_SOLVER_SRC) $(BUILD_INFO_H) | $(BIN_DIR)
	@echo "Compilando Path-Based Formulation..."
	$(CXX) $(CXXFLAGS) $(PATH_BASED_INCLUDES) -Isrc/cpp/common $(PATH_BASED_SRC) $(PATH_BASED_SOLVER_SRC) -o $@ $(GUROBI_FLAGS)
	@echo "✓ Compilación exitosa"

$(BIN_DIR)/path_based_formulation_original: $(PATH_BASED_ORIGINAL_SRC) | $(BIN_DIR)
	@echo "Compilando Path-Based Formulation Original (Baseline)..."
	$(CXX) $(CXXFLAGS) $< -o $@ $(GUROBI_FLAGS)
	@echo "✓ Compilación exitosa"

$(BIN_DIR)/flow_based_formulation: $(FLOW_BASED_SRC) | $(BIN_DIR)
	@echo "Compilando Flow-Based Formulation..."
	$(CXX) $(CXXFLAGS) $< -o $@ $(GUROBI_FLAGS)
	@echo "✓ Compilación exitosa"

$(BIN_DIR)/flow_based_relaxed_formulation: $(FLOW_BASED_RELAXED_SRC) | $(BIN_DIR)
	@echo "Compilando Flow-Based Relaxed Formulation..."
	$(CXX) $(CXXFLAGS) $< -o $@ $(GUROBI_FLAGS)
	@echo "✓ Compilación exitosa"

$(BIN_DIR)/rooted_tree_based_formulation: $(ROOTED_TREE_BASED_SRC) | $(BIN_DIR)
	@echo "Compilando Rooted Tree-Based Formulation..."
	$(CXX) $(CXXFLAGS) $< -o $@ $(GUROBI_FLAGS)
	@echo "✓ Compilación exitosa"

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
	@echo "  make setup                 - Crear directorios necesarios"
	@echo "  make test_gurobi           - Compilar test de Gurobi"
	@echo "  make path_based            - Compilar Path-Based Formulation"
	@echo "  make path_based_original   - Compilar Path-Based Formulation Original (Baseline)"
	@echo "  make flow_based            - Compilar Flow-Based Formulation"
	@echo "  make flow_based_relaxed    - Compilar Flow-Based Relaxed Formulation"
	@echo "  make rooted_tree_based     - Compilar Rooted Tree-Based Formulation"
	@echo "  make run_test              - Ejecutar test de permisos académicos"
	@echo "  make check_gurobi          - Verificar configuración de Gurobi"
	@echo "  make clean                 - Limpiar archivos compilados"
	@echo "  make help                  - Mostrar esta ayuda"

# Objetivos que no son archivos
.PHONY: all setup build_info test_gurobi path_based path_based_original flow_based flow_based_relaxed rooted_tree_based run_test clean check_gurobi help FORCE

