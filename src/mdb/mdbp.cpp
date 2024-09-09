#include "mdbp.h"

#include "../utils/log.hpp"

using namespace logging;

// #define PIVOTING_V2

void MDBP::run(const std::string &dataPath, int q[2], int k, int flags) {
	MDBP solver;
	solver.findMDB(dataPath, q, k, flags);
}

void MDBP::branch(int dep) {
	// log("\n---------- dep=%d ----------", dep);
	// logSet(S[0]);
	// logSet(S[1]);
	// logSet(C[0]);
	// logSet(C[1]);

	// for (int s = 0; s <= 1; ++s) {
	// 	for (int u : G.V[s]) {
	// 		int degSu = 0, degCu = 0;
	// 		for (int v : G.nbr[s][u]) {
	// 			if (S[s^1].inside(v)) ++degSu;
	// 			if (C[s^1].inside(v)) ++degCu;
	// 		}
	// 		if (degSu != degS[s][u] || degCu != degC[s][u]) {
	// 			log("Wrong degree of %d-%d", s, u);
	// 		}
	// 	}
	// 	// for (int u : C[s]) {
	// 	// 	int degSu = 0, degCu = 0;
	// 	// 	for (int v : G.nbr[s][u]) {
	// 	// 		if (S[s^1].inside(v)) ++degSu;
	// 	// 		if (C[s^1].inside(v)) ++degCu;
	// 	// 	}
	// 	// 	if (degSu != degS[s][u] || degCu != degC[s][u]) {
	// 	// 		log("Wrong degree of %d-%d", s, u);
	// 	// 	}
	// 	// }
	// }

	if (C[0].size() == 0 && C[1].size() == 0) {
		if (S[0].size() >= lb[0] && S[1].size() >= lb[1] && numEdgesS > numEdgesSs) {
			Ss[0].clear(); Ss[1].clear();
			for (int u : S[0]) Ss[0].push(u);
			for (int v : S[1]) Ss[1].push(v);
			numNnbSs = numNnbS;
			log("New S*! |E|=%d", numEdgesSs);
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

	int cntNnbS = 0, u = -1, uSide = -1, uVal = G.n[0]+G.n[1];
	bool flagPivot = true;
	for (int s = 0; s <= 1; ++s) {
		for (int v : C[s]) {
			// if ((cntNnbS += (nnbS(s, v) > 0)) > k-numNnbS) {
			// 	flagPivot = false;
			// 	break;
			// }
// #ifdef PIVOTING_V2
// 			if (nnbS(s, v) == 1 && degC[s][v] > uVal) {

// 			}
// #endif
			if (nnbS(s, v) == 0 && nnbC(s, v) < uVal) {
				uVal = nnbC(s, v);
				u = v;
				uSide = s;
			}
		}	
		// if (!flagPivot) break;
	}

	
	if (/*flagPivot && */u != -1) {
	// if (false) {

		++numPivoting;

		BakPos pos0;
		pos0.backup(C);

		if (upperbound(uSide, u)) {
			auto pos = update(uSide, u);
			branch(dep+1);
			restore(pos);
		} 
		else ++numUbPruned;

		// subC(uSide, u);
		minus(uSide, u);

		std::vector<int> cand;

		for (int v : C[uSide^1]) if (!G.connect(uSide, u, v))
			cand.push_back(v);

		// if (nnbS(uSide, u) > 0) {
		// 	cand.reserve(C[0].size()+C[1].size());
		// 	for (int v : C[uSide]) if (nnbS(uSide, v) > 0) {
		// 		cand.push_back(v);
		// 	}
		// 	for (int v : C[uSide^1]) if (nnbS(uSide^1, v) > 0) {
		// 		cand.push_back(v);
		// 	}
		// }

		for (int v : cand) if (C[uSide^1].inside(v)) {
			if (upperbound(uSide^1, v)) {
				auto pos = update(uSide^1, v);
				branch(dep+1);
				restore(pos);
			} 
			else ++numUbPruned;
			//subC(uSide^1, v);
			minus(uSide^1, v);
		}

		// pos0.backup(1, C);
		restore(pos0);

	}

	else {

		++numBipartite;

		uVal = -1;

		for (int s = 0; s <= 1; ++s) {
			for (int v : C[s]) if (nnbS(s, v) > uVal) {
				uVal = nnbS(s, v);
				u = v;
				uSide = s;
			}
		}

		// log("Choose: %d-%d", uSide, u);

		if (upperbound(uSide, u)) {
			auto pos = update(uSide, u);
			branch(dep+1);
			restore(pos);
		}
		else ++numUbPruned;
		
		auto pos = minus(uSide, u);
		branch(dep+1);
		restore(pos);
	}

	// log(">>>>>>>>>> dep=%d <<<<<<<<<<", dep);
	// logSet(S[0]);
	// logSet(S[1]);
	// logSet(C[0]);
	// logSet(C[1]);

}
