#include "Population.hpp"

#include <nuclear>

namespace nsga2 {
    Population::Population(const int& _size,
                           const int& _realVars,
                           const int& _binVars,
                           const int& _constraints,
                           const std::vector<int>& _binBits,
                           const std::vector<std::pair<double, double>>& _realLimits,
                           const std::vector<std::pair<double, double>>& _binLimits,
                           const int& _objectives,
                           const double& _realMutProb,
                           const double& _binMutProb,
                           const double& _etaM,
                           const double& _epsC,
                           const bool& _crowdObj,
                           std::shared_ptr<RandomGenerator<>> _randGen,
                           const std::vector<double>& _initialRealVars)
        : generation(1)
        , indConfig({_realVars,
                     _realLimits,
                     _realMutProb,
                     _binVars,
                     _binBits,
                     _binLimits,
                     _binMutProb,
                     _objectives,
                     _constraints,
                     _etaM,
                     _epsC,
                     _randGen,
                     _initialRealVars})
        , size(_size)
        , crowdObj(_crowdObj) {

        for (int i = 0; i < _size; i++) {
            inds.emplace_back(indConfig);
        }
    }

    void Population::Initialize(const bool& randomInitialize) {
        for (int i = 0; i < size; i++) {
            inds[i].Initialize(i, randomInitialize);
        }
    }
    void Population::Decode() {
        for (auto& ind : inds) {
            ind.Decode();
        }
    }

    std::vector<double> Population::GetIndReals(const int& _id) {
        return inds[_id].reals;
    }

    void Population::SetIndObjectiveScore(const int& _id, const std::vector<double>& _objScore) {
        inds[_id].objScore = _objScore;
    }

    void Population::SetIndConstraints(const int& _id, const std::vector<double>& _constraints) {
        inds[_id].constr = _constraints;
    }

    void Population::CheckConstraints() {
        for (auto& ind : inds) {
            ind.CheckConstraints();
        }
    }

    void Population::FastNDS() {
        front.resize(1);
        front[0].clear();

        for (int i = 0; i < int(inds.size()); i++) {
            std::vector<int> dominationList;
            int dominationCount = 0;
            Individual& indP    = inds[i];

            for (int j = 0; j < int(inds.size()); j++) {
                const Individual& indQ = inds[j];

                int comparison = indP.CheckDominance(indQ);
                if (comparison == 1) {
                    dominationList.push_back(j);
                }
                else if (comparison == -1) {
                    dominationCount++;
                }
            }

            indP.dominations = dominationCount;
            indP.dominated.clear();
            indP.dominated = dominationList;

            if (indP.dominations == 0) {
                indP.rank = 1;
                front[0].push_back(i);
            }
        }

        std::sort(front[0].begin(), front[0].end());

        int fi = 1;
        while (front[fi - 1].size() > 0) {
            std::vector<int>& fronti = front[fi - 1];
            std::vector<int> Q;
            for (int i = 0; i < int(fronti.size()); i++) {
                Individual& indP = inds[fronti[i]];

                for (int j = 0; j < int(indP.dominated.size()); j++) {
                    Individual& indQ = inds[indP.dominated[j]];
                    indQ.dominations--;

                    if (indQ.dominations == 0) {
                        indQ.rank = fi + 1;
                        Q.push_back(indP.dominated[j]);
                    }
                }
            }

            fi++;
            front.push_back(Q);
        }
    }

    void Population::CrowdingDistanceAll() {
        for (int i = 0; i < int(front.size()); i++) {
            CrowdingDistance(i);
        }
    }

    void Population::CrowdingDistance(const int& _frontI) {
        std::vector<int>& F = front[_frontI];
        if (F.size() == 0) {
            return;
        }

        const int l = F.size();
        for (int i = 0; i < l; i++) {
            inds[F[i]].crowdDist = 0;
        }
        const int limit = crowdObj ? indConfig.objectives : indConfig.realVars;
        for (int i = 0; i < limit; i++) {
            std::sort(F.begin(), F.end(), [&](const int& a, const int& b) {
                return crowdObj ? inds[a].objScore[i] < inds[b].objScore[i] : inds[a].reals[i] < inds[b].reals[i];
            });

            inds[F[0]].crowdDist = std::numeric_limits<double>::infinity();
            if (l > 1)
                inds[F[l - 1]].crowdDist = std::numeric_limits<double>::infinity();

            for (int j = 1; j < l - 1; j++) {
                if (inds[F[j]].crowdDist != std::numeric_limits<double>::infinity()) {
                    if (crowdObj && inds[F[l - 1]].objScore[i] != inds[F[0]].objScore[i]) {
                        inds[F[j]].crowdDist += (inds[F[j + 1]].objScore[i] - inds[F[j - 1]].objScore[i])
                                                / (inds[F[l - 1]].objScore[i] - inds[F[0]].objScore[i]);
                    }
                    else if (!crowdObj && inds[F[l - 1]].reals[i] != inds[F[0]].reals[i]) {
                        inds[F[j]].crowdDist += (inds[F[j + 1]].reals[i] - inds[F[j - 1]].reals[i])
                                                / (inds[F[l - 1]].reals[i] - inds[F[0]].reals[i]);
                    }
                }
            }
        }
    }

    void Population::Merge(const Population& _pop1, const Population& _pop2) {
        if (GetSize() < _pop1.GetSize() + _pop2.GetSize()) {
            NUClear::log<NUClear::WARN>("Merge: target population not big enough");
            inds.reserve(_pop1.GetSize() + _pop2.GetSize());
        }

        std::copy(_pop1.inds.begin(), _pop1.inds.end(), inds.begin());
        std::copy(_pop2.inds.begin(), _pop2.inds.end(), inds.begin() + _pop1.GetSize());
    }

    void Population::Report(std::ostream& os, int generation) const {
        std::vector<Individual>::const_iterator it;

        for (it = inds.begin(); it != inds.end(); it++) {
            os << generation << "," << it->id << ",";

            for (int i = 0; i < indConfig.objectives; i++) {
                os << it->objScore[i] << ",";
            }

            for (int i = 0; i < indConfig.constraints; i++) {
                os << it->constr[i] << ",";
            }

            for (int i = 0; i < indConfig.realVars; i++) {
                os << it->reals[i] << ",";
            }

            for (int i = 0; i < indConfig.binVars; i++) {
                for (int j = 0; j < indConfig.binBits[i]; j++) {
                    os << it->gene[i][j] << ",";
                }
            }

            os << it->constrViolation << "," << it->rank << "," << it->crowdDist << std::endl;
        }
    }

    std::pair<int, int> Population::Mutate() {
        std::pair<int, int> mutCount    = std::make_pair(0, 0);
        std::pair<int, int> indMutCount = std::make_pair(0, 0);
        std::vector<Individual>::iterator it;
        for (it = inds.begin(); it != inds.end(); it++) {
            indMutCount = it->Mutate();
            mutCount.first += indMutCount.first;
            mutCount.second += indMutCount.second;
        }
        return mutCount;
    }

    std::ostream& operator<<(std::ostream& _os, const Population& _pop) {
        _os << "Population: {\n";
        std::vector<Individual>::const_iterator it;
        for (it = _pop.inds.begin(); it != _pop.inds.end(); it++) {
            _os << *it;
        }
        _os << "}";
        return _os;
    }
}  // namespace nsga2
