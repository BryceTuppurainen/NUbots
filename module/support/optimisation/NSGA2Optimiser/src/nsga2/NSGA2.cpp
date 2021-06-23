#include "NSGA2.hpp"

#include <cmath>
#include <nuclear>

namespace nsga2 {
    int NSGA2::PreEvaluationInitialize() {
        NUClear::log<NUClear::INFO>("Initializing NSGA-II\nChecking configuration...");

        if (realVars < 0) {
            NUClear::log<NUClear::INFO>("Invalid number of real variables");
            return -1;
        }
        if (binVars < 0) {
            NUClear::log<NUClear::INFO>("Invalid number of binary variables");
            return -1;
        }
        if (realVars == 0 && binVars == 0) {
            NUClear::log<NUClear::INFO>("Zero real and binary variables");
            return -1;
        }
        if (objectives < 1) {
            NUClear::log<NUClear::INFO>("Invalid number of objective functions");
            return -1;
        }
        if (constraints < 0) {
            NUClear::log<NUClear::INFO>("Invalid number of constraints");
            return -1;
        }
        if (popSize < 4 || (popSize % 4) != 0) {
            NUClear::log<NUClear::INFO>("Invalid size of population");
            return -1;
        }
        if (realCrossProb < 0.0 || realCrossProb > 1.0) {
            NUClear::log<NUClear::INFO>("Invalid probability of real crossover");
            return -1;
        }
        if (realMutProb < 0.0 || realMutProb > 1.0) {
            NUClear::log<NUClear::INFO>("Invalid probability of real mutation");
            return -1;
        }
        if (binCrossProb < 0.0 || binCrossProb > 1.0) {
            NUClear::log<NUClear::INFO>("Invalid probability of binary crossover");
            return -1;
        }
        if (binMutProb < 0.0 || binMutProb > 1.0) {
            NUClear::log<NUClear::INFO>("Invalid probability of binary mutation");
            return -1;
        }
        if (etaC <= 0) {
            NUClear::log<NUClear::INFO>("Invalid distribution index for crossover");
            return -1;
        }
        if (etaM <= 0) {
            NUClear::log<NUClear::INFO>("Invalid distribution index for mutation");
            return -1;
        }
        if (generations < 1) {
            NUClear::log<NUClear::INFO>("Invalid number of generations");
            return -1;
        }
        if (binVars != 0 && binBits.size() == 0) {
            NUClear::log<NUClear::INFO>("Invalid number of bits for binary variables");
            return -1;
        }
        if (int(realLimits.size()) != realVars) {
            NUClear::log<NUClear::INFO>("Invalid number of real variable limits");
            return -1;
        }
        if (int(binLimits.size()) != binVars) {
            NUClear::log<NUClear::INFO>("Invalid number of binary variable limits");
            return -1;
        }
        if (!randomInitialize && (int(initialRealVars.size()) != realVars)) {
            NUClear::log<NUClear::INFO>("Invalid number of initial real variables");
            return -1;
        }

        InitStreams();
        ReportParams(fpt5);

        binMutCount    = 0;
        realMutCount   = 0;
        binCrossCount  = 0;
        realCrossCount = 0;

        bitLength = std::accumulate(binBits.begin(), binBits.end(), 0);

        parentPop = std::make_shared<Population>(popSize,
                                                 realVars,
                                                 binVars,
                                                 constraints,
                                                 binBits,
                                                 realLimits,
                                                 binLimits,
                                                 objectives,
                                                 realMutProb,
                                                 binMutProb,
                                                 etaM,
                                                 epsC,
                                                 crowdObj,
                                                 randGen,
                                                 initialRealVars);

        childPop = std::make_shared<Population>(popSize,
                                                realVars,
                                                binVars,
                                                constraints,
                                                binBits,
                                                realLimits,
                                                binLimits,
                                                objectives,
                                                realMutProb,
                                                binMutProb,
                                                etaM,
                                                epsC,
                                                crowdObj,
                                                randGen,
                                                initialRealVars);

        mixedPop = std::make_shared<Population>(popSize * 2,
                                                realVars,
                                                binVars,
                                                constraints,
                                                binBits,
                                                realLimits,
                                                binLimits,
                                                objectives,
                                                realMutProb,
                                                binMutProb,
                                                etaM,
                                                epsC,
                                                crowdObj,
                                                randGen,
                                                initialRealVars);

        parentPop->Initialize(randomInitialize);
        NUClear::log<NUClear::INFO>("Initialization done!");

        parentPop->Decode();
        // parentPop->EvaluateInd(); // split here
        return 0;
    }

    void NSGA2::PostEvaluationInitialize() {
        parentPop->FastNDS();
        parentPop->CrowdingDistanceAll();

        currentGen = 1;

        // NUClear::log<NUClear::INFO>("Generation 1 complete!");

        ReportPop(parentPop, fpt1);
        fpt4 << "# gen = " << currentGen << "\n";
        ReportPop(parentPop, fpt4);

        fpt1.flush();
        fpt4.flush();
        fpt5.flush();
    }

    void NSGA2::InitStreams() {
        fpt1.open("nsga2_initial_pop.out", std::ios::out | std::ios::trunc);
        fpt2.open("nsga2_final_pop.out", std::ios::out | std::ios::trunc);
        fpt3.open("nsga2_best_pop.out", std::ios::out | std::ios::trunc);
        fpt4.open("nsga2_all_pop.out", std::ios::out | std::ios::trunc);
        fpt5.open("nsga2_params.out", std::ios::out | std::ios::trunc);

        fpt1.setf(std::ios::scientific);
        fpt2.setf(std::ios::scientific);
        fpt3.setf(std::ios::scientific);
        fpt4.setf(std::ios::scientific);
        fpt4.precision(16);
        fpt5.setf(std::ios::scientific);

        fpt1 << "# This file contains the data of initial population\n";
        fpt2 << "# This file contains the data of final population\n";
        fpt3 << "# This file contains the data of final feasible population (if found)\n";
        fpt4 << "# This file contains the data of all generations\n";
        fpt5 << "# This file contains information about inputs as read by the program\n";

        fpt1 << "# of objectives = " << objectives << ", # of constraints = " << constraints
             << ", # of real variables = " << realVars << ", # of bits of binary variables = " << bitLength
             << ", constraint violation, rank, crowding_distance\n";
        fpt2 << "# of objectives = " << objectives << ", # of constraints = " << constraints
             << ", # of real variables = " << realVars << ", # of bits of binary variables = " << bitLength
             << ", constraint violation, rank, crowding_distance\n";
        fpt3 << "# of objectives = " << objectives << ", # of constraints = " << constraints
             << ", # of real variables = " << realVars << ", # of bits of binary variables = " << bitLength
             << ", constrViolation, rank, crowding_distance\n";
        fpt4 << "# of objectives = " << objectives << ", # of constraints = " << constraints
             << ", # of real variables = " << realVars << ", # of bits of binary variables = " << bitLength
             << ", constrViolation, rank, crowding_distance\n";
    }

    void NSGA2::ReportParams(std::ostream& _os) const {
        _os << "Population size = " << popSize << "\nNumber of generations = " << generations
            << "\nNumber of objective functions = " << objectives << "\nNumber of constraints = " << constraints
            << "\nNumber of real variables = " << realVars;

        if (realVars != 0) {
            for (int i = 0; i < realVars; i++) {
                _os << "\nLower limit of real variable " << (i + 1) << " = " << realLimits[i].first;
                _os << "\nUpper limit of real variable " << (i + 1) << " = " << realLimits[i].second;
            }
            _os << "\nProbability of crossover of real variable = " << realCrossProb;
            _os << "\nProbability of mutation of real variable = " << realMutProb;
            _os << "\nDistribution index for crossover = " << etaC;
            _os << "\nDistribution index for mutation = " << etaM;
        }

        _os << "\nNumber of binary variables = " << binVars;
        if (binVars != 0) {
            for (int i = 0; i < binVars; i++) {
                _os << "\nNumber of bits for binary variable " << (i + 1) << " = " << binBits[i];
                _os << "\nLower limit of real variable " << (i + 1) << " = " << binLimits[i].first;
                _os << "\nUpper limit of real variable " << (i + 1) << " = " << binLimits[i].second;
            }
            _os << "Probability of crossover of binary variable = " << binCrossProb;
            _os << "Probability of mutation of binary variable = " << binMutProb;
        }
        _os << "\nSeed for random number generator = " << randGen->GetSeed() << std::endl;
    }

    void NSGA2::ReportPop(const std::shared_ptr<Population>& _pop, std::ostream& _os) const {
        _pop->Report(_os);
    }

    void NSGA2::Selection(const std::shared_ptr<Population>& _oldPop, std::shared_ptr<Population>& _newPop) {
        const int oldPopSize = _oldPop->GetSize();
        if (_newPop->GetSize() != oldPopSize) {
            NUClear::log<NUClear::ERROR>("Selection error: new and old pops don't have the same size");
        }

        std::vector<int> indList1(oldPopSize), indList2(oldPopSize);
        for (int i = 0; i < oldPopSize; i++) {
            indList1[i] = i;
            indList2[i] = i;
        }

        for (int i = 0; i < oldPopSize; i++) {
            int randInt = randGen->Integer(i, oldPopSize - 1);
            std::swap(indList1[randInt], indList1[i]);
            randInt = randGen->Integer(i, oldPopSize - 1);
            std::swap(indList2[randInt], indList2[i]);
        }

        for (int i = 0; i < oldPopSize; i += 4) {
            const Individual& p11 = Tournament(_oldPop->inds[indList1[i]], _oldPop->inds[indList1[i + 1]]);
            const Individual& p12 = Tournament(_oldPop->inds[indList1[i + 2]], _oldPop->inds[indList1[i + 3]]);
            Crossover(p11, p12, _newPop->inds[i], _newPop->inds[i + 1]);

            const Individual& p21 = Tournament(_oldPop->inds[indList2[i]], _oldPop->inds[indList2[i + 1]]);
            const Individual& p22 = Tournament(_oldPop->inds[indList2[i + 2]], _oldPop->inds[indList2[i + 3]]);
            Crossover(p21, p22, _newPop->inds[i + 2], _newPop->inds[i + 3]);
        }
    }

    const Individual& NSGA2::Tournament(const Individual& _ind1, const Individual& _ind2) const {
        const int flag = _ind1.CheckDominance(_ind2);
        if (flag == 1) {  // ind1 dominates ind2
            return _ind1;
        }
        else if (flag == -1) {  // ind2 dominates ind1
            return _ind2;
        }
        else if (_ind1.crowdDist > _ind2.crowdDist) {
            return _ind1;
        }
        else if (_ind2.crowdDist > _ind1.crowdDist) {
            return _ind2;
        }
        else if (randGen->Realu() <= 0.5) {
            return _ind1;
        }
        else {
            return _ind2;
        }
    }

    void NSGA2::Crossover(const Individual& _parent1,
                          const Individual& _parent2,
                          Individual& _child1,
                          Individual& _child2) {
        if (realVars) {
            Realcross(_parent1, _parent2, _child1, _child2);
        }
        if (binVars) {
            Bincross(_parent1, _parent2, _child1, _child2);
        }

        _child1.evaluated = false;
        _child2.evaluated = false;
    }

    void NSGA2::Realcross(const Individual& _parent1,
                          const Individual& _parent2,
                          Individual& _child1,
                          Individual& _child2) {
        double y1, y2, yLower, yUpper;
        double c1, c2;
        double alpha, beta, betaQ;

        if (randGen->Realu() <= realCrossProb) {
            realCrossCount++;
            for (int i = 0; i < realVars; i++) {
                if (std::fabs(_parent1.reals[i] - _parent2.reals[i]) > std::numeric_limits<double>::epsilon()) {
                    if (_parent1.reals[i] < _parent2.reals[i]) {
                        y1 = _parent1.reals[i];
                        y2 = _parent2.reals[i];
                    }
                    else {
                        y1 = _parent2.reals[i];
                        y2 = _parent1.reals[i];
                    }

                    yLower = realLimits[i].first;
                    yUpper = realLimits[i].second;

                    double randDouble = randGen->Realu();
                    beta              = 1.0 + (2.0 * (y1 - yLower) / (y2 - y1));
                    alpha             = 2.0 - std::pow(beta, -(etaC + 1.0));
                    if (randDouble <= (1.0 / alpha))
                        betaQ = std::pow((randDouble * alpha), (1.0 / (etaC + 1.0)));
                    else
                        betaQ = std::pow((1.0 / (2.0 - randDouble * alpha)), (1.0 / (etaC + 1.0)));

                    c1    = 0.5 * ((y1 + y2) - betaQ * (y2 - y1));
                    beta  = 1.0 + (2.0 * (yUpper - y2) / (y2 - y1));
                    alpha = 2.0 - std::pow(beta, -(etaC + 1.0));

                    if (randDouble <= (1.0 / alpha))
                        betaQ = std::pow((randDouble * alpha), (1.0 / (etaC + 1.0)));
                    else
                        betaQ = std::pow((1.0 / (2.0 - randDouble * alpha)), (1.0 / (etaC + 1.0)));

                    c2 = 0.5 * ((y1 + y2) + betaQ * (y2 - y1));
                    c1 = std::min(std::max(c1, yLower), yUpper);
                    c2 = std::min(std::max(c2, yLower), yUpper);

                    if (randGen->Realu() <= 0.5) {
                        _child1.reals[i] = c2;
                        _child2.reals[i] = c1;
                    }
                    else {
                        _child1.reals[i] = c1;
                        _child2.reals[i] = c2;
                    }
                }
                else {
                    _child1.reals[i] = _parent1.reals[i];
                    _child2.reals[i] = _parent2.reals[i];
                }
            }
        }
        else {
            for (int i = 0; i < realVars; i++) {
                _child1.reals[i] = _parent1.reals[i];
                _child2.reals[i] = _parent2.reals[i];
            }
        }
    }

    void NSGA2::Bincross(const Individual& _parent1,
                         const Individual& _parent2,
                         Individual& _child1,
                         Individual& _child2) {
        int temp, site1, site2;
        for (int i = 0; i < binVars; i++) {
            double randDouble = randGen->Realu();
            if (randDouble <= binCrossProb) {
                binCrossCount++;
                site1 = randGen->Integer(0, binBits[i] - 1);
                site2 = randGen->Integer(0, binBits[i] - 1);
                if (site1 > site2) {
                    temp  = site1;
                    site1 = site2;
                    site2 = temp;
                }

                for (int j = 0; j < site1; j++) {
                    _child1.gene[i][j] = _parent1.gene[i][j];
                    _child2.gene[i][j] = _parent2.gene[i][j];
                }
                for (int j = site1; j < site2; j++) {
                    _child1.gene[i][j] = _parent2.gene[i][j];
                    _child2.gene[i][j] = _parent1.gene[i][j];
                }
                for (int j = site2; j < binBits[i]; j++) {
                    _child1.gene[i][j] = _parent1.gene[i][j];
                    _child2.gene[i][j] = _parent2.gene[i][j];
                }
            }
            else {
                for (int j = 0; j < binBits[i]; j++) {
                    _child1.gene[i][j] = _parent1.gene[i][j];
                    _child2.gene[i][j] = _parent2.gene[i][j];
                }
            }
        }
    }

    struct sort_n {
        const Population& pop;
        sort_n(const Population& population) : pop(population) {}
        bool operator()(int i, int j) {
            const Individual& ind1 = pop.inds[i];
            const Individual& ind2 = pop.inds[j];
            if (ind1.rank < ind2.rank)
                return true;
            else if (ind1.rank == ind2.rank && ind1.crowdDist > ind2.crowdDist)
                return true;
            return false;
        }
    };

    void NSGA2::PreEvaluationAdvance() {
        NUClear::log<NUClear::INFO>("Advancing to generation", currentGen + 1);

        // create next population Qt
        Selection(parentPop, childPop);
        std::pair<int, int> mutationsCount = childPop->Mutate();
        // mutation book-keeping
        realMutCount += mutationsCount.first;
        binMutCount += mutationsCount.second;

        childPop->generation = currentGen + 1;
        childPop->Decode();
        // childPop->Evaluate();
    }

    void NSGA2::PostEvaluationAdvance() {
        // create population Rt = Pt U Qt
        mixedPop->Merge(*parentPop, *childPop);
        mixedPop->generation = currentGen + 1;
        mixedPop->FastNDS();

        // Pt + 1 = empty
        parentPop->inds.clear();

        int i = 0;
        // until |Pt+1| + |Fi| <= N, i.e. until parent population is filled
        while (parentPop->GetSize() + int(mixedPop->front[i].size()) < popSize) {
            std::vector<int>& Fi = mixedPop->front[i];
            mixedPop->CrowdingDistance(i);            // calculate crowding in Fi
            for (int j = 0; j < int(Fi.size()); j++)  // Pt+1 = Pt+1 U Fi
            {
                parentPop->inds.push_back(mixedPop->inds[Fi[j]]);
            }
            i++;
        }

        mixedPop->CrowdingDistance(i);  // calculate crowding in Fi

        std::sort(mixedPop->front[i].begin(),
                  mixedPop->front[i].end(),
                  sort_n(*mixedPop));  // sort remaining front using <n

        const int extra = popSize - parentPop->GetSize();
        for (int j = 0; j < extra; j++)  // Pt+1 = Pt+1 U Fi[1:N-|Pt+1|]
        {
            parentPop->inds.push_back(mixedPop->inds[mixedPop->front[i][j]]);
        }

        currentGen++;

        parentPop->generation = currentGen;

        fpt4 << "# gen = " << currentGen << "\n";
        ReportPop(parentPop, fpt4);
        fpt4.flush();
    }

    void NSGA2::ReportFinalGenerationPop() {
        ReportPop(parentPop, fpt2);
    }
}  // namespace nsga2
