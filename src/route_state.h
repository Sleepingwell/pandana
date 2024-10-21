#ifndef PANDANA_ROUTE_STATE_HEADER_INCLUDED_A0897G0AOIU
#define PANDANA_ROUTE_STATE_HEADER_INCLUDED_A0897G0AOIU

#include <map>
#include <string>
#include <vector>
#include <utility> // pair

namespace MTC::accessibility {

class RoutingStatsState {
public:
    using StatsVector = std::vector<std::pair<double, double>>;
    explicit RoutingStatsState(int);
    StatsVector& operator[](std::string const&);
    void serialise(const char*, int);
    int max_link_id() const { return max_link_id_; }

private:
    std::map<std::string, StatsVector> stats_;
    int max_link_id_;
};

} // end MTC::accessibility

#endif // PANDANA_ROUTE_STATE_HEADER_INCLUDED_A0897G0AOIU
