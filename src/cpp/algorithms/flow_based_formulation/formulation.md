### 4.2.2 Formulación Basada en Flujos (Flow-Based)

En la formulación Basada en Flujos (Flow-Based, FB), el problema OCST se modela como un conjunto de flujos en un árbol. Esta idea fue propuesta por Fernández et al. [FLH+13]. Dada una instancia $(G, c, R, w)$ del problema OCST, donde $G = (V, E)$, queremos encontrar un subgrafo $H$ de $G$ tal que $H$ sea un árbol. Además, para cada $o \in V$, obtenemos una $o$-arborescencia contenida en $D(H)$, el dígrafo asociado con $H$. A través de cada una de estas arborescencias, enviamos flujo desde $o$ hacia los otros vértices.

Consideremos la instancia $(G, c, R, w)$ mostrada en la Figura 4.6. Para explicar cómo calcular el costo de comunicación de un árbol, consideraremos el árbol generador $T$, compuesto por las aristas más gruesas $\{0, 1\}$, $\{0, 2\}$, $\{1, 3\}$ y $\{1, 4\}$.

| Requerimiento (r) | Demanda ($w_r$) |
| :---------------: | :-------------: |
|      (0, 1)       |        4        |
|      (0, 3)       |        3        |
|      (2, 1)       |        2        |
|      (2, 4)       |        1        |
|      (3, 4)       |        5        |

**Figura 4.6:** Una instancia del problema OCST y una solución factible $T$, indicada con aristas más gruesas.

En este caso, nótese que $R$ contiene tres orígenes diferentes para todos los requerimientos; estos son los vértices 0, 2 y 3. Para calcular el costo de comunicación de un árbol, cada arista $ij$ en el árbol contribuye al costo total de comunicación con el valor: $\sum_{ij \in P(r_o, r_d)} c_{ij} w_r$.

**Figura 4.7:** Todos los caminos de comunicación en $T$.

Calculamos ahora el costo de comunicación de $T$, comenzando con los requerimientos que tienen su origen en el vértice 0. Como la arista $\{0, 1\}$ está en el camino desde los requerimientos con destinos 1 y 3, entonces su contribución es $c_{01}(w_{01} + w_{03}) = 1(4 + 3) = 7$; la arista $\{1, 3\}$ está en el camino al requerimiento con destino 3, entonces su contribución es $c_{13}w_{03} = 3(3) = 9$. Así, su contribución total es $7 + 9 = 16$ para todos los requerimientos con origen 0. Las aristas restantes contribuyen 0 ya que no participan en ningún camino de comunicación con origen 0. De manera similar, concluimos que los caminos de comunicación que comienzan desde 2 y 3 contribuyen 8 y 25, respectivamente.

Podemos observar los flujos de red asociados con este ejemplo en la Figura 4.8.

**Figura 4.8:** Todos los caminos de comunicación en $T$ como flujos de red.

Más formalmente, consideraremos una variable binaria $x = (x_e)_{e \in E}$, tal que $x_e = 1$ si, y solo si, $e$ pertenece a $H$. Además de eso, para cada vértice $o \in V$, tenemos una variable $y = (y_a^o)$, tal que $y_{ij}^o = 1$ si, y solo si, $ij$ pertenece a la $o$-arborescencia. Finalmente, la variable $f_{ij}^o$ indica la cantidad de flujo en el arco $ij$ que pertenece a la red asociada con el origen $o$.

La formulación Basada en Flujos (FB) se define por el siguiente programa lineal entero mixto:

$$
\begin{aligned}
\min \sum_{o \in V} \sum_{ij \in A} c_{ij} f_{ij}^o && (4.25) \\
\text{s.t.} \quad \sum_{ij \in E} x_{ij} &= |V| - 1 && (4.26) \\
\sum_{ij \in E(S)} x_{ij} &\leq |S| - 1 && \forall S \subset V, S \neq \emptyset \quad (4.27) \\
\sum_{ij \in A} f_{ij}^o - \sum_{jk \in A} f_{jk}^o &= w_{oj} && \forall o \in V, \forall j \in V \setminus \{o\} \quad (4.28) \\
\sum_{ok \in A} f_{ok}^o &= \sum_{(o,d) \in R} w_{od} && \forall o \in V \quad (4.29) \\
f_{ij}^o &\leq M y_{ij}^o && \forall o \in V, \forall ij \in A \quad (4.30) \\
\sum_{ij \in A} y_{ij}^o &= |V| - 1 && \forall o \in V \quad (4.31) \\
y_{ij}^o + y_{ji}^o &\leq x_{ij} && \forall o \in V, \forall ij \in E \quad (4.32) \\
f_{ij}^o &\geq 0 && \forall o \in V, \forall ij \in A \quad (4.33) \\
y_{ij}^o &\in \{0, 1\} && \forall o \in V, \forall ij \in A \quad (4.34) \\
x_{ij} &\in \{0, 1\} && \forall ij \in E \quad (4.35)
\end{aligned}
$$

Nótese que la formulación FB puede verse como $|V|$ instancias del problema OCST de 1-origen. Las restricciones (4.26) y (4.27) aseguran que el soporte de $x$ induce un árbol generador, digamos $H$. Además de eso, las restricciones (4.31) y (4.32) garantizan que el dígrafo inducido por el soporte de $y^o$ es una $o$-arborescencia contenida en $D(H)$. La familia de restricciones (4.29) garantiza que el flujo inicial que sale de $o$ es igual a la suma de todas las demandas con su origen en $o$. Las restricciones (4.30) y (4.32) aseguran que si la variable $x_{ij}$ está activada, solo uno de los arcos $ij$ o $ji$ puede tener un flujo positivo. Las restricciones (4.28) son las demandas de los vértices. Estas restricciones garantizan que el flujo retenido por un vértice $j$ es igual a $w_{oj}$. Las restricciones (4.29) garantizan que cada vértice elegido como origen es la única fuente para su red asociada (demanda negativa).

Nótese que, para garantizar la conectividad, no es suficiente agregar requerimientos artificiales a $R$ con demanda cero. Por lo tanto, contrario a la formulación PB, consideramos las Restricciones de Eliminación de Subtours (4.27) para garantizar la conectividad de $H$.