#include "accessibility.h"
#include <algorithm> // min, transform
#include <filesystem> // path
#include <iterator> // ostream_iterator
#include <fstream>  // ofstream

namespace fs = std::filesystem;

namespace MTC::accessibility {

    vector<int> Accessibility::RoutesToFile(
        const vector<long>& sources,
        const vector<long>& targets,
        int graphno,
        const vector<int>& trip_ids,
        char const* file_name
    ) {
        return RoutesInternal(sources, targets, graphno, trip_ids, file_name, nullptr);
    }

    std::vector<int> Accessibility::RoutesInternal(
        vector<long> const& sources,
        vector<long> const& targets,
        int graphno,
        vector<int> const& trip_ids,
        char const* output_file,
        vector<vector<int>>* routes_result
    ) {
         // Returned results are only populated if routes_result == nullptr.

        static constexpr size_t max_batch_size = 10000;
        const auto n = sources.size();

        if(n != targets.size()) {
            throw std::runtime_error("size of sources and targets differ");
        }

        bool use_dir = false;
        vector<vector<std::pair<int, int>>> routes;
        vector<int> results;
        std::ofstream results_file;
        auto results_filename = fs::path(output_file);

        if(results_filename.empty()) {
            if(routes_result == nullptr) {
                throw std::runtime_error("file_name cannot be empty if result is nullptr");
            }
            routes.resize(n);
        } else {
            use_dir = true;

            if(fs::is_directory(results_filename)) {
                throw std::runtime_error("results_file must not be a directory");
            } else if(!fs::exists(results_filename.parent_path())) {
                throw std::runtime_error("results_file must specify a directory that exists");
            }

            if(trip_ids.size() != n) {
                throw std::runtime_error("size of sources and trip_ids differ");
            }

            auto write_header = !fs::exists(results_filename);
            results_file = std::ofstream(results_filename, std::ios::app);
            if(write_header) {
                // use std::endl TO flush
                results_file << "trip_id,edge_id,link_id" << std::endl;
            }

            routes.resize(std::min(max_batch_size, n));
        }

        if(routes_result == nullptr) {
            results.reserve(routes.size());
        } else {
            routes_result->reserve(routes.size());
        }

        size_t n_to_go = n, n_done = 0;
        auto bi = trip_ids.cbegin();
        do {
            auto current_batch_size = use_dir ? std::min(max_batch_size, n_to_go) : n;

#pragma omp parallel
#pragma omp for schedule(guided)
            for (int i = 0; i < current_batch_size; i++) {
                // remember we can't just ++n_done here, because we are in a parallel block
                auto trip = n_done + i;
                vector<NodeID> ret = this->ga[graphno]->Route(sources[trip], targets[trip], omp_get_thread_num());
                if (ret.size() > 1) {
                    auto& routed_vec = routes[i];
                    routed_vec.resize(ret.size() - 1);
                    try {
                        std::transform(
                                ret.cbegin(), ret.cend() - 1, ret.cbegin() + 1,
                                routed_vec.begin(),
                                [&](NodeID src, NodeID dst) { return this->nodeIdsToEdgeId.at({src, dst}); });
                    } catch (std::exception const &e) {
                        throw;
                    }
                } else {
                    routes[i].resize(0);
                }
            }

            for (int i = 0; i < current_batch_size; ++i, ++bi) {
                auto const &vec = routes[i];
                if (!vec.empty()) {
                    vector<int>* routes_result_vec = nullptr;
                    if(routes_result == nullptr) {
                        results.push_back(*bi);
                    } else {
                        routes_result->emplace_back();
                        routes_result_vec = &routes_result->back();
                        routes_result_vec->reserve(vec.size());
                    }
                    for (auto br = vec.cbegin(), er = vec.cend(); br != er; ++br) {
                        if(use_dir) {
                            results_file << *bi << ',' << br->first << ',' << br->second << '\n';
                        }
                        if(routes_result_vec != nullptr) {
                            routes_result_vec->push_back(return_edge_ids ? br->first : br->second);
                        }
                    }
                }
            }

            n_to_go -= current_batch_size;
            n_done += current_batch_size;
        } while(n_to_go > 0);

        return results;
    }
} // end namespace MTC::accessibility
