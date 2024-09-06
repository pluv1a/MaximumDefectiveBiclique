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
		inline void backup(int id, VertexSet *V) {
			p[id][0] = V[0].frontPos();
			p[id][1] = V[1].frontPos();	
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
	void add(VertexSet V[2], std::vector<int> degV[2], int uSide, int u);
	void sub(VertexSet V[2], std::vector<int> degV[2], int uSide, int u);

	inline void addS(int s, int v) { add(S, degS, s, v); }
	inline void addC(int s, int v) { add(C, degC, s, v); }
	inline void subS(int s, int v) { sub(S, degS, s, v); }
	inline void subC(int s, int v) { sub(C, degC, s, v); }

	inline void moveC2S(int s, int v) { 
		numNnbS += nnbS(s, v);
		subC(s, v);
		addS(s, v);
	}

	inline void moveS2C(int s, int v) { 
		subS(s, v);
		addC(s, v);
		numNnbS -= nnbS(s, v);
	}
	
};

#endif