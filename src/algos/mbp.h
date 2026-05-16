#ifndef MBP_H
#define MBP_H

#include "mdb.h"

class MBP : public MDB {
public:
	static void run(const std::string &dataPath, int q[2], int k, int flags, int numThreads);
private:
	void branch(int dep) override;
};


#endif