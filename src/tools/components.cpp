#include "extractor/tarjan_scc.hpp"
#include "util/coordinate.hpp"
#include "util/coordinate_calculation.hpp"
#include "util/dynamic_graph.hpp"
#include "util/fingerprint.hpp"
#include "util/graph_loader.hpp"
#include "util/log.hpp"
#include "util/static_graph.hpp"
#include "util/typedefs.hpp"

#include <boost/filesystem.hpp>
#include <boost/function_output_iterator.hpp>

#include <tbb/parallel_sort.h>

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace osrm
{
namespace tools
{

struct TarjanEdgeData
{
    TarjanEdgeData() : distance(INVALID_EDGE_WEIGHT), name_id(INVALID_NAMEID) {}

    TarjanEdgeData(std::uint32_t distance, std::uint32_t name_id)
        : distance(distance), name_id(name_id)
    {
    }

    std::uint32_t distance;
    std::uint32_t name_id;
};

using TarjanGraph = util::StaticGraph<TarjanEdgeData>;
using TarjanEdge = TarjanGraph::InputEdge;

std::size_t loadGraph(const std::string &path,
                      std::vector<extractor::QueryNode> &coordinate_list,
                      std::vector<TarjanEdge> &graph_edge_list)
{
    storage::io::FileReader file_reader(path, storage::io::FileReader::VerifyFingerprint);

    std::vector<extractor::NodeBasedEdge> edge_list;

    auto nop = boost::make_function_output_iterator([](auto) {});

    const auto number_of_nodes = util::loadNodesFromFile(file_reader, nop, nop, coordinate_list);

    util::loadEdgesFromFile(file_reader, edge_list);

    // Building a node-based graph
    for (const auto &input_edge : edge_list)
    {
        if (input_edge.source == input_edge.target)
        {
            continue;
        }

        if (input_edge.forward)
        {
            graph_edge_list.emplace_back(input_edge.source,
                                         input_edge.target,
                                         (std::max)(input_edge.weight, 1),
                                         input_edge.name_id);
        }

        if (input_edge.backward)
        {
            graph_edge_list.emplace_back(input_edge.target,
                                         input_edge.source,
                                         (std::max)(input_edge.weight, 1),
                                         input_edge.name_id);
        }
    }

    return number_of_nodes;
}

struct FeatureWriter
{
    FeatureWriter(std::ostream &out_) : out(out_)
    {
        out << "{\"type\":\"FeatureCollection\",\"features\":[";
    }

    void
    AddLine(const extractor::QueryNode from, const extractor::QueryNode to, const std::string &type)
    {
        const auto from_lon = static_cast<double>(util::toFloating(from.lon));
        const auto from_lat = static_cast<double>(util::toFloating(from.lat));
        const auto to_lon = static_cast<double>(util::toFloating(to.lon));
        const auto to_lat = static_cast<double>(util::toFloating(to.lat));

        static bool first = true;

        if (!first)
        {
            out << ",";
        }

        out << "{\"type\":\"Feature\",\"properties\":{\"from\":" << from.node_id << ","
            << "\"to\":" << to.node_id << ",\"type\":\"" << type
            << "\"},\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[" << from_lon << ","
            << from_lat << "],[" << to_lon << "," << to_lat << "]]}}";

        first = false;
    }

    ~FeatureWriter() { out << "]}" << std::flush; }

    std::ostream &out;
};

//
}
}

int main(int argc, char *argv[])
{
    using namespace osrm;

    std::vector<extractor::QueryNode> coordinate_list;
    util::LogPolicy::GetInstance().Unmute();

    if (argc < 3)
    {
        util::Log(logWARNING) << "Usage: " << argv[0] << " map.osrm components.geojson";
        return EXIT_FAILURE;
    }

    const std::string inpath{argv[1]};
    const std::string outpath{argv[2]};

    if (boost::filesystem::exists(outpath))
    {
        util::Log(logWARNING) << "Components file " << outpath << " already exists";
        return EXIT_FAILURE;
    }

    std::ofstream outfile{outpath};

    if (!outfile)
    {
        util::Log(logWARNING) << "Unable to open components file " << outpath << " for writing";
        return EXIT_FAILURE;
    }

    std::vector<tools::TarjanEdge> graph_edge_list;
    auto number_of_nodes = tools::loadGraph(inpath, coordinate_list, graph_edge_list);

    tbb::parallel_sort(graph_edge_list.begin(), graph_edge_list.end());

    const auto graph = std::make_shared<osrm::tools::TarjanGraph>(number_of_nodes, graph_edge_list);
    graph_edge_list.clear();
    graph_edge_list.shrink_to_fit();

    util::Log() << "Starting SCC graph traversal";

    extractor::TarjanSCC<tools::TarjanGraph> tarjan{graph};
    tarjan.Run();

    util::Log() << "Identified: " << tarjan.GetNumberOfComponents() << " components";
    util::Log() << "Identified " << tarjan.GetSizeOneCount() << " size one components";

    std::uint64_t total_network_length = 0;

    tools::FeatureWriter writer{outfile};

    for (const NodeID source : osrm::util::irange(0u, graph->GetNumberOfNodes()))
    {
        for (const auto current_edge : graph->GetAdjacentEdgeRange(source))
        {
            const auto target = graph->GetTarget(current_edge);

            if (source < target || SPECIAL_EDGEID == graph->FindEdge(target, source))
            {
                BOOST_ASSERT(current_edge != SPECIAL_EDGEID);
                BOOST_ASSERT(source != SPECIAL_NODEID);
                BOOST_ASSERT(target != SPECIAL_NODEID);

                total_network_length += 100 * util::coordinate_calculation::greatCircleDistance(
                                                  coordinate_list[source], coordinate_list[target]);

                auto source_component_id = tarjan.GetComponentID(source);
                auto target_component_id = tarjan.GetComponentID(target);

                auto source_component_size = tarjan.GetComponentSize(source_component_id);
                auto target_component_size = tarjan.GetComponentSize(target_component_id);

                const auto smallest = std::min(source_component_size, target_component_size);

                if (smallest < 1000)
                {
                    auto same_component = source_component_id == target_component_id;
                    std::string type = same_component ? "inner" : "border";

                    writer.AddLine(coordinate_list[source], coordinate_list[target], type);
                }
            }
        }
    }

    util::Log() << "Total network distance: " << (total_network_length / 100 / 1000) << " km";
}
