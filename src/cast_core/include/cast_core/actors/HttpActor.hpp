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

#ifndef HEADER_59D36525_C78E_47A5_AF99_FBB3D8E6D11B_INCLUDED
#define HEADER_59D36525_C78E_47A5_AF99_FBB3D8E6D11B_INCLUDED

#include <string_view>

#include <mongocxx/pool.hpp>

#include <gennylib/Actor.hpp>
#include <gennylib/PhaseLoop.hpp>
#include <gennylib/context.hpp>

#include <metrics/metrics.hpp>

namespace genny::actor {

/**
 * Indicate what the Actor does and give an example yaml configuration.
 * Markdown is supported in all docstrings so you could list an example here:
 *
 * ```yaml
 * SchemaVersion: 2017-07-01
 * Actors:
 * - Name: HttpActor
 *   Type: HttpActor
 *   Phases:
 *   - Document: foo
 * ```
 *
 * Or you can fill out the generated workloads/docs/HttpActor.yml
 * file with extended documentation. If you do this, please mention
 * that extended documentation can be found in the docs/HttpActor.yml
 * file.
 *
 * Owner: TODO (which github team owns this Actor?)
 */
class HttpActor : public Actor {
public:

    explicit HttpActor(ActorContext& context);

    ~HttpActor() = default;

    void run() override;

    static std::string_view defaultName() {
        return "HttpActor";
    }

private:

    //
    // Each Actor can get its own connection from a number of connection-pools
    // configured in the `Clients` section of the workload yaml. Since each
    // Actor is its own thread, there is no need for you to worry about
    // thread-safety in your Actor's internals. You likely do not need to have
    // more than one connection open per Actor instance but of course you do
    // you™️.
    //
    mongocxx::pool::entry _client;

    //
    // Your Actor can record an arbitrary number of different metrics which are
    // tracked by the `metrics::Operation` type. This skeleton Actor does a
    // simple `insert_one` operation so the name of this property corresponds
    // to that. Rename this and/or add additional `metrics::Operation` types if
    // you do more than one thing. In addition, you may decide that you want
    // to support recording different metrics in different Phases in which case
    // you can remove this from the class and put it in the `PhaseConfig` struct,
    // discussed in the .cpp implementation.
    //
    genny::metrics::Operation _totalRequests;

    std::string _url;

    //
    // The below struct and PhaseConfig are discussed in depth in the
    // `HttpActor.cpp` implementation file.
    //
    // Note that since `PhaseLoop` uses pointers internally you don't need to
    // define anything about this type in this header, it just needs to be
    // pre-declared. The `@private` docstring is to prevent doxygen from
    // showing your Actor's internals on the genny API docs website.
    //
    /** @private */
    struct PhaseConfig;
    PhaseLoop<PhaseConfig> _loop;
};

}  // namespace genny::actor

#endif  // HEADER_59D36525_C78E_47A5_AF99_FBB3D8E6D11B_INCLUDED
