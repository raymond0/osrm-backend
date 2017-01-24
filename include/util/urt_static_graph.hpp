#ifndef URT_STATIC_GRAPH_HPP
#define URT_STATIC_GRAPH_HPP

#include "util/integer_range.hpp"
#include "util/percent.hpp"
#include "util/shared_memory_vector_wrapper.hpp"
#include "util/typedefs.hpp"

#include <boost/assert.hpp>

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "urt_datatypes.hpp"


namespace osrm
{
namespace util
{

class UrtStaticGraph
{
  public:
    using NodeIterator = NodeID;
    using EdgeIterator = NodeID;
    using EdgeRange = range<EdgeIterator>;


    struct NodeArrayEntry
    {
        // index of the first edge
        EdgeIterator first_edge;
    };

    
    EdgeRange GetAdjacentEdgeRange(const NodeID node)
    {
        NodeID ourNode = node - m_node_start_offset;
        
        if (!m_hsgrStream.good())
        {
            m_hsgrStream.clear(std::ios::goodbit);
            std::cout << "Resetting stale filestream\n";
        }
        
        m_hsgrStream.seekg( m_nodes_start_offset +  ( ourNode * sizeof( NodeID ) ) );
        NodeID nodes[2];
        m_hsgrStream.read((char *)&(nodes), sizeof(NodeID) * 2);
        
        return irange(nodes[0], nodes[1]);
    }
    
    
    UrtStaticGraph( const std::string &path )
    {
        m_hsgrStream.open( path, std::ifstream::in | std::ifstream::binary );
        
        FingerPrint fingerprint;
        m_hsgrStream.read(reinterpret_cast<char *>(&fingerprint), sizeof(FingerPrint));
        
        // ToDo - add back in?
        /*if ( ! fingerprint.IsMagicNumberOK() )
        {
            throw util::exception( "Static Graph magic number was incorrect" );
        }*/
        
        m_hsgrStream.read(reinterpret_cast<char *>(&m_check_sum), sizeof(unsigned));
        m_hsgrStream.read(reinterpret_cast<char *>(&m_node_start_offset), sizeof(unsigned));
        m_hsgrStream.read(reinterpret_cast<char *>(&m_number_of_nodes), sizeof(unsigned));
        BOOST_ASSERT_MSG(0 != m_number_of_nodes, "number of nodes is zero");
        m_hsgrStream.read(reinterpret_cast<char *>(&m_number_of_edges), sizeof(unsigned));
        
        m_nodes_start_offset = sizeof(FingerPrint) + ( sizeof(unsigned) * 4 );
        size_t nodes_size = m_number_of_nodes * sizeof( NodeArrayEntry );
        size_t edges_size = m_number_of_edges * sizeof( EdgeArrayEntryApp );
        size_t total_size = m_nodes_start_offset + nodes_size + edges_size;
        
        m_edges_start_offset = m_nodes_start_offset + nodes_size;
        
        m_hsgrStream.seekg( 0, m_hsgrStream.end );
        size_t length = (size_t) m_hsgrStream.tellg();
        
        assert ( total_size == length );
    }


    typedef struct NodeRange
    {
        NodeID start;
        NodeID end;
        NodeRange( NodeID s, NodeID e ) { start = s; end = e; }
    } NodeRange;
    
    
    NodeRange RangeOfGraph()
    {
        return NodeRange( m_node_start_offset, m_node_start_offset + m_number_of_nodes );
    }
    
    
    inline bool NodeInRange(NodeID nodeId)
    {
        return m_node_start_offset <= nodeId && nodeId <= m_node_start_offset + m_number_of_nodes;
    }
    
    
    void GetEdge(const EdgeIterator e, EdgeArrayEntryApp &edgesEntry )
    {
        if (!m_hsgrStream.good())
        {
            m_hsgrStream.clear(std::ios::goodbit);
            std::cout << "Resetting stale filestream\n";
        }
        
        m_hsgrStream.seekg( m_edges_start_offset +  ( e * sizeof( EdgeArrayEntryApp ) ) );
        m_hsgrStream.read((char *)&(edgesEntry), sizeof(EdgeArrayEntryApp));
    }
    
    
    void GetAdjacentEdges( const NodeID node, EdgeArray &edges )
    {
        BOOST_ASSERT( NodeInRange(node) );
        
        if ( node == SPECIAL_NODEID )
            return;
        
        const auto edgeRange = GetAdjacentEdgeRange(node);
        
        for ( auto edgeId : edgeRange )
        {
            if ( edgeId == 2147483647 || edgeId == SPECIAL_NODEID )
                continue;
            
            EdgeArrayEntryApp edgeData;
            GetEdge( edgeId, edgeData );
            
            if ( edgeData.shortcut && ( ( edgeData.middleNodeId == 2147483647 ) || ( edgeData.middleNodeId == SPECIAL_NODEID ) ) )
                continue;
            
            if ( edgeData.target == SPECIAL_NODEID || edgeData.target == 2147483647 )
                continue;
                        
            edges.emplace_back( edgeData );
        }
    }


    /**
     * Finds the edge with the smallest `.weight` going from `from` to `to`
     * @param from the source node ID
     * @param to the target node ID
     * @param filter a functor that returns a `bool` that determines whether an edge should be
     * tested or not.
     *   Takes `EdgeData` as a parameter.
     * @return the ID of the smallest edge if any were found that satisfied *filter*, or
     * `SPECIAL_EDGEID` if no
     *   matching edge is found.
     */
    bool
    FindSmallestForwardEdge(const NodeIterator from, const NodeIterator to, EdgeArrayEntryApp &smallest_edge)
    {
        EdgeWeight smallest_weight = INVALID_EDGE_WEIGHT;
        bool found_edge = false;
        
        EdgeArray edges;
        GetAdjacentEdges( from, edges );
        for (auto edge : edges)
        {
            if (edge.target == to && edge.weight < smallest_weight &&
                edge.forward)
            {
                smallest_edge = edge;
                smallest_weight = edge.weight;
                found_edge = true;
            }
        }
        return found_edge;
    }
    
    
    bool
    FindSmallestBackwardEdge(const NodeIterator from, const NodeIterator to, EdgeArrayEntryApp &smallest_edge)
    {
        EdgeWeight smallest_weight = INVALID_EDGE_WEIGHT;
        bool found_edge = false;
        
        EdgeArray edges;
        GetAdjacentEdges( from, edges );
        for (auto edge : edges)
        {
            if (edge.target == to && edge.weight < smallest_weight &&
                edge.backward)
            {
                smallest_edge = edge;
                smallest_weight = edge.weight;
                found_edge = true;
            }
        }
        return found_edge;
    }
    

  private:
    NodeID m_node_start_offset;
    std::ifstream m_hsgrStream;

    unsigned m_check_sum;
    NodeIterator m_number_of_nodes;
    EdgeIterator m_number_of_edges;
    
    size_t m_nodes_start_offset;
    size_t m_edges_start_offset;
};
}
}

#endif // URT_STATIC_GRAPH_HPP
