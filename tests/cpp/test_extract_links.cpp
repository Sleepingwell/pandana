#include <route_state.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

int main() {
    static auto test_data_file_name = fs::path{"/tmp/extract_links_test_data_ao08upliurlllhjleh.dat"};
    std::atexit([]() {
        if(exists(test_data_file_name)) {
            remove(test_data_file_name);
        }
    });
    auto test_data_file = fs::path{test_data_file_name};
    auto dat = std::vector<float>{1,1,1,2,2,2,3,3,3};
    auto exp = std::vector<float>{1,1,1,3,3,3};
    {
        auto ds = std::ofstream(test_data_file, std::ios::binary);
        ds.write(reinterpret_cast<char *>(dat.data()), dat.size() * sizeof(float));
    }
    dat = MTC::accessibility::do_extract_rows(test_data_file_name.c_str(), {0, 2}, 3);
    assert(dat == exp);
}
