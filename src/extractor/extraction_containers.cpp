#include "extractor/extraction_containers.hpp"
#include "extractor/extraction_way.hpp"

#include "util/coordinate_calculation.hpp"
#include "util/range_table.hpp"

#include "util/exception.hpp"
#include "util/exception_utils.hpp"
#include "util/fingerprint.hpp"
#include "util/io.hpp"
#include "util/log.hpp"
#include "util/timing_util.hpp"

#include <boost/assert.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/ref.hpp>

#include <stxxl/sort>

#include <chrono>
#include <limits>
#include <mutex>
#include <sstream>

#include <tbb/tbb.h>

namespace
{
namespace oe = osrm::extractor;

// Needed for STXXL comparison - STXXL requires max_value(), min_value(), so we can not use
// std::less<OSMNodeId>{}. Anonymous namespace to keep translation unit local.
struct OSMNodeIDSTXXLLess
{
    using value_type = OSMNodeID;
    bool operator()(const value_type left, const value_type right) const { return left < right; }
    value_type max_value() { return MAX_OSM_NODEID; }
    value_type min_value() { return MIN_OSM_NODEID; }
};

struct CmpEdgeByOSMStartID
{
    using value_type = oe::InternalExtractorEdge;
    bool operator()(const value_type &lhs, const value_type &rhs) const
    {
        return lhs.result.osm_source_id < rhs.result.osm_source_id;
    }

    value_type max_value() { return value_type::max_osm_value(); }
    value_type min_value() { return value_type::min_osm_value(); }
};

struct CmpEdgeByOSMTargetID
{
    using value_type = oe::InternalExtractorEdge;
    bool operator()(const value_type &lhs, const value_type &rhs) const
    {
        return lhs.result.osm_target_id < rhs.result.osm_target_id;
    }

    value_type max_value() { return value_type::max_osm_value(); }
    value_type min_value() { return value_type::min_osm_value(); }
};

struct CmpEdgeByInternalSourceTargetAndName
{
    using value_type = oe::InternalExtractorEdge;
    bool operator()(const value_type &lhs, const value_type &rhs) const
    {
        if (lhs.result.source != rhs.result.source)
            return lhs.result.source < rhs.result.source;

        if (lhs.result.source == SPECIAL_NODEID)
            return false;

        if (lhs.result.target != rhs.result.target)
            return lhs.result.target < rhs.result.target;

        if (lhs.result.target == SPECIAL_NODEID)
            return false;

        if (lhs.result.name_id == rhs.result.name_id)
            return false;

        if (lhs.result.name_id == EMPTY_NAMEID)
            return false;

        if (rhs.result.name_id == EMPTY_NAMEID)
            return true;

        std::lock_guard<std::mutex> lock(mutex);
        BOOST_ASSERT(!name_offsets.empty() && name_offsets.back() == name_data.size());
        const oe::ExtractionContainers::STXXLNameCharData::const_iterator data = name_data.begin();
        return std::lexicographical_compare(data + name_offsets[lhs.result.name_id],
                                            data + name_offsets[lhs.result.name_id + 1],
                                            data + name_offsets[rhs.result.name_id],
                                            data + name_offsets[rhs.result.name_id + 1]);
    }

    value_type max_value() { return value_type::max_internal_value(); }
    value_type min_value() { return value_type::min_internal_value(); }

    std::mutex &mutex;
    const oe::ExtractionContainers::STXXLNameCharData &name_data;
    const oe::ExtractionContainers::STXXLNameOffsets &name_offsets;
};
}

namespace osrm
{
namespace extractor
{

static const int WRITE_BLOCK_BUFFER_SIZE = 8000;

ExtractionContainers::ExtractionContainers()
{
    // Check if stxxl can be instantiated
    stxxl::vector<unsigned> dummy_vector;

    // Insert four empty strings offsets for name, ref, destination and pronunciation
    name_offsets.push_back(0);
    name_offsets.push_back(0);
    name_offsets.push_back(0);
    name_offsets.push_back(0);
    // Insert the total length sentinel (corresponds to the next name string offset)
    name_offsets.push_back(0);
}

void ExtractionContainers::FlushVectors()
{
    used_node_id_list.flush();
    all_nodes_list.flush();
    all_edges_list.flush();
    name_char_data.flush();
    name_offsets.flush();
    restrictions_list.flush();
    way_start_end_id_list.flush();
}

/**
 * Processes the collected data and serializes it.
 * At this point nodes are still referenced by their OSM id.
 *
 * - map start-end nodes of ways to ways used int restrictions to compute compressed
 *   trippe representation
 * - filter nodes list to nodes that are referenced by ways
 * - merge edges with nodes to include location of start/end points and serialize
 *
 */
void ExtractionContainers::PrepareData(ScriptingEnvironment &scripting_environment,
                                       const std::string &output_file_name,
                                       const std::string &restrictions_file_name,
                                       const std::string &name_file_name,
                                       BoundaryList &boundaryList)
{
    std::ofstream file_out_stream;
    file_out_stream.open(output_file_name.c_str(), std::ios::binary);
    const util::FingerPrint fingerprint = util::FingerPrint::GetValid();
    file_out_stream.write((char *)&fingerprint, sizeof(util::FingerPrint));

    FlushVectors();

    PrepareNodes();
    WriteNodes(file_out_stream);
    PrepareEdges(scripting_environment, boundaryList);
    WriteEdges(file_out_stream);

    PrepareRestrictions();
    WriteRestrictions(restrictions_file_name);
    WriteCharData(name_file_name);
}

void ExtractionContainers::WriteCharData(const std::string &file_name)
{
    util::UnbufferedLog log;
    log << "writing street name index ... ";
    TIMER_START(write_index);
    boost::filesystem::ofstream file_stream(file_name, std::ios::binary);

    // transforms in-place name offsets to name lengths
    BOOST_ASSERT(!name_offsets.empty());
    for (auto curr = name_offsets.begin(), next = name_offsets.begin() + 1;
         next != name_offsets.end();
         ++curr, ++next)
    {
        *curr = *next - *curr;
    }

    // removes the total length sentinel
    unsigned total_length = name_offsets.back();
    name_offsets.pop_back();

    // builds and writes the index
    util::RangeTable<> index_range(name_offsets);
    file_stream << index_range;

    file_stream.write((char *)&total_length, sizeof(unsigned));

    // write all chars consecutively
    char write_buffer[WRITE_BLOCK_BUFFER_SIZE];
    unsigned buffer_len = 0;

    for (const auto c : name_char_data)
    {
        write_buffer[buffer_len++] = c;

        if (buffer_len >= WRITE_BLOCK_BUFFER_SIZE)
        {
            file_stream.write(write_buffer, WRITE_BLOCK_BUFFER_SIZE);
            buffer_len = 0;
        }
    }

    file_stream.write(write_buffer, buffer_len);

    TIMER_STOP(write_index);
    log << "ok, after " << TIMER_SEC(write_index) << "s";
}

void ExtractionContainers::PrepareNodes()
{
    {
        util::UnbufferedLog log;
        log << "Sorting used nodes        ... " << std::flush;
        TIMER_START(sorting_used_nodes);
        stxxl::sort(
            used_node_id_list.begin(), used_node_id_list.end(), OSMNodeIDSTXXLLess(), stxxl_memory);
        TIMER_STOP(sorting_used_nodes);
        log << "ok, after " << TIMER_SEC(sorting_used_nodes) << "s";
    }

    {
        util::UnbufferedLog log;
        log << "Erasing duplicate nodes   ... " << std::flush;
        TIMER_START(erasing_dups);
        auto new_end = std::unique(used_node_id_list.begin(), used_node_id_list.end());
        used_node_id_list.resize(new_end - used_node_id_list.begin());
        TIMER_STOP(erasing_dups);
        log << "ok, after " << TIMER_SEC(erasing_dups) << "s";
    }

    {
        util::UnbufferedLog log;
        log << "Sorting all nodes         ... " << std::flush;
        TIMER_START(sorting_nodes);
        stxxl::sort(all_nodes_list.begin(),
                    all_nodes_list.end(),
                    ExternalMemoryNodeSTXXLCompare(),
                    stxxl_memory);
        TIMER_STOP(sorting_nodes);
        log << "ok, after " << TIMER_SEC(sorting_nodes) << "s";
    }

    {
        util::UnbufferedLog log;
        log << "Building node id map      ... " << std::flush;
        TIMER_START(id_map);
        external_to_internal_node_id_map.reserve(used_node_id_list.size());
        auto node_iter = all_nodes_list.begin();
        auto ref_iter = used_node_id_list.begin();
        const auto all_nodes_list_end = all_nodes_list.end();
        const auto used_node_id_list_end = used_node_id_list.end();
        // Note: despite being able to handle 64 bit OSM node ids, we can't
        // handle > uint32_t actual usable nodes.  This should be OK for a while
        // because we usually route on a *lot* less than 2^32 of the OSM
        // graph nodes.
        std::uint64_t internal_id = 0;

        // compute the intersection of nodes that were referenced and nodes we actually have
        while (node_iter != all_nodes_list_end && ref_iter != used_node_id_list_end)
        {
            if (node_iter->node_id < *ref_iter)
            {
                node_iter++;
                continue;
            }
            if (node_iter->node_id > *ref_iter)
            {
                ref_iter++;
                continue;
            }
            BOOST_ASSERT(node_iter->node_id == *ref_iter);
            external_to_internal_node_id_map[*ref_iter] = static_cast<NodeID>(internal_id++);
            node_iter++;
            ref_iter++;
        }
        if (internal_id > std::numeric_limits<NodeID>::max())
        {
            throw util::exception("There are too many nodes remaining after filtering, OSRM only "
                                  "supports 2^32 unique nodes, but there were " +
                                  std::to_string(internal_id) + SOURCE_REF);
        }
        max_internal_node_id = boost::numeric_cast<NodeID>(internal_id);
        TIMER_STOP(id_map);
        log << "ok, after " << TIMER_SEC(id_map) << "s";
    }
}


static tbb::atomic<size_t> country = 0, city = 0, count = 0;
size_t edgesQueued = 0;
    
#define EDGE_MULTI_THREAD
#ifdef EDGE_MULTI_THREAD
    
class ProcessEdgeClassData
{
public:
    ProcessEdgeClassData( BoundaryList &bl, util::Coordinate &c, bool &itr ) : boundaryList(bl), coord(c), inTownResult(itr) {}
    
    BoundaryList &boundaryList;
    util::Coordinate coord;
    bool &inTownResult;
};
    
    
typedef std::shared_ptr<ProcessEdgeClassData> ProcessEdgeClassDataPtr;
    
    
class ProcessEdgeClassDataClass
{
public:
    ProcessEdgeClassDataClass() { }
    int operator()( ProcessEdgeClassDataPtr item )
    {
        bool result = item->boundaryList.FixedPointCoordinateIsInTown(item->coord);
        
        if ( result )
        {
            item->inTownResult = true;
            city++;
        }
        else
        {
            country++;
        }
        
        count++;
        
        if ( count % 100000 == 0 )
        {
            size_t countryTemp = country;
            size_t cityTemp = city;
            size_t countTemp = count;
            size_t percent = (countTemp * 100) / edgesQueued;
            std::cout << "Country: " << countryTemp << ", city: " << cityTemp <<
            ", " << countTemp << "/" << edgesQueued << " = " << percent << "%\n";
        }

        return 0;
    }
};
    
#endif

using namespace std;
using namespace tbb;
using namespace tbb::flow;

void ExtractionContainers::PrepareEdges(ScriptingEnvironment &scripting_environment, BoundaryList &boundaryList)
{
    // Sort edges by start.
    {
        util::UnbufferedLog log;
        log << "Sorting edges by start    ... " << std::flush;
        TIMER_START(sort_edges_by_start);
        stxxl::sort(
            all_edges_list.begin(), all_edges_list.end(), CmpEdgeByOSMStartID(), stxxl_memory);
        TIMER_STOP(sort_edges_by_start);
        log << "ok, after " << TIMER_SEC(sort_edges_by_start) << "s";
    }

    {
        util::UnbufferedLog log;
        log << "Setting start coords      ... " << std::flush;
        TIMER_START(set_start_coords);
        // Traverse list of edges and nodes in parallel and set start coord
        auto node_iterator = all_nodes_list.begin();
        auto edge_iterator = all_edges_list.begin();

        const auto all_edges_list_end = all_edges_list.end();
        const auto all_nodes_list_end = all_nodes_list.end();

        while (edge_iterator != all_edges_list_end && node_iterator != all_nodes_list_end)
        {
            if (edge_iterator->result.osm_source_id < node_iterator->node_id)
            {
                util::Log(logDEBUG) << "Found invalid node reference "
                                    << edge_iterator->result.source;
                edge_iterator->result.source = SPECIAL_NODEID;
                ++edge_iterator;
                continue;
            }
            if (edge_iterator->result.osm_source_id > node_iterator->node_id)
            {
                node_iterator++;
                continue;
            }

            // remove loops
            if (edge_iterator->result.osm_source_id == edge_iterator->result.osm_target_id)
            {
                edge_iterator->result.source = SPECIAL_NODEID;
                edge_iterator->result.target = SPECIAL_NODEID;
                ++edge_iterator;
                continue;
            }

            BOOST_ASSERT(edge_iterator->result.osm_source_id == node_iterator->node_id);

            // assign new node id
            auto id_iter = external_to_internal_node_id_map.find(node_iterator->node_id);
            BOOST_ASSERT(id_iter != external_to_internal_node_id_map.end());
            edge_iterator->result.source = id_iter->second;

            edge_iterator->source_coordinate.lat = node_iterator->lat;
            edge_iterator->source_coordinate.lon = node_iterator->lon;
            ++edge_iterator;
        }

        // Remove all remaining edges. They are invalid because there are no corresponding nodes for
        // them. This happens when using osmosis with bbox or polygon to extract smaller areas.
        auto markSourcesInvalid = [](InternalExtractorEdge &edge) {
            util::Log(logDEBUG) << "Found invalid node reference " << edge.result.source;
            edge.result.source = SPECIAL_NODEID;
            edge.result.osm_source_id = SPECIAL_OSM_NODEID;
        };
        std::for_each(edge_iterator, all_edges_list_end, markSourcesInvalid);
        TIMER_STOP(set_start_coords);
        log << "ok, after " << TIMER_SEC(set_start_coords) << "s";
    }

    {
        // Sort Edges by target
        util::UnbufferedLog log;
        log << "Sorting edges by target   ... " << std::flush;
        TIMER_START(sort_edges_by_target);
        stxxl::sort(
            all_edges_list.begin(), all_edges_list.end(), CmpEdgeByOSMTargetID(), stxxl_memory);
        TIMER_STOP(sort_edges_by_target);
        log << "ok, after " << TIMER_SEC(sort_edges_by_target) << "s";
    }

    {
        // Compute edge weights
        util::UnbufferedLog log;
        auto node_iterator = all_nodes_list.begin();
        auto edge_iterator = all_edges_list.begin();
        const auto all_edges_list_begin_ = all_edges_list.begin();
        const auto all_edges_list_end_ = all_edges_list.end();
        const auto all_nodes_list_end_ = all_nodes_list.end();
        
        log << "Computing edge weights for " << all_edges_list.size() << " edges  ... " << std::flush;
        TIMER_START(compute_weights);
        
#ifdef EDGE_MULTI_THREAD
        graph g;
        broadcast_node<ProcessEdgeClassDataPtr> input(g);
        function_node<ProcessEdgeClassDataPtr, int> processor( g, unlimited, ProcessEdgeClassDataClass());
        make_edge( input, processor );
#endif
        
        concurrent_vector<bool> edgeStartsInTown;
        edgeStartsInTown.resize(all_edges_list.size(), false);

        while (edge_iterator != all_edges_list_end_ && node_iterator != all_nodes_list_end_)
        {
            // skip all invalid edges
            if (edge_iterator->result.source == SPECIAL_NODEID)
            {
                ++edge_iterator;
                continue;
            }

            if (edge_iterator->result.osm_target_id < node_iterator->node_id)
            {
                util::Log(logDEBUG) << "Found invalid node reference "
                                    << static_cast<uint64_t>(edge_iterator->result.osm_target_id);
                edge_iterator->result.target = SPECIAL_NODEID;
                ++edge_iterator;
                continue;
            }
            if (edge_iterator->result.osm_target_id > node_iterator->node_id)
            {
                ++node_iterator;
                continue;
            }

            BOOST_ASSERT(edge_iterator->result.osm_target_id == node_iterator->node_id);
            BOOST_ASSERT(edge_iterator->weight_data.city_speed >= 0);
            BOOST_ASSERT(edge_iterator->weight_data.country_speed >= 0);
            BOOST_ASSERT(edge_iterator->source_coordinate.lat !=
                         util::FixedLatitude{std::numeric_limits<std::int32_t>::min()});
            BOOST_ASSERT(edge_iterator->source_coordinate.lon !=
                         util::FixedLongitude{std::numeric_limits<std::int32_t>::min()});
            
            auto edgeIndex = edge_iterator - all_edges_list_begin_;
            
#ifdef EDGE_MULTI_THREAD
            ProcessEdgeClassDataPtr dataPtr = std::make_shared<ProcessEdgeClassData>(boundaryList,
                                                                                     edge_iterator->source_coordinate,
                                                                                     edgeStartsInTown[edgeIndex]);
            input.try_put( dataPtr );
            edgesQueued++;
#else
            ProcessEdgeItem(distance, boundaryList,
                            external_to_internal_node_id_map,
                            *edge_iterator, *node_iterator );
#endif

            ++edge_iterator;
        }
        
#ifdef EDGE_MULTI_THREAD
        std::cout << "Finished queueing " << edgesQueued << " edges\n";
        g.wait_for_all();
#endif
        
        node_iterator = all_nodes_list.begin();
        edge_iterator = all_edges_list.begin();
        
        //
        // Second pass to swap the nodes
        //
        while (edge_iterator != all_edges_list_end_ && node_iterator != all_nodes_list_end_)
        {
            auto edgeIndex = edge_iterator - all_edges_list_begin_;

            // skip all invalid edges
            if (edge_iterator->result.source == SPECIAL_NODEID)
            {
                BOOST_ASSERT(!edgeStartsInTown[edgeIndex]);
                ++edge_iterator;
                continue;
            }
            
            if (edge_iterator->result.osm_target_id < node_iterator->node_id)
            {
                BOOST_ASSERT(!edgeStartsInTown[edgeIndex]);
                util::Log(logDEBUG) << "Found invalid node reference "
                << static_cast<uint64_t>(edge_iterator->result.osm_target_id);
                edge_iterator->result.target = SPECIAL_NODEID;
                ++edge_iterator;
                continue;
            }
            if (edge_iterator->result.osm_target_id > node_iterator->node_id)
            {
                ++node_iterator;
                continue;
            }
            
            BOOST_ASSERT(edge_iterator->result.osm_target_id == node_iterator->node_id);
            BOOST_ASSERT(edge_iterator->weight_data.city_speed >= 0);
            BOOST_ASSERT(edge_iterator->weight_data.country_speed >= 0);
            BOOST_ASSERT(edge_iterator->source_coordinate.lat !=
                         util::FixedLatitude{std::numeric_limits<std::int32_t>::min()});
            BOOST_ASSERT(edge_iterator->source_coordinate.lon !=
                         util::FixedLongitude{std::numeric_limits<std::int32_t>::min()});

            const double distance = util::coordinate_calculation::greatCircleDistance(
                edge_iterator->source_coordinate,
                util::Coordinate(node_iterator->lon, node_iterator->lat));

            scripting_environment.ProcessSegment(edge_iterator->source_coordinate,
                                                 *node_iterator,
                                                 distance,
                                                 edge_iterator->weight_data);
            
            bool inTown = edgeStartsInTown[edgeIndex];

            const double weight = [distance, edge_iterator, node_iterator, inTown](
                const InternalExtractorEdge::WeightData &data) {
                switch (data.type)
                {
                case InternalExtractorEdge::WeightType::EDGE_DURATION:
                case InternalExtractorEdge::WeightType::WAY_DURATION:
                    return data.duration * 10.;
                    break;
                case InternalExtractorEdge::WeightType::SPEED:
                    if ( inTown )
                    {
                        return (distance * 10.) / (data.city_speed / 3.6);
                    }
                    else
                    {
                        return (distance * 10.) / (data.country_speed / 3.6);
                    }
                    break;
                case InternalExtractorEdge::WeightType::INVALID:
                    std::stringstream coordstring;
                    coordstring << edge_iterator->source_coordinate << " to " << node_iterator->lon
                                << "," << node_iterator->lat;
                    util::exception("Encountered invalid weight at segment " + coordstring.str() +
                                    SOURCE_REF);
                }
                return -1.0;
            }(edge_iterator->weight_data);

            auto &edge = edge_iterator->result;
            edge.weight = std::max(1, static_cast<int>(std::floor(weight + .5)));

            // assign new node id
            auto id_iter = external_to_internal_node_id_map.find(node_iterator->node_id);
            BOOST_ASSERT(id_iter != external_to_internal_node_id_map.end());
            edge.target = id_iter->second;

            // orient edges consistently: source id < target id
            // important for multi-edge removal
            if (edge.source > edge.target)
            {
                std::swap(edge.source, edge.target);

                // std::swap does not work with bit-fields
                bool temp = edge.forward;
                edge.forward = edge.backward;
                edge.backward = temp;
            }
            ++edge_iterator;
        }

        // Remove all remaining edges. They are invalid because there are no corresponding nodes for
        // them. This happens when using osmosis with bbox or polygon to extract smaller areas.
        auto markTargetsInvalid = [](InternalExtractorEdge &edge) {
            util::Log(logDEBUG) << "Found invalid node reference " << edge.result.target;
            edge.result.target = SPECIAL_NODEID;
        };
        std::for_each(edge_iterator, all_edges_list_end_, markTargetsInvalid);
        TIMER_STOP(compute_weights);
        log << "ok, after " << TIMER_SEC(compute_weights) << "s";
    }

    // Sort edges by start.
    {
        util::UnbufferedLog log;
        log << "Sorting edges by renumbered start ... ";
        TIMER_START(sort_edges_by_renumbered_start);
        std::mutex name_data_mutex;
        stxxl::sort(
            all_edges_list.begin(),
            all_edges_list.end(),
            CmpEdgeByInternalSourceTargetAndName{name_data_mutex, name_char_data, name_offsets},
            stxxl_memory);
        TIMER_STOP(sort_edges_by_renumbered_start);
        log << "ok, after " << TIMER_SEC(sort_edges_by_renumbered_start) << "s";
    }

    BOOST_ASSERT(all_edges_list.size() > 0);
    for (unsigned i = 0; i < all_edges_list.size();)
    {
        // only invalid edges left
        if (all_edges_list[i].result.source == SPECIAL_NODEID)
        {
            break;
        }
        // skip invalid edges
        if (all_edges_list[i].result.target == SPECIAL_NODEID)
        {
            ++i;
            continue;
        }

        unsigned start_idx = i;
        NodeID source = all_edges_list[i].result.source;
        NodeID target = all_edges_list[i].result.target;

        int min_forward_weight = std::numeric_limits<int>::max();
        int min_backward_weight = std::numeric_limits<int>::max();
        unsigned min_forward_idx = std::numeric_limits<unsigned>::max();
        unsigned min_backward_idx = std::numeric_limits<unsigned>::max();

        // find minimal edge in both directions
        while (all_edges_list[i].result.source == source &&
               all_edges_list[i].result.target == target)
        {
            if (all_edges_list[i].result.forward &&
                all_edges_list[i].result.weight < min_forward_weight)
            {
                min_forward_idx = i;
                min_forward_weight = all_edges_list[i].result.weight;
            }
            if (all_edges_list[i].result.backward &&
                all_edges_list[i].result.weight < min_backward_weight)
            {
                min_backward_idx = i;
                min_backward_weight = all_edges_list[i].result.weight;
            }

            // this also increments the outer loop counter!
            i++;
        }

        BOOST_ASSERT(min_forward_idx == std::numeric_limits<unsigned>::max() ||
                     min_forward_idx < i);
        BOOST_ASSERT(min_backward_idx == std::numeric_limits<unsigned>::max() ||
                     min_backward_idx < i);
        BOOST_ASSERT(min_backward_idx != std::numeric_limits<unsigned>::max() ||
                     min_forward_idx != std::numeric_limits<unsigned>::max());

        if (min_backward_idx == min_forward_idx)
        {
            all_edges_list[min_forward_idx].result.is_split = false;
            all_edges_list[min_forward_idx].result.forward = true;
            all_edges_list[min_forward_idx].result.backward = true;
        }
        else
        {
            bool has_forward = min_forward_idx != std::numeric_limits<unsigned>::max();
            bool has_backward = min_backward_idx != std::numeric_limits<unsigned>::max();
            if (has_forward)
            {
                all_edges_list[min_forward_idx].result.forward = true;
                all_edges_list[min_forward_idx].result.backward = false;
                all_edges_list[min_forward_idx].result.is_split = has_backward;
            }
            if (has_backward)
            {
                std::swap(all_edges_list[min_backward_idx].result.source,
                          all_edges_list[min_backward_idx].result.target);
                all_edges_list[min_backward_idx].result.forward = true;
                all_edges_list[min_backward_idx].result.backward = false;
                all_edges_list[min_backward_idx].result.is_split = has_forward;
            }
        }

        // invalidate all unused edges
        for (unsigned j = start_idx; j < i; j++)
        {
            if (j == min_forward_idx || j == min_backward_idx)
            {
                continue;
            }
            all_edges_list[j].result.source = SPECIAL_NODEID;
            all_edges_list[j].result.target = SPECIAL_NODEID;
        }
    }
}

void ExtractionContainers::WriteEdges(std::ofstream &file_out_stream) const
{

    std::size_t start_position = 0;
    std::uint64_t used_edges_counter = 0;
    std::uint32_t used_edges_counter_buffer = 0;
    {
        util::UnbufferedLog log;
        log << "Writing used edges       ... " << std::flush;
        TIMER_START(write_edges);
        // Traverse list of edges and nodes in parallel and set target coord

        start_position = file_out_stream.tellp();
        file_out_stream.write((char *)&used_edges_counter_buffer,
                              sizeof(used_edges_counter_buffer));

        for (const auto &edge : all_edges_list)
        {
            if (edge.result.source == SPECIAL_NODEID || edge.result.target == SPECIAL_NODEID)
            {
                continue;
            }

            // IMPORTANT: here, we're using slicing to only write the data from the base
            // class of NodeBasedEdgeWithOSM
            NodeBasedEdge tmp = edge.result;
            file_out_stream.write((char *)&tmp, sizeof(NodeBasedEdge));
            used_edges_counter++;
        }

        if (used_edges_counter > std::numeric_limits<unsigned>::max())
        {
            throw util::exception("There are too many edges, OSRM only supports 2^32" + SOURCE_REF);
        }
        TIMER_STOP(write_edges);
        log << "ok, after " << TIMER_SEC(write_edges) << "s";
    }

    {
        util::UnbufferedLog log;
        log << "setting number of edges   ... " << std::flush;

        used_edges_counter_buffer = boost::numeric_cast<std::uint32_t>(used_edges_counter);

        file_out_stream.seekp(start_position);
        file_out_stream.write((char *)&used_edges_counter_buffer,
                              sizeof(used_edges_counter_buffer));
        log << "ok";
    }

    util::Log() << "Processed " << used_edges_counter << " edges";
}

void ExtractionContainers::WriteNodes(std::ofstream &file_out_stream) const
{
    {
        // write dummy value, will be overwritten later
        util::UnbufferedLog log;
        log << "setting number of nodes   ... " << std::flush;
        file_out_stream.write((char *)&max_internal_node_id, sizeof(unsigned));
        log << "ok";
    }

    {
        util::UnbufferedLog log;
        log << "Confirming/Writing used nodes     ... ";
        TIMER_START(write_nodes);
        // identify all used nodes by a merging step of two sorted lists
        auto node_iterator = all_nodes_list.begin();
        auto node_id_iterator = used_node_id_list.begin();
        const auto used_node_id_list_end = used_node_id_list.end();
        const auto all_nodes_list_end = all_nodes_list.end();

        while (node_id_iterator != used_node_id_list_end && node_iterator != all_nodes_list_end)
        {
            if (*node_id_iterator < node_iterator->node_id)
            {
                ++node_id_iterator;
                continue;
            }
            if (*node_id_iterator > node_iterator->node_id)
            {
                ++node_iterator;
                continue;
            }
            BOOST_ASSERT(*node_id_iterator == node_iterator->node_id);

            file_out_stream.write((char *)&(*node_iterator), sizeof(ExternalMemoryNode));

            ++node_id_iterator;
            ++node_iterator;
        }
        TIMER_STOP(write_nodes);
        log << "ok, after " << TIMER_SEC(write_nodes) << "s";
    }

    util::Log() << "Processed " << max_internal_node_id << " nodes";
}

void ExtractionContainers::WriteRestrictions(const std::string &path) const
{
    // serialize restrictions
    std::ofstream restrictions_out_stream;
    unsigned written_restriction_count = 0;
    restrictions_out_stream.open(path.c_str(), std::ios::binary);
    const util::FingerPrint fingerprint = util::FingerPrint::GetValid();
    restrictions_out_stream.write((char *)&fingerprint, sizeof(util::FingerPrint));
    const auto count_position = restrictions_out_stream.tellp();
    restrictions_out_stream.write((char *)&written_restriction_count, sizeof(unsigned));

    for (const auto &restriction_container : restrictions_list)
    {
        if (SPECIAL_NODEID != restriction_container.restriction.from.node &&
            SPECIAL_NODEID != restriction_container.restriction.via.node &&
            SPECIAL_NODEID != restriction_container.restriction.to.node)
        {
            restrictions_out_stream.write((char *)&(restriction_container.restriction),
                                          sizeof(TurnRestriction));
            ++written_restriction_count;
        }
    }
    restrictions_out_stream.seekp(count_position);
    restrictions_out_stream.write((char *)&written_restriction_count, sizeof(unsigned));
    util::Log() << "usable restrictions: " << written_restriction_count;
}

void ExtractionContainers::PrepareRestrictions()
{
    {
        util::UnbufferedLog log;
        log << "Sorting used ways         ... ";
        TIMER_START(sort_ways);
        stxxl::sort(way_start_end_id_list.begin(),
                    way_start_end_id_list.end(),
                    FirstAndLastSegmentOfWayStxxlCompare(),
                    stxxl_memory);
        TIMER_STOP(sort_ways);
        log << "ok, after " << TIMER_SEC(sort_ways) << "s";
    }

    {
        util::UnbufferedLog log;
        log << "Sorting " << restrictions_list.size() << " restriction. by from... ";
        TIMER_START(sort_restrictions);
        stxxl::sort(restrictions_list.begin(),
                    restrictions_list.end(),
                    CmpRestrictionContainerByFrom(),
                    stxxl_memory);
        TIMER_STOP(sort_restrictions);
        log << "ok, after " << TIMER_SEC(sort_restrictions) << "s";
    }

    {
        util::UnbufferedLog log;
        log << "Fixing restriction starts ... " << std::flush;
        TIMER_START(fix_restriction_starts);
        auto restrictions_iterator = restrictions_list.begin();
        auto way_start_and_end_iterator = way_start_end_id_list.cbegin();
        const auto restrictions_list_end = restrictions_list.end();
        const auto way_start_end_id_list_end = way_start_end_id_list.cend();

        while (way_start_and_end_iterator != way_start_end_id_list_end &&
               restrictions_iterator != restrictions_list_end)
        {
            if (way_start_and_end_iterator->way_id <
                OSMWayID{static_cast<std::uint32_t>(restrictions_iterator->restriction.from.way)})
            {
                ++way_start_and_end_iterator;
                continue;
            }

            if (way_start_and_end_iterator->way_id >
                OSMWayID{static_cast<std::uint32_t>(restrictions_iterator->restriction.from.way)})
            {
                util::Log(logDEBUG) << "Restriction references invalid way: "
                                    << restrictions_iterator->restriction.from.way;
                restrictions_iterator->restriction.from.node = SPECIAL_NODEID;
                ++restrictions_iterator;
                continue;
            }

            BOOST_ASSERT(
                way_start_and_end_iterator->way_id ==
                OSMWayID{static_cast<std::uint32_t>(restrictions_iterator->restriction.from.way)});
            // we do not remap the via id yet, since we will need it for the to node as well
            const OSMNodeID via_node_id = OSMNodeID{restrictions_iterator->restriction.via.node};

            // check if via is actually valid, if not invalidate
            auto via_id_iter = external_to_internal_node_id_map.find(via_node_id);
            if (via_id_iter == external_to_internal_node_id_map.end())
            {
                util::Log(logDEBUG) << "Restriction references invalid node: "
                                    << restrictions_iterator->restriction.via.node;
                restrictions_iterator->restriction.via.node = SPECIAL_NODEID;
                ++restrictions_iterator;
                continue;
            }

            if (way_start_and_end_iterator->first_segment_source_id == via_node_id)
            {
                // assign new from node id
                auto id_iter = external_to_internal_node_id_map.find(
                    way_start_and_end_iterator->first_segment_target_id);
                if (id_iter == external_to_internal_node_id_map.end())
                {
                    util::Log(logDEBUG) << "Way references invalid node: "
                                        << way_start_and_end_iterator->first_segment_target_id;
                    restrictions_iterator->restriction.from.node = SPECIAL_NODEID;
                    ++restrictions_iterator;
                    ++way_start_and_end_iterator;
                    continue;
                }
                restrictions_iterator->restriction.from.node = id_iter->second;
            }
            else if (way_start_and_end_iterator->last_segment_target_id == via_node_id)
            {
                // assign new from node id
                auto id_iter = external_to_internal_node_id_map.find(
                    way_start_and_end_iterator->last_segment_source_id);
                if (id_iter == external_to_internal_node_id_map.end())
                {
                    util::Log(logDEBUG) << "Way references invalid node: "
                                        << way_start_and_end_iterator->last_segment_target_id;
                    restrictions_iterator->restriction.from.node = SPECIAL_NODEID;
                    ++restrictions_iterator;
                    ++way_start_and_end_iterator;
                    continue;
                }
                restrictions_iterator->restriction.from.node = id_iter->second;
            }
            ++restrictions_iterator;
        }

        TIMER_STOP(fix_restriction_starts);
        log << "ok, after " << TIMER_SEC(fix_restriction_starts) << "s";
    }

    {
        util::UnbufferedLog log;
        log << "Sorting restrictions. by to  ... " << std::flush;
        TIMER_START(sort_restrictions_to);
        stxxl::sort(restrictions_list.begin(),
                    restrictions_list.end(),
                    CmpRestrictionContainerByTo(),
                    stxxl_memory);
        TIMER_STOP(sort_restrictions_to);
        log << "ok, after " << TIMER_SEC(sort_restrictions_to) << "s";
    }

    {
        util::UnbufferedLog log;
        log << "Fixing restriction ends   ... " << std::flush;
        TIMER_START(fix_restriction_ends);
        auto restrictions_iterator = restrictions_list.begin();
        auto way_start_and_end_iterator = way_start_end_id_list.cbegin();
        const auto way_start_end_id_list_end_ = way_start_end_id_list.cend();
        const auto restrictions_list_end_ = restrictions_list.end();

        while (way_start_and_end_iterator != way_start_end_id_list_end_ &&
               restrictions_iterator != restrictions_list_end_)
        {
            if (way_start_and_end_iterator->way_id <
                OSMWayID{static_cast<std::uint32_t>(restrictions_iterator->restriction.to.way)})
            {
                ++way_start_and_end_iterator;
                continue;
            }
            if (restrictions_iterator->restriction.from.node == SPECIAL_NODEID ||
                restrictions_iterator->restriction.via.node == SPECIAL_NODEID)
            {
                ++restrictions_iterator;
                continue;
            }
            if (way_start_and_end_iterator->way_id >
                OSMWayID{static_cast<std::uint32_t>(restrictions_iterator->restriction.to.way)})
            {
                util::Log(logDEBUG) << "Restriction references invalid way: "
                                    << restrictions_iterator->restriction.to.way;
                restrictions_iterator->restriction.to.way = SPECIAL_NODEID;
                ++restrictions_iterator;
                continue;
            }
            BOOST_ASSERT(
                way_start_and_end_iterator->way_id ==
                OSMWayID{static_cast<std::uint32_t>(restrictions_iterator->restriction.to.way)});
            const OSMNodeID via_node_id = OSMNodeID{restrictions_iterator->restriction.via.node};

            // assign new via node id
            auto via_id_iter = external_to_internal_node_id_map.find(via_node_id);
            BOOST_ASSERT(via_id_iter != external_to_internal_node_id_map.end());
            restrictions_iterator->restriction.via.node = via_id_iter->second;

            if (way_start_and_end_iterator->first_segment_source_id == via_node_id)
            {
                auto to_id_iter = external_to_internal_node_id_map.find(
                    way_start_and_end_iterator->first_segment_target_id);
                if (to_id_iter == external_to_internal_node_id_map.end())
                {
                    util::Log(logDEBUG) << "Way references invalid node: "
                                        << way_start_and_end_iterator->first_segment_source_id;
                    restrictions_iterator->restriction.to.node = SPECIAL_NODEID;
                    ++restrictions_iterator;
                    ++way_start_and_end_iterator;
                    continue;
                }
                restrictions_iterator->restriction.to.node = to_id_iter->second;
            }
            else if (way_start_and_end_iterator->last_segment_target_id == via_node_id)
            {
                auto to_id_iter = external_to_internal_node_id_map.find(
                    way_start_and_end_iterator->last_segment_source_id);
                if (to_id_iter == external_to_internal_node_id_map.end())
                {
                    util::Log(logDEBUG) << "Way references invalid node: "
                                        << way_start_and_end_iterator->last_segment_source_id;
                    restrictions_iterator->restriction.to.node = SPECIAL_NODEID;
                    ++restrictions_iterator;
                    ++way_start_and_end_iterator;
                    continue;
                }
                restrictions_iterator->restriction.to.node = to_id_iter->second;
            }
            ++restrictions_iterator;
        }
        TIMER_STOP(fix_restriction_ends);
        log << "ok, after " << TIMER_SEC(fix_restriction_ends) << "s";
    }
}
}
}
