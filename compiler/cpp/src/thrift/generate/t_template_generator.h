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

#ifndef T_TEMPLATE_GENERATOR_H
#define T_TEMPLATE_GENERATOR_H

#include "thrift/generate/t_generator.h"

// Include third-party libraries (relative to compiler/cpp)
#include "../../../third_party/nlohmann/json.hpp"
#include "../../../third_party/inja.hpp"

#include <string>
#include <map>
#include <vector>

using json = nlohmann::json;

/**
 * Base class for template-based code generators.
 *
 * This class provides infrastructure for using Jinja2-style templates
 * to generate code, as an alternative to the traditional programmatic
 * string concatenation approach.
 */
class t_template_generator : public t_generator {
public:
  t_template_generator(t_program* program,
                      const std::string& template_dir = "")
    : t_generator(program)
    , template_dir_(template_dir.empty() ? get_default_template_dir() : template_dir)
    , env_(template_dir_) {

    // Configure inja environment
    env_.set_trim_blocks(true);
    env_.set_lstrip_blocks(true);
  }

  virtual ~t_template_generator() override = default;

protected:
  /**
   * Render a template with the given data.
   *
   * @param template_name Name of template file (relative to template_dir)
   * @param data JSON data to pass to template
   * @return Rendered string output
   */
  std::string render_template(const std::string& template_name, const json& data) {
    try {
      return env_.render_file(template_name, data);
    } catch (const std::exception& e) {
      std::cerr << "Template rendering error (" << template_name << "): " << e.what() << std::endl;
      throw;
    }
  }

  /**
   * Load a template from file.
   *
   * @param template_name Name of template file
   * @return Loaded template
   */
  inja::Template load_template(const std::string& template_name) {
    return env_.parse_template(template_name);
  }

  /**
   * Convert a Thrift type to JSON representation for templates.
   */
  json type_to_json(t_type* ttype);

  /**
   * Convert a Thrift field to JSON representation for templates.
   */
  json field_to_json(t_field* tfield);

  /**
   * Convert a Thrift enum to JSON representation for templates.
   */
  json enum_to_json(t_enum* tenum);

  /**
   * Convert a Thrift struct to JSON representation for templates.
   */
  json struct_to_json(t_struct* tstruct);

  /**
   * Convert a Thrift service to JSON representation for templates.
   */
  json service_to_json(t_service* tservice);

  /**
   * Convert a Thrift function to JSON representation for templates.
   */
  json function_to_json(t_function* tfunc);

  /**
   * Convert a Thrift constant value to JSON representation for templates.
   */
  json const_value_to_json(t_const_value* tvalue);

  /**
   * Prepare base data that's common across all templates.
   */
  json prepare_base_data() {
    json data;
    data["program_name"] = program_->get_name();
    data["program_path"] = program_->get_path();
    data["program_namespace"] = program_->get_namespace();
    return data;
  }

  /**
   * Get the default template directory for this generator.
   * Subclasses should override this to specify their template location.
   */
  virtual std::string get_default_template_dir() const {
    return "templates/";
  }

  /**
   * Write rendered output to a file.
   */
  void write_output(const std::string& filename, const std::string& content) {
    ofstream_with_content_based_conditional_update out;
    std::string full_path = get_out_dir() + filename;
    out.open(full_path);
    out << content;
    out.close();
  }

  // Template directory and environment
  std::string template_dir_;
  inja::Environment env_;
};

#endif // T_TEMPLATE_GENERATOR_H
