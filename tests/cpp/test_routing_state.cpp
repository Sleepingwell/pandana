#include <route_state.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

int main() {
    // We use a static name so we can easily identify the outputs when debugging.
    static auto test_data_dir_name = fs::path{"/tmp/routing_state_test_data_from_test_routing_state"};
    create_directory(test_data_dir_name);
    std::atexit([]() {
        if(exists(test_data_dir_name)) {
            remove_all(test_data_dir_name);
        }
    });

    fs::remove(test_data_dir_name / "apples.dat");
    MTC::accessibility::do_create_output_file(test_data_dir_name, "apples", 3, 3);

    fs::remove(test_data_dir_name / "oranges.dat");
    MTC::accessibility::do_create_output_file(test_data_dir_name, "oranges", 3, 3);

    auto routing_state = MTC::accessibility::RoutingStatsState(3);

    routing_state["apples"][0] = 1;
    routing_state["apples"][1] = 1;
    routing_state["apples"][2] = 1;
    routing_state["oranges"][0] = 1;
    routing_state["oranges"][1] = 2;
    routing_state["oranges"][2] = 3;

    routing_state.serialise(test_data_dir_name, 0);
    routing_state.serialise(test_data_dir_name, 2);

    assert(exists(test_data_dir_name / "apples.dat"));
    assert(exists(test_data_dir_name / "oranges.dat"));

    {
        auto dat = std::vector<float>(9);
        auto is = std::ifstream(test_data_dir_name / "apples.dat", std::ios::binary);
        is.read(reinterpret_cast<char *>(dat.data()), dat.size() * sizeof(float));
        auto exp = std::vector<float>{1,1,1,0,0,0,1,1,1};
        assert(dat == exp);
    }

    {
        auto dat = std::vector<float>(9);
        auto is = std::ifstream(test_data_dir_name / "oranges.dat", std::ios::binary);
        is.read(reinterpret_cast<char *>(dat.data()), dat.size() * sizeof(float));
        auto exp = std::vector<float>{1,2,3,0,0,0,1,2,3};
        assert(dat == exp);
    }
}
