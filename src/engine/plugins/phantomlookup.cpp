#include "engine/plugins/phantomlookup.hpp"
#include "engine/api/route_api.hpp"
#include "engine/datafacade/datafacade_base.hpp"
#include "engine/status.hpp"

#include "util/for_each_pair.hpp"
#include "util/integer_range.hpp"
#include "util/json_container.hpp"

#include <cstdlib>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace osrm
{
namespace engine
{
namespace plugins
{

PhantomLoopupPlugin::PhantomLoopupPlugin(int max_locations_viaroute)
    : shortest_path(heaps), alternative_path(heaps), direct_shortest_path(heaps),
      max_locations_viaroute(max_locations_viaroute)
{
}
    

Status PhantomLoopupPlugin::HandleRequest(const std::shared_ptr<const osrm::engine::datafacade::BaseDataFacade> &immutable_facade,
                                          const osrm::util::FloatCoordinate &coordinate, osrm::engine::PhantomNode &result) const
{
    util::json::Object json_result;
    
    if (!coordinate.IsValid())
    {
        return Error("InvalidValue", "Invalid coordinate value.", json_result);
    }

    api::BaseParameters route_parameters;
    route_parameters.coordinates.emplace_back(coordinate);
    
    auto phantom_node_pairs = GetPhantomNodes(*immutable_facade, route_parameters);
    if (phantom_node_pairs.size() != route_parameters.coordinates.size())
    {
        return Error("NoSegment",
                     std::string("Could not find a matching segment for coordinate ") +
                         std::to_string(phantom_node_pairs.size()),
                     json_result);
    }
    BOOST_ASSERT(phantom_node_pairs.size() == route_parameters.coordinates.size());

    auto snapped_phantoms = SnapPhantomNodes(phantom_node_pairs);
    
    BOOST_ASSERT(snapped_phantoms.size() == 1);
    result = snapped_phantoms[0];

    return Status::Ok;
}
}
}
}
