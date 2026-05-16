#include "mbp.h"
#include "../utils/log.hpp"

void MBP::run(const std::string &dataPath, int q[2], int k, int flags, int numThreads) {
    MBP solver;
    solver.findMDB(dataPath, q, k, flags, numThreads);
}

void MBP::branch(int dep) {
    if (C[0].size() == 0 && C[1].size() == 0) {
        if (S[0].size() >= lb[0] && S[1].size() >= lb[1]) {
            if (numEdgesS > numEdgesSs) {
                for (int s = 0; s <= 1; ++s) {
                    Ss[s].clear();
                    for (int v : S[s]) Ss[s].push(v);
                }
                numNnbSs = numNnbS;
                log("New S*! |E|=%d", numEdgesSs);
            }
        }
        return;
    }

    if (S[0].size()+C[0].size()<lb[0] || S[1].size()+C[1].size()<lb[1]) return;

    // int numEdgesSub = 0, s = (int)(S[0].size()+C[0].size() > S[1].size()+C[1].size());
    // for (int v : S[s]) numEdgesSub += degSub(s, v);
    // for (int v : C[s]) numEdgesSub += degSub(s, v);
    // if (numEdgesSub <= numEdgesSs) return;

    // if (!upperbound()) { ++numUbPruned; return; }

    int u = -1, uSide = -1, uVal = -1;

    for (int s = 0; s <= 1; ++s) {
        for (int v : S[s]) {
            if (nnbSub(s, v) > uVal) {
                u = v;
                uSide = s;
                uVal = nnbSub(s, v);
            }
        }
        for (int v : C[s]) {
            if (nnbSub(s, v) > uVal) {
                u = v;
                uSide = s;
                uVal = nnbSub(s, v);
            }
        }
    }


    auto pos = backup(C);
    if (C[uSide].inside(u)) {
        auto p = minus(uSide, u);
        branch(dep+1);
        restore(p);
        update(uSide, u);
    }
    std::vector<int> cands[2];
    int ncnt = 0;

    for (int v : C[uSide^1]) if (!G.connect(uSide, u, v)) {
        if (ncnt > k-numNnbS) break;
        cands[uSide^1].push_back(v);
        ncnt += nnbS(uSide^1, v);
    }

    for (int v : C[uSide^1]) if (G.connect(uSide, u, v)) {
        if (ncnt > k-numNnbS) break;
        cands[uSide^1].push_back(v);
        ncnt += nnbS(uSide^1, v);
    }

    for (int v : C[uSide]) if (v != u) {
        if (ncnt > k-numNnbS) break;
        cands[uSide].push_back(v);
        ncnt += nnbS(uSide, v);
    }

    for (int v : cands[uSide^1]) if (C[uSide^1].inside(v)) {
        auto p = minus(uSide^1, v);
        branch(dep+1);
        restore(p); 
        update(uSide^1, v);
    }

    for (int v : cands[uSide]) if (C[uSide].inside(v)) {
        auto p = minus(uSide, v);
        branch(dep+1);
        restore(p); 
        update(uSide, v);
    }

    branch(dep+1);

    restore(pos);

}