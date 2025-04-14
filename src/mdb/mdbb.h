#ifndef MDBB_H
#define MDBB_H

#include "mdb.h"

class MDBB : public MDB {
public:
	static void run(const std::string &dataPath, int q[2], int k, int flags, int numFakes, int numK);
private:
	void branch(int dep) override;
};


#endif