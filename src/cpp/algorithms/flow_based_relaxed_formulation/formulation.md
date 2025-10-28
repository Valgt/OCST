## Relaxation of the Flow-Based formulation

In the Flow-Based formulation, the set of feasible solutions corresponds to arborescences.  
If we relax the problem to find digraphs whose underlying graph corresponds to a tree, the set of optimal solutions is the same, as we will show later.  
Note that this relaxation is on the set of feasible solutions. Thus, we will call this formulation the **Relaxation of the Flow-Based (RFB) formulation**.

Given an instance  
\( I = (G, c, R, w) \)  
of the OCST problem, we want to find a spanning tree \( T \) of \( G \).

For that, for each vertex \( o \) we obtain a network such that we distribute flow from \( o \) to the remaining vertices. Each variable \( f^o_{ij} \) indicates the quantity of flow in the arc \( a_{ij} \) that belongs to the network associated to origin \( o \).

The RFB formulation was introduced by [Lun16] in 2016. It is the following mixed integer linear program:

\[
\min \sum_{o \in V} \sum_{ij \in A} c_{ij} f^o_{ij}
\tag{4.36}
\]

subject to

\[
\sum_{ij \in E} x_{ij} = |V| - 1
\tag{4.37}
\]

\[
\sum_{ij \in E(S)} x_{ij} \le |S| - 1 
\quad \forall S \subset V,\, S \neq \emptyset
\tag{4.38}
\]

\[
\sum_{ij \in A} f^o_{ij}
\;-\;
\sum_{jk \in A} f^o_{jk}
=
w^o_j
\quad \forall o \in V,\ \forall j \in V \setminus \{o\}
\tag{4.39}
\]

\[
\sum_{ok \in A} f^o_{ok}
=
\sum_{(o,d) \in R} w^o_d
\quad \forall o \in V
\tag{4.40}
\]

\[
f^o_{ij} + f^o_{ji}
\;\le\;
\Bigg(
\sum_{(o,d) \in R} w^o_d
\Bigg)
x_{ij}
\quad \forall o \in V,\ \forall ij \in E
\tag{4.41}
\]

\[
f^o_{ij} \ge 0
\quad \forall o \in V,\ \forall ij \in A
\tag{4.42}
\]

\[
x_{ij} \in \{0,1\}
\quad \forall ij \in E
\tag{4.43}
\]

---

### Meaning of the constraints

- **(4.37)–(4.38):**  
  These ensure that the support of \( x \) induces a spanning tree.  
  - (4.37) forces exactly \(|V|-1\) edges.  
  - (4.38) are subtour elimination / acyclicity constraints: any subset \( S \) can have at most \(|S|-1\) edges.

- **(4.40):**  
  Guarantees that the initial flow going out from \( o \) equals the sum of the demands of all requirements that have origin at \( o \).

- **(4.41):**  
  Ensures that if any of the arcs \( ij \) or \( ji \) has nonzero flow, then the associated variable \( x_{ij} \) must be activated.  
  Note: both arcs \( ij \) and \( ji \) may still simultaneously have nonzero flow.

- **(4.39):**  
  Guarantees flow conservation: the flow retained by a vertex \( j \) in the network associated with \( o \) is equal to \( w^o_j \).

In contrast with the FB formulation, we do **not** consider variables \( y \).  
However, if an optimal solution \((x,f)\) for this MILP exists, each digraph induced by the variables \( f^o \) is an \( o \)-arborescence, as shown in Proposition 4.2.1.

---

### Figure 4.9
**Procedure to delete arcs that form a cycle and still get a feasible solution.**

> (Descripción textual porque no tenemos la imagen:  
> Se observa cómo, si en el flujo aparecen arcos opuestos formando un ciclo, se puede reducir flujo en uno de ellos y mantener factibilidad, disminuyendo el costo.)

---

## Proposition 4.2.1

**Statement.**  
Let \( I = (G, c, R, w) \) be an instance of the OCST problem and let \( o \) be any vertex of \( G \).  
Let \((x,f)\) be an optimal solution of the RFB formulation for the instance \( I \), assuming the problem is feasible.  
Let \( T \) be the graph induced by the support of \( x \), and let \( T' \) be the digraph induced by the support of \( f^o \).  
Then the digraph \( T' \) is an \( o \)-arborescence contained in \( D(G) \).

### Proof.

1. First, observe that \( T \) is the underlying (undirected) graph of \( T' \).

2. Suppose, for contradiction, that \( T' \) contains opposite arcs.  
   Consider two opposite arcs in \( T' \), say \( a \) and \( d \), with \( f_a \le f_d \).  
   We can construct a new feasible solution \((x, f')\), where \( f' \) is defined, for each arc \( a' \in T \), as:

   \[
   f'_{a'} :=
   \begin{cases}
   0, & \text{if } a' = a \\
   f_d - f_a, & \text{if } a' = d \\
   f_{a'}, & \text{otherwise}
   \end{cases}
   \tag{4.44}
   \]

   Clearly, \((x,f')\) is a feasible solution with a smaller cost than \((x,f)\), contradicting the optimality of \((x,f)\).

3. Since \( T \) is a tree and each vertex \( u \) of \( T' \), different than \( o \), satisfies \( f^-_u + f^+_u \le 0 \) (i.e. net inflow \(\le 0\)), it follows that \( T' \) is an \( o \)-arborescence.

∎

---

## Corollary 4.2.2

Each digraph induced by \( f^o \) is an \( o \)-arborescence.  
Moreover, the optimal value of the RFB formulation is equal to the communication cost of an OCST.

---

## Complexity note

Since we consider \( n \) origins and the number of edges can be at most \( n^2 \), the FB formulation and its relaxation (RFB) have at most  
\[
O(n^3)
\]
variables.
