#include "route_state.h"
#include <algorithm>
#include <filesystem>
#include <fcntl.h>    // For open, O_CREAT, O_WRONLY
#include <unistd.h>   // For pwrite, close
#include <fstream>

namespace fs = std::filesystem;

namespace MTC::accessibility {
    RoutingStatsState::StatsVector& RoutingStatsState::operator[](std::string const& commodity) {
        auto& res = stats_[commodity];
        res.resize(n_links());
        return res;
    }


    void RoutingStatsState::serialise(const fs::path& output_dir, int job_id) {
        using float_t = StatsVector::value_type;
        static constexpr long sz = sizeof(float_t);

        if(job_id < 0) {
            throw std::runtime_error("job_id must be non-negative");
        }
        if(!fs::is_directory(output_dir)) {
            throw std::runtime_error("output_dir \"" + output_dir.generic_string() + "\" must be a directory");
        }

        for(auto const& [commodity, edge_stats]: stats_) {
            const auto output_file_name = output_dir / (commodity + ".dat");

            // Open the file with read/write permissions and create it if it doesn't exist.
            int fd = open(output_file_name.c_str(), O_CREAT | O_WRONLY, 0666);

            if (fd == -1) {
                throw std::runtime_error("Error opening file \"" + output_file_name.generic_string() + "\"");
            }

            const auto data_size = n_links() * sz;
            const char *data = reinterpret_cast<const char*>(edge_stats.data());
            const auto bytes_written = pwrite(fd, data, data_size, job_id * data_size);
            close(fd);

            if (bytes_written == -1) {
                throw std::runtime_error("Error writing to file \"" + output_file_name.generic_string() + "\"");
            } else if (bytes_written != data_size) {
                throw std::runtime_error("incorrect number of bytes written to file \"" + output_file_name.generic_string() + "\"");
            }
        }
    }


    void do_transpose(const char* filename, int n_simulations, int n_links) {
        using float_t = RoutingStatsState::StatsVector::value_type;
        static constexpr long sz = sizeof(float_t);

        const size_t ns = n_simulations;
        const size_t nl = n_links;
        auto idata = std::vector<float_t>(nl);
        auto odata = std::vector<float_t>(ns * nl);
        auto input_data = std::ifstream(filename, std::ios::binary);

        if(!input_data) {
            throw std::runtime_error("failed to open input file \"" + std::string(filename) + "\"");
        }

        for(size_t s=0; s<ns; ++s) {
            input_data.read(reinterpret_cast<char*>(idata.data()), nl * sz);
            if(!input_data) {
                throw std::runtime_error("failed reading from input file \"" + std::string(filename) + "\"");
            }
            auto bi = idata.cbegin();
            for(size_t l=0; l<nl; ++l, ++bi) {
                odata[l*ns + s] = *bi;
            }
        }

        auto output_data = std::ofstream(filename, std::ios::binary);
        if(!output_data) {
            throw std::runtime_error("failed opening output file \"" + std::string(filename) + "\"");
        }

        output_data.write(reinterpret_cast<const char *>(odata.data()), ns * nl * sz);
        if(!output_data) {
            throw std::runtime_error("failed writing to output file \"" + std::string(filename) + "\"");
        }
    }


    std::vector<float> do_extract_rows(const char* filename, std::vector<int> const& link_ids, int n_simulations) {
        using float_t = RoutingStatsState::StatsVector::value_type;
        using dv = std::vector<float_t>;
        static constexpr long sz = sizeof(float_t);

        auto result = dv(n_simulations * link_ids.size());
        auto input_data = std::ifstream(filename, std::ios::binary);

        if(!input_data) {
            throw std::runtime_error("failed to open input file \"" + std::string(filename) + "\"");
        }

        auto dp = result.data();
        for(auto link_id : link_ids) {
            input_data.seekg(link_id * n_simulations * sz);
            input_data.read(reinterpret_cast<char*>(dp), n_simulations * sz);
            if(!input_data) {
                throw std::runtime_error("failed reading from input file \"" + std::string(filename) + "\"");
            }
            dp += n_simulations;
        }

        return result;
    }
} // end namespace MTC::accessibility
