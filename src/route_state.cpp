#include "route_state.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace MTC::accessibility {
    RoutingStatsState::RoutingStatsState(int max_link_id) : max_link_id_(max_link_id) {}

    RoutingStatsState::StatsVector& RoutingStatsState::operator[](std::string const& commodity) {
        auto& res = stats_[commodity];
        if(res.size() != max_link_id() + 1) {
            res.resize(max_link_id() + 1);
        }
        return res;
    }

    void RoutingStatsState::serialise(const char * output_directory, int n_runs, int job_id) {
        auto output_dir = fs::path(output_directory);
        if(!fs::is_directory(output_dir)) {
            throw std::runtime_error("output_dir must be a directory");
        }
        for(auto const& [commodity, edge_stats]: stats_) {
            auto output_file = std::ofstream(fs::path(output_dir) / (commodity + "_" + std::to_string(job_id) + ".csv"));
            output_file << "link_id,mean,sd\n";
            auto id = 0;
            output_file << std::setprecision(2);
            for(auto const& moments: edge_stats) {
                if(moments.first <= 0.0) {
                    continue;
                }
                auto mean = static_cast<long double>(moments.first) / n_runs;
                auto var = static_cast<long double>(moments.second) / n_runs - mean * mean;
                if(var < -1e-2 && (std::sqrt(-var) / mean) > 1e-6) {
                    throw std::runtime_error(
                        std::string("negative variance: (") + commodity + ") " + std::to_string(var) +
                        "\n\tmean: " + std::to_string(mean) +
                        "\n\tsum(x): " + std::to_string(moments.first) +
                        "\n\tsum(x^2): " + std::to_string(moments.second) +
                        "\n");
                }
                output_file << id << ',' << mean << ',' << (var > 0.L ? std::sqrt(var) : 0.L) << '\n';
                ++id;
            }
        }
    }
};
