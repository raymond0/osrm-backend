#include "engine/engine.hpp"
#include "engine/api/route_parameters.hpp"
#include "engine/engine_config.hpp"
#include "engine/status.hpp"

#include "engine/datafacade/process_memory_datafacade.hpp"
#include "engine/datafacade/shared_memory_datafacade.hpp"

#include "storage/shared_barriers.hpp"
#include "util/log.hpp"

#include <boost/assert.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>

#include <algorithm>
#include <fstream>
#include <memory>
#include <utility>
#include <vector>

namespace
{
// Abstracted away the query locking into a template function
// Works the same for every plugin.
template <typename ParameterT, typename PluginT, typename ResultT>
osrm::engine::Status
RunQuery(const std::unique_ptr<osrm::engine::DataWatchdog> &watchdog,
         const std::shared_ptr<osrm::engine::datafacade::BaseDataFacade> &facade,
         const ParameterT &parameters,
         PluginT &plugin,
         ResultT &result)
{
    if (watchdog)
    {
        BOOST_ASSERT(!facade);
        auto lock_and_facade = watchdog->GetDataFacade();

        return plugin.HandleRequest(lock_and_facade.second, parameters, result);
    }

    BOOST_ASSERT(facade);

    return plugin.HandleRequest(facade, parameters, result);
}

} // anon. ns

namespace osrm
{
namespace engine
{

Engine::Engine(const EngineConfig &config)
    : lock(config.use_shared_memory ? std::make_unique<storage::SharedBarriers>()
                                    : std::unique_ptr<storage::SharedBarriers>()),
      route_plugin(config.max_locations_viaroute),       //
      table_plugin(config.max_locations_distance_table), //
      nearest_plugin(config.max_results_nearest),        //
      trip_plugin(config.max_locations_trip),            //
      match_plugin(config.max_locations_map_matching),   //
      tile_plugin()                                      //

{
    if (config.use_shared_memory)
    {
        if (!DataWatchdog::TryConnect())
        {
            throw util::exception(
                std::string(
                    "No shared memory blocks found, have you forgotten to run osrm-datastore?") +
                SOURCE_REF);
        }

        watchdog = std::make_unique<DataWatchdog>();
        BOOST_ASSERT(watchdog);
    }
    else
    {
        if (!config.storage_config.IsValid())
        {
            throw util::exception("Invalid file paths given!" + SOURCE_REF);
        }
        immutable_data_facade =
            std::make_shared<datafacade::ProcessMemoryDataFacade>(config.storage_config);
    }
}

Status Engine::Route(const api::RouteParameters &params, util::json::Object &result) const
{
    return RunQuery(watchdog, immutable_data_facade, params, route_plugin, result);
}

Status Engine::Table(const api::TableParameters &params, util::json::Object &result) const
{
    return RunQuery(watchdog, immutable_data_facade, params, table_plugin, result);
}

Status Engine::Nearest(const api::NearestParameters &params, util::json::Object &result) const
{
    return RunQuery(watchdog, immutable_data_facade, params, nearest_plugin, result);
}

Status Engine::Trip(const api::TripParameters &params, util::json::Object &result) const
{
    return RunQuery(watchdog, immutable_data_facade, params, trip_plugin, result);
}

Status Engine::Match(const api::MatchParameters &params, util::json::Object &result) const
{
    return RunQuery(watchdog, immutable_data_facade, params, match_plugin, result);
}

Status Engine::Tile(const api::TileParameters &params, std::string &result) const
{
    return RunQuery(watchdog, immutable_data_facade, params, tile_plugin, result);
}

} // engine ns
} // osrm ns
