#include "mbc.h"
#include "../utils/vertexset.hpp"
#include "../utils/bigraph.hpp"
#include "../utils/ordering.hpp"
#include "../utils/log.hpp"
#include "../utils/traversal.hpp"
#include <chrono>
#include <queue>

using namespace logging;

void MBC::run(const std::string &dataPath, int q[2], int flags) {
    MBC solver;
    solver.findMBC(dataPath, q, flags);
}

void MBC::findMBC(const std::string &dataPath, int q[2], int flags) {
	BiGraph Graph(dataPath);
	logBiGraph(Graph);

	lb[0] = q[0];
	lb[1] = q[1];
	MBC::flags = flags;

	numBranches = numPivoting = numBipartite = \
	numUbPruned = branchTime = reductionTime = 0;

	for (int s = 0; s <= 1; ++s) {
		Ss[s].reserve(Graph.n[s]);
		S[s].reserve(Graph.n[s]);
		C[s].reserve(Graph.n[s]);
		X[s].reserve(Graph.n[s]);
		degS[s].resize(Graph.n[s]);
		degC[s].resize(Graph.n[s]);
	}

	if (flags & FLAG_HEU) {
		log("Start heuristic...");
		auto heuristicStartTime = std::chrono::steady_clock::now();
		heuristic(Graph);
		int heuristicTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-heuristicStartTime).count();
		log("Heuristic done! Time: %d ms", heuristicTime);
		log("G[S*] info: |U|=%d, |V|=%d, |E|=%d", Ss[0].size(), Ss[1].size(), Ss[0].size() * Ss[1].size());
		logSetInfo(Ss[0], "U*");
		logSetInfo(Ss[1], "V*");

	}
	
	log("Start searching...");
	auto searchStartTime = std::chrono::steady_clock::now();
	lb[0] = Graph.maxDeg[1];
    while (lb[0] > q[0]) {
        MBC::lb[1] = std::max(q[1], (Ss[0].size() * Ss[1].size()) / lb[0]);
        MBC::lb[0] = std::max(q[0], lb[0] >> 1);
        log("*** lb = (%d, %d) ***", lb[0], lb[1]);

        G = (flags & FLAG_CORE) ? core(Graph, lb[1], lb[0]) : Graph;
        logBiGraph(G);

        if (G.numVertices(0) < lb[0] || G.numVertices(1) < lb[1]) continue;

        if (flags & FLAG_CN) G = comm(G, lb);
        logBiGraph(G);

        if (G.numVertices(0) < lb[0] || G.numVertices(1) < lb[1]) continue;

        auto branchStartTime = std::chrono::steady_clock::now();
        searchAll(); 
        branchTime += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - branchStartTime).count();
    }
	int searchTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - searchStartTime).count();
	log("Searching done! Total time: %d ms, Branch time: %d ms, branch num: %d (pivoting: %d, bipartite: %d), ub pruned num: %d", 
		searchTime, branchTime, numBranches, numPivoting, numBipartite, numUbPruned);

	log("G[S*] info: |U|=%d, |V|=%d, |E|=%d", Ss[0].size(), Ss[1].size(), Ss[0].size() * Ss[1].size());
	logSetInfo(Ss[0], "U*");
	logSetInfo(Ss[1], "V*");
}

BiGraph MBC::core(BiGraph &G, int a, int b) {
	std::queue<int> q[2];
	std::vector<bool> vis[2] = {std::vector<bool>(G.n[0]), std::vector<bool>(G.n[1])};
	std::vector<int> deg[2] = {std::vector<int>(G.n[0]), std::vector<int>(G.n[1])};
	int c[2] = {std::max(a, 0), std::max(b, 0)};

	for (int s = 0; s <= 1; ++s) {
		for (int v : G.V[s]) {
			deg[s][v] = G.degree(s, v);
			if (deg[s][v] < c[s]) {
				vis[s][v] = true;
				q[s].push(v);
			}
		}
	}

	while (!q[0].empty() || !q[1].empty()) {
		for (int s = 0; s <= 1; ++s) {
			while (!q[s].empty()) {
				int u = q[s].front(); q[s].pop();
				for (int v : G.nbr[s][u]) if (!vis[s^1][v] && --deg[s^1][v] < c[s^1]) {
					vis[s^1][v] = true;
					q[s^1].push(v);
				}
			}
		}
	}

	BiGraph Core;

	for (int u : G.V[0]) if (!vis[0][u])
		for (int v : G.nbr[0][u]) if (!vis[1][v])
			Core.addEdge(u, v);

	return Core;
}

BiGraph MBC::comm(BiGraph &G, int q[2]) {
	std::vector<bool> visv[2] = {std::vector<bool>(G.n[0]), std::vector<bool>(G.n[1])};
	std::vector<int> deg[2] = {std::vector<int>(G.n[0]), std::vector<int>(G.n[1])};
	std::vector<CuckooHash> vise(G.n[0]);
	std::vector<int> cn(std::max(G.n[0], G.n[1]));
	std::vector<int> cnt[2] = {std::vector<int>(G.n[0]), std::vector<int>(G.n[1])};

	for (int s = 0; s <= 1; ++s) {
		for (int v : G.V[s]) {
			deg[s][v] = G.degree(s, v);

		}
	}
	for (int s = 0; s <= 1; ++s) {
		for (int v : G.V[s]) {
			if (deg[s][v] < q[s^1]) {
				visv[s][v] = true;
				for (int w : G.nbr[s][v]) if (--deg[s^1][w] < q[s])
					visv[s^1][w] = true;
			}
		}
	}
			

	Ordering o;
	o.degeneracyOrdering(G);
	for (int t = 0; t < COMM_ROUNDS; ++t) {
	// for (int side = 0; side <= 1; ++side) {
		for (int i = 0; i < o.numOrdered; ++i) {
			//fprintf(stderr, "\rCommon Neighbor Reduction: %d/%d", o.numOrdered-i, o.numOrdered);
			int s = o.ordered[i] >= G.n[0];
			int u = o.ordered[i] - s * G.n[0];
			// if (side != s) continue;
			if (visv[s][u]) continue;
			for (int v : G.nbr[s][u]) if (!visv[s^1][v]/* && !(s ? vise[v].find(u) : vise[u].find(v))*/)
				for (int w : G.nbr[s^1][v]) if (!visv[s][w]/* && !(s ? vise[v].find(w) : vise[w].find(v))*/)
					cn[w] = 0;
			for (int v : G.nbr[s][u]) if (!visv[s^1][v]/* && !(s ? vise[v].find(u) : vise[u].find(v))*/)
				for (int w : G.nbr[s^1][v]) if (!visv[s][w]/* && !(s ? vise[v].find(w) : vise[w].find(v))*/)
					++cn[w];

			int cntv = 0;
			for (int v : G.nbr[s][u]) {
				int cntw = 0;
				if (visv[s^1][v] || (s ? vise[v].find(u) : vise[u].find(v))) continue;
				for (int w : G.nbr[s^1][v]) if (!visv[s][w] && cn[w] >= q[s^1] && ++cntw >= q[s]) break;
				if (cntw < q[s]) s ? vise[v].insert(u) : vise[u].insert(v);
				else if (++cntv >= q[s^1]) break;
			}


			if (cntv < q[s^1]) {
				visv[s][u] = true;
				for (int v : G.nbr[s][u]) if (--deg[s^1][v] < q[s])
					visv[s^1][v] = true;
			}
		}
	}

	BiGraph CN;
	for (int u : G.V[0]) if (!visv[0][u])
		for (int v : G.nbr[0][u]) if (!visv[1][v] && !vise[u].find(v))
			CN.addEdge(u, v);
	return CN;
}

void MBC::heuristic(BiGraph &Graph) {

	// G = core(Graph, lb[1], lb[0]);

	Ordering o;
	o.degeneracyOrdering(G);

    for (int s = 0; s <= 1; ++s) {
        S[s].clear();
    }

    for (int i = o.numOrdered-1; i >= 0; --i) {
        int uSide = o.ordered[i] >= G.n[0];
        int u = o.ordered[i] - uSide * G.n[0];
        if (nnbS(uSide, u) > 0) continue;
        addS(uSide, u);
    }

    if (S[0].size() >= lb[0] && S[1].size() >= lb[1]) {
        for (int s = 0; s <= 1; ++s) {
            Ss[s].clear();
            for (int v : S[s]) Ss[s].push(v);
        }
    }

}

void MBC::searchAll() {
	for (int s = 0; s <= 1; ++s) {
		S[s].clear(); C[s].clear(); X[s].clear();
    }

    for (int s = 0; s <= 1; ++s) {
        for (int u : G.V[s]) {
            degS[s][u] = 0;
            degC[s][u] = G.degree(s, u);
            C[s].push(u);
        }
    }
	branch(0);
}

void MBC::branch(int dep) {

    if (C[0].size() >= lb[0] && S[1].size() >= lb[1] && C[0].size()*S[1].size() > Ss[0].size()*Ss[1].size()) {
        Ss[0].clear(); Ss[1].clear();
        for (int u : C[0]) Ss[0].push(u);
        for (int v : S[1]) Ss[1].push(v);
        log("New S*! |E|=%d", Ss[0].size()*Ss[1].size());
    }

    BakPos pos;
    pos.backup(C);

    for (int v : C[1]) {

        moveC2S(1, v);

        BakPos posC, posX; 
        posC.backup(C);
        posX.backup(X);
        update(1, v);
        
        bool flagX = false;
        for (int x : X[1]) {
            if (nnbC(1, x) == 0) {
                flagX = true;
                break;
            }
        }

        if (!flagX && C[0].size() >= lb[0] && C[1].size() + S[1].size() >= lb[1] && 
        ((long long)C[0].size() * (S[1].size()+C[1].size())) > Ss[0].size() * Ss[1].size()) {
            branch(dep+1);
        }

        restore(posC, posX);

        subS(1, v);
        X[1].push(v);
    }

    for (int i = C[1].backPos(); i < pos.p[1][1]; ++i) {
        X[1].popBack(C[1][i]);
        addC(1, C[1][i]);
    }

}

void MBC::update(int uSide, int u) {
	using namespace traversal;

    for (int v : C[0]) if (nnbS(0, v) > 0)
        subC(0, v);

    for (int v : C[1]) if (degSub(1, v) < lb[0]) 
        subC(1, v);

    for (int v : reverse(C[1])) if (nnbSub(1, v) == 0)
        moveC2S(1, v);

    for (int v : X[1]) if (degSub(1, v) < lb[0])
        X[1].pop(v);
}

void MBC::restore(BakPos &posC, BakPos &posX) {
    for (int i = C[1].backPos(); i < posC.p[1][1]; ++i) {
        moveS2C(1, C[1][i]);
    }

	for (int s = 0; s <= 1; ++s) {
		for (int i = C[s].frontPos()-1; i >= posC.p[0][s]; --i) {
			addC(s, C[s][i]);
        }
    }

    for (int i = X[1].frontPos()-1; i >= posX.p[0][1]; --i) {
        X[1].push(X[1][i]);
    }

}