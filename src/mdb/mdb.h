#ifndef MDB_H
#define MDB_H

#include "../utils/vertexset.hpp"
#include "../utils/hash.hpp"
#include "../utils/bigraph.hpp"

#define numEdgesSs 		(Ss[0].size() * Ss[1].size() - numNnbSs)
#define numEdgesS 		(S[0].size() * S[1].size() - numNnbS)
#define nnbS(s, v) 		(S[s^1].size() - degS[s][v])
#define nnbC(s, v) 		(C[s^1].size() - degC[s][v])
#define degSub(s, v) 	(degS[s][v] + degC[s][v])
#define nnbSub(s, v) 	(nnbS(s, v) + nnbC(s, v))

#define COMM_ROUNDS				1
#define FLAG_UPPERBOUND			1
#define FLAG_ORDERING			2
#define FLAG_REDUCTION			4
#define FLAG_QUEUEING			8

class MDB {
public:
	void findMDB(const std::string &dataPath, int q[2], int k, int flags=7);
protected:
	VertexSet Ss[2], S[2], C[2], X[2];
	int k, numNnbS, numNnbSs, lb[2], flags;
	std::vector<int> degS[2], degC[2];
	BiGraph G;
	int branchTime, reductionTime, numBranches, numUbPruned, numPivoting, numBipartite;

	struct BakPos {
		int p[2][2];
		inline void backup(VertexSet *V) {
			p[0][0] = V[0].frontPos();
			p[0][1] = V[1].frontPos();	
			p[1][0] = V[0].backPos();
			p[1][1] = V[1].backPos();
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
	
	BakPos update(int uSide, int u);
	BakPos minus(int uSide, int u);
	void restore(BakPos &pos);
	// void rearrange(BakPos &pos);
	// void add(VertexSet V[2], std::vector<int> degV[2], int uSide, int u);
	// void sub(VertexSet V[2], std::vector<int> degV[2], int uSide, int u);

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