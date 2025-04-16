#ifndef LOG_HPP
#define LOG_HPP

#pragma once

#include <cstring>
#include <cstdio>
#include <chrono>
#include <string>
#include <iomanip>
#include "vertexset.hpp"
#include "graph.hpp"
#include "bigraph.hpp"
#include <sstream>

/*
namespace logging {
	static std::string formatCurrentTime() {
		auto timepoint = std::chrono::system_clock::now();
		auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timepoint.time_since_epoch());
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timepoint.time_since_epoch() - seconds);
		auto sec = static_cast<time_t>(seconds.count());
		std::tm tm;
		gmtime_r(&sec, &tm);
		char buf[32] = {0};
		auto size = std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
		return std::string(buf, size) + "." + std::to_string(milliseconds.count());
	}

};
*/

#define log(fmt, args...) do {														\
	/* fprintf(stderr, "[\033[34m%s\033[0m:\033[32m%d\033[0m in \033[35m%s\033[0m] ",\ 
		strrchr(__FILE__, '/')+1, __LINE__, __FUNCTION__); */ 						\
	fprintf(stderr, fmt, ##args);													\
	fprintf(stderr, "\n");															\
} while (0)

#define logGraph(G) logGraphInfo(G, #G)
#define logBiGraph(G) logBiGraphInfo(G, #G)
#define logSet(V) logSetInfo(V, #V)
#define logVec(a, b, e) logVecInfo(a, #a, b, e)

namespace logging {
	inline void logSetInfo(VertexSet &V, const std::string &name) {
		std::vector<int> S(V.begin(), V.end());
		std::sort(S.begin(), S.end());
		std::stringstream ss;
		ss << "{";
		for (int i = 0; i < S.size(); ++i) {
			if (i > 0) ss << ',';
			ss << S[i];
		}
		ss << "}";
		log("%s: size=%d, content=%s", 
			name.c_str(), V.size(), ss.str().c_str());
	}
	inline void logGraphInfo(Graph &G, const std::string &name) {
		log("%s info: |V|=%d, |E|=%d", name.c_str(), G.V.size(), G.m);
	}
	inline void logBiGraphInfo(BiGraph &G, const std::string &name) {
		log("%s info: |U|=%d, |V|=%d, |E|=%d", name.c_str(), G.V[0].size(), G.V[1].size(), G.m);
	}
	inline void logVecInfo(std::vector<int> a, const std::string &name, int begin, int end) {
		std::stringstream ss;
		ss << '{';
		for (int i = begin; i < end; ++i) {
			if (i > begin) ss << ',';
			ss << a[i];
		}
		ss << '}';
		log("%s: content=%s", name.c_str(), ss.str().c_str());
	}
}

#endif