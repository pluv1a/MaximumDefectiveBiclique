#include "mdb.h"
#include "../utils/vertexset.hpp"
#include "../utils/bigraph.hpp"
#include "../utils/ordering.hpp"
#include "../utils/log.hpp"
#include "../utils/traversal.hpp"
#include <chrono>
#include <queue>

using namespace logging;

// #define PIVOTING_V2

// void MDB::cnExclusion(BiGraph &g, int q[2], int k) {
// 	static std::vector<int> cn;
// 	if (cn.size() < std::max(g.n[0], g.n[1])) cn.resize(std::max(g.n[0], g.n[1]));

// 	for (int s = 0; s <= 1; ++s) {
// 		for (int u : g.V[s]) {  
// 			coexist[s][u].clear();
// 			for (int v : g.nbr[s][u])
// 				for (int w : g.nbr[s^1][v])
// 					cn[w] = 0;
// 			for (int v : g.nbr[s][u])
// 				for (int w : g.nbr[s^1][v])
// 					if (++cn[w] == q[s^1]-k)
// 						coexist[s][u].insert(w);
// 		}
// 	}
// }

thread_local VertexSet MDB::S[2], MDB::C[2], MDB::X[2];
thread_local std::vector<int> MDB::degS[2], MDB::degC[2];
thread_local BiGraph MDB::G;
thread_local int MDB::numNnbS;

void MDB::findMDB(const std::string &dataPath, int q[2], int k, int flags, int numThreads) {
	BiGraph G(dataPath);
	logBiGraph(G);
	int sumDeg[2] = {0};
	for (int s = 0; s <= 1; ++s)
		for (int v : G.V[s])
			sumDeg[s] += G.degree(s, v);
	log("maxdegU = %d, maxdegV = %d, avgdegU = %d, avgdegV = %d", G.maxDeg[0], G.maxDeg[1], sumDeg[0] / G.V[0].size(), sumDeg[1] / G.V[1].size());

	lb[0] = q[0];
	lb[1] = q[1];
	MDB::k = k;
	MDB::flags = flags;
	MDB::numThreads = numThreads;

	numNnbS = numNnbSs = \
	numBranches = numPivoting = numBipartite = \
	numUbPruned = branchTime = reductionTime = 0;

	for (int s = 0; s <= 1; ++s) {
		Ss[s].reserve(G.n[s]);
		#pragma omp parallel num_threads(numThreads)
		{
			S[s].reserve(G.n[s]);
			C[s].reserve(G.n[s]);
			// X[s].reserve(G.n[s]);
			degS[s].resize(G.n[s]);
			degC[s].resize(G.n[s]);
		}
		// coexist[s].resize(G.n[s]);
	}

	if (flags & FLAG_HEU) {
		log("Start heuristic...");
		auto heuristicStartTime = std::chrono::steady_clock::now();
		heuristic(G);
		int heuristicTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-heuristicStartTime).count();
		log("Heuristic done! Time: %d ms", heuristicTime);
		log("G[S*] info: |U|=%d, |V|=%d, |E|=%d", Ss[0].size(), Ss[1].size(), numEdgesSs);
		// logSetInfo(Ss[0], "U*");
		// logSetInfo(Ss[1], "V*");

	}
	
	log("Start searching...");
	auto searchStartTime = std::chrono::steady_clock::now();
	BiGraph &Sub = this->G;

	if (flags & FLAG_PB) {
		lb[0] = G.maxDeg[1]+k;
		while (lb[0] > q[0]) {
			MDB::lb[1] = std::max(q[1], numEdgesSs / lb[0]);
			MDB::lb[0] = std::max(q[0], lb[0] >> 1);
			log("*** lb = (%d, %d) ***", lb[0], lb[1]);

			Sub = (flags & FLAG_CORE) ? core(G, lb[1]-k, lb[0]-k) : G;
			logBiGraph(Sub);

			if (Sub.numVertices(0) < lb[0] || Sub.numVertices(1) < lb[1]) continue;

			if (flags & FLAG_CN) Sub = comm(Sub, lb, k);
			logBiGraph(Sub);

			if (Sub.numVertices(0) < lb[0] || Sub.numVertices(1) < lb[1]) continue;

			// cnExclusion(Sub, lb, k);

			auto branchStartTime = std::chrono::steady_clock::now();
			if ((flags & FLAG_ORDER) && lb[0] > k && lb[1] > k) {
				log("Search scheme: RussianDoll"); 
				russianDoll();
			} 
			else {
				log("Search scheme: All");
				searchAll(); 
			}
			branchTime += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - branchStartTime).count();
		}
	}
	else {
		log("*** lb = (%d, %d) ***", lb[0], lb[1]);

		Sub = (flags & FLAG_CORE) ? core(G, lb[1]-k, lb[0]-k) : G;
		logBiGraph(Sub);

		if (flags & FLAG_CN) Sub = comm(Sub, lb, k);
		logBiGraph(Sub);

		auto branchStartTime = std::chrono::steady_clock::now();
		if ((flags & FLAG_ORDER) && lb[0] > k && lb[1] > k) {
			log("Search scheme: RussianDoll"); 
			russianDoll();
		} 
		else {
			log("Search scheme: All");
			searchAll(); 
		}
		branchTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - branchStartTime).count();
	}
	int searchTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - searchStartTime).count();
	log("Searching done! Total time: %d ms, Branch time: %d ms, branch num: %d (pivoting: %d, bipartite: %d), ub pruned num: %d", 
		searchTime, branchTime, numBranches, numPivoting, numBipartite, numUbPruned);

	log("G[S*] info: |U|=%d, |V|=%d, |E|=%d", Ss[0].size(), Ss[1].size(), numEdgesSs);
	// logSetInfo(Ss[0], "U*");
	// logSetInfo(Ss[1], "V*");

	// log("Missing edges:");
	// for (int u : Ss[0])
	// 	for (int v : Ss[1])
	// 		if (!G.connect(u, v))
	// 			log("(%d, %d)", u, v);
}

BiGraph MDB::core(BiGraph &G, int a, int b) {
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

BiGraph MDB::comm(BiGraph &G, int q[2], int k) {
	std::vector<bool> visv[2] = {std::vector<bool>(G.n[0]), std::vector<bool>(G.n[1])};
	std::vector<int> deg[2] = {std::vector<int>(G.n[0]), std::vector<int>(G.n[1])};
	std::vector<CuckooHash> vise(G.n[0]);
	thread_local static std::vector<int> cn(std::max(G.n[0], G.n[1]));
	std::vector<int> sumDeg[2] = {std::vector<int>(G.n[0]), std::vector<int>(G.n[1])};
	std::vector<int> orderedV[2];

	#pragma omp parallel num_threads(numThreads)
	{
		cn.resize(std::max(G.n[0], G.n[1]));
	}

	for (int s = 0; s <= 1; ++s) {
		// #pragma omp parallel for num_threads(numThreads)
		for (int u : G.V[s]) {
			orderedV[s].push_back(u);
			deg[s][u] = G.degree(s, u);
			for (int v : G.nbr[s][u]) {
				sumDeg[s][u] += G.degree(s^1, v);
			}
		}
		std::sort(orderedV[s].begin(), orderedV[s].end(), [&](int u, int v) {
			return sumDeg[s][u] > sumDeg[s][v];
		});
	}

	// Ordering o;
	// o.degeneracyOrdering(G);
	for (int s = 0; s <= 1; ++s) {
		#pragma omp parallel for num_threads(numThreads) schedule(dynamic)
		for (int i = 0; i < orderedV[s].size(); ++i) {
			//fprintf(stderr, "\rCommon Neighbor Reduction: %d/%d", o.numOrdered-i, o.numOrdered);
			// int s = o.ordered[i] >= G.n[0];
			// int u = o.ordered[i] - s * G.n[0];
			int u = orderedV[s][i];
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
				for (int w : G.nbr[s^1][v]) if (!visv[s][w] && cn[w] >= q[s^1]-k && ++cntw >= q[s]-k) break;
				if (cntw < q[s]-k) {
					#pragma omp critical(vise) 
					{
						s ? vise[v].insert(u) : vise[u].insert(v);
					}
				}
				else if (++cntv >= q[s^1]-k) break;
			}


			if (cntv < q[s^1]-k) {
				visv[s][u] = true;
				for (int v : G.nbr[s][u]) {
					#pragma omp atomic
					--deg[s^1][v];
					if (deg[s^1][v] < q[s]-k)
						visv[s^1][v] = true;
				}
			}
		}
	}

	BiGraph CN;
	for (int u : G.V[0]) if (!visv[0][u])
		for (int v : G.nbr[0][u]) if (!visv[1][v] && !vise[u].find(v))
			CN.addEdge(u, v);
	return CN;
}

void MDB::heuristic(BiGraph &G) {

	BiGraph Core = core(G, lb[1]-k, lb[0]-k);

	for (int s = 0; s <= 1; ++s) {
		S[s].clear();
		C[s].clear();
		degS[s].assign(Core.n[s], 0);
	}

	Ordering o;
	o.degeneracyOrdering(Core);

	for (int i = o.numOrdered-1; i >= 0; --i) {
		int s = o.ordered[i] >= Core.n[0];
		int u = o.ordered[i] - s * Core.n[0];
		if (numNnbS + nnbS(s, u) <= k) {
			S[s].push(u);
			for (int v : Core.nbr[s][u]) ++degS[s^1][v];
			numNnbS += nnbS(s, u);
		}
	}

	if (S[0].size() >= lb[0] && S[1].size() >= lb[1] && numEdgesS > numEdgesSs) {
		numNnbSs = numNnbS;
		for (int s = 0; s <= 1; ++s) {
			Ss[s].clear();
			for (int v : S[s])
				Ss[s].push(v);	
		}
	}


	for (int s = 0; s <= 1; ++s) {

		S[s].clear();

		for (int v : Core.V[s^1]) {
			S[s^1].push(v);
			degS[s^1][v] = 0;
		}

		for (int v : Core.V[s]) {
			C[s].push(v);
			degS[s][v] = Core.degree(s, v);
		}

		LinearHeap U(C[s], [&](int v){ return degS[s][v] + Core.maxDeg[s]*o.value[v+s*Core.n[0]]; });
		LinearHeap V(S[s^1], [&](int v){ return degS[s^1][v]; });

		numNnbS = 0;

		std::vector<std::vector<int>> bin(k+2);

		while (S[s].size() < lb[s] && C[s].size() > 0 && !U.empty()) {
			int u = U.max(); U.popMax();

			if (degS[s][u] < lb[s^1]-k) break;

			S[s].push(u);
			numNnbS += nnbS(s, u);
			for (int v : Core.nbr[s][u]) {
				V.inc(v);
				++degS[s^1][v];
			}

			int nd = 0;

			for (int i = 0; i < lb[s^1]; ++i)
				nd += nnbS(s^1, V.max(i));

			if (nd <= k) {
				while (numNnbS > k) {
					int v = V.min(); V.popMin();
					numNnbS -= nnbS(s^1, v);
					S[s^1].pop(v);
					for (int w : Core.nbr[s^1][v]) {
						U.dec(w);
						--degS[s][w];
					}
				}
			}
			else {
				S[s].pop(u);
				numNnbS -= nnbS(s, u);
				for (int v : Core.nbr[s][u]) {
					V.dec(v);
					--degS[s^1][v];
				}
			}
		}

		if (S[0].size() >= lb[0] && S[1].size() >= lb[1] && numEdgesS > numEdgesSs) {
			numNnbSs = numNnbS;
			for (int s = 0; s <= 1; ++s) {
				Ss[s].clear();
				for (int v : S[s])
					Ss[s].push(v);	
			}
		}

	}

}

void MDB::searchAll() {
	numNnbS = 0;
	for (int s = 0; s <= 1; ++s) {
		S[s].clear(); C[s].clear(); //X[s].clear();
		for (int u : G.V[s]) {
			degS[s][u] = 0;
			degC[s][u] = G.degree(s, u);
			C[s].push(u);
		}
	}
	//this->G = std::move(G);
	branch(0);
}

void MDB::russianDoll() {
	static Ordering o;
	thread_local static std::vector<int> q[2];

	int tSide = G.maxDeg[1] > G.maxDeg[0];	

	// auto addNbr2C = [&](int uSide, int u, int rk) {
	// 	for (int v : G.nbr[uSide][u])  {
	// 		if (X[uSide^1].inside(v) || (uSide != tSide && o.order[v+(uSide^1)*G.n[0]] < rk)) continue;
	// 		C[uSide^1].push(v);
	// 		++degC[uSide][u];
	// 		S[uSide].inside(u) ? ++degS[uSide^1][v] : ++degC[uSide^1][v];
	// 	}

	// };

	BiGraph G0 = std::move(G);


	auto prune = [&]() {
		for (int s = 0; s <= 1; ++s) {
			q[s].clear();
			for (int v : S[s]) if (degC[s][v]+degS[s][v] < lb[s^1]-k)
				return false;
			for (int v : C[s]) if (degC[s][v]+degS[s][v] < lb[s^1]-k) {
				q[s].push_back(v);
				C[s].pop(v);
			}
		}

		while (!q[0].empty() || !q[1].empty()) {
			for (int s = 0; s <= 1; ++s) {
				while (!q[s].empty()) {
					int v = q[s].back(); q[s].pop_back();
					//log("Prune: %d-%d", s, v);
					for (int w : G0.nbr[s][v]) if (S[s^1].inside(w) || C[s^1].inside(w)) {
						if (--degC[s^1][w]+degS[s^1][w] < lb[s]-k) {
							if (C[s^1].inside(w)) {
								q[s^1].push_back(w);
								C[s^1].pop(w);
							} 
							else
								return false;
						}
					}
				}
			}
		}
		return true;
	};

	o.degeneracyOrdering(G0);

	#pragma omp parallel num_threads(numThreads)
	{
		for (int s = 0; s <= 1; ++s) {
			degS[s].assign(G0.n[s], 0);
			degC[s].assign(G0.n[s], 0);
		}
		G.clear();
		for (int s = 0; s <= 1; ++s)
			G.nbrMap[s] = G0.nbrMap[s];
	}


	#pragma omp parallel for num_threads(numThreads) schedule(dynamic)
	for (int i = o.numOrdered-1; i >= 0; --i) {
		int uSide = o.ordered[i] >= G0.n[0];
		int u = o.ordered[i] - uSide * G0.n[0];

		if (uSide != tSide) continue;

		numNnbS = 0;
		for (int s = 0; s <= 1; ++s) {
			S[s].clear();
			C[s].clear();
		}


		S[uSide].push(u);
		degS[uSide][u] = degC[uSide][u] = 0;

		for (int v : G0.nbr[uSide][u]) {
			if (!C[uSide^1].inside(v)) {
				degS[uSide^1][v] = degC[uSide^1][v] = 0;
				C[uSide^1].push(v);	
			}
			++degC[uSide][u]; 
			++degS[uSide^1][v];
			for (int w : G0.nbr[uSide^1][v]) if (w != u && o.order[w+uSide*G0.n[0]] > i) {
				if (!C[uSide].inside(w)) {
					degS[uSide][w] = degC[uSide][w] = 0;
					C[uSide].push(w); 
				}
				++degC[uSide^1][v]; 
				++degC[uSide][w];
			}
		}


		if (!prune()) continue;

		if (k > 0) {
		
			for (int w : C[uSide]) {
				for (int x : G0.nbr[uSide][w]) if (!G0.connect(uSide, u, x)) {
					if (!C[uSide^1].inside(x)) {
						degS[uSide^1][x] = degC[uSide^1][x] = 0;
						C[uSide^1].push(x);
					}
					//X[uSide^1].push(x);
					++degC[uSide][w];
					++degC[uSide^1][x];
				}
			}

			if (!prune()) continue;
		}


		G.subGraph(G0, S, C);

		// for (int s = 0; s <= 1; ++s) {
		// 	for (int v : Sub.V[s]) {
		// 		int deg = 0;
		// 		for (int w : G.nbr[s][v]) if (C[s^1].inside(w))
		// 			++deg;
		// 		if (deg != degC[s][v])
		// 			log("?????? %d-%d wrong degC ??????", s, v);
		// 	}
		// }

		
		//auto branchStartTime = std::chrono::steady_clock::now();
		branch(0);
		//branchTime += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - branchStartTime).count();

		// for (int s = 0; s <= 1; ++s) {
		// 	for (int v : X[s]) degS[s][v] = degC[s][v] = 0;
		// }

		// for (int s = 0; s <= 1; ++s) {
		// 	for (int v : G.V[s])
		// 		if (degS[s][v] || degC[s][v]) {
		// 			log("?????? %d-%d not clear ??????", s, v);
		// 		}
		// }

	}
}


bool MDB::upperbound() {

	// if (!(flags & FLAG_UB)) return true;

	for (int s = 0; s <= 1; ++s) {
		for (int u : S[s]) if (degSub(s, u) < lb[s^1]-k)
			return false;
	}

	thread_local static std::vector<int> bin[2] = {std::vector<int>(k+1), std::vector<int>(k+1)};
	
	int idx[2] = {k-numNnbS, k-numNnbS}, sum[2] = {0, 0}, num[2] = {0, 0};

	for (int s = 0; s <= 1; ++s) {
		for (int i = 0; i <= k-numNnbS; ++i) bin[s][i] = 0;
		for (int v : C[s]) ++bin[s][nnbS(s, v)]; 
		for (int i = 0; i <= k-numNnbS; ++i) {
			if (sum[s] + bin[s][i]*i > k-numNnbS) {
				bin[s][i] = (k-sum[s]-numNnbS) / i;
				// for (int j = i+1; j <= k-numNnbS; ++j) bin[s][j] = 0;
				sum[s] += bin[s][i]*i;
				num[s] += bin[s][i];
				idx[s] = i;
				break;
			}
			sum[s] += bin[s][i]*i;
			num[s] += bin[s][i];
		}
	}

	

	if (num[0]+S[0].size() < lb[0] || num[1]+S[1].size() < lb[1]) return false;


	int s = (int)(num[0] > num[1]);
	long long numEdges = (long long)S[s].size() * (num[s^1] + S[s^1].size()) - numNnbS - sum[s^1];

	sum[s] = num[s] = 0;

	for (int i = 0, j = idx[s^1]; i <= idx[s] && j >= 0; ++i) {
		while (bin[s][i]--) {
			while (j > 0 && sum[0]+sum[1]+i > k-numNnbS) {
				int x = std::min(bin[s^1][j], (sum[0]+sum[1]+i-k+numNnbS-1)/j + 1);
				sum[s^1] -= x*j; num[s^1] -= x; bin[s^1][j] -= x;
				j -= (int)(!bin[s^1][j]);
			}
			if (sum[0]+i+sum[1] > k-numNnbS) return numEdges > numEdgesSs;
			sum[s] += i; ++num[s];
			numEdges = std::max(numEdges, (long long)(num[0] + S[0].size())*(num[1] + S[1].size())-numNnbS-sum[0]-sum[1]);
			if (numEdges > numEdgesSs) return true;
		}
	}

	return numEdges > numEdgesSs;
}

bool MDB::upperbound(int uSide, int u) {

	return true;

	// if (!(flags & FLAG_UB)) return true;

	thread_local static std::vector<int> cn;

	if (cn.size() < G.n[uSide]) cn.resize(G.n[uSide]);

	for (int w : S[uSide]) cn[w] = 0;

	for (int v : G.nbr[uSide][u]) if (S[uSide^1].inside(v) || C[uSide^1].inside(v)) {
		if (S[uSide].size() > G.degree(uSide^1, v)) {
			for (int w : G.nbr[uSide^1][v]) if (S[uSide].inside(w))
				++cn[w];
		}
		else {
			for (int w : S[uSide]) if (G.connect(uSide, w, v))
				++cn[w];
		}
	}

	for (int w : S[uSide]) if (cn[w] < lb[uSide^1]-k) 
		return false;
	return true;
}


MDB::BakPos MDB::update(int uSide, int u) {
	using namespace traversal;

	BakPos pos;
	pos.backup(C);

	moveC2S(uSide, u);

	//int nnbSu = nnbS(uSide, u);
	for (int s = 0; s <= 1; ++s) {
		for (int v : C[s]) /*if (!(s == uSide && v == u)) */{
			//int nnbSuv = (int)(s != uSide && !G.connect(uSide, u, v));
			if (numNnbS /*+ nnbSu */+ nnbS(s, v)/* + nnbSuv*/ > k) {
				subC(s, v);
			}
		}
	}
	// thread_local static std::vector<int> q[2];
	// if (flags & FLAG_QUEUE) {
		// q[0].clear();
		// q[1].clear();
	// }

	//if (flags & FLAG_CORE) {
		for (int s = 0; s <= 1; ++s) {
			for (int v : C[s]) if (degSub(s, v) < lb[s^1]-k+numNnbS) {
				subC(s, v);
				// if (flags & FLAG_QUEUE) q[s].push_back(v);
			}
		}
	//}

	// if (flags & FLAG_QUEUE) {
	// 	while (!q[0].empty() || !q[1].empty()) {
	// 		for (int s = 0; s <= 1; ++s) {
	// 			while (!q[s].empty()) {
	// 				int v = q[s].back(); q[s].pop_back();
	// 				for (int w : G.nbr[s][v]) if (C[s^1].inside(w) && degSub(s^1, w) < lb[s]-k+numNnbS) {
	// 					subC(s^1, w);
	// 					q[s^1].push_back(w);
	// 				}
	// 			}
	// 		}
	// 	}
	// }

	// for (int t = 0; t < 2; ++t) {
	// 	for (int s = 0; s <= 1; ++s) {
	// 		for (int v : C[s]) if (/*(!(s == uSide && v == u)) && */degSub(s,v) < lb[s^1]-k+numNnbS) {
	// 			subC(s, v);
	// 		}
	// 	}
	// }

	// thread_local static std::vector<int> cn;

	// if (flags & FLAG_CN) {
	// 	if (cn.size() < G.n[uSide]) cn.resize(G.n[uSide]);
	// 	for (int w : C[uSide]) cn[w] = 0;

	// 	for (int v : G.nbr[uSide][u]) if (S[uSide^1].inside(v) || C[uSide^1].inside(v)) {
	// 		if (C[uSide].size() < G.degree(uSide^1, v)) {
	// 			for (int w : C[uSide]) if (G.connect(uSide, w, v))
	// 				++cn[w];
	// 		}
	// 		else {
	// 			for (int w : G.nbr[uSide^1][v]) if (C[uSide].inside(w)) 
	// 				++cn[w];
	// 		}
	// 	}

	// 	for (int w : C[uSide]) if (/*w != u && */cn[w] < lb[uSide^1]-k+numNnbS-nnbS(uSide,u))
	// 		subC(uSide, w);
	// }

	for (int s = 0; s <= 1; ++s) {
		for (int v : reverse(C[s])) if (nnbSub(s, v) == 0) {
			moveC2S(s, v);
		}
	}

	return pos;
}

MDB::BakPos MDB::minus(int uSide, int u) {
	using namespace traversal;

	BakPos pos;
	pos.backup(C);

	subC(uSide, u);

	// u has 1 non-neighbor
	if ((flags & FLAG_1NN) && nnbS(uSide, u) + nnbC(uSide, u) == 1) {

		int uNnbC = -1;
		if (nnbC(uSide, u) == 1) {
			for (int v : C[uSide^1]) if (!G.connect(uSide, u, v)) {
				uNnbC = v;
				break;
			}
		}

		for (int v : C[uSide]) if (nnbS(uSide, v) > 0 || (uNnbC != -1 && !G.connect(uSide, v, uNnbC))) {
			subC(uSide, v);
		}
	}

	// thread_local static std::vector<int> q[2];
	// q[0].clear(); q[1].clear();

	//if (flags & FLAG_CORE) {
		for (int s = 0; s <= 1; ++s) {
			for (int v : C[s]) if (degSub(s, v) < lb[s^1]-k+numNnbS) {
				subC(s, v);
				// if (flags & FLAG_QUEUE) q[s].push_back(v);
			}
		}
	//}

	// if (flags & FLAG_QUEUE) {
	// 	while (!q[0].empty() || !q[1].empty()) {
	// 		for (int s = 0; s <= 1; ++s) {
	// 			while (!q[s].empty()) {
	// 				int v = q[s].back(); q[s].pop_back();
	// 				for (int w : G.nbr[s][v]) if (C[s^1].inside(w) && degSub(s^1, w) < lb[s]-k+numNnbS) {
	// 					subC(s^1, w);
	// 					q[s^1].push_back(w);
	// 				}
	// 			}
	// 		}
	// 	}
	// }

	//pos.backup(1, C);

	for (int s = 0; s <= 1; ++s) {
		for (int v : reverse(C[s])) if (nnbSub(s, v) == 0) {
			moveC2S(s, v);
		}
	}

	return pos;
}


void MDB::restore(BakPos &pos) {
	for (int s = 0; s <= 1; ++s) {
		for (int i = C[s].backPos(); i < pos.p[1][s]; ++i) {
			moveS2C(s, C[s][i]);
		}
	}

	for (int s = 0; s <= 1; ++s) {
		for (int i = C[s].frontPos()-1; i >= pos.p[0][s]; --i) {
			addC(s, C[s][i]);
		}
	}
}

// void MDB::add(VertexSet V[2], std::vector<int> degV[2], int uSide, int u) {
// 	V[uSide].push(u);
// 	for (int v : G.nbr[uSide][u]) {
// 		++degV[uSide^1][v];
// 	}
// }

// void MDB::sub(VertexSet V[2], std::vector<int> degV[2], int uSide, int u) {
// 	V[uSide].pop(u);
// 	for (int v : G.nbr[uSide][u]) {
// 		--degV[uSide^1][v];
// 	}
// }
