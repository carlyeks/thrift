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

#include <fstream>
#include <iostream>
#include <sstream>

#include <stdlib.h>
#include <sys/stat.h>

#include "thrift/platform.h"
#include "thrift/generate/t_template_generator.h"
#include "thrift/generate/t_generator_registry.h"

using json = nlohmann::json;

/**
 * JSON code generator using templates.
 * This is a proof-of-concept to demonstrate template-based code generation.
 */
class t_json_template_generator : public t_template_generator {
public:
  t_json_template_generator(t_program* program,
                           const std::map<std::string, std::string>& parsed_options,
                           const std::string& option_string)
    : t_template_generator(program, "")
    , should_merge_includes_(false) {

    (void)option_string;

    for (auto iter = parsed_options.begin(); iter != parsed_options.end(); ++iter) {
      if (iter->first == "merge") {
        should_merge_includes_ = true;
      } else {
        throw "unknown option json_template:" + iter->first;
      }
    }

    out_dir_base_ = "gen-json-template";
  }

  ~t_json_template_generator() override = default;

  void init_generator() override {
    MKDIR(get_out_dir().c_str());
  }

  void close_generator() override {
    // Nothing to do
  }

  std::string display_name() const override {
    return "JSON (Template-Based)";
  }

  void generate_typedef(t_typedef* ttypedef) override {
    (void)ttypedef;
    // Typedefs are collected in generate_program()
  }

  void generate_enum(t_enum* tenum) override {
    (void)tenum;
    // Enums are collected in generate_program()
  }

  void generate_struct(t_struct* tstruct) override {
    (void)tstruct;
    // Structs are collected in generate_program()
  }

  void generate_service(t_service* tservice) override {
    (void)tservice;
    // Services are collected in generate_program()
  }

  void generate_program() override {
    init_generator();

    // Prepare all data for the template
    json data = prepare_program_data();

    // Render the main program template
    std::string output = render_template("json/program.json.j2", data);

    // Write to file
    std::string filename = program_->get_name() + ".json";
    write_output(filename, output);

    close_generator();
  }

protected:
  std::string get_default_template_dir() const override {
    // Use absolute path to templates directory
    return "/home/user/thrift/compiler/cpp/templates/";
  }

private:
  bool should_merge_includes_;

  json prepare_program_data() {
    json data;

    // Basic program info
    data["program_name"] = program_->get_name();
    data["merge_includes"] = should_merge_includes_;

    if (program_->has_doc()) {
      data["program_doc"] = program_->get_doc();
    }

    // Namespaces
    if (!should_merge_includes_) {
      data["namespaces"] = json::array();
      const std::map<std::string, std::string>& namespaces = program_->get_namespaces();
      for (auto& ns : namespaces) {
        json ns_obj;
        ns_obj["key"] = ns.first;
        ns_obj["value"] = ns.second;
        data["namespaces"].push_back(ns_obj);
      }

      // Includes
      data["includes"] = json::array();
      const std::vector<t_program*>& includes = program_->get_includes();
      for (auto inc : includes) {
        data["includes"].push_back(inc->get_name());
      }
    }

    // Enums
    data["enums"] = json::array();
    const std::vector<t_enum*>& enums = program_->get_enums();
    for (auto tenum : enums) {
      data["enums"].push_back(enum_to_json(tenum));
    }

    // Typedefs
    data["typedefs"] = json::array();
    const std::vector<t_typedef*>& typedefs = program_->get_typedefs();
    for (auto ttypedef : typedefs) {
      json typedef_obj;
      typedef_obj["name"] = ttypedef->get_name();
      typedef_obj["type"] = type_to_json(ttypedef->get_type());
      data["typedefs"].push_back(typedef_obj);
    }

    // Structs (including exceptions)
    data["structs"] = json::array();
    const std::vector<t_struct*>& structs = program_->get_structs();
    for (auto tstruct : structs) {
      data["structs"].push_back(struct_to_json(tstruct));
    }
    const std::vector<t_struct*>& xceptions = program_->get_xceptions();
    for (auto xception : xceptions) {
      data["structs"].push_back(struct_to_json(xception));
    }

    // Constants
    data["constants"] = json::array();
    const std::vector<t_const*>& constants = program_->get_consts();
    for (auto tconst : constants) {
      json const_obj;
      const_obj["name"] = tconst->get_name();
      const_obj["type"] = type_to_json(tconst->get_type());
      const_obj["value"] = const_value_to_json(tconst->get_value());
      data["constants"].push_back(const_obj);
    }

    // Services
    data["services"] = json::array();
    const std::vector<t_service*>& services = program_->get_services();
    for (auto tservice : services) {
      data["services"].push_back(service_to_json(tservice));
    }

    return data;
  }
};

THRIFT_REGISTER_GENERATOR(
    json_template,
    "JSON (Template-Based)",
    "    merge:           Generate a single JSON file with all included programs merged\n"
)
