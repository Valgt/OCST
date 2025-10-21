### 4.2.1 Formulación Basada en Caminos (Path-Based) - Versión Corregida

Contreras, Fernández y Marín [CFM10a] propusieron la formulación Basada en Caminos (Path-Based, PB). Esta implementación corrige los problemas críticos identificados en la formulación original mediante la incorporación de cortes SEC (Subtour Elimination Constraints) y separación fraccionaria robusta.

Sea $I = (G, c, R, w)$ una instancia del problema OCST, donde $G = (V, E)$. Para cada requerimiento $r \in R$, consideramos el dígrafo $D = (V, A)$ (asociado con $G$ y $r$), en el cual queremos encontrar un camino que una $(r_o, r_d)$.

**Variables de Decisión:**
- $x_{ij} \in \{0,1\}$: Variable binaria que indica si la arista $(i,j) \in E$ pertenece al árbol solución $T$
- $y_{ij}^r \geq 0$: Variable continua que indica el flujo unitario del requerimiento $r$ a través del arco $(i,j) \in A$

**Formulación Corregida:**

$$
\begin{aligned}
\min \sum_{r \in R} w_r \sum_{(i,j) \in A} c_{ij} y_{ij}^r && (4.17) \\
\text{s.t.} \quad \sum_{(i,j) \in E} x_{ij} &= |V| - 1 && (4.18) \\
\sum_{i: (i, r_d) \in A} y_{i, r_d}^r &= 1 && \forall r \in R \quad (4.19) \\
\sum_{k: (r_o, k) \in A} y_{r_o, k}^r &= 1 && \forall r \in R \quad (4.20) \\
\sum_{i: (i,j) \in A} y_{ij}^r - \sum_{k: (j,k) \in A} y_{jk}^r &= 0 && \forall r \in R, \forall j \in V \setminus \{r_o, r_d\} \quad (4.21) \\
y_{ij}^r + y_{ji}^r &\leq x_{ij} && \forall r \in R, \forall (i,j) \in E \quad (4.22) \\
x_{ij} &\in \{0, 1\} && \forall (i,j) \in E \quad (4.23) \\
y_{ij}^r &\geq 0 && \forall r \in R, \forall (i,j) \in A \quad (4.24)
\end{aligned}
$$

**Restricciones Adicionales (SEC Callback):**

Para garantizar la conectividad y eliminar subciclos, se implementan cortes lazy:

1. **Cortes de Conectividad:** Para cualquier conjunto $S \subset V$ con $|S| < |V|$:
   $$\sum_{i \in S, j \notin S} x_{ij} \geq 1 \quad (4.25)$$

2. **Cortes de Ciclo:** Para cualquier ciclo $C$ detectado en la solución:
   $$\sum_{(i,j) \in C} x_{ij} \leq |C| - 1 \quad (4.26)$$

**Interpretación de las Restricciones:**

- **(4.18)**: El árbol debe tener exactamente $|V|-1$ aristas
- **(4.19)**: Cada requerimiento debe tener exactamente un arco entrante al destino
- **(4.20)**: Cada requerimiento debe tener exactamente un arco saliente del origen  
- **(4.21)**: Conservación de flujo en nodos intermedios (flujo unitario)
- **(4.22)**: Acoplamiento entre variables de flujo y variables de arista
- **(4.25)**: Garantiza conectividad mediante cortes de frontera
- **(4.26)**: Elimina ciclos mediante cortes de subciclo

**Complejidad:**
- **Variables**: $O(|E| + |R| \cdot |A|) = O(n^2 + |R| \cdot n^2) = O(n^4)$
- **Restricciones**: $O(|R| \cdot n) = O(n^3)$ (sin contar cortes lazy)

**Características Clave de la Implementación:**

1. **Flujo Unitario**: Cada requerimiento usa flujo de valor 1, independientemente de su peso $w_r$
2. **SEC Callback**: Implementa separación lazy de cortes de conectividad y ciclo
3. **Separación Fraccionaria**: Usa algoritmo max-flow min-cut para detectar violaciones
4. **Precisión Numérica**: Maneja capacidades fraccionarias con precisión `double`
5. **Warm-start**: Inicializa con solución MST para acelerar convergencia