#!/usr/bin/env python3
"""
Script para verificar el entorno de desarrollo de OCST
Incluye verificación de Gurobi y permisos académicos
"""

import os
import sys
import subprocess
import platform
from pathlib import Path

class EnvironmentChecker:
    def __init__(self):
        self.errors = []
        self.warnings = []
        
    def check_gurobi_home(self):
        """Verificar que GUROBI_HOME esté configurado"""
        gurobi_home = os.environ.get('GUROBI_HOME')
        if not gurobi_home:
            self.errors.append("❌ GUROBI_HOME no está configurado")
            return False
        
        if not os.path.exists(gurobi_home):
            self.errors.append(f"❌ GUROBI_HOME apunta a un directorio inexistente: {gurobi_home}")
            return False
            
        print(f"✓ GUROBI_HOME configurado: {gurobi_home}")
        return True
    
    def check_gurobi_directories(self):
        """Verificar directorios de Gurobi"""
        gurobi_home = os.environ.get('GUROBI_HOME')
        if not gurobi_home:
            return False
            
        include_dir = os.path.join(gurobi_home, 'include')
        lib_dir = os.path.join(gurobi_home, 'lib')
        
        if not os.path.exists(include_dir):
            self.errors.append(f"❌ Directorio include no encontrado: {include_dir}")
            return False
            
        if not os.path.exists(lib_dir):
            self.errors.append(f"❌ Directorio lib no encontrado: {lib_dir}")
            return False
            
        print(f"✓ Directorio include: {include_dir}")
        print(f"✓ Directorio lib: {lib_dir}")
        return True
    
    def check_compiler(self):
        """Verificar que g++ esté disponible"""
        try:
            result = subprocess.run(['g++', '--version'], 
                                 capture_output=True, text=True, timeout=10)
            if result.returncode == 0:
                version = result.stdout.split('\n')[0]
                print(f"✓ Compilador C++: {version}")
                return True
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
            
        self.errors.append("❌ Compilador g++ no encontrado")
        return False
    
    def check_python_packages(self):
        """Verificar paquetes de Python necesarios"""
        required_packages = ['numpy', 'matplotlib', 'pandas']
        missing_packages = []
        
        for package in required_packages:
            try:
                __import__(package)
                print(f"✓ {package} disponible")
            except ImportError:
                missing_packages.append(package)
                self.warnings.append(f"⚠️  {package} no instalado (opcional)")
        
        return len(missing_packages) == 0
    
    def run_gurobi_test(self):
        """Ejecutar el test de Gurobi compilado"""
        test_executable = Path("build/executables/test_gurobi")
        
        if not test_executable.exists():
            print("❌ Test ejecutable no encontrado. Compilando...")
            try:
                result = subprocess.run(['make', 'test_gurobi'], 
                                     capture_output=True, text=True, timeout=60)
                if result.returncode != 0:
                    self.errors.append("❌ Error al compilar test de Gurobi")
                    print(result.stderr)
                    return False
            except subprocess.TimeoutExpired:
                self.errors.append("❌ Timeout al compilar test de Gurobi")
                return False
        
        print("Ejecutando test de permisos académicos de Gurobi...")
        print("=" * 50)
        
        try:
            result = subprocess.run([str(test_executable)], 
                                 capture_output=True, text=True, timeout=30)
            
            print(result.stdout)
            if result.stderr:
                print("Stderr:", result.stderr)
            
            if result.returncode == 0:
                print("✓ Test de Gurobi ejecutado exitosamente")
                print("✓ Permisos académicos verificados")
                return True
            else:
                self.errors.append("❌ Test de Gurobi falló")
                return False
                
        except subprocess.TimeoutExpired:
            self.errors.append("❌ Timeout al ejecutar test de Gurobi")
            return False
        except Exception as e:
            self.errors.append(f"❌ Error al ejecutar test: {e}")
            return False
    
    def print_system_info(self):
        """Mostrar información del sistema"""
        print("\n" + "=" * 50)
        print("INFORMACIÓN DEL SISTEMA")
        print("=" * 50)
        print(f"Sistema operativo: {platform.system()} {platform.release()}")
        print(f"Arquitectura: {platform.machine()}")
        print(f"Python: {sys.version}")
        print(f"Directorio de trabajo: {os.getcwd()}")
        print("=" * 50)
    
    def run_all_checks(self):
        """Ejecutar todas las verificaciones"""
        print("🔍 VERIFICANDO ENTORNO DE DESARROLLO OCST")
        print("=" * 50)
        
        self.print_system_info()
        
        print("\n🔧 VERIFICACIONES TÉCNICAS")
        print("-" * 30)
        
        checks = [
            self.check_gurobi_home(),
            self.check_gurobi_directories(),
            self.check_compiler(),
            self.check_python_packages(),
        ]
        
        print("\n🧪 EJECUTANDO TEST DE GUROBI")
        print("-" * 30)
        gurobi_test = self.run_gurobi_test()
        
        print("\n📊 RESUMEN")
        print("=" * 50)
        
        if self.errors:
            print("❌ ERRORES ENCONTRADOS:")
            for error in self.errors:
                print(f"  {error}")
        
        if self.warnings:
            print("\n⚠️  ADVERTENCIAS:")
            for warning in self.warnings:
                print(f"  {warning}")
        
        if not self.errors and gurobi_test:
            print("✅ ¡ENTORNO CONFIGURADO CORRECTAMENTE!")
            print("   Todos los tests pasaron exitosamente")
            print("   Permisos académicos de Gurobi verificados")
            return True
        else:
            print("❌ CONFIGURACIÓN INCOMPLETA")
            print("   Revisa los errores anteriores")
            return False

def main():
    """Función principal"""
    checker = EnvironmentChecker()
    success = checker.run_all_checks()
    
    if success:
        print("\n🎉 ¡Listo para comenzar el desarrollo de OCST!")
        sys.exit(0)
    else:
        print("\n💡 SUGERENCIAS:")
        print("   1. Verifica que GUROBI_HOME esté configurado")
        print("   2. Asegúrate de tener una licencia académica de Gurobi")
        print("   3. Instala las dependencias de Python: pip install numpy matplotlib pandas")
        sys.exit(1)

if __name__ == "__main__":
    main()


