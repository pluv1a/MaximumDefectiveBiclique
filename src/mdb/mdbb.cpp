#include "mdbb.h"

#include "../utils/log.hpp"

using namespace logging;

void MDBB::run(const std::string &dataPath, int q[2], int k, int flags) {
	MDBB solver;
	solver.findMDB(dataPath, q, k, flags);
}

void MDBB::branch(int dep) {

	if (flags & FLAG_DEBUG) {
		log("------ dep=%d, numNnbS=%d ------", dep, numNnbS);
		logSet(S[0]);
		logSet(S[1]);
		logSet(C[0]);
		logSet(C[1]);
		// logVec(degS[0], 0, G.n[0]);
		// logVec(degS[1], 0, G.n[1]);
		// logVec(degC[0], 0, G.n[0]);
		// logVec(degC[1], 0, G.n[1]);
		for (int s = 0; s <= 1; ++s) {
			for (int u : G.V[s]) {
				int degSu = 0, degCu = 0;
				for (int v : G.nbr[s][u]) {
					if (S[s^1].inside(v)) ++degSu;
					if (C[s^1].inside(v)) ++degCu;
				}
				if (degSu != degS[s][u] || degCu != degC[s][u]) {
					log("Wrong degree of %d-%d", s, u);
				}
			}
		}
	}


	if (C[0].size() == 0 && C[1].size() == 0) {
		if (S[0].size() >= lb[0] && S[1].size() >= lb[1] && numEdgesS > numEdgesSs) {
			Ss[0].clear(); Ss[1].clear();
			for (int u : S[0]) Ss[0].push(u);
			for (int v : S[1]) Ss[1].push(v);
			numNnbSs = numNnbS;
			log("New MDB found! |E|=%d", numEdgesSs);
			// logSet(Ss[0]);
			// logSet(Ss[1]);
		}
		return;
	}

	if (S[0].size()+C[0].size()<lb[0] || S[1].size()+C[1].size()<lb[1]) return;

	int numEdgesSub = 0, s = (int)(S[0].size()+C[0].size() > S[1].size()+C[1].size());
	for (int v : S[s]) numEdgesSub += degSub(s, v);
	for (int v : C[s]) numEdgesSub += degSub(s, v);
	if (numEdgesSub <= numEdgesSs) return;

	if (!upperbound()) { ++numUbPruned; return; }

	++numBranches;
	++numBipartite;

	int u = -1, uSide = -1, uVal = -1;
	
	if (u == -1) {
		// most non-neighbors in S
		for (int s = 0; s <= 1; ++s) {
			for (int v : C[s]) if (nnbS(s, v) > uVal) {
				u = v;
				uSide = s;
				uVal = nnbS(s, v);
			}
		}

		// most non-neighbors in C
		if (uVal == 0) {
		// if (true) {
			uVal = -1;
			for (int s = 0; s <= 1; ++s) {
				for (int v: C[s]) if (nnbC(s, v) > uVal) {
					u = v;
					uSide = s;
					uVal = nnbC(s, v);
				}
			}
		}
	}

	assert(u != -1);

	// log("Choose: %d-%d, degS=%d, degC=%d", uSide, u, degS[uSide][u], degC[uSide][u]);

	if (upperbound(uSide, u)) {
	// if (true) {
		auto pos = update(uSide, u);
		branch(dep+1);
		restore(pos);
	}
	else {
		++numUbPruned;
		//log("1");
	}
	
	auto pos = minus(uSide, u);
	// rearrange(pos);
	branch(dep+1);
	restore(pos);

	if (flags & FLAG_DEBUG) {
		// log(">>>>>> dep=%d <<<<<<", dep);
		// logSet(S[0]);
		// logSet(S[1]);
		// logSet(C[0]);
		// logSet(C[1]);

		// logVec(degS[0], 0, G.n[0]);
		// logVec(degS[1], 0, G.n[1]);
		// logVec(degC[0], 0, G.n[0]);
		// logVec(degC[1], 0, G.n[1]);
	}

}