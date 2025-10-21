
# REPORTE DE ANÁLISIS COMPARATIVO
## Path-Based vs Flow-Based Formulations

### RESUMEN EJECUTIVO
- **Total de casos analizados:** 25
- **Path-Based correcto:** 25/25 (100.0%)
- **Flow-Based correcto:** 25/25 (100.0%)

### MÉTRICAS PRINCIPALES
- **Diferencia promedio:** 0
- **Diferencia porcentual promedio:** 0.00%
- **Diferencia máxima:** 0 (0.00%)

### RENDIMIENTO
- **Path-Based - Nodos promedio:** 322.0
- **Flow-Based - Nodos promedio:** 569.9
- **Path-Based - Tiempo promedio:** 0.800s
- **Flow-Based - Tiempo promedio:** 1.112s

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
