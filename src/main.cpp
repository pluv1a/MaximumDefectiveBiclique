#include <chrono>
#include "utils/cmdline.hpp"
#include "utils/log.hpp"
#include "mdb/mdbp.h"
#include "mdb/mdbb.h"

using namespace logging;

int main(int argc, char *argv[]) {
	cmdline::parser args;
	args.add<std::string>("data", 'd', "dataset path", true, "");
	args.add<int>("key", 'k', "value of k", true, 0);
	args.add<int>("lb", 'q', "lower bound size", false, 0);
	args.add<std::string>("algo", 'a', "algorithm", false, "p", cmdline::oneof<std::string>("pivoting", "bipartite", "p", "b"));
	args.add<int>("ub-level", 'u', "specify upper bound level: 0 (disable), 1 (basic), 2 (improved), 3 (full)", false, 2);
	// args.add("no-upperbound", '\0', "disable upperbound techniques");
	args.add("no-reduction", '\0', "disable reduction techniques");
	args.add("no-ordering", '\0', "disable ordering technique");
	args.add("no-queueing", '\0', "disable queueing in update");
	args.parse_check(argc, argv);


	auto dataPath = args.get<std::string>("data");
	int k = args.get<int>("key");
	int lb[2] = {args.get<int>("lb"), args.get<int>("lb")};
	int flags = !args.exist("no-ordering") * FLAG_ORDERING \
			| !args.exist("no-queueing") * FLAG_QUEUEING \
			| !args.exist("no-reduction") * FLAG_REDUCTION;

	if (args.get<int>("ub-level") == 1 || args.get<int>("ub-level") == 3)
		flags |= FLAG_UB_BASIC;

	if (args.get<int>("ub-level") == 2 || args.get<int>("ub-level") == 3)
		flags |= FLAG_UB_IMPRO;


	bool pivoting = args.get<std::string>("algo")[0] == 'p';
	
	auto startTimePoint = std::chrono::steady_clock::now();


	if (pivoting) 
		MDBP::run(dataPath, lb, k, flags);
	else
		MDBB::run(dataPath, lb, k, flags);

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - startTimePoint);

	log("Total time spent: %ld ms", duration.count());

}