# MaximumDefectiveBiclique
Maximum defective biclique search

### Two proposed algorithms and a baseline algorithm
- MDBB, a branch-and-bound-based algorithm
- MDBP, a pivoting-based algorithm
- MDC, an algorithm adapted from maximal defective clique enumeration

### Build:
```bash
cmake . && make
```
### Run:
##### Basic usage:
```bash
bin/run -d <dataset> -k <integer> -q <size threshold> -a <algorithm: p/b/mdc> [commands]
```
### Help:
```bash
bin/run -h
```

### dataset format:
The first line contains three integers: number of edges, number of vertices in $U$ and number of vertices in $V$;

Each of the next $m$ lines contains two vertices connected by an edge.

> $m$ $n_U$, $n_V$
> 
> $u_1$ $v_1$
> 
> $u_2$ $v_2$
> 
> ...
