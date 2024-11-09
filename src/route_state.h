#ifndef PANDANA_ROUTE_STATE_HEADER_INCLUDED_A0897G0AOIU
#define PANDANA_ROUTE_STATE_HEADER_INCLUDED_A0897G0AOIU

#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <utility> // pair

namespace MTC::accessibility {

class RoutingStatsState {
public:
    using StatsVector = std::vector<float>;
    explicit RoutingStatsState(int n_links) : n_links_(n_links) {}
    StatsVector& operator[](std::string const&);
    void serialise(const std::filesystem::path& output_dir, int);
    int n_links() const { return n_links_; }

private:
    std::map<std::string, StatsVector> stats_;
    int n_links_;
};

void do_transpose(const char* filename, int n_simulations, int n_links);
std::vector<float> do_extract_rows(const char* filename, std::vector<int> const& link_ids, int n_simulations);

} // end MTC::accessibility

#endif // PANDANA_ROUTE_STATE_HEADER_INCLUDED_A0897G0AOIU
