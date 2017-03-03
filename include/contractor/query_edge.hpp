#ifndef QUERYEDGE_HPP
#define QUERYEDGE_HPP

#include "util/typedefs.hpp"

#include <tuple>

namespace osrm
{
namespace contractor
{

struct QueryEdge
{
    NodeID source;
    NodeID target;
    struct EdgeData
    {
        explicit EdgeData()
            : id(0), shortcut(false), weight(0), duration(0), forward(false), backward(false)
        {
        }

        template <class OtherT> EdgeData(const OtherT &other)
        {
            weight = other.weight;
            duration = 0;
            shortcut = other.shortcut;
            id = other.geometryId.id;
            forward = other.forward;
            backward = other.backward;
        }
        // this ID is either the middle node of the shortcut, or the ID of the edge based node (node
        // based edge) storing the appropriate data. If `shortcut` is set to true, we get the middle
        // node. Otherwise we see the edge based node to access node data.
        NodeID id : 31;
        bool shortcut : 1;
        EdgeWeight weight;
        EdgeWeight duration : 30;
        std::uint32_t forward : 1;
        std::uint32_t backward : 1;
    } data;

    QueryEdge() : source(SPECIAL_NODEID), target(SPECIAL_NODEID) {}

    QueryEdge(NodeID source, NodeID target, EdgeData data)
        : source(source), target(target), data(std::move(data))
    {
    }

    bool operator<(const QueryEdge &rhs) const
    {
        return std::tie(source, target) < std::tie(rhs.source, rhs.target);
    }

    bool operator==(const QueryEdge &right) const
    {
        return (source == right.source && target == right.target &&
                data.weight == right.data.weight && data.duration == right.data.duration &&
                data.shortcut == right.data.shortcut && data.forward == right.data.forward &&
                data.backward == right.data.backward && data.id == right.data.id);
    }
};
}
}

#endif // QUERYEDGE_HPP
