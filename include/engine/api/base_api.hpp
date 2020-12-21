#ifndef ENGINE_API_BASE_API_HPP
#define ENGINE_API_BASE_API_HPP

#include "engine/api/base_parameters.hpp"
#include "engine/api/flatbuffers/fbresult_generated.h"
#include "engine/datafacade/datafacade_base.hpp"

#include "engine/api/json_factory.hpp"
#include "engine/hint.hpp"
#include "util/coordinate_calculation.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/assert.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/transform.hpp>

#include <memory>
#include <vector>

namespace osrm
{
namespace engine
{
namespace api
{

static const constexpr char *INTERSECTION_DELIMITER = " / ";

class BaseAPI
{
  public:
    BaseAPI(const datafacade::BaseDataFacade &facade_, const BaseParameters &parameters_)
        : facade(facade_), parameters(parameters_)
    {
    }

    util::json::Array
    MakeWaypoints(const std::vector<PhantomNodeCandidates> &waypoint_candidates) const
    {
        BOOST_ASSERT(parameters.coordinates.size() > 0);
        BOOST_ASSERT(parameters.coordinates.size() == waypoint_candidates.size());

        util::json::Array waypoints;
        waypoints.values.resize(parameters.coordinates.size());

        boost::range::transform(
            waypoint_candidates,
            waypoints.values.begin(),
            [this](const PhantomNodeCandidates &candidates) { return MakeWaypoint(candidates); });
        return waypoints;
    }

    // FIXME: gcc 4.9 does not like MakeWaypoints to be protected
    // protected:
    util::json::Object MakeWaypoint(const PhantomNodeCandidates &candidates) const
    {
        // TODO: check forward/reverse
        const auto toName = [this](const auto &phantom) {
            return facade.GetNameForID(facade.GetNameIndex(phantom.forward_segment_id.id))
                .to_string();
        };
        std::string waypoint_name = boost::algorithm::join(
            candidates | boost::adaptors::transformed(toName), INTERSECTION_DELIMITER);

        if (parameters.generate_hints)
        {
            std::vector<Hint> hints(candidates.size());
            std::transform(
                candidates.begin(), candidates.end(), hints.begin(), [this](const auto &phantom) {
                    return Hint{phantom, facade.GetCheckSum()};
                });
            return json::makeWaypoint(SnappedLocation(candidates),
                                      util::coordinate_calculation::fccApproximateDistance(
                                          SnappedLocation(candidates), InputLocation(candidates)),
                                      waypoint_name,
                                      hints);
        }
        else
        {
            return json::makeWaypoint(SnappedLocation(candidates),
                                      util::coordinate_calculation::fccApproximateDistance(
                                          SnappedLocation(candidates), InputLocation(candidates)),
                                      waypoint_name);
        }
    }

    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<fbresult::Waypoint>>>
    MakeWaypoints(flatbuffers::FlatBufferBuilder *builder,
                  const std::vector<PhantomNodeCandidates> &waypoint_candidates) const
    {
        BOOST_ASSERT(parameters.coordinates.size() > 0);
        BOOST_ASSERT(parameters.coordinates.size() == waypoint_candidates.size());

        std::vector<flatbuffers::Offset<fbresult::Waypoint>> waypoints;
        waypoints.resize(parameters.coordinates.size());

        std::transform(waypoint_candidates.begin(),
                       waypoint_candidates.end(),
                       waypoints.begin(),
                       [this, builder](const PhantomNodeCandidates &candidates) {
                           return MakeWaypoint(builder, candidates)->Finish();
                       });
        return builder->CreateVector(waypoints);
    }

    // FIXME: gcc 4.9 does not like MakeWaypoints to be protected
    // protected:
    std::unique_ptr<fbresult::WaypointBuilder>
    MakeWaypoint(flatbuffers::FlatBufferBuilder *builder,
                 const PhantomNodeCandidates &candidates) const
    {

        const auto &snapped_location = SnappedLocation(candidates);
        auto location =
            fbresult::Position(static_cast<double>(util::toFloating(snapped_location.lon)),
                               static_cast<double>(util::toFloating(snapped_location.lat)));

        const auto toName = [this](const auto &phantom) {
            return facade.GetNameForID(facade.GetNameIndex(phantom.forward_segment_id.id))
                .to_string();
        };
        std::string waypoint_name = boost::algorithm::join(
            candidates | boost::adaptors::transformed(toName), INTERSECTION_DELIMITER);
        auto name_string = builder->CreateString(waypoint_name);

        flatbuffers::Offset<flatbuffers::String> hint_string;
        if (parameters.generate_hints)
        {
            std::vector<Hint> hints(candidates.size());
            std::transform(
                candidates.begin(), candidates.end(), hints.begin(), [this](const auto &phantom) {
                    return Hint{phantom, facade.GetCheckSum()};
                });
            hint_string = builder->CreateString(ToBase64(hints));
        }

        auto waypoint = std::make_unique<fbresult::WaypointBuilder>(*builder);
        waypoint->add_location(&location);
        waypoint->add_distance(util::coordinate_calculation::fccApproximateDistance(
            snapped_location, InputLocation(candidates)));
        waypoint->add_name(name_string);
        if (parameters.generate_hints)
        {
            waypoint->add_hint(hint_string);
        }
        return waypoint;
    }

    const datafacade::BaseDataFacade &facade;
    const BaseParameters &parameters;
};

} // namespace api
} // namespace engine
} // namespace osrm

#endif
