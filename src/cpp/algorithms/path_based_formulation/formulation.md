### 4.2.1 Formulación Basada en Caminos (Path-Based)

Contreras, Fernández y Marín [CFM10a] propusieron la formulación Basada en Caminos (Path-Based, PB). A continuación, describiremos la idea detrás de esta formulación.

Sea $I = (G, c, R, w)$ una instancia del problema OCST, donde $G = (V, E)$. Para cada requerimiento $r \in R$, consideramos el dígrafo $D = (V, A)$ (asociado con $G$ y $r$), en el cual queremos encontrar un camino que una $(r_o, r_d)$.

En la Figura 4.2 se muestra una instancia del problema OCST. Además, en la misma figura, indicamos con aristas más gruesas una solución, digamos $T$, para dicha instancia. Adicionalmente, en la Figura 4.3 se muestran los caminos de comunicación, para $T$, entre cada par de origen y destino.



| Requerimiento (r) | Demanda ($w_r$) |
| :---------------: | :-------------: |
|      (0, 1)       |        4        |
|      (2, 1)       |        2        |
|      (2, 3)       |        1        |

**Figura 4.2:** Una instancia del problema OCST y una solución factible $T$, indicada con aristas más gruesas.

---

Consideramos una variable binaria $x = (x_e)_{e \in E}$ tal que $x_e = 1$ si, y solo si, $e$ pertenece a nuestra solución, digamos $T$. Además, consideramos una variable binaria $y = (y_{ar})$, donde $r \in R$ y $a \in A(D)$, tal que $y_{ar} = 1$ si, y solo si, $a$ pertenece al camino $P(r_o, r_d)$ en $T$. Nótese que no estamos considerando SEC (restricciones de eliminación de subciclos) porque modificamos el conjunto $R$, como se describe en los párrafos siguientes, para asegurar la conectividad.

La formulación Basada en Caminos (PB) se define por el siguiente programa lineal entero mixto:



**Figura 4.3:** Todos los caminos de comunicación para $R$, modelados como flujos de red, en $T$.

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

Las restricciones (4.19), (4.20) y (4.21) modelan el camino entre el origen y el destino de cada requerimiento $r \in R$ como un flujo unitario. Además, la restricción (4.22) asegura que el soporte de $y^r$ induce un camino en $D$ que une $r_o$ y $r_d$.

---

La Figura 4.4 muestra una instancia $(G, c, R, w)$ del problema OCST, donde $G = (V, E)$ es el grafo representado y los requerimientos $r \in R$ se indican en la tabla. Adicionalmente, en la Figura 4.5, mostramos una solución que no es conexa. Obsérvese que, agregar un requerimiento artificial $(0, 4)$ a $R$ con $w_{04} = 0$ garantizará la conectividad de nuestra solución. Así, fijaremos un vértice $o$ en $V$ y para cada vértice $v \neq o$ en $V$, agregamos el par $(o, v)$ a $R$ con $w_{o,v} = 0$ si dicho par no existe. Esto asegura que exista un camino entre cada par de vértices. Por lo tanto, $T$ es conexo. En consecuencia, por la restricción (4.18), $T$ es un árbol.



| Requerimiento (r) | Demanda ($w_r$) |
| :---------------: | :-------------: |
|      (0, 1)       |        4        |
|      (2, 1)       |        2        |
|      (2, 0)       |        1        |
|      (4, 5)       |        3        |

**Figura 4.4:** Una instancia del problema OCST.



**Figura 4.5:** Un grafo $T$ que no es conexo, inducido por el soporte de una solución óptima para la formulación PB.

Según Luna [Lun16], la formulación PB produce la cota de relajación lineal más ajustada. A pesar de eso, debido al enorme número de variables y restricciones (hasta $O(n^4)$), solo se logra un buen rendimiento cuando las instancias son pequeñas o medianas.