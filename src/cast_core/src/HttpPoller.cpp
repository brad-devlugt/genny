// Copyright 2022-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cast_core/actors/HttpPoller.hpp>

#include <memory>
#include <chrono>
#include <thread>
#include <yaml-cpp/yaml.h>

#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <simple-beast-client/httpclient.hpp>
#include <value_generators/DocumentGenerator.hpp>


namespace genny::actor {

struct HttpPoller::PhaseConfig {
    std::string route;
    PhaseConfig(PhaseContext& phaseContext, ActorId id)
        : route{phaseContext["Route"].to<std::string>()}
        {
        }
};

void HttpPoller::run() {
    for (auto&& config : _loop) {
        if (!config.isNop()) {
            BOOST_LOG_TRIVIAL(info) << "Polling endpoint at " << _url << config->route;
        }
        for (const auto&& _ : config) {
            try {
                long lagtime = std::numeric_limits<long>::max();
                bool can_commit = false;

                // Poll until lag is < 2 minutes and we can run a commit
                while (lagtime > 120 || !can_commit) {
                    auto endpoint = _url + config->route;
                    boost::asio::io_context ioContext;
                    auto client = std::make_shared<simple_http::get_client>(
                        ioContext,
                        [&](simple_http::empty_body_request& /*req*/,
                            simple_http::string_body_response& resp) {
                            auto resp_parsed = bsoncxx::from_json(resp.body());
                            auto resp_bson = resp_parsed.view();

                            auto progress_node = resp_bson.find("progress");

                            if(progress_node == resp_bson.end()) {
                                BOOST_LOG_TRIVIAL(error) <<"Could not parse response " << resp.body();
                                return;
                            }
                            lagtime = (*progress_node)["lagTimeSeconds"].get_int32();
                            can_commit = (*progress_node)["canCommit"].get_bool();

                            BOOST_LOG_TRIVIAL(info) <<"Response = " << resp.body();
                        });
                    client->get(simple_http::url{endpoint});
                    ioContext.run();  // blocks until requests are complete.

                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }

                BOOST_LOG_TRIVIAL(info) <<"Stopped polling because lagtime = " << lagtime << " and canCommit = " <<can_commit;
            } catch (const std::exception& x) {
                BOOST_LOG_TRIVIAL(error) <<"Encountered exception while polling endpoint: " << x.what();
            }
        }
    }
}

HttpPoller::HttpPoller(genny::ActorContext& context)
    : Actor{context},
      _client{context.client()},
      _url{context.workload()["HttpClients"]["URL"].to<std::string>()},
      _loop{context, HttpPoller::id()}
      {
      }

namespace {
auto registerHttpPoller = Cast::registerDefault<HttpPoller>();
}  // namespace
}  // namespace genny::actor
