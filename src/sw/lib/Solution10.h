#ifndef SOLUTION10_H_
#define SOLUTION10_H_

#ifndef __cplusplus
#error a C++ compiler is required
#endif

#include <cstring>
#include <sstream>
#include "Solution.h"

using namespace std;

class Solution10 : public Solution {
public:
  Solution10();
  ~Solution10();

  virtual int process(const string& b, const string& a, int qsc, int qec,
                 int mm, int mi, int o, int e, int dir,
                 int *opt, int *te, int *qe, int *n_best);
private:
};

#endif
