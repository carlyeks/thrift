/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/**
 * Stream Test - Test cases for stream support in Thrift
 * Based on THRIFT-1948 proposal
 */

namespace cpp streamtest
namespace java streamtest
namespace py streamtest

// Example 1: Lock Server with server-side streaming
struct Update {
  1: required string lock_handle,
  2: required i64 owner
}

service LockService {
  /**
   * Server streaming: returns a stream of updates for locks matching the prefix
   */
  stream<Update> updates_for(1: string prefix)
}

// Example 2: Query Provider with server-side streaming
struct Result {
  1: required i64 id,
  2: required string data
}

service QueryProvider {
  /**
   * Server streaming: run query and stream results as they come in
   */
  stream<Result> run_query(1: string query)
}

// Example 3: Bidirectional streaming
service RandomNumberService {
  /**
   * Bidirectional streaming: client sends stream of max values,
   * server sends back stream of random numbers
   */
  stream<i64> random_numbers(1: stream<i64> max)
}

// Example 4: Client streaming
service DataAggregator {
  /**
   * Client streaming: client sends stream of values,
   * server returns aggregated result
   */
  i64 sum_values(1: stream<i64> values)
}
