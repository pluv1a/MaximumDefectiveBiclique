#ifndef MBC_H
#define MBC_H

#include <string>
#include "../utils/vertexset.hpp"
#include "../utils/hash.hpp"
#include "../utils/bigraph.hpp"

// #define numEdgesSs 		(Ss[0].size() * Ss[1].size())
// #define numEdgesS 		(S[0].size() * S[1].size())
#define nnbS(s, v) 		(S[s^1].size() - degS[s][v])
#define nnbC(s, v) 		(C[s^1].size() - degC[s][v])
#define degSub(s, v) 	(degS[s][v] + degC[s][v])
#define nnbSub(s, v) 	(nnbS(s, v) + nnbC(s, v))

#define COMM_ROUNDS			1

/* Flags to control algorithm options */
#define FLAG_DEBUG			(1<<0)
#define FLAG_UB				(1<<1)
#define FLAG_CORE			(1<<2)
#define FLAG_CN				(1<<3)
#define FLAG_1NN			(1<<4)
#define FLAG_QUEUE			(1<<5)
#define FLAG_ORDER			(1<<6)
#define FLAG_PB				(1<<7)
#define FLAG_HEU			(1<<8)
#define FLAG_BR				(1<<9)

class MBC {
public:
	void findMBC(const std::string &dataPath, int q[2], int flags=7);
	static void run(const std::string &dataPath, int q[2], int flags=7);
protected:
	VertexSet Ss[2], S[2], C[2], X[2];
	int lb[2], flags;
	std::vector<int> degS[2], degC[2];
	// std::vector<CuckooHash> coexist[2];
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

	void branch(int dep);

	static BiGraph core(BiGraph &G, int a, int b);
	static BiGraph comm(BiGraph &G, int q[2]);
	
	void heuristic(BiGraph &G);

	void searchAll();

	// void cnExclusion(BiGraph &g, int q[2], int k);
	
	void update(int uSide, int u);
	void restore(BakPos &pos, BakPos &posX);

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
	}
	
};
#endif