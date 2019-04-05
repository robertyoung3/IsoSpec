/*
 *   Copyright (C) 2015-2019 Mateusz Łącki and Michał Startek.
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


#include <cmath>
#include <algorithm>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <tuple>
#include <unordered_map>
#include <queue>
#include <utility>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <stdexcept>
#include <string>
#include <limits>
#include <assert.h>
#include <ctype.h>
#include "platform.h"
#include "conf.h"
#include "dirtyAllocator.h"
#include "operators.h"
#include "summator.h"
#include "marginalTrek++.h"
#include "isoSpec++.h"
#include "misc.h"
#include "element_tables.h"


using namespace std;

namespace IsoSpec
{

Iso::Iso() :
disowned(false),
dimNumber(0),
isotopeNumbers(new int[0]),
atomCounts(new int[0]),
confSize(0),
allDim(0),
marginals(new Marginal*[0]),
modeLProb(0.0)
{}


Iso::Iso(
    int             _dimNumber,
    const int*      _isotopeNumbers,
    const int*      _atomCounts,
    const double* const *  _isotopeMasses,
    const double* const *  _isotopeProbabilities
) :
disowned(false),
dimNumber(_dimNumber),
isotopeNumbers(array_copy<int>(_isotopeNumbers, _dimNumber)),
atomCounts(array_copy<int>(_atomCounts, _dimNumber)),
confSize(_dimNumber * sizeof(int)),
allDim(0),
marginals(nullptr),
modeLProb(0.0)
{
    try{
        setupMarginals(_isotopeMasses, _isotopeProbabilities);
    }
    catch(...)
    {
        delete[] isotopeNumbers;
        delete[] atomCounts;
        throw;
    }
}

Iso::Iso(Iso&& other) :
disowned(other.disowned),
dimNumber(other.dimNumber),
isotopeNumbers(other.isotopeNumbers),
atomCounts(other.atomCounts),
confSize(other.confSize),
allDim(other.allDim),
marginals(other.marginals),
modeLProb(other.modeLProb)
{
    other.disowned = true;
}


Iso::Iso(const Iso& other, bool fullcopy) :
disowned(fullcopy ? throw std::logic_error("Not implemented") : true),
dimNumber(other.dimNumber),
isotopeNumbers(fullcopy ? array_copy<int>(other.isotopeNumbers, dimNumber) : other.isotopeNumbers),
atomCounts(fullcopy ? array_copy<int>(other.atomCounts, dimNumber) : other.atomCounts),
confSize(other.confSize),
allDim(other.allDim),
marginals(fullcopy ? throw std::logic_error("Not implemented") : other.marginals),
modeLProb(other.modeLProb)
{}


inline void Iso::setupMarginals(const double* const * _isotopeMasses, const double* const * _isotopeProbabilities)
{
    if (marginals == nullptr)
    {
        int ii = 0;
        try
        {
            marginals = new Marginal*[dimNumber];
            while(ii < dimNumber)
            {
                allDim += isotopeNumbers[ii];
                marginals[ii] = new Marginal(
                        _isotopeMasses[ii],
                        _isotopeProbabilities[ii],
                        isotopeNumbers[ii],
                        atomCounts[ii]
                    );
                modeLProb += marginals[ii]->getModeLProb();
                ii++;
            }
        }
        catch(...)
        {
            ii--;
            while(ii >= 0)
            {
                delete marginals[ii];
                ii--;
            }
            delete[] marginals;
            marginals = nullptr;
            throw;
        }
    }

}

Iso::~Iso()
{
    if(!disowned)
    {
    if (marginals != nullptr)
        dealloc_table(marginals, dimNumber);
    delete[] isotopeNumbers;
    delete[] atomCounts;
    }
}


double Iso::getLightestPeakMass() const
{
    double mass = 0.0;
    for (int ii=0; ii<dimNumber; ii++)
        mass += marginals[ii]->getLightestConfMass();
    return mass;
}

double Iso::getHeaviestPeakMass() const
{
    double mass = 0.0;
    for (int ii=0; ii<dimNumber; ii++)
        mass += marginals[ii]->getHeaviestConfMass();
    return mass;
}

double Iso::getMonoisotopicPeakMass() const
{
    double mass = 0.0;
    for (int ii=0; ii<dimNumber; ii++)
        mass += marginals[ii]->getMonoisotopicConfMass();
    return mass;
}

double Iso::getUnlikeliestPeakLProb() const
{
    double lprob = 0.0;
    for (int ii=0; ii<dimNumber; ii++)
        lprob += marginals[ii]->getSmallestLProb();
    return lprob;
}

double Iso::getModeMass() const
{
    double mass = 0.0;
    for (int ii=0; ii<dimNumber; ii++)
        mass += marginals[ii]->getModeMass();
    return mass;
}

double Iso::getTheoreticalAverageMass() const
{
    double mass = 0.0;
    for (int ii=0; ii<dimNumber; ii++)
        mass += marginals[ii]->getTheoreticalAverageMass();
    return mass;
}


Iso::Iso(const char* formula) :
disowned(false),
allDim(0),
marginals(nullptr),
modeLProb(0.0)
{
    std::vector<const double*> isotope_masses;
    std::vector<const double*> isotope_probabilities;

    dimNumber = parse_formula(formula, isotope_masses, isotope_probabilities, &isotopeNumbers, &atomCounts, &confSize);

    setupMarginals(isotope_masses.data(), isotope_probabilities.data());
}


void Iso::addElement(int atomCount, int noIsotopes, const double* isotopeMasses, const double* isotopeProbabilities)
{
    Marginal* m = new Marginal(isotopeMasses, isotopeProbabilities, noIsotopes, atomCount);
    modeLProb += m->getModeLProb();
    realloc_append<int>(&isotopeNumbers, noIsotopes, dimNumber);
    realloc_append<int>(&atomCounts, atomCount, dimNumber);
    realloc_append<Marginal*>(&marginals, m, dimNumber);
    dimNumber++;
    confSize += sizeof(int);
    allDim += noIsotopes;

}

unsigned int parse_formula(const char* formula, std::vector<const double*>& isotope_masses, std::vector<const double*>& isotope_probabilities, int** isotopeNumbers, int** atomCounts, unsigned int* confSize)
{
    // This function is NOT guaranteed to be secure against malicious input. It should be used only for debugging.
    size_t slen = strlen(formula);
    // Yes, it would be more elegant to use std::string here, but it's the only promiment place where it would be used in IsoSpec, and avoiding it here
    // means we can run the whole thing through Clang's memory sanitizer without the need for instrumented libc++/libstdc++. That's worth messing with char pointers a
    // little bit.
    std::vector<std::pair<const char*, size_t> > elements;
    std::vector<int> numbers;

    if(slen == 0)
        throw invalid_argument("Invalid formula: can't be empty");

    if(!isdigit(formula[slen-1]))
        throw invalid_argument("Invalid formula: every element must be followed by a number - write H2O1 and not H2O for water");

    for(size_t ii=0; ii<slen; ii++)
        if(!isdigit(formula[ii]) && !isalpha(formula[ii]))
            throw invalid_argument("Invalid formula: contains invalid (non-digit, non-alpha) character");

    size_t position = 0;
    size_t elem_end = 0;
    size_t digit_end = 0;

    while(position < slen)
    {
        elem_end = position;
        while(isalpha(formula[elem_end]))
            elem_end++;
        digit_end = elem_end;
        while(isdigit(formula[digit_end]))
            digit_end++;
        elements.emplace_back(&formula[position], elem_end-position);
        numbers.push_back(atoi(&formula[elem_end]));
        position = digit_end;
    }

    std::vector<int> element_indexes;

    for (unsigned int i=0; i<elements.size(); i++)
    {
        int idx = -1;
        for(int j=0; j<ISOSPEC_NUMBER_OF_ISOTOPIC_ENTRIES; j++)
        {
            if ((strlen(elem_table_symbol[j]) == elements[i].second) && (strncmp(elements[i].first, elem_table_symbol[j], elements[i].second) == 0))
            {
                idx = j;
                break;
            }
        }
        if(idx < 0)
            throw invalid_argument("Invalid formula");
        element_indexes.push_back(idx);
    }

    vector<int> _isotope_numbers;

    for(vector<int>::iterator it = element_indexes.begin(); it != element_indexes.end(); ++it)
    {
        int num = 0;
        int at_idx = *it;
        int atomicNo = elem_table_atomicNo[at_idx];
        while(at_idx < ISOSPEC_NUMBER_OF_ISOTOPIC_ENTRIES && elem_table_atomicNo[at_idx] == atomicNo)
        {
            at_idx++;
            num++;
        }
        _isotope_numbers.push_back(num);
    }

    for(vector<int>::iterator it = element_indexes.begin(); it != element_indexes.end(); ++it)
    {
        isotope_masses.push_back(&elem_table_mass[*it]);
        isotope_probabilities.push_back(&elem_table_probability[*it]);
    };

    const unsigned int dimNumber = elements.size();

    *isotopeNumbers = array_copy<int>(_isotope_numbers.data(), dimNumber);
    *atomCounts = array_copy<int>(numbers.data(), dimNumber);
    *confSize = dimNumber * sizeof(int);

    return dimNumber;

}


/*
 * ----------------------------------------------------------------------------------------------------------
 */



IsoGenerator::IsoGenerator(Iso&& iso, bool alloc_partials) :
    Iso(std::move(iso)),
    partialLProbs(alloc_partials ? new double[dimNumber+1] : nullptr),
    partialMasses(alloc_partials ? new double[dimNumber+1] : nullptr),
    partialProbs(alloc_partials ? new double[dimNumber+1] : nullptr)
{
    if(alloc_partials)
    {
        partialLProbs[dimNumber] = 0.0;
        partialMasses[dimNumber] = 0.0;
        partialProbs[dimNumber] = 1.0;
    }
}


IsoGenerator::~IsoGenerator()
{
    if(partialLProbs != nullptr)
        delete[] partialLProbs;
    if(partialMasses != nullptr)
        delete[] partialMasses;
    if(partialProbs != nullptr)
        delete[] partialProbs;
}



/*
 * ----------------------------------------------------------------------------------------------------------
 */



IsoThresholdGenerator::IsoThresholdGenerator(Iso&& iso, double _threshold, bool _absolute, int tabSize, int hashSize, bool reorder_marginals)
: IsoGenerator(std::move(iso)),
Lcutoff(_threshold <= 0.0 ? std::numeric_limits<double>::lowest() : (_absolute ? log(_threshold) : log(_threshold) + modeLProb))
{
    counter = new int[dimNumber];
    maxConfsLPSum = new double[dimNumber-1];
    marginalResultsUnsorted = new PrecalculatedMarginal*[dimNumber];

    empty = false;

    for(int ii=0; ii<dimNumber; ii++)
    {
        counter[ii] = 0;
        marginalResultsUnsorted[ii] = new PrecalculatedMarginal(std::move(*(marginals[ii])),
                                                        Lcutoff - modeLProb + marginals[ii]->getModeLProb(),
                                                        true,
                                                        tabSize,
                                                        hashSize);

        if(!marginalResultsUnsorted[ii]->inRange(0))
            empty = true;
    }

    if(reorder_marginals && dimNumber > 1)
    {
        OrderMarginalsBySizeDecresing comparator(marginalResultsUnsorted);
        int* tmpMarginalOrder = new int[dimNumber];

        for(int ii=0; ii<dimNumber; ii++)
            tmpMarginalOrder[ii] = ii;

        std::sort(tmpMarginalOrder, tmpMarginalOrder + dimNumber, comparator);
        marginalResults = new PrecalculatedMarginal*[dimNumber];

        for(int ii=0; ii<dimNumber; ii++)
            marginalResults[ii] = marginalResultsUnsorted[tmpMarginalOrder[ii]];

        marginalOrder = new int[dimNumber];
        for(int ii = 0; ii<dimNumber; ii++)
            marginalOrder[tmpMarginalOrder[ii]] = ii;

        delete[] tmpMarginalOrder;

    }
    else
    {
        marginalResults = marginalResultsUnsorted;
        marginalOrder = nullptr;
    }

    lProbs_ptr_start = marginalResults[0]->get_lProbs_ptr();

    if(dimNumber > 1)
        maxConfsLPSum[0] = marginalResults[0]->getModeLProb();

    for(int ii=1; ii<dimNumber-1; ii++)
        maxConfsLPSum[ii] = maxConfsLPSum[ii-1] + marginalResults[ii]->getModeLProb();

    lProbs_ptr = lProbs_ptr_start;

    partialLProbs_second = partialLProbs;
    partialLProbs_second++;

    if(!empty)
    {
        recalc(dimNumber-1);
        counter[0]--;
        lProbs_ptr--;
    }
    else
    {
        terminate_search();
        lcfmsv = std::numeric_limits<double>::infinity();
    }



}

void IsoThresholdGenerator::terminate_search()
{
    for(int ii=0; ii<dimNumber; ii++)
    {
        counter[ii] = marginalResults[ii]->get_no_confs()-1;
        partialLProbs[ii] = -std::numeric_limits<double>::infinity();
    }
    partialLProbs[dimNumber] = -std::numeric_limits<double>::infinity();
    lProbs_ptr = lProbs_ptr_start + marginalResults[0]->get_no_confs()-1;
}

size_t IsoThresholdGenerator::count_confs()
{
    // Smarter algorithm forthcoming in 2.0
    size_t ret = 0;
    while(advanceToNextConfiguration())
        ret++;
    reset();
    return ret;
}

void IsoThresholdGenerator::reset()
{
    if(empty)
    {
        terminate_search();
        return;
    }

    partialLProbs[dimNumber] = 0.0;

    memset(counter, 0, sizeof(int)*dimNumber);
    recalc(dimNumber-1);
    counter[0]--;

    lProbs_ptr = lProbs_ptr_start - 1;
}

/*
 * ------------------------------------------------------------------------------------------------------------------------
 */


IsoLayeredGenerator::IsoLayeredGenerator(Iso&& iso, int tabSize, int hashSize, bool reorder_marginals, double t_prob_hint)
: IsoGenerator(std::move(iso))
{
    counter = new int[dimNumber];
    maxConfsLPSum = new double[dimNumber-1];
    currentLThreshold = nextafter(getModeLProb(), -std::numeric_limits<double>::infinity());
    lastLThreshold = std::numeric_limits<double>::min();
    marginalResultsUnsorted = new LayeredMarginal*[dimNumber];
    resetPositions = new const double*[dimNumber];

    for(int ii=0; ii<dimNumber; ii++)
    {
        counter[ii] = 0;
        marginalResultsUnsorted[ii] = new LayeredMarginal(std::move(*(marginals[ii])), tabSize, hashSize);
    }

    if(reorder_marginals && dimNumber > 1)
    {
        double* marginal_priorities = new double[dimNumber];

        /*
         * We shall now use Gaussian approximations of the marginal multinomial distributions to estimate
         * how many configurations we shall need to visit from each marginal. This should be approximately
         * proportional to the volume of the optimal P-ellipsoid of the marginal, which, in turn is defined
         * by the quantile function of the chi-square distribution plus some modifications.
         *
         * We're dropping the constant factor and the (monotonic) exp() transform - these will be used as keys
         * for sorting, so only the ordering is important.
         */

        double K = allDim - dimNumber;

        double log_R2 = log(InverseChiSquareCDF2(K, t_prob_hint));

        for(int ii = 0; ii < dimNumber; ii++)
        {
            const int i = marginalResultsUnsorted[ii]->get_isotopeNo();
            if(i == 1)
                marginal_priorities[ii] = 0.0;
            else
            {
                double k = static_cast<double>(i - 1);
                const int n = atomCounts[ii];

                double sum_lprobs = 0.0;
                for(int jj = 0; jj < i; jj++)
                    sum_lprobs += marginalResultsUnsorted[ii]->get_lProbs()[jj];

                double sum_rademacher = 0.0;
                for(int jj = 1; jj < i; jj++)
                    sum_rademacher += log1p((static_cast<double>(jj)) / static_cast<double>(n));

                marginal_priorities[ii] = -(sum_lprobs/2.0 + sum_rademacher - lgamma((k+2.0)/2.0) + k/2.0 * (log_R2 + log2pluslogpi + log(n)));
            }
        }

        int* tmpMarginalOrder = new int[dimNumber];

        for(int ii=0; ii<dimNumber; ii++)
            tmpMarginalOrder[ii] = ii;

        TableOrder<double> TO(marginal_priorities);

        std::sort(tmpMarginalOrder, tmpMarginalOrder + dimNumber, TO);
        marginalResults = new LayeredMarginal*[dimNumber];

        for(int ii=0; ii<dimNumber; ii++)
            marginalResults[ii] = marginalResultsUnsorted[tmpMarginalOrder[ii]];

        marginalOrder = new int[dimNumber];
        for(int ii = 0; ii<dimNumber; ii++)
            marginalOrder[tmpMarginalOrder[ii]] = ii;

        delete[] tmpMarginalOrder;
        delete[] marginal_priorities;
    }
    else
    {
        marginalResults = marginalResultsUnsorted;
        marginalOrder = nullptr;
    }

    lProbs_ptr_start = marginalResults[0]->get_lProbs_ptr();

    if(dimNumber > 1)
        maxConfsLPSum[0] = marginalResults[0]->getModeLProb();

    for(int ii=1; ii<dimNumber-1; ii++)
        maxConfsLPSum[ii] = maxConfsLPSum[ii-1] + marginalResults[ii]->getModeLProb();

    lProbs_ptr = lProbs_ptr_start;

    partialLProbs_second = partialLProbs;
    partialLProbs_second++;

    counter[0]--;
    lProbs_ptr--;
    lastLThreshold = 10.0;
    nextLayer(-0.00001);
}

bool IsoLayeredGenerator::nextLayer(double offset)
{
    size_t first_mrg_size = marginalResults[0]->get_no_confs();

    if(lastLThreshold < getUnlikeliestPeakLProb())
        return false;

    lastLThreshold = currentLThreshold;
    currentLThreshold += offset;

    for(int ii=0; ii<dimNumber; ii++)
    {
        marginalResults[ii]->extend(currentLThreshold - modeLProb + marginalResults[ii]->getModeLProb());
        counter[ii] = 0;
    }

    lProbs_ptr_start = marginalResults[0]->get_lProbs_ptr(); // vector relocation might have happened

    lProbs_ptr = lProbs_ptr_start + first_mrg_size - 1;

    for(int ii=0; ii<dimNumber; ii++)
        resetPositions[ii] = lProbs_ptr;

    recalc(dimNumber-1);

    return true;
}

void IsoLayeredGenerator::terminate_search()
{
    for(int ii=0; ii<dimNumber; ii++)
    {
        counter[ii] = marginalResults[ii]->get_no_confs()-1;
        partialLProbs[ii] = -std::numeric_limits<double>::infinity();
    }
    partialLProbs[dimNumber] = -std::numeric_limits<double>::infinity();
    lProbs_ptr = lProbs_ptr_start + marginalResults[0]->get_no_confs()-1;
}

/*
 * ------------------------------------------------------------------------------------------------------------------------
 */


IsoOrderedGenerator::IsoOrderedGenerator(Iso&& iso, int _tabSize, int _hashSize) :
IsoGenerator(std::move(iso), false), allocator(dimNumber, _tabSize)
{
    partialLProbs = &currentLProb;
    partialMasses = &currentMass;
    partialProbs = &currentProb;

    marginalResults = new MarginalTrek*[dimNumber];

    for(int i = 0; i<dimNumber; i++)
        marginalResults[i] = new MarginalTrek(std::move(*(marginals[i])), _tabSize, _hashSize);

    logProbs        = new const vector<double>*[dimNumber];
    masses          = new const vector<double>*[dimNumber];
    marginalConfs   = new const vector<int*>*[dimNumber];

    for(int i = 0; i<dimNumber; i++)
    {
        masses[i] = &marginalResults[i]->conf_masses();
        logProbs[i] = &marginalResults[i]->conf_lprobs();
        marginalConfs[i] = &marginalResults[i]->confs();
    }

    topConf = allocator.newConf();
    memset(
            reinterpret_cast<char*>(topConf) + sizeof(double),
            0,
            sizeof(int)*dimNumber
    );

    *(reinterpret_cast<double*>(topConf)) =
    combinedSum(
                getConf(topConf),
                logProbs,
                dimNumber
    );

    pq.push(topConf);

}


IsoOrderedGenerator::~IsoOrderedGenerator()
{
    dealloc_table<MarginalTrek*>(marginalResults, dimNumber);
    delete[] logProbs;
    delete[] masses;
    delete[] marginalConfs;
    partialLProbs = nullptr;
    partialMasses = nullptr;
    partialProbs = nullptr;
}


bool IsoOrderedGenerator::advanceToNextConfiguration()
{
    if(pq.size() < 1)
        return false;


    topConf = pq.top();
    pq.pop();

    int* topConfIsoCounts = getConf(topConf);

    currentLProb = *(reinterpret_cast<double*>(topConf));
    currentMass = combinedSum( topConfIsoCounts, masses, dimNumber );
    currentProb = exp(currentLProb);

    ccount = -1;
    for(int j = 0; j < dimNumber; ++j)
    {
        if(marginalResults[j]->probeConfigurationIdx(topConfIsoCounts[j] + 1))
        {
            if(ccount == -1)
            {
                topConfIsoCounts[j]++;
                *(reinterpret_cast<double*>(topConf)) = combinedSum(topConfIsoCounts, logProbs, dimNumber);
                pq.push(topConf);
                topConfIsoCounts[j]--;
                ccount = j;
            }
            else
            {
                void* acceptedCandidate = allocator.newConf();
                int* acceptedCandidateIsoCounts = getConf(acceptedCandidate);
                memcpy(acceptedCandidateIsoCounts, topConfIsoCounts, confSize);

                acceptedCandidateIsoCounts[j]++;

                *(reinterpret_cast<double*>(acceptedCandidate)) = combinedSum(acceptedCandidateIsoCounts, logProbs, dimNumber);

                pq.push(acceptedCandidate);
            }
        }
        if(topConfIsoCounts[j] > 0)
            break;
    }
    if(ccount >=0)
        topConfIsoCounts[ccount]++;

    return true;
}



/*
 * ---------------------------------------------------------------------------------------------------
 */




#if !ISOSPEC_BUILDING_R

void printConfigurations(
    const   std::tuple<double*,double*,int*,int>& results,
    int     dimNumber,
    int*    isotopeNumbers
){
    int m = 0;

    for(int i=0; i<std::get<3>(results); i++){

        std::cout << "Mass = "  << std::get<0>(results)[i] <<
        "\tand log-prob = "     << std::get<1>(results)[i] <<
        "\tand prob = "                 << exp(std::get<1>(results)[i]) <<
        "\tand configuration =\t";


        for(int j=0; j<dimNumber; j++){
            for(int k=0; k<isotopeNumbers[j]; k++ )
            {
                std::cout << std::get<2>(results)[m] << " ";
                m++;
            }
            std::cout << '\t';
        }


        std::cout << std::endl;
    }
}

#endif /* !ISOSPEC_BUILDING_R */




} // namespace IsoSpec

