#include "route_state.h"
#include <algorithm>
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

    void RoutingStatsState::serialise(const char * output_directory, int job_id) {
        auto output_dir = fs::path(output_directory);
        if(!fs::is_directory(output_dir)) {
            throw std::runtime_error("output_dir must be a directory");
        }
        for(auto const& [commodity, edge_stats]: stats_) {
            auto output_file = std::ofstream(fs::path(output_dir) / (commodity + "_" + std::to_string(job_id) + ".csv"));
            output_file << "link_id,x,x2\n";
            auto id = 0;
            output_file << std::setprecision(2);
            for(auto const& moments: edge_stats) {
                if(moments.first > 0.0) {
                    output_file << id << ',' << moments.first << ',' << moments.second << '\n';
                }
                ++id;
            }
        }
    }
};
