#ifndef MDB_H
#define MDB_H

#include "../utils/vertexset.hpp"
#include "../utils/hash.hpp"
#include "../utils/bigraph.hpp"
#include <queue>

#define numEdgesSs 		(Ss[0].size() * Ss[1].size() - numNnbSs)
#define numEdgesS 		(S[0].size() * S[1].size() - numNnbS)
#define nnbS(s, v) 		(S[s^1].size() - degS[s][v])
#define nnbC(s, v) 		(C[s^1].size() - degC[s][v])
#define degSub(s, v) 	(degS[s][v] + degC[s][v])
#define nnbSub(s, v) 	(nnbS(s, v) + nnbC(s, v))

#define COMM_ROUNDS			1

/* Flags to control algorithm options */
#define FLAG_DEBUG			(1<<0)
#define FLAG_UB_BASIC		(1<<1)
#define FLAG_UB_IMPRO		(1<<2)
#define FLAG_CORE			(1<<3)
#define FLAG_CN				(1<<4)
#define FLAG_1NN			(1<<5)
#define FLAG_QUEUE			(1<<6)
#define FLAG_ORDER			(1<<7)
#define FLAG_PB				(1<<8)
#define FLAG_HEU			(1<<9)

struct SubGraph {
	std::vector<int> V[2];
	int numEdges;
	bool operator < (const SubGraph& s) const {
		return numEdges < s.numEdges;
	}
	SubGraph (const VertexSet *S, int numEdges): numEdges(numEdges) {
		for (int s = 0; s <= 1; ++s) {
			for (int u : S[s])
				V[s].push_back(u);
		}
	}
};

class MDB {
public:
	void findMDB(const std::string &dataPath, int q[2], int k, int flags=7, int numFakes=100, int numK=2000);
protected:
	VertexSet Ss[2], S[2], C[2], X[2];
	int k, numNnbS, numNnbSs, lb[2], flags;
	std::vector<int> degS[2], degC[2];
	// std::vector<CuckooHash> coexist[2];
	std::priority_queue<SubGraph> topK;
	BiGraph G;
	int branchTime, reductionTime, numBranches, numUbPruned, numPivoting, numBipartite;

	struct BakPos {
		int pC[2][2], pX[2][2];
		inline void backup(VertexSet *C, VertexSet *X) {
			for (int s = 0; s <= 1; ++s) {
				pC[0][s] = C[s].frontPos();
				pC[1][s] = C[s].backPos();
				pX[0][s] = X[s].frontPos();
				pX[1][s] = X[s].backPos();
			}
		}
	};

	virtual void branch(int dep) = 0;

	static BiGraph core(BiGraph &G, int a, int b);
	static BiGraph comm(BiGraph &G, int q[2], int k);
	
	void heuristic(BiGraph &G);

	bool upperbound();
	bool upperbound(int uSide, int u);

	void russianDoll();
	void searchAll();

	// void cnExclusion(BiGraph &g, int q[2], int k);
	
	BakPos update(int uSide, int u);
	BakPos minus(int uSide, int u);
	void restore(BakPos &pos);

	inline void addS(int s, int u) { 
		S[s].push(u);
		for (int v : G.nbr[s][u]) ++degS[s^1][v];
	}
	inline void addC(int s, int u) { 
		C[s].push(u);
		for (int v : G.nbr[s][u]) ++degC[s^1][v];
	}
	inline void subS(int s, int u) { 
		S[s].pop(u);
		for (int v : G.nbr[s][u]) --degS[s^1][v];
	}
	inline void subC(int s, int u) { 
		C[s].pop(u);
		for (int v : G.nbr[s][u]) --degC[s^1][v];
	}
	inline void addX(int s, int u) { 
		X[s].push(u);
	}
	inline void subX(int s, int u) { 
		X[s].pop(u);
	}

	inline void moveC2S(int s, int u) { 
		numNnbS += nnbS(s, u);
		C[s].popBack(u);
		S[s].push(u);
		for (int v : G.nbr[s][u]) {
			--degC[s^1][v];
			++degS[s^1][v];
		}
	}

	inline void moveS2C(int s, int u) { 
		S[s].pop(u);
		C[s].push(u);
		for (int v : G.nbr[s][u]) {
			--degS[s^1][v];
			++degC[s^1][v];
		}
		numNnbS -= nnbS(s, u);
	}
	
};

#endif