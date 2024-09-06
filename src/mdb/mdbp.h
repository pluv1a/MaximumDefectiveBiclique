#ifndef MDBP_H
#define MDBP_H

#include "mdb.h"

class MDBP : public MDB {
public:
	static void run(const std::string &dataPath, int q[2], int k, int flags);
private:
	void branch(int dep) override;
};


#endif