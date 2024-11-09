#include <route_state.h>
#include <accessibility.h>
#include <algorithm>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

using Node = long;
using Edge = long;
using Weight = double;

static constexpr char const* COMMODITY = "widgets";

template<typename Generator>
auto build_graph(int n_nodes, Generator& gen) {
    std::vector<Node> nodes;
    std::vector<std::vector<Node>> edges;
    std::vector<std::vector<Weight>> weights(1);
    std::map<std::pair<Node, Node>, Edge> edge_map{};

    static constexpr double a = 1.0;
    static constexpr double b = 10.0;
    std::uniform_real_distribution<> dis(a, b);

    nodes.reserve(n_nodes);
    for(Node n=0; n<n_nodes; ++n) {
        nodes.push_back(n);
    }

    Edge edge_id = 0;
    for(Node n0=0; n0<n_nodes; ++n0) {
        for(Node n1=n0+1; n1<n_nodes; ++n1) {
            edges.emplace_back(std::vector<Node>{n0, n1});
            edge_map[{n0, n1}] = edge_id++;
            edges.emplace_back(std::vector<Node>{n1, n0});
            edge_map[{n1, n0}] = edge_id++;
            weights[0].push_back(dis(gen));
            weights[0].push_back(dis(gen));
        }
    }

    return std::make_tuple(nodes, edges, weights, edge_map);
}

template<typename Generator>
auto generate_random_ods(int n_routes, Node max_node_id, Generator& gen) {
    std::uniform_int_distribution<Node> dis(0, max_node_id);
    std::set<std::pair<Node, Node>> sampled;
    while(sampled.size() < n_routes) {
        Node dst, src = dis(gen);
        do {
            do {
                dst = dis(gen);
            } while (dst == src);
        } while(sampled.count(std::make_pair(src, dst)));
        sampled.emplace(src, dst);
    }

    std::vector<Node> sources, targets;
    sources.reserve(sampled.size());
    targets.reserve(sampled.size());

    for(auto& p: sampled) {
        sources.push_back(p.first);
        targets.push_back(p.second);
    }

    return std::make_tuple(sources, targets);
}


int main() {
    static constexpr int n_nodes = 10;
    static constexpr int n_links = n_nodes * (n_nodes - 1);
    static constexpr int n_routes = 20;
    static constexpr int n_simulations = 10;

    auto gen = std::mt19937(static_cast<std::random_device::result_type>(42));

    auto edge_ids = std::vector<int>(n_links);
    for (int e = 0; e < n_links; ++e) { edge_ids[e] = e; }

    auto tonnes = std::vector<double>(n_routes, 1.0);

    for (auto sim = 0; sim < n_simulations; ++sim) {
        auto [nodes, edges, weights, edge_map] = build_graph(n_nodes, gen);
        auto [sources, targets] = generate_random_ods(n_routes, n_nodes - 1, gen);

        auto a = MTC::accessibility::Accessibility(nodes.size(), edges, weights, false, edge_ids, edge_ids);

        // Note that the following also tests (some of) RoutesInternal.
        // It only works because the preprocessor macro FORCE_ORIGINAL_ROUTES_FUNCTION
        // is set for this target (see CMakeLists.txt). This causes the returned values
        // to be nodes, which we convert to edges by lookup in edge_map (which is
        // what would happen internally otherwise.
        auto routes_nodes = a.Routes(sources, targets, 0);
        auto routes = std::vector<std::vector<long>>{};
        routes.reserve(routes_nodes.size());
        for(auto const& nodesv: routes_nodes) {
            routes.emplace_back();
            auto& res = routes.back();
            res.reserve(nodesv.size() - 1);
            for(auto b0=nodesv.cbegin(), b1=nodesv.cbegin() + 1, e=nodesv.cend(); b1!=e; ++b0, ++b1) {
                res.push_back(edge_map[{*b0, *b1}]);
            }
        }

        auto routed_edge_ids = std::vector<double>(n_links);
        for(auto const& route: routes) {
            for(auto edge_id: route) {
                routed_edge_ids[edge_id] += 1.;
            }
        }

        auto routing_stats = MTC::accessibility::RoutingStatsState(n_links);
        a.RoutesStats(sources, targets, 0, tonnes, COMMODITY, &routing_stats);

        int edge_id = 0;
        assert(std::all_of(
                routing_stats[COMMODITY].cbegin(),
                routing_stats[COMMODITY].cend(),
                [&](auto tonnes) { return routed_edge_ids[edge_id++] == tonnes; }));
    }
}
