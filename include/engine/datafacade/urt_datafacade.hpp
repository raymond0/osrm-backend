#ifndef URT_DATAFACADE_HPP
#define URT_DATAFACADE_HPP

#include "engine/datafacade/datafacade_base.hpp"

#include "extractor/compressed_edge_container.hpp"
#include "extractor/guidance/turn_instruction.hpp"
#include "extractor/guidance/turn_lane_types.hpp"
#include "extractor/profile_properties.hpp"
#include "util/guidance/bearing_class.hpp"
#include "util/guidance/entry_class.hpp"
#include "util/guidance/turn_lanes.hpp"

#include "engine/geospatial_query.hpp"
#include "util/exception.hpp"
#include "util/exception_utils.hpp"
#include "util/guidance/turn_bearing.hpp"
#include "util/log.hpp"
#include "util/packed_vector.hpp"
#include "util/range_table.hpp"
#include "util/rectangle.hpp"
#include "util/urt_static_graph.hpp"
#include "util/static_rtree.hpp"
#include "util/typedefs.hpp"

#include <boost/assert.hpp>
#include <boost/interprocess/sync/named_sharable_mutex.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/thread/tss.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "GeometryFile.h"
#include "CoordinatesFile.h"

using namespace std;

namespace osrm
{
namespace engine
{
namespace datafacade
{

/**
 * This base class implements the Datafacade interface for accessing
 * data that's stored in a single large block of memory (RAM).
 *
 * In this case "internal memory" refers to RAM - as opposed to "external memory",
 * which usually refers to disk.
 */
class UrtDataFacade : public BaseDataFacade
{
public:
    explicit UrtDataFacade(const storage::StorageConfig &config)
    {
        LoadGraphs(config);
        LoadNodeFiles( config.ur_shortnodes_paths );
        LoadGeometries( config.ur_geometry_paths );
    }

  private:
    using super = BaseDataFacade;
    using QueryGraph = util::UrtStaticGraph;
    using GraphNode = QueryGraph::NodeArrayEntry;
    using IndexBlock = util::RangeTable<16, true>::BlockT;
    using RTreeLeaf = super::RTreeLeaf;
    using SharedRTree =
        util::StaticRTree<RTreeLeaf, util::ShM<util::Coordinate, true>::vector, true>;
    using SharedGeospatialQuery = GeospatialQuery<SharedRTree, BaseDataFacade>;
    using RTreeNode = SharedRTree::TreeNode;

    vector< QueryGraph::NodeRange > m_graph_ranges;
    vector< shared_ptr< QueryGraph > > m_query_graphs;
    vector< shared_ptr< GeometryFile > > m_geometryFiles;
    vector< shared_ptr< CoordinatesFile > > m_coordinatesFiles;

    unsigned m_check_sum;
    std::string m_timestamp;


    util::ShM<std::size_t, true>::vector m_datasource_name_lengths;

    std::unique_ptr<SharedRTree> m_static_rtree;
    std::unique_ptr<SharedGeospatialQuery> m_geospatial_query;
    boost::filesystem::path file_index_path;
    
    
    shared_ptr< QueryGraph > LoadGraph(const string &hsgr_path)
    {
        shared_ptr< QueryGraph > query_graph = shared_ptr< QueryGraph > (new QueryGraph( hsgr_path ));
        
        return query_graph;
    }
    
    void LoadGeometryFile(const string &geometry_file)
    {
        shared_ptr<GeometryFile> geomFile ( new GeometryFile( geometry_file ) );
        
        if ( ! geomFile->LoadGeometryFile()  )
        {
            cout << "Failed loading geometry file: " << geometry_file << "\n";
            return;
        }
        
        m_geometryFiles.emplace_back ( geomFile );
    }
    
    
    void LoadGeometries( vector<string> filenames )
    {
        for ( string filename : filenames )
        {
            LoadGeometryFile( filename );
        }
    }
    
    void LoadNodeFiles( const vector<string> &filenames )
    {
        for ( string filename : filenames )
        {
            shared_ptr<CoordinatesFile> coordFile ( new CoordinatesFile( filename ) );
            if ( ! coordFile->LoadCoordinatesFile() )
            {
                cout << "Failed to load coords file: " << filename << "\n";
                continue;
            }
            
            m_coordinatesFiles.emplace_back(coordFile);
        }
    }

    
    void LoadGraphs( const storage::StorageConfig &config )
    {
        for ( const auto &graphPath : config.ur_hsgr_paths )
        {
            try
            {
                auto graph = LoadGraph( graphPath );
                m_query_graphs.emplace_back( graph );
                m_graph_ranges.emplace_back( graph->RangeOfGraph() );
            }
            catch ( util::exception e )
            {
                cout << "Failed to open " << graphPath << "\n";
                
                if ( graphPath.compare( graphPath.length() - 4, 4, "hsgr" ) != 0 )
                {
                    cout << "Extension was not 'hsgr' - unsure how to prevent coordinates/geometries from loading";
                    continue;
                }
                
                //
                // Don't load the nodes or geometries
                //
                string path = graphPath.substr( 0, graphPath.length() - 4 );
                
                /* ToDo - fix or remove this const auto new_end_iter =
                std::remove_if(config.ur_shortnodes_paths.begin(), config.ur_shortnodes_paths.end(), [&config, path](const string &nodePath)
                {
                   return nodePath.compare( 0, path.length(), path ) == 0;
                });
                config.ur_shortnodes_paths.erase(new_end_iter, config.ur_shortnodes_paths.end()); // remove excess candidates.
                
                const auto new_geom_iter =
                std::remove_if(config.geometryPaths.begin(), config.geometryPaths.end(), [&config, path](const string &geomPath)
                {
                   return geomPath.compare( 0, path.length(), path ) == 0;
                });
                config.geometryPaths.erase(new_geom_iter, config.geometryPaths.end()); // remove excess candidates.
                */
            }
            
        }
    }

  public:
    // search graph access
    unsigned GetNumberOfNodes() const override final {
        // ToDo - check this matters or what I did before. Potential speed issue it we have to resize the
        // heap too often
        return 1000;
    }

    unsigned GetNumberOfEdges() const override final {
        BOOST_ASSERT(false);
    }

    unsigned GetOutDegree(const NodeID n) const override final
    {
        BOOST_ASSERT(false);
    }

    NodeID GetTarget(const EdgeID e) const override final {
        BOOST_ASSERT(false);
        //return m_query_graph->GetTarget(e); // RHCALLED
    }

    EdgeData &GetEdgeData(const EdgeID e) const override final
    {
        BOOST_ASSERT(false);
        //return m_query_graph->GetEdgeData(e);   // RHCALLED
    }

    EdgeID BeginEdges(const NodeID n) const override final {
        BOOST_ASSERT(false);
    }

    EdgeID EndEdges(const NodeID n) const override final {
        BOOST_ASSERT(false);
    }

    EdgeRange GetAdjacentEdgeRange(const NodeID node) const override final
    {
        BOOST_ASSERT(false);
        //return m_query_graph->GetAdjacentEdgeRange(node);   // RHCALLED
    }
    
    //shared_ptr< QueryGraph > m_lastQueryGraph;
    shared_ptr< QueryGraph > QueryGraphForNode( const NodeID node ) const
    {
        // ToDo - fix cache
        /*if ( m_lastQueryGraph != nullptr )
        {
            if ( m_lastQueryGraph->NodeInRange( node ) )
                return m_lastQueryGraph;
        }*/
        
        for ( const auto graph : m_query_graphs )
        {
            if ( graph->NodeInRange( node ) )
            {
                //m_lastQueryGraph = graph;
                return graph;
            }
        }
        
        return nullptr;
    }
    
    virtual void GetAdjacentEdges( const NodeID node, EdgeArray &edges ) const override final
    {
        if ( node == SPECIAL_NODEID )
            return;
        
        auto query_graph = QueryGraphForNode( node );
        if ( query_graph == nullptr )
            return;
        
        const auto edgeRange = query_graph->GetAdjacentEdgeRange(node);
        
        for ( auto edgeId : edgeRange )
        {
            if ( edgeId == 2147483647 || edgeId == SPECIAL_NODEID )
                continue;
            
            EdgeArrayEntryApp edgeData;
            query_graph->GetEdge( edgeId, edgeData );
            
            if ( edgeData.shortcut && ( ( edgeData.middleNodeId == 2147483647 ) || ( edgeData.middleNodeId == SPECIAL_NODEID ) ) )
                continue;
            
            if ( edgeData.target == SPECIAL_NODEID || edgeData.target == 2147483647 )
                continue;
            
            /*if ( ! NodeAvailableInGraphs( edgeData.target ) )
             continue;*/
            
            /*printf( "    edge: %d with data: %d ", complete.edgeId, complete.edgeDataId );*/
            
            //printf( " shortcut %d, distance: %d, target: %d\n", complete.shortcut, complete.distance, complete.targetNodeId );
            
            edges.emplace_back( edgeData );
        }
    }

    // searches for a specific edge
    EdgeID FindEdge(const NodeID from, const NodeID to) const override final
    {
        BOOST_ASSERT(false);
    }

    EdgeID FindEdgeInEitherDirection(const NodeID from, const NodeID to) const override final
    {
        BOOST_ASSERT(false);
    }

    EdgeID
    FindEdgeIndicateIfReverse(const NodeID from, const NodeID to, bool &result) const override final
    {
        BOOST_ASSERT(false);
    }
    
    
    bool FindSmallestForwardEdge(const NodeID from, const NodeID to, EdgeArrayEntryApp &smallest_edge) override final
    {
        if ( from == SPECIAL_NODEID )
            return false;
        
        auto query_graph = QueryGraphForNode( from );
        if ( query_graph == nullptr )
            return false;

        return query_graph->FindSmallestForwardEdge( from, to, smallest_edge );
    }
    
    
    bool FindSmallestBackwardEdge(const NodeID from, const NodeID to, EdgeArrayEntryApp &smallest_edge) override final
    {
        if ( from == SPECIAL_NODEID )
            return false;
        
        auto query_graph = QueryGraphForNode( from );
        if ( query_graph == nullptr )
            return false;
        
        return query_graph->FindSmallestBackwardEdge( from, to, smallest_edge );
    }


    EdgeID FindSmallestEdge(const NodeID from,
                            const NodeID to,
                            std::function<bool(EdgeData)> filter) const override final
    {
        BOOST_ASSERT(false);
        //return m_query_graph->FindSmallestEdge(from, to, filter);   // RHCALLED
    }

    // node and edge information access
    util::Coordinate GetCoordinateOfNode(const NodeID id) const override final
    {
        /*if ( m_lastCoordinatesFile != nullptr && m_lastCoordinatesFile->CanResolveNode(id))
        {
            return m_lastCoordinatesFile->GetNodeCoords( id );
        }*/
        
        for ( const auto &nodeFile : m_coordinatesFiles )
        {
            if ( nodeFile->CanResolveNode(id) )
            {
                //m_lastCoordinatesFile = nodeFile;
                return nodeFile->GetNodeCoords( id );
            }
        }
        
        // ToDo
        throw util::exception( ROUTING_FAILED_SEGMENTATION );
    }

    OSMNodeID GetOSMNodeIDOfNode(const unsigned id) const override final
    {
        OSMNodeID nodeId;
        return nodeId;
    }

    virtual std::vector<NodeID> GetUncompressedForwardGeometry(const EdgeID id) override final
    {
        std::vector<NodeID> result_nodes;

        for ( const auto &geomFile : m_geometryFiles )
        {
            if ( geomFile->CanResolveGeometry( id ))
            {
                //m_lastGeometryFile = geomFile;
                geomFile->GetUncompressedForwardGeometry( id, result_nodes );
                return result_nodes;
            }
        }

        return result_nodes;
    }

    virtual std::vector<NodeID> GetUncompressedReverseGeometry(const EdgeID id) override final
    {
        std::vector<NodeID> result_nodes;
        
        for ( const auto &geomFile : m_geometryFiles )
        {
            if ( geomFile->CanResolveGeometry( id ))
            {
                //m_lastGeometryFile = geomFile;
                geomFile->GetUncompressedReverseGeometry( id, result_nodes );
                return result_nodes;
            }
        }
        
        return result_nodes;
    }

    virtual std::vector<EdgeWeight>
    GetUncompressedForwardWeights(const EdgeID id) const override final
    {
        std::vector<EdgeWeight> result_weights;
        return result_weights;
    }

    virtual std::vector<EdgeWeight>
    GetUncompressedReverseWeights(const EdgeID id) const override final
    {
        std::vector<EdgeWeight> result_weights;
        return result_weights;
    }

    virtual GeometryID GetGeometryIndexForEdgeID(const unsigned id) const override final
    {
        GeometryID gid;
        return gid; // ToDo - still not sure what to do here
        //return m_via_geometry_list.at(id);  // RHCALLED
    }

    extractor::guidance::TurnInstruction
    GetTurnInstructionForEdgeID(const unsigned id) const override final
    {
        extractor::guidance::TurnInstruction ti( extractor::guidance::TurnType::Turn );
        return ti;
    }

    extractor::TravelMode GetTravelModeForEdgeID(const unsigned id) const override final
    {
        return TRAVEL_MODE_DRIVING;
    }

    std::vector<RTreeLeaf> GetEdgesInBox(const util::Coordinate south_west,
                                         const util::Coordinate north_east) const override final
    {
        BOOST_ASSERT(false);
    }

    std::vector<PhantomNodeWithDistance>
    NearestPhantomNodesInRange(const util::Coordinate input_coordinate,
                               const float max_distance) const override final
    {
        BOOST_ASSERT(false);
    }

    std::vector<PhantomNodeWithDistance>
    NearestPhantomNodesInRange(const util::Coordinate input_coordinate,
                               const float max_distance,
                               const int bearing,
                               const int bearing_range) const override final
    {
        BOOST_ASSERT(false);
    }

    std::vector<PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate input_coordinate,
                        const unsigned max_results) const override final
    {
        BOOST_ASSERT(false);
    }

    std::vector<PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate input_coordinate,
                        const unsigned max_results,
                        const double max_distance) const override final
    {
        BOOST_ASSERT(false);
    }

    std::vector<PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate input_coordinate,
                        const unsigned max_results,
                        const int bearing,
                        const int bearing_range) const override final
    {
        BOOST_ASSERT(false);
    }

    std::vector<PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate input_coordinate,
                        const unsigned max_results,
                        const double max_distance,
                        const int bearing,
                        const int bearing_range) const override final
    {
        BOOST_ASSERT(false);
    }

    std::pair<PhantomNode, PhantomNode> NearestPhantomNodeWithAlternativeFromBigComponent(
        const util::Coordinate input_coordinate) const override final
    {
        BOOST_ASSERT(m_geospatial_query.get()); // RHCALLED - only for phantom node lookup

        return m_geospatial_query->NearestPhantomNodeWithAlternativeFromBigComponent(
            input_coordinate);
    }

    std::pair<PhantomNode, PhantomNode> NearestPhantomNodeWithAlternativeFromBigComponent(
        const util::Coordinate input_coordinate, const double max_distance) const override final
    {
        BOOST_ASSERT(false);
    }

    std::pair<PhantomNode, PhantomNode>
    NearestPhantomNodeWithAlternativeFromBigComponent(const util::Coordinate input_coordinate,
                                                      const double max_distance,
                                                      const int bearing,
                                                      const int bearing_range) const override final
    {
        BOOST_ASSERT(false);
    }

    std::pair<PhantomNode, PhantomNode>
    NearestPhantomNodeWithAlternativeFromBigComponent(const util::Coordinate input_coordinate,
                                                      const int bearing,
                                                      const int bearing_range) const override final
    {
        BOOST_ASSERT(false);
    }

    unsigned GetCheckSum() const override final {
        return m_check_sum; // RHCALLED
    }

    unsigned GetNameIndexFromEdgeID(const unsigned id) const override final
    {
        return 0;
    }

    std::string GetNameForID(const unsigned name_id) const override final
    {
        return "";  // RHCALLED
    }

    std::string GetRefForID(const unsigned name_id) const override final
    {
        // We store the ref after the name, destination and pronunciation of a street.
        // We do this to get around the street length limit of 255 which would hit
        // if we concatenate these. Order (see extractor_callbacks):
        // name (0), destination (1), pronunciation (2), ref (3)
        BOOST_ASSERT(false);
    }

    std::string GetPronunciationForID(const unsigned name_id) const override final
    {
        // We store the pronunciation after the name and destination of a street.
        // We do this to get around the street length limit of 255 which would hit
        // if we concatenate these. Order (see extractor_callbacks):
        // name (0), destination (1), pronunciation (2), ref (3)
        BOOST_ASSERT(false);
    }

    std::string GetDestinationsForID(const unsigned name_id) const override final
    {
        // We store the destination after the name of a street.
        // We do this to get around the street length limit of 255 which would hit
        // if we concatenate these. Order (see extractor_callbacks):
        // name (0), destination (1), pronunciation (2), ref (3)
        BOOST_ASSERT(false);
    }

    bool IsCoreNode(const NodeID id) const override final
    {
        BOOST_ASSERT(false);
    }

    virtual std::size_t GetCoreSize() const override final {
        return 0; // ToDo - something
        // return m_is_core_node.size();   // RHCALLED - not sure
    }

    // Returns the data source ids that were used to supply the edge
    // weights.
    virtual std::vector<uint8_t>
    GetUncompressedForwardDatasources(const EdgeID id) const override final
    {
        /*
         * Data sources for geometries are stored in one place for
         * both forward and reverse segments along the same bi-
         * directional edge. The m_geometry_indices stores
         * refences to where to find the beginning of the bi-
         * directional edge in the m_geometry_list vector. For
         * forward datasources of bi-directional edges, edges 2 to
         * n of that edge need to be read.
         */
        //onst unsigned begin = m_geometry_indices.at(id) + 1;   // RHCALLED - geometry lookup (unpacking)
        //const unsigned end = m_geometry_indices.at(id + 1);

        std::vector<uint8_t> result_datasources;
        //result_datasources.resize(end - begin);

        // If there was no datasource info, return an array of 0's.
        //if (m_datasource_list.empty())
        /*{
            for (unsigned i = 0; i < end - begin; ++i)
            {
                result_datasources.push_back(0);
            }
        }*/
        /*else
        {
            std::copy(m_datasource_list.begin() + begin,
                      m_datasource_list.begin() + end,
                      result_datasources.begin());
        }*/

        return result_datasources;
    }

    // Returns the data source ids that were used to supply the edge
    // weights.
    virtual std::vector<uint8_t>
    GetUncompressedReverseDatasources(const EdgeID id) const override final
    {
        /*
         * Datasources for geometries are stored in one place for
         * both forward and reverse segments along the same bi-
         * directional edge. The m_geometry_indices stores
         * refences to where to find the beginning of the bi-
         * directional edge in the m_geometry_list vector. For
         * reverse datasources of bi-directional edges, edges 1 to
         * n-1 of that edge need to be read in reverse.
         */
        //const unsigned begin = m_geometry_indices.at(id);   // RHCALLED - geometry lookup
        //const unsigned end = m_geometry_indices.at(id + 1) - 1;

        std::vector<uint8_t> result_datasources;
        //result_datasources.resize(end - begin);

        // If there was no datasource info, return an array of 0's.
        //if (m_datasource_list.empty())
        /*{
            for (unsigned i = 0; i < end - begin; ++i)
            {
                result_datasources.push_back(0);
            }
        }*/
        /*else
        {
            std::copy(m_datasource_list.rbegin() + (m_datasource_list.size() - end),
                      m_datasource_list.rbegin() + (m_datasource_list.size() - begin),
                      result_datasources.begin());
        }*/

        return result_datasources;
    }

    virtual std::string GetDatasourceName(const uint8_t datasource_name_id) const override final
    {
        BOOST_ASSERT(false);
    }

    std::string GetTimestamp() const override final {
        BOOST_ASSERT(false);
    }

    bool GetContinueStraightDefault() const override final
    {
        BOOST_ASSERT(false);    // Not called if specified in route parameters
    }

    double GetMapMatchingMaxSpeed() const override final
    {
        BOOST_ASSERT(false);
    }

    BearingClassID GetBearingClassID(const NodeID id) const override final
    {
        BOOST_ASSERT(false);
    }

    util::guidance::BearingClass
    GetBearingClass(const BearingClassID bearing_class_id) const override final
    {
        BOOST_ASSERT(false);
    }

    EntryClassID GetEntryClassID(const EdgeID eid) const override final
    {
        EntryClassID ecid;
        return ecid;
    }

    util::guidance::TurnBearing PreTurnBearing(const EdgeID eid) const override final
    {
        util::guidance::TurnBearing tb;
        return tb;
    }
    util::guidance::TurnBearing PostTurnBearing(const EdgeID eid) const override final
    {
        util::guidance::TurnBearing tb;
        return tb;
    }

    util::guidance::EntryClass GetEntryClass(const EntryClassID entry_class_id) const override final
    {
        BOOST_ASSERT(false);
    }

    bool hasLaneData(const EdgeID id) const override final
    {
        return false;
    }

    util::guidance::LaneTupleIdPair GetLaneData(const EdgeID id) const override final
    {
        BOOST_ASSERT(false);
    }

    extractor::guidance::TurnLaneDescription
    GetTurnDescription(const LaneDescriptionID lane_description_id) const override final
    {
        BOOST_ASSERT(false);
    }
};
}
}
}

#endif // URT_DATAFACADE_HPP
