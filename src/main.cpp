#include <chrono>
#include <algorithm>
#include "utils/cmdline.hpp"
#include "utils/log.hpp"
#include "algos/mdbp.h"
#include "algos/mdbb.h"
#include "algos/mdc.h"
#include "algos/mbc.h"
#include "algos/mbp.h"
#include "algos/maxbc.h"

using namespace logging;

int main(int argc, char *argv[]) {
	cmdline::parser args;
	args.add<std::string>("data", 'd', "dataset path", true, "");
	args.add<int>("key", 'k', "value of k", true, 0);
	args.add<int>("lb", 'q', "lower bound size", false, 0);
	args.add<int>("jobs", 'j', "number of parallel jobs", false, 0);
	args.add<std::string>("algo", 'a', "algorithm", false, "p", cmdline::oneof<std::string>("pivoting", "bisect", "baseline", "p", "b", "mdc", "mbc", "mbp", "maxbc"));
	args.add("no-ub", '\0', "disable upper bound techniques");
	args.add("no-core", '\0', "disable core reduction");
	args.add("no-cn", '\0', "disable common neighbor reduction");
	args.add("no-1nn", '\0', "disable one non-neighbor reduction");
	args.add("no-pb", '\0', "disable progressive bounding");
	args.add("no-order", '\0', "disable ordering reduction");
	args.add("no-heu", '\0', "disable heuristic algorithm");
	args.add("no-br", '\0', "disable branching rules");
	args.add("debug", '\0', "output intermediate data");
	args.parse_check(argc, argv);


	auto dataPath = args.get<std::string>("data");
	int k = args.get<int>("key");
	int lb[2] = {args.get<int>("lb"), args.get<int>("lb")};
	int flags = !args.exist("no-order") * FLAG_ORDER \
			| !args.exist("no-core") * FLAG_CORE \
			| !args.exist("no-cn") * FLAG_CN \
			| !args.exist("no-1nn") * FLAG_1NN \
			| !args.exist("no-pb") * FLAG_PB \
			| !args.exist("no-heu") * FLAG_HEU \
			| !args.exist("no-br") * FLAG_BR \
			| !args.exist("no-ub") * FLAG_UB \
			| args.exist("debug") * FLAG_DEBUG;

	int numThreads = std::max(args.get<int>("jobs"), 1);


	// if (args.get<int>("ub-level") == 1 || args.get<int>("ub-level") == 3)
	// 	flags |= FLAG_UB_BASIC;

	// if (args.get<int>("ub-level") == 2 || args.get<int>("ub-level") == 3)
	// 	flags |= FLAG_UB_IMPRO;

	auto startTimePoint = std::chrono::steady_clock::now();


	if (args.get<std::string>("algo")[0] == 'p') 
		MDBP::run(dataPath, lb, k, flags, numThreads);
	else if (args.get<std::string>("algo") == "mdc")
		MDC::run(dataPath, lb, k, flags);
	else if (args.get<std::string>("algo") == "mbc")
		MBC::run(dataPath, lb, flags);
	else if (args.get<std::string>("algo")[0] == 'b')
		MDBB::run(dataPath, lb, k, flags, numThreads);
	else if (args.get<std::string>("algo") == "mbp")
		MBP::run(dataPath, lb, k, flags, numThreads);
	else if (args.get<std::string>("algo") == "maxbc") 
		MAXBC::run(dataPath, lb, k, flags, numThreads);
	else {
		log("Unknown algorithm: %s", args.get<std::string>("algo").c_str());
		return 1;
	}

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - startTimePoint);

	log("Total time spent: %ld ms", duration.count());
}
