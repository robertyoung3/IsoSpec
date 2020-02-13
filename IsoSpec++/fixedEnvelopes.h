/*
 *   Copyright (C) 2015-2020 Mateusz Łącki and Michał Startek.
 *
 *   This file is part of IsoSpec.
 *
 *   IsoSpec is free software: you can redistribute it and/or modify
 *   it under the terms of the Simplified ("2-clause") BSD licence.
 *
 *   IsoSpec is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *   You should have received a copy of the Simplified BSD Licence
 *   along with IsoSpec.  If not, see <https://opensource.org/licenses/BSD-2-Clause>.
 */

#pragma once

#include <stdlib.h>
#include <algorithm>

#include "isoSpec++.h"

#define ISOSPEC_INIT_TABLE_SIZE 1024

namespace IsoSpec
{

class ISOSPEC_EXPORT_SYMBOL FixedEnvelope {
protected:
    double* _masses;
    double* _probs;
    int*    _confs;
    size_t  _confs_no;
    int     allDim;
    bool sorted_by_mass;
    bool sorted_by_prob;
    double total_prob;
    size_t current_size;

public:
    FixedEnvelope() : _masses(nullptr),
        _probs(nullptr),
        _confs(nullptr),
        _confs_no(0),
        sorted_by_mass(false),
        sorted_by_prob(false),
        total_prob(NAN),
        current_size(0)
        {};

    FixedEnvelope(const FixedEnvelope& other);
    FixedEnvelope(FixedEnvelope&& other);

    FixedEnvelope(double* masses, double* probs, size_t confs_no, bool masses_sorted = false, bool probs_sorted = false, double _total_prob = NAN);

    virtual ~FixedEnvelope()
    {
        if( _masses != nullptr ) free(_masses);
        if( _probs  != nullptr ) free(_probs);
        if( _confs  != nullptr ) free(_confs);
    };

    FixedEnvelope operator+(const FixedEnvelope& other) const;
    FixedEnvelope operator*(const FixedEnvelope& other) const;

    inline size_t    confs_no()  const { return _confs_no; };
    inline int       getAllDim() const { return allDim; };

    inline const double*   masses() const { return _masses; };
    inline const double*   probs()  const { return _probs; };
    inline const int*      confs()  const { return _confs; };

    inline double*   release_masses()   { double* ret = _masses; _masses = nullptr; return ret; };
    inline double*   release_probs()    { double* ret = _probs;  _probs  = nullptr; return ret; };
    inline int*      release_confs()    { int*    ret = _confs;  _confs  = nullptr; return ret; };


    inline double     mass(size_t i)  const { return _masses[i]; };
    inline double     prob(size_t i)  const { return _probs[i];  };
    inline const int* conf(size_t i)  const { return _confs + i*allDim; };

    void sort_by_mass();
    void sort_by_prob();

    double get_total_prob();
    void scale(double factor);
    void normalize();

    double WassersteinDistance(FixedEnvelope& other);
    double OrientedWassersteinDistance(FixedEnvelope& other);

    static FixedEnvelope LinearCombination(const std::vector<const FixedEnvelope*>& spectra, const std::vector<double>& intensities);
    static FixedEnvelope LinearCombination(const FixedEnvelope* const * spectra, const double* intensities, size_t size);


    FixedEnvelope bin(double bin_width = 1.0, double middle = 0.0);

private:

    void sort_by(double* order);

protected:
    double* tmasses;
    double* tprobs;
    int*    tconfs;

    int allDimSizeofInt;

    template<typename T, bool tgetConfs> ISOSPEC_FORCE_INLINE void store_conf(T& generator)
    {
        *tmasses = generator.mass();  tmasses++;
        *tprobs  = generator.prob();  tprobs++;
        constexpr_if(tgetConfs)  { generator.get_conf_signature(tconfs); tconfs += allDim; };
    }

    ISOSPEC_FORCE_INLINE void store_conf(double mass, double prob)
    {
        if(_confs_no == current_size)
        {
            current_size *= 2;
            reallocate_memory<false>(current_size);
        }

        *tprobs = prob;
	*tmasses = mass;
	tprobs++;
	tmasses++;

	_confs_no++;
    }

    template<bool tgetConfs> ISOSPEC_FORCE_INLINE void swap([[maybe_unused]] size_t idx1, [[maybe_unused]] size_t idx2, [[maybe_unused]] int* conf_swapspace)
    {
        std::swap<double>(this->_probs[idx1],  this->_probs[idx2]);
        std::swap<double>(this->_masses[idx1], this->_masses[idx2]);
        constexpr_if(tgetConfs)
        {
            int* c1 = this->_confs + (idx1*this->allDim);
            int* c2 = this->_confs + (idx2*this->allDim);
            memcpy(conf_swapspace, c1, this->allDimSizeofInt);
            memcpy(c1, c2, this->allDimSizeofInt);
            memcpy(c2, conf_swapspace, this->allDimSizeofInt);
        }
    }

    template<bool tgetConfs> void reallocate_memory(size_t new_size);
    void slow_reallocate_memory(size_t new_size);
};

template<typename T> void call_init(T* tabulator, Iso&& iso, bool tgetConfs);

class ISOSPEC_EXPORT_SYMBOL ThresholdFixedEnvelope : public FixedEnvelope
{
    const double threshold;
    const bool absolute;
public:
    ThresholdFixedEnvelope(Iso&& iso, double _threshold, bool _absolute, bool tgetConfs = false) :
    FixedEnvelope(),
    threshold(_threshold),
    absolute(_absolute)
    {
        call_init<ThresholdFixedEnvelope>(this, std::move(iso), tgetConfs);
    }

    inline ThresholdFixedEnvelope(const Iso& iso, double _threshold, bool _absolute, bool tgetConfs = false) :
    ThresholdFixedEnvelope(Iso(iso, false), _threshold, _absolute, tgetConfs) {};

    virtual ~ThresholdFixedEnvelope() {};

private:
    template<bool tgetConfs> void init(Iso&& iso);

    template<typename T> friend void call_init(T* tabulator, Iso&& iso, bool tgetConfs);
};


class ISOSPEC_EXPORT_SYMBOL TotalProbFixedEnvelope : public FixedEnvelope
{
    const bool optimize;
    double target_total_prob;

public:
    TotalProbFixedEnvelope(Iso&& iso, double _target_total_prob, bool _optimize, bool tgetConfs = false) :
    FixedEnvelope(),
    optimize(_optimize),
    target_total_prob(_target_total_prob >= 1.0 ? std::numeric_limits<double>::infinity() : _target_total_prob)
    {
        current_size = ISOSPEC_INIT_TABLE_SIZE;
        if(_target_total_prob <= 0.0)
            return;

        call_init(this, std::move(iso), tgetConfs);
    }

    inline TotalProbFixedEnvelope(const Iso& iso, double _target_total_prob, bool _optimize, bool tgetConfs = false) :
    TotalProbFixedEnvelope(Iso(iso, false), _target_total_prob, _optimize, tgetConfs) {};

    virtual ~TotalProbFixedEnvelope() {};

private:

    template<bool tgetConfs> void init(Iso&& iso);

    template<bool tgetConfs> void addConf(IsoLayeredGenerator& generator)
    {
        if(this->_confs_no == this->current_size)
        {
            this->current_size *= 2;
            this->template reallocate_memory<tgetConfs>(this->current_size);
        }

        this->template store_conf<IsoLayeredGenerator, tgetConfs>(generator);
        this->_confs_no++;
    }

    template<typename T> friend void call_init(T* tabulator, Iso&& iso, bool tgetConfs);

};


} // namespace IsoSpec

