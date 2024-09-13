#include "accessibility.h"
#include "route_state.h"
#include <algorithm>  // min, transform
#include <cmath> // sqrt
#include <filesystem> // path
#include <fstream>    // ifstream, ofstream

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
        const auto n_trips = sources.size();

        if(n_trips != targets.size()) {
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
            routes.resize(n_trips);
        } else {
            use_dir = true;

            if(fs::is_directory(results_filename)) {
                throw std::runtime_error("results_file must not be a directory");
            } else if(!fs::exists(results_filename.parent_path())) {
                throw std::runtime_error("results_file must specify a directory that exists");
            }

            if(trip_ids.size() != n_trips) {
                throw std::runtime_error("size of sources and trip_ids differ");
            }

            auto write_header = !fs::exists(results_filename);
            results_file = std::ofstream(results_filename, std::ios::app);
            if(write_header) {
                // use std::endl TO flush
                results_file << "trip_id,edge_id,link_id" << std::endl;
            }

            routes.resize(std::min(max_batch_size, n_trips));
        }

        if(routes_result == nullptr) {
            results.reserve(routes.size());
        } else {
            routes_result->reserve(routes.size());
        }

        size_t n_to_go = n_trips, n_done = 0;
        auto bi = trip_ids.cbegin();
        do {
            auto current_batch_size = use_dir ? std::min(max_batch_size, n_to_go) : n_trips;

#pragma omp parallel
#pragma omp for schedule(guided)
            for (int i = 0; i < current_batch_size; ++i) {
                // remember we can't just ++n_done here, because we are in a parallel block
                auto trip = n_done + i;
                vector<NodeID> ret = this->ga[graphno]->Route(sources[trip], targets[trip], omp_get_thread_num());
                if (ret.size() > 1) {
                    auto& routed_vec = routes[i];
                    routed_vec.resize(ret.size() - 1);
                    std::transform(
                        ret.cbegin(), ret.cend() - 1, ret.cbegin() + 1,
                        routed_vec.begin(),
                        [&](NodeID src, NodeID dst) { return this->nodeIdsToEdgeId.at({src, dst}); });
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
                    for (auto const& p : vec) {
                        if(use_dir) {
                            results_file << *bi << ',' << p.first << ',' << p.second << '\n';
                        }
                        if(routes_result_vec != nullptr) {
                            // TODO: Determine if we ever want to return trips ids, for now
                            //       always return the edge ids
                            //routes_result_vec->push_back(return_edge_ids ? p.first : p.second);
                            routes_result_vec->push_back(p.first);
                        }
                    }
                }
            }

            n_to_go -= current_batch_size;
            n_done += current_batch_size;
        } while(n_to_go > 0);

        return results;
    }


    void Accessibility::RoutesStats(
        vector<long> const& sources,
        vector<long> const& targets,
        int graphno,
        vector<double> const& tonnes,
        char const* commodity,
        RoutingStatsState* routing_stats_state
    ) const {
        static constexpr size_t max_batch_size = 10000;

        const auto num_threads = omp_get_max_threads();
        const auto n_links = routing_stats_state->max_link_id() + 1;
        const auto n_trips = sources.size();

        if(targets.size() != n_trips) {
            throw std::runtime_error("size of sources and targets differ");
        }
        if(tonnes.size() != n_trips) {
            throw std::runtime_error("size of sources and tonnes differ");
        }

        if(!has_link_ids) {
            throw std::runtime_error("requested stats for links but links weren't provided");
        }

        auto total_tonnes_for_trips = vector<double>(n_links);
        auto total_tonnes_for_trips_vectors = std::vector<vector<double>>(num_threads, std::vector<double>(n_links));

        size_t n_to_go = n_trips, n_done = 0;
        auto tonnes_iter = tonnes.cbegin();
        do {
            auto current_batch_size = std::min(max_batch_size, n_to_go);

#pragma omp parallel
            {
                auto included_ids_thread = std::vector<bool>(n_links);
                auto& total_tonnes_for_trips_thread = total_tonnes_for_trips_vectors[omp_get_thread_num()];

#pragma omp for schedule(guided)
                for (int i = 0; i < current_batch_size; ++i) {
                    // remember we can't just ++n_done here, because we are in a parallel block
                    auto trip = n_done + i;
                    auto trip_size_tonnes = *(tonnes_iter + i);
                    vector<NodeID> ret = this->ga[graphno]->Route(sources[trip], targets[trip], omp_get_thread_num());
                    if (ret.size() > 1) {
                        std::fill(included_ids_thread.begin(), included_ids_thread.end(), false);
                        for (
                            auto src = ret.cbegin(), dst = ret.cbegin() + 1, end = ret.cend();
                            dst != end;
                            ++src, ++dst
                        ) {
                            auto link_id = this->nodeIdsToEdgeId.at({*src, *dst}).second;
                            if (link_id >= n_links) {
                                throw std::runtime_error("link_id " + std::to_string(link_id) + " out of bounds");
                            } else if (link_id >= 0) {
                                included_ids_thread[link_id] = true;
                            }
                        }

                        auto bt = total_tonnes_for_trips_thread.begin();
                        for (auto id_included: included_ids_thread) {
                            if (id_included) {
                                *bt += trip_size_tonnes;
                            }
                            ++bt;
                        }
                    }
                }
            }

            for(auto i=0; i<num_threads; ++i) {
                std::transform(
                    total_tonnes_for_trips_vectors[i].cbegin(),
                    total_tonnes_for_trips_vectors[i].cend(),
                    total_tonnes_for_trips.cbegin(),
                    total_tonnes_for_trips.begin(),
                    [](double thread_total, double total) { return total + thread_total; });
            }

            n_to_go -= current_batch_size;
            n_done += current_batch_size;
            tonnes_iter += current_batch_size;
        } while(n_to_go > 0);

        auto ba = (*routing_stats_state)[commodity].begin();
        for(auto val: total_tonnes_for_trips) {
            ba->first += val;
            ba->second += val * val;
            ++ba;
        }
    }
} // end namespace MTC::accessibility
