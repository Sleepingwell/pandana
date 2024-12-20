#include "route_state.h"
#include <algorithm>
#include <filesystem>
#include <fcntl.h>    // For open, O_CREAT, O_WRONLY
#include <unistd.h>   // For pwrite, close
#include <fstream>

namespace fs = std::filesystem;

namespace MTC::accessibility {
    void do_create_output_file(const fs::path& output_dir, const char* commodity, int n_links, int n_simulations) {
        using float_t = RoutingStatsState::StatsVector::value_type;
        static constexpr long sz_bytes = sizeof(float_t);

        if(!fs::is_directory(output_dir)) {
            throw std::runtime_error("output_dir_in \"" + output_dir.generic_string() + "\" must be a directory");
        }

        const auto output_file_name = output_dir / (std::string(commodity) + ".dat");
        if(fs::exists(output_file_name)) {
            throw std::runtime_error("output file \"" + output_file_name.generic_string() + "\" already exists");
        }

        // Open the file with read/write permissions and create it if it doesn't exist.
        const auto fd = open(output_file_name.c_str(), O_CREAT | O_WRONLY, 0666);

        std::vector<float_t> existing_data(n_links);
        void* const data = reinterpret_cast<void*>(existing_data.data());
        const auto data_size_bytes = sz_bytes * n_links;

        for(int sim=0; sim<n_simulations; ++sim) {
            const auto bytes_written = pwrite(fd, data, data_size_bytes, sim * data_size_bytes);
            if (bytes_written == -1) {
                close(fd);
                throw std::runtime_error("Error writing to file \"" + output_file_name.generic_string() + "\"");
            } else if (bytes_written != data_size_bytes) {
                close(fd);
                throw std::runtime_error("incorrect number of bytes written to file \"" + output_file_name.generic_string() + "\"");
            }
        }

        close(fd);
    }


    RoutingStatsState::StatsVector& RoutingStatsState::operator[](std::string const& commodity) {
        auto& res = stats_[commodity];
        res.resize(n_links());
        return res;
    }


    void RoutingStatsState::serialise(const fs::path& output_dir, int simulation_number) {
        using float_t = StatsVector::value_type;
        static constexpr long sz_bytes = sizeof(float_t);

        if(simulation_number < 0) {
            throw std::runtime_error("simulation_number must be non-negative");
        }
        if(!fs::is_directory(output_dir)) {
            throw std::runtime_error("output_dir \"" + output_dir.generic_string() + "\" must be a directory");
        }

        std::vector<float_t> existing_data(n_links());
        char* const data = reinterpret_cast<char*>(existing_data.data());

        for(auto const& [commodity, edge_stats]: stats_) {
            std::fill(existing_data.begin(), existing_data.end(), 0.0);
            const auto output_file_name = output_dir / (commodity + ".dat");

            if(!exists(output_file_name)) {
                throw std::runtime_error("output file \"" + output_file_name.generic_string() + "\" does not exist");
            }

            // Open the file with read/write permissions and create it if it doesn't exist.
            const auto fd = open(output_file_name.c_str(), O_CREAT | O_RDWR, 0666);

            if (fd == -1) {
                throw std::runtime_error("Error opening file \"" + output_file_name.generic_string() + "\"");
            }

            const auto data_size_bytes = sz_bytes * n_links();

            const auto bytes_read = pread(fd, data, data_size_bytes, simulation_number * data_size_bytes);
            if (bytes_read == -1) {
                close(fd);
                throw std::runtime_error("Error reading from file \"" + output_file_name.generic_string() + "\"");
            } else if (bytes_read != data_size_bytes) {
                close(fd);
                throw std::runtime_error("incorrect number of bytes read from file \"" + output_file_name.generic_string() + "\"");
            }
            std::transform(
                edge_stats.cbegin(),
                edge_stats.cend(),
                existing_data.cbegin(),
                existing_data.begin(),
                [](auto a, auto b) { return a + b; });

            const auto bytes_written = pwrite(fd, data, data_size_bytes, simulation_number * data_size_bytes);

            close(fd);

            if (bytes_written == -1) {
                throw std::runtime_error("Error writing to file \"" + output_file_name.generic_string() + "\"");
            } else if (bytes_written != data_size_bytes) {
                throw std::runtime_error("incorrect number of bytes written to file \"" + output_file_name.generic_string() + "\"");
            }
        }
    }


    void do_transpose(const char* filename, int n_simulations, int n_links) {
        using float_t = RoutingStatsState::StatsVector::value_type;
        static constexpr long sz = sizeof(float_t);

        auto idata = std::vector<float_t>(n_links);
        auto odata = std::vector<float_t>(n_simulations * n_links);
        auto input_data = std::ifstream(filename, std::ios::binary);

        if(!input_data) {
            throw std::runtime_error("failed to open input file \"" + std::string(filename) + "\"");
        }

        for(int simulation=0; simulation<n_simulations; ++simulation) {
            input_data.read(reinterpret_cast<char*>(idata.data()), n_links * sz);
            if(!input_data) {
                throw std::runtime_error("failed reading from input file \"" + std::string(filename) + "\"");
            }
            auto bi = idata.cbegin();
            for(int link=0; link<n_links; ++link, ++bi) {
                odata[link*n_simulations + simulation] = *bi;
            }
        }

        auto output_data = std::ofstream(filename, std::ios::binary);
        if(!output_data) {
            throw std::runtime_error("failed opening output file \"" + std::string(filename) + "\"");
        }

        output_data.write(reinterpret_cast<const char *>(odata.data()), n_simulations * n_links * sz);
        if(!output_data) {
            throw std::runtime_error("failed writing to output file \"" + std::string(filename) + "\"");
        }
    }


    std::vector<float> do_extract_rows(const char* filename, std::vector<int> const& link_ids, int n_simulations) {
        using float_t = RoutingStatsState::StatsVector::value_type;
        using float_vector = std::vector<float_t>;
        static constexpr long size_bytes = sizeof(float_t);

        auto result = float_vector(n_simulations * link_ids.size());
        auto input_data = std::ifstream(filename, std::ios::binary);

        if(!input_data) {
            throw std::runtime_error("failed to open input file \"" + std::string(filename) + "\"");
        }

        auto dp = result.data();
        for(auto link_id : link_ids) {
            input_data.seekg(size_bytes * link_id * n_simulations);
            input_data.read(reinterpret_cast<char*>(dp), size_bytes * n_simulations);
            if(!input_data) {
                throw std::runtime_error("failed reading from input file \"" + std::string(filename) + "\"");
            }
            dp += n_simulations;
        }

        return result;
    }
} // end namespace MTC::accessibility
