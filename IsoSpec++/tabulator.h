#ifndef __TABULATOR_H__
#define __TABULATOR_H__

#include "isoSpec++.h"

class Tabulator
{
private:
    double* _masses;
    double* _lprobs;
    double* _probs;
    int*    _confs;
    int     _confs_no;
public:
    Tabulator(IsoThresholdGenerator* generator,
              bool get_masses, bool get_probs,
              bool get_lprobs, bool get_confs);

    ~Tabulator();

    inline const double* masses()   { return _masses; };
    inline const double* lprobs()   { return _lprobs; };
    inline const double* probs()    { return _probs; };
    inline const int*    confs()    { return _confs; };
    inline const int     confs_no() { return _confs_no; };
};

#endif  // __TABULATOR_H__
