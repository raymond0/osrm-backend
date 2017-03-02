#ifndef NEAREST_HPP
#define NEAREST_HPP

#include "engine/api/nearest_parameters.hpp"
#include "engine/plugins/plugin_base.hpp"
#include "osrm/json_container.hpp"

namespace osrm
{
namespace engine
{
namespace plugins
{

class NearestPlugin final : public BasePlugin
{
  public:
    explicit NearestPlugin(const int max_results);

    Status HandleRequest(const std::shared_ptr<const datafacade::BaseDataFacade> facade,
                         const api::NearestParameters &params,
                         util::json::Object &result) const;

  private:
    const int max_results;
};
}
}
}

#endif /* NEAREST_HPP */
