#include "accessibility.h"


namespace MTC::accessibility {
    vector<vector<int>> Accessibility::RoutesInternal(vector<long> const& sources, vector<long> const& targets, int graphno) {
        int n = std::min(sources.size(), targets.size()); // in case lists don't match
        vector<vector<int>> routes(n);

#pragma omp parallel
#pragma omp for schedule(guided)
        for (int i = 0; i < n; i++) {
            vector<NodeID> ret = this->ga[graphno]->Route(sources[i], targets[i], omp_get_thread_num());
            if (ret.size() > 1) {
                routes[i].resize(ret.size() - 1);
                try {
                    std::transform(
                            ret.cbegin(), ret.cend() - 1, ret.cbegin() + 1,
                            routes[i].begin(),
                            [&](NodeID src, NodeID dst) { return this->nodeIdsToEdgeId.at({src, dst}); });
                } catch (std::exception const &e) {
                    throw;
                }
            }

        }

        return routes;
    }
} // end namespace MTC::accessibility
