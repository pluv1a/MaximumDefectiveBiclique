#ifndef MAXBC_H
#define MAXBC_H

#include "mdb.h"
#include "../utils/graph.hpp"

class MAXBC : public MDB {
	struct CDAG {
		std::vector<std::vector<int>> nbr;
		std::vector<int> deg;
		void addEdge(int u, int v) {
			// int x = std::max(u, v);
			// if (nbr.size() <= x) {
			// 	nbr.resize(x+1);
			// 	deg.resize(x+1);
			// }
			nbr[u].push_back(v);
			++deg[v];
		}
		void clear(int n = 0) {
			std::vector<std::vector<int>>(n).swap(nbr);
			std::vector<int>(n).swap(deg);
		}
	};

	std::vector<CuckooHash> dom[2];
	CDAG g;
	std::vector<int> rk[2];
	// 在 MAXBC 类定义中添加：
	std::vector<int> in_idx[2];                               // 存储 DFS 发现时间戳
	std::vector<std::vector<std::pair<int, int>>> R[2];       // R[s][u] 存储顶点 u 的合并区间集合
public:
	static void run(const std::string &dataPath, int q[2], int k, int flags, int numThreads);
private:
	void branch(int dep) override;
};

#endif