#include "maxbc.h"
#include "../utils/log.hpp"
#include <stack>

void MAXBC::run(const std::string &dataPath, int q[2], int k, int flags, int numThreads) {
    MAXBC solver;
    solver.findMDB(dataPath, q, k, flags, numThreads);
}

void MAXBC::branch(int dep) {
    if (dep == 0) {
        // create CDAG
        for (int s = 0; s <= 1; ++s) {
            // 初始化空间
            if (rk[s].size() < G.n[s]) rk[s].resize(G.n[s]);
            if (in_idx[s].size() < G.n[s]) in_idx[s].resize(G.n[s]);
            if (R[s].size() < G.n[s]) R[s].resize(G.n[s]);
            
            if (g.nbr.size() < G.n[s]) {
                g.nbr.resize(G.n[s]);
                g.deg.resize(G.n[s]);
            }
            
            for (int u : C[s]) {
                g.nbr[u].clear();
                g.deg[u] = 0;
                R[s][u].clear();
                in_idx[s][u] = 0;
            }
            
            // 构建支配有向无环图 (DAG)
            for (int u : C[s]) {
                for (int w : C[s]) if (w > u) {
                    bool flag1 = true, flag2 = true;
                    for (int v : G.nbr[s][w]) if (C[s^1].inside(v)) {
                        if (!G.connect(s, u, v)) { flag1 = false; break; }
                    }
                    for (int v : G.nbr[s][u]) if (C[s^1].inside(v)) {
                        if (!G.connect(s, w, v)) { flag2 = false; break; }
                    }
                    if (flag1 && flag2) continue; // 双向包含说明邻居完全相同，防止成环，保证是严格DAG
                    if (flag1) g.addEdge(u, w);   // u 包含 w
                    if (flag2) g.addEdge(w, u);   // w 包含 u
                }
            }

            // 1. DFS 分配基础区间 [in_idx, out_idx]
            std::vector<bool> vis(G.n[s], false);
            std::vector<int> out_idx(G.n[s], 0);
            int timer = 0;

            auto dfs = [&](auto& self, int u) -> void {
                vis[u] = true;
                in_idx[s][u] = ++timer;
                for (int v : g.nbr[u]) {
                    if (!vis[v]) self(self, v);
                }
                out_idx[u] = timer;
            };

            // 2. 拓扑排序 (Kahn算法) 并寻找所有根节点作为 DFS 的起点
            std::vector<int> top_order;
            std::stack<int> st;
            for (int u : C[s]) {
                if (g.deg[u] == 0) {
                    st.push(u);
                    if (!vis[u]) dfs(dfs, u); // 仅对入度为0的根节点触发 DFS
                }
            }

            int timestamp = 0;
            while (!st.empty()) {
                int u = st.top(); st.pop();
                rk[s][u] = ++timestamp;
                top_order.push_back(u);
                for (int v : g.nbr[u]) {
                    if (--g.deg[v] == 0) st.push(v);
                }
            }

            // 3. 逆拓扑序 (Rev-Topological Order) 自底向上合并区间
            for (int i = (int)top_order.size() - 1; i >= 0; --i) {
                int u = top_order[i];
                std::vector<std::pair<int, int>> intervals;
                
                // 将自己及其 DFS 树上的子树区间加入
                intervals.push_back({in_idx[s][u], out_idx[u]});
                
                // 收集非树边（所有能到达的子节点）的区间
                for (int v : g.nbr[u]) {
                    intervals.insert(intervals.end(), R[s][v].begin(), R[s][v].end());
                }

                // 合并重叠和连续的区间
                if (!intervals.empty()) {
                    std::sort(intervals.begin(), intervals.end());
                    std::vector<std::pair<int, int>> merged;
                    merged.push_back(intervals[0]);
                    for (size_t j = 1; j < intervals.size(); ++j) {
                        // "+1" 允许将如 [1,2] 和 [3,4] 这种连续的区间也合并为 [1,4]
                        if (intervals[j].first <= merged.back().second + 1) {
                            merged.back().second = std::max(merged.back().second, intervals[j].second);
                        } else {
                            merged.push_back(intervals[j]);
                        }
                    }
                    R[s][u] = std::move(merged);
                }
            }
        }
    }

    if (C[0].size() == 0 && C[1].size() == 0) {
        if (S[0].size() >= lb[0] && S[1].size() >= lb[1]) {
            #pragma omp critical(update)
            {
                if (numEdgesS > numEdgesSs) {
                    for (int s = 0; s <= 1; ++s) {
                        Ss[s].clear();
                        for (int v : S[s]) Ss[s].push(v);
                    }
                    numNnbSs = numNnbS;
                    log("New S*! |E|=%d", numEdgesSs);
                }
            }
        }
        return;
    }

    if (S[0].size()+C[0].size()<lb[0] || S[1].size()+C[1].size()<lb[1]) return;

    int u = -1, uSide = -1, uVal = 0x3f3f3f3f;
    for (int s = 0; s <= 1; ++s) {
        for (int v : C[s]) if (rk[s][v] < uVal) {
            uVal = rk[s][v];
            u = v;
            uSide = s;
        }
    }

    // 第一个分支 (包含 Pivot u)
    auto pos = update(uSide, u);
    branch(dep+1);
    restore(pos);

    // 第二个分支 (排除 Pivot u 及其包含的所有顶点)
    pos = minus(uSide, u);
    
    // 使用一个缓存数组来避免在遍历 C[uSide] 时修改它导致的迭代器失效或跳过元素的问题
    std::vector<int> to_remove;
    for (int v : C[uSide]) {
        if (v == u) continue;
        
        // Rangefinder 逻辑：判断 v 的 in_idx 是否落在 u 的某个区间内
        int target = in_idx[uSide][v];
        bool contained = false;
        for (const auto& interval : R[uSide][u]) {
            if (target >= interval.first && target <= interval.second) {
                contained = true;
                break;
            }
        }
        if (contained) {
            to_remove.push_back(v);
        }
    }
    
    // 统一删除被支配的顶点
    for (int v : to_remove) {
        minus(uSide, v);
    }
    
    branch(dep+1);
    restore(pos);
}