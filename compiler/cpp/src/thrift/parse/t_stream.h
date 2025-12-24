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

#ifndef T_STREAM_H
#define T_STREAM_H

#include "thrift/parse/t_container.h"
#include "thrift/parse/t_list.h"

/**
 * A stream is a container type that represents a sequence of values
 * that can be transmitted between client and server without buffering
 * the entire sequence in memory.
 *
 * For backward compatibility with languages that do not support streaming,
 * streams can be treated as lists during code generation.
 */
class t_stream : public t_container {
public:
  t_stream(t_type* elem_type) : elem_type_(elem_type) {}

  t_type* get_elem_type() const { return elem_type_; }

  bool is_stream() const override { return true; }

  void validate() const override {
#ifndef ALLOW_EXCEPTIONS_AS_TYPE
    if( get_elem_type()->get_true_type()->is_xception()) {
      failure("exception type \"%s\" cannot be used inside a stream", get_elem_type()->get_name().c_str());
    }
#endif
  }

  /**
   * Get a list representation of this stream for backward compatibility.
   * This allows code generators for languages without stream support to
   * treat stream<T> as list<T>.
   */
  t_list* as_list() const {
    return new t_list(elem_type_);
  }

private:
  t_type* elem_type_;
};

#endif
