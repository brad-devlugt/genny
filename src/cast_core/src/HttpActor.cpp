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

#include <cast_core/actors/HttpActor.hpp>

#include <memory>

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

class Operation {
public:
    Operation() {}
    virtual ~Operation() {}
    virtual void run(simple_http::url endpoint) = 0;
};

class PostOperation : public Operation {
public:
    PostOperation(PhaseContext& phaseContext) {

    }

    void run(simple_http::url endpoint) override {
        boost::asio::io_context ioContext;
        auto client = std::make_shared<simple_http::post_client>(
            ioContext,
            [](simple_http::string_body_request& /*req*/,
                simple_http::string_body_response& resp) {});

        BOOST_LOG_TRIVIAL(info) << "Sending post to";

        client->post(endpoint, "{\"foo\": \"bar\"}", "application/json");

        ioContext.run();  // blocks until requests are complete.
    }
};

class GetOperation : public Operation {
public:
    GetOperation() {
    }

    void run(simple_http::url endpoint) override {
        boost::asio::io_context ioContext;
        auto client = std::make_shared<simple_http::get_client>(
            ioContext,
            [](simple_http::empty_body_request& /*req*/,
                simple_http::string_body_response& resp) {
                // noop for successful HTTP
            });

        client->get(endpoint);
        ioContext.run();  // blocks until requests are complete.
    }
};

struct HttpActor::PhaseConfig {
    std::string route;
    std::unique_ptr<Operation> operation;
    // std::optional<DocumentGenerator> documentExpr;
    PhaseConfig(PhaseContext& phaseContext, ActorId id)
        : route{phaseContext["Route"].to<std::string>()}
        {
            auto operationType = phaseContext["Operation"].to<std::string>();
            if (operationType == "GET") {
                operation = std::make_unique<GetOperation>();
            }
            else if (operationType == "POST") {
                operation = std::make_unique<PostOperation>(phaseContext);
            }
        }
};

//
// Genny spins up a thread for each Actor instance. The `Threads:` configuration
// tells Genny how many such instances to create. See further documentation in
// the `Actor.hpp` file.
//
void HttpActor::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            auto requests = _totalRequests.start();
            auto endpoint = _url + config->route;
            BOOST_LOG_TRIVIAL(info) << "Sending requests to " << endpoint;

            try {
                config->operation->run(simple_http::url{endpoint});
                requests.success();
            } catch (mongocxx::operation_exception& e) {
                requests.failure();
                //
                // MongoException lets you include a "causing" bson document in the
                // exception message for help debugging.
                //
                // BOOST_THROW_EXCEPTION(MongoException(e, document.view()));
            } catch (...) {
                requests.failure();
                throw std::current_exception();
            }
        }
    }
}

HttpActor::HttpActor(genny::ActorContext& context)
    : Actor{context},
      _totalRequests{context.operation("Requests", HttpActor::id())},
      _client{context.client()},
      _url{context.workload()["HttpClients"]["URL"].to<std::string>()},
      _loop{context, HttpActor::id()}
      {
      }

namespace {
auto registerHttpActor = Cast::registerDefault<HttpActor>();
}  // namespace
}  // namespace genny::actor
