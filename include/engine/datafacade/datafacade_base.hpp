#ifndef DATAFACADE_BASE_HPP
#define DATAFACADE_BASE_HPP

// Exposes all data access interfaces to the algorithms via base class ptr

#include "contractor/query_edge.hpp"
#include "extractor/edge_based_node.hpp"
#include "extractor/external_memory_node.hpp"
#include "extractor/guidance/turn_instruction.hpp"
#include "extractor/guidance/turn_lane_types.hpp"
#include "extractor/original_edge_data.hpp"
#include "engine/phantom_node.hpp"
#include "util/exception.hpp"
#include "util/guidance/bearing_class.hpp"
#include "util/guidance/entry_class.hpp"
#include "util/guidance/turn_bearing.hpp"
#include "util/guidance/turn_lanes.hpp"
#include "util/integer_range.hpp"
#include "util/string_util.hpp"
#include "util/string_view.hpp"
#include "util/typedefs.hpp"

#include "osrm/coordinate.hpp"

#include <cstddef>

#include <string>
#include <utility>
#include <vector>

#include "urt_datatypes.hpp"

namespace osrm
{
namespace engine
{
namespace datafacade
{

using StringView = util::StringView;
using EdgeRange = util::range<EdgeID>;

class BaseDataFacade
{
  public:
    using EdgeData = contractor::QueryEdge::EdgeData;
    using RTreeLeaf = extractor::EdgeBasedNode;
    BaseDataFacade() {}
    virtual ~BaseDataFacade() {}

    // search graph access
    virtual unsigned GetNumberOfNodes() const = 0;

    virtual unsigned GetNumberOfEdges() const = 0;

    virtual unsigned GetOutDegree(const NodeID n) const = 0;

    virtual NodeID GetTarget(const EdgeID e) const = 0;

    virtual const EdgeData &GetEdgeData(const EdgeID e) const = 0;

    virtual EdgeID BeginEdges(const NodeID n) const = 0;

    virtual EdgeID EndEdges(const NodeID n) const = 0;

    virtual EdgeRange GetAdjacentEdgeRange(const NodeID node) const = 0;
    
    // URT Addition
    virtual void GetAdjacentEdges( const NodeID node, EdgeArray &edges ) {}

    // searches for a specific edge
    virtual EdgeID FindEdge(const NodeID from, const NodeID to) const = 0;

    virtual EdgeID FindEdgeInEitherDirection(const NodeID from, const NodeID to) const = 0;

    virtual EdgeID
    FindEdgeIndicateIfReverse(const NodeID from, const NodeID to, bool &result) const = 0;

    virtual EdgeID FindSmallestEdge(const NodeID from,
                                    const NodeID to,
                                    const std::function<bool(EdgeData)> filter) const = 0;
    
    // 2 URT Additions
    virtual bool FindSmallestForwardEdge(const NodeID from, const NodeID to, EdgeArrayEntryApp &smallest_edge) { BOOST_ASSERT( false ); }
    virtual bool FindSmallestBackwardEdge(const NodeID from, const NodeID to, EdgeArrayEntryApp &smallest_edge) { BOOST_ASSERT( false ); }

    // node and edge information access
    virtual util::Coordinate GetCoordinateOfNode(const NodeID id) = 0;
    virtual OSMNodeID GetOSMNodeIDOfNode(const NodeID id) const = 0;

    virtual GeometryID GetGeometryIndexForEdgeID(const EdgeID id) const = 0;

    virtual std::vector<NodeID> GetUncompressedForwardGeometry(const EdgeID id) = 0;
    virtual std::vector<NodeID> GetUncompressedReverseGeometry(const EdgeID id) = 0;

    virtual TurnPenalty GetWeightPenaltyForEdgeID(const unsigned id) const = 0;

    virtual TurnPenalty GetDurationPenaltyForEdgeID(const unsigned id) const = 0;

    // Gets the weight values for each segment in an uncompressed geometry.
    // Should always be 1 shorter than GetUncompressedGeometry
    virtual std::vector<EdgeWeight> GetUncompressedForwardWeights(const EdgeID id) const = 0;
    virtual std::vector<EdgeWeight> GetUncompressedReverseWeights(const EdgeID id) const = 0;

    // Gets the duration values for each segment in an uncompressed geometry.
    // Should always be 1 shorter than GetUncompressedGeometry
    virtual std::vector<EdgeWeight> GetUncompressedForwardDurations(const EdgeID id) const = 0;
    virtual std::vector<EdgeWeight> GetUncompressedReverseDurations(const EdgeID id) const = 0;

    // Returns the data source ids that were used to supply the edge
    // weights.  Will return an empty array when only the base profile is used.
    virtual std::vector<DatasourceID> GetUncompressedForwardDatasources(const EdgeID id) const = 0;
    virtual std::vector<DatasourceID> GetUncompressedReverseDatasources(const EdgeID id) const = 0;

    // Gets the name of a datasource
    virtual StringView GetDatasourceName(const DatasourceID id) const = 0;

    virtual extractor::guidance::TurnInstruction
    GetTurnInstructionForEdgeID(const EdgeID id) const = 0;

    virtual extractor::TravelMode GetTravelModeForEdgeID(const EdgeID id) const = 0;

    virtual std::vector<RTreeLeaf> GetEdgesInBox(const util::Coordinate south_west,
                                                 const util::Coordinate north_east) const = 0;

    virtual std::vector<PhantomNodeWithDistance>
    NearestPhantomNodesInRange(const util::Coordinate input_coordinate,
                               const float max_distance,
                               const int bearing,
                               const int bearing_range) const = 0;
    virtual std::vector<PhantomNodeWithDistance>
    NearestPhantomNodesInRange(const util::Coordinate input_coordinate,
                               const float max_distance) const = 0;

    virtual std::vector<PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate input_coordinate,
                        const unsigned max_results,
                        const double max_distance,
                        const int bearing,
                        const int bearing_range) const = 0;
    virtual std::vector<PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate input_coordinate,
                        const unsigned max_results,
                        const int bearing,
                        const int bearing_range) const = 0;
    virtual std::vector<PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate input_coordinate,
                        const unsigned max_results) const = 0;
    virtual std::vector<PhantomNodeWithDistance>
    NearestPhantomNodes(const util::Coordinate input_coordinate,
                        const unsigned max_results,
                        const double max_distance) const = 0;

    virtual std::pair<PhantomNode, PhantomNode> NearestPhantomNodeWithAlternativeFromBigComponent(
        const util::Coordinate input_coordinate) const = 0;
    virtual std::pair<PhantomNode, PhantomNode>
    NearestPhantomNodeWithAlternativeFromBigComponent(const util::Coordinate input_coordinate,
                                                      const double max_distance) const = 0;
    virtual std::pair<PhantomNode, PhantomNode>
    NearestPhantomNodeWithAlternativeFromBigComponent(const util::Coordinate input_coordinate,
                                                      const double max_distance,
                                                      const int bearing,
                                                      const int bearing_range) const = 0;
    virtual std::pair<PhantomNode, PhantomNode>
    NearestPhantomNodeWithAlternativeFromBigComponent(const util::Coordinate input_coordinate,
                                                      const int bearing,
                                                      const int bearing_range) const = 0;

    virtual bool hasLaneData(const EdgeID id) const = 0;
    virtual util::guidance::LaneTupleIdPair GetLaneData(const EdgeID id) const = 0;
    virtual extractor::guidance::TurnLaneDescription
    GetTurnDescription(const LaneDescriptionID lane_description_id) const = 0;

    virtual unsigned GetCheckSum() const = 0;

    virtual bool IsCoreNode(const NodeID id) const = 0;

    virtual NameID GetNameIndexFromEdgeID(const EdgeID id) const = 0;

    virtual StringView GetNameForID(const NameID id) const = 0;

    virtual StringView GetRefForID(const NameID id) const = 0;

    virtual StringView GetPronunciationForID(const NameID id) const = 0;

    virtual StringView GetDestinationsForID(const NameID id) const = 0;

    virtual std::size_t GetCoreSize() const = 0;

    virtual std::string GetTimestamp() const = 0;

    virtual bool GetContinueStraightDefault() const = 0;

    virtual double GetMapMatchingMaxSpeed() const = 0;

    virtual const char *GetWeightName() const = 0;

    virtual unsigned GetWeightPrecision() const = 0;

    virtual double GetWeightMultiplier() const = 0;

    virtual BearingClassID GetBearingClassID(const NodeID id) const = 0;

    virtual util::guidance::TurnBearing PreTurnBearing(const EdgeID eid) const = 0;
    virtual util::guidance::TurnBearing PostTurnBearing(const EdgeID eid) const = 0;

    virtual util::guidance::BearingClass
    GetBearingClass(const BearingClassID bearing_class_id) const = 0;

    virtual EntryClassID GetEntryClassID(const EdgeID eid) const = 0;

    virtual util::guidance::EntryClass GetEntryClass(const EntryClassID entry_class_id) const = 0;
};
}
}
}

#endif // DATAFACADE_BASE_HPP
