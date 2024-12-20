#include "accessibility.h"
#include "route_state.h"
#include <algorithm>  // min, transform
#include <cmath> // sqrt
#include <filesystem> // path
#include <fstream>    // ifstream, ofstream

#define INCLUDE_LINKS_ONCE false

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
                vector<NodeID> route = this->ga[graphno]->Route(sources[trip], targets[trip], omp_get_thread_num());
                if (route.size() > 1) {
                    auto& routed_segment_ids = routes[i];
                    routed_segment_ids.resize(route.size() - 1);
                    std::transform(
                        route.cbegin(), route.cend() - 1, route.cbegin() + 1,
                        routed_segment_ids.begin(),
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
        const auto num_threads = omp_get_max_threads();
        const auto n_links = routing_stats_state->n_links();
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

        auto total_tonnes_for_trips_vectors = std::vector<vector<double>>(num_threads, std::vector<double>(n_links, 0.0));

#pragma omp parallel
        {
#if INCLUDE_LINKS_ONCE
            auto included_ids_thread = std::vector<bool>(n_links);
#endif // INCLUDE_LINKS_ONCE
            auto& total_tonnes_for_trips_thread = total_tonnes_for_trips_vectors[omp_get_thread_num()];

#pragma omp for schedule(guided)
            for (int trip = 0u; trip < n_trips; ++trip) {

                // We can't ++sources.begin() and ++targets.begin() here, because we are in a parallel block
                vector<NodeID> link_ids = this->ga[graphno]->Route(sources[trip], targets[trip], omp_get_thread_num());

                if (link_ids.size() > 1) {
                    // If a trip traverses the same link more than once, we might only want to count it once. This can
                    // happen, for example, when decoupling. If, however we allowed multiple crossing to count (i.e., if
                    // a link is traversed n times we count it n times), then we could avoid all the conditionals in the
                    // following loop and the need for the transform. This might make a performance difference.
#if INCLUDE_LINKS_ONCE
                    std::fill(included_ids_thread.begin(), included_ids_thread.end(), false);
#endif // INCLUDE_LINKS_ONCE
                    auto trip_size_tonnes = tonnes[trip];
                    for (
                        auto
                            src = link_ids.cbegin(),
                            dst = link_ids.cbegin() + 1,
                            end = link_ids.cend();
                        dst != end;
                        ++src, ++dst
                    ) {
                        auto link_id = this->nodeIdsToEdgeId.at({*src, *dst}).second;
                        if (link_id >= n_links) {
                            // TODO: We probably don't this check.
                            throw std::runtime_error("link_id " + std::to_string(link_id) + " out of bounds");
                        } else if (link_id >= 0) {
#if INCLUDE_LINKS_ONCE
                            included_ids_thread[link_id] = true;
#else // then not INCLUDE_LINKS_ONCE
                            total_tonnes_for_trips_thread[link_id] += trip_size_tonnes;
#endif // INCLUDE_LINKS_ONCE
                        }
                    }

#if INCLUDE_LINKS_ONCE
                    std::transform(
                        included_ids_thread.cbegin(),
                        included_ids_thread.cend(),
                        total_tonnes_for_trips_thread.cbegin(),
                        total_tonnes_for_trips_thread.begin(),
                        [=](bool included, double total) { return included ? (total + trip_size_tonnes) : total; });
#endif // INCLUDE_LINKS_ONCE
                }
            } // end omp for schedule
        } // end omp parallel

        // We don't write directly to output because it contains vectors of
        // floats and we don't want rounding problems while aggregating.
        auto total_for_all_threads = std::vector<double>(n_links, 0.0);
        for(auto const& thread_totals: total_tonnes_for_trips_vectors) {
            std::transform(
                thread_totals.cbegin(),
                thread_totals.cend(),
                total_for_all_threads.cbegin(),
                total_for_all_threads.begin(),
                [](double thread_total, double total) { return total + thread_total; });
        }
        std::copy(
            total_for_all_threads.cbegin(),
            total_for_all_threads.cend(),
            (*routing_stats_state)[commodity].begin());
    }
} // end namespace MTC::accessibility
