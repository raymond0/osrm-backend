#ifndef PHANTOM_LOOKUP_HPP
#define PHANTOM_LOOKUP_HPP

#include "engine/api/route_api.hpp"
#include "engine/datafacade/datafacade_base.hpp"
#include "engine/plugins/plugin_base.hpp"

#include "engine/routing_algorithms/alternative_path.hpp"
#include "engine/routing_algorithms/direct_shortest_path.hpp"
#include "engine/routing_algorithms/shortest_path.hpp"
#include "engine/search_engine_data.hpp"
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

class PhantomLoopupPlugin final : public BasePlugin
{
  private:
    mutable SearchEngineData heaps;
    mutable routing_algorithms::ShortestPathRouting shortest_path;
    mutable routing_algorithms::AlternativeRouting alternative_path;
    mutable routing_algorithms::DirectShortestPathRouting direct_shortest_path;
    const int max_locations_viaroute;

  public:
    explicit PhantomLoopupPlugin(int max_locations_viaroute);

    Status HandleRequest(const std::shared_ptr<const osrm::engine::datafacade::BaseDataFacade> &immutable_facade,
                         const osrm::util::FloatCoordinate &coordinate,
                         osrm::engine::PhantomNode &result) const;
};
}
}
}

#endif // PHANTOM_LOOKUP_HPP
