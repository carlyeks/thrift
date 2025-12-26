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

#include "thrift/generate/t_template_generator.h"
#include "thrift/parse/t_program.h"
#include "thrift/parse/t_struct.h"
#include "thrift/parse/t_enum.h"
#include "thrift/parse/t_service.h"
#include "thrift/parse/t_field.h"
#include "thrift/parse/t_function.h"
#include "thrift/parse/t_const.h"
#include "thrift/parse/t_const_value.h"
#include "thrift/parse/t_typedef.h"

using json = nlohmann::json;

std::string get_type_name(t_type* ttype) {
  if (ttype->is_base_type()) {
    t_base_type* base_type = (t_base_type*)ttype;
    switch (base_type->get_base()) {
      case t_base_type::TYPE_VOID:   return "void";
      case t_base_type::TYPE_STRING:
        return base_type->is_binary() ? "binary" : "string";
      case t_base_type::TYPE_BOOL:   return "bool";
      case t_base_type::TYPE_I8:     return "i8";
      case t_base_type::TYPE_I16:    return "i16";
      case t_base_type::TYPE_I32:    return "i32";
      case t_base_type::TYPE_I64:    return "i64";
      case t_base_type::TYPE_DOUBLE: return "double";
      default: return "unknown";
    }
  } else if (ttype->is_enum()) {
    return "enum";
  } else if (ttype->is_struct()) {
    return "struct";
  } else if (ttype->is_xception()) {
    return "exception";
  } else if (ttype->is_container()) {
    if (ttype->is_list()) {
      return "list";
    } else if (ttype->is_set()) {
      return "set";
    } else if (ttype->is_map()) {
      return "map";
    }
  } else if (ttype->is_typedef()) {
    return "typedef";
  }
  return "unknown";
}

json t_template_generator::type_to_json(t_type* ttype) {
  json result;

  result["name"] = ttype->get_name();
  result["type"] = get_type_name(ttype);

  // Type classification flags
  result["is_base_type"] = ttype->is_base_type();
  result["is_container"] = ttype->is_container();
  result["is_string"] = ttype->is_string();
  result["is_binary"] = ttype->is_binary();
  result["is_bool"] = ttype->is_bool();
  result["is_enum"] = ttype->is_enum();
  result["is_struct"] = ttype->is_struct();
  result["is_xception"] = ttype->is_xception();
  result["is_list"] = ttype->is_list();
  result["is_set"] = ttype->is_set();
  result["is_map"] = ttype->is_map();
  result["is_typedef"] = ttype->is_typedef();

  // Container type details
  if (ttype->is_list()) {
    t_list* tlist = (t_list*)ttype;
    result["elem_type"] = type_to_json(tlist->get_elem_type());
  } else if (ttype->is_set()) {
    t_set* tset = (t_set*)ttype;
    result["elem_type"] = type_to_json(tset->get_elem_type());
  } else if (ttype->is_map()) {
    t_map* tmap = (t_map*)ttype;
    result["key_type"] = type_to_json(tmap->get_key_type());
    result["val_type"] = type_to_json(tmap->get_val_type());
  }

  return result;
}

json t_template_generator::field_to_json(t_field* tfield) {
  json result;

  result["name"] = tfield->get_name();
  result["key"] = tfield->get_key();
  result["type"] = type_to_json(tfield->get_type());

  // Field properties
  result["required"] = tfield->get_req() == t_field::T_REQUIRED;
  result["optional"] = tfield->get_req() == t_field::T_OPTIONAL;

  // Default value
  if (tfield->get_value()) {
    result["default_value"] = const_value_to_json(tfield->get_value());
  } else {
    result["default_value"] = nullptr;
  }

  // Documentation
  if (!tfield->get_doc().empty()) {
    result["doc"] = tfield->get_doc();
  }

  return result;
}

json t_template_generator::enum_to_json(t_enum* tenum) {
  json result;

  result["name"] = tenum->get_name();

  // Documentation
  if (!tenum->get_doc().empty()) {
    result["doc"] = tenum->get_doc();
  }

  // Enum values
  json values = json::array();
  const std::vector<t_enum_value*>& constants = tenum->get_constants();
  for (auto constant : constants) {
    json val;
    val["name"] = constant->get_name();
    val["value"] = constant->get_value();
    if (!constant->get_doc().empty()) {
      val["doc"] = constant->get_doc();
    }
    values.push_back(val);
  }
  result["values"] = values;

  return result;
}

json t_template_generator::struct_to_json(t_struct* tstruct) {
  json result;

  result["name"] = tstruct->get_name();
  result["is_xception"] = tstruct->is_xception();
  result["is_union"] = tstruct->is_union();

  // Documentation
  if (!tstruct->get_doc().empty()) {
    result["doc"] = tstruct->get_doc();
  }

  // Fields
  json fields = json::array();
  const std::vector<t_field*>& members = tstruct->get_members();
  for (auto member : members) {
    fields.push_back(field_to_json(member));
  }
  result["fields"] = fields;

  return result;
}

json t_template_generator::function_to_json(t_function* tfunc) {
  json result;

  result["name"] = tfunc->get_name();
  result["return_type"] = type_to_json(tfunc->get_returntype());
  result["oneway"] = tfunc->is_oneway();

  // Documentation
  if (!tfunc->get_doc().empty()) {
    result["doc"] = tfunc->get_doc();
  }

  // Arguments
  json args = json::array();
  const std::vector<t_field*>& arglist = tfunc->get_arglist()->get_members();
  for (auto arg : arglist) {
    args.push_back(field_to_json(arg));
  }
  result["args"] = args;

  // Exceptions
  json exceptions = json::array();
  t_struct* xceptions = tfunc->get_xceptions();
  if (xceptions) {
    const std::vector<t_field*>& xlist = xceptions->get_members();
    for (auto xception : xlist) {
      exceptions.push_back(field_to_json(xception));
    }
  }
  result["exceptions"] = exceptions;

  return result;
}

json t_template_generator::service_to_json(t_service* tservice) {
  json result;

  result["name"] = tservice->get_name();

  // Documentation
  if (!tservice->get_doc().empty()) {
    result["doc"] = tservice->get_doc();
  }

  // Extends
  if (tservice->get_extends()) {
    result["extends"] = tservice->get_extends()->get_name();
  } else {
    result["extends"] = nullptr;
  }

  // Functions
  json functions = json::array();
  const std::vector<t_function*>& func_list = tservice->get_functions();
  for (auto function : func_list) {
    functions.push_back(function_to_json(function));
  }
  result["functions"] = functions;

  return result;
}

json t_template_generator::const_value_to_json(t_const_value* tvalue) {
  json result;

  switch (tvalue->get_type()) {
    case t_const_value::CV_INTEGER:
      result = tvalue->get_integer();
      break;
    case t_const_value::CV_DOUBLE:
      result = tvalue->get_double();
      break;
    case t_const_value::CV_STRING:
      result = tvalue->get_string();
      break;
    case t_const_value::CV_MAP: {
      json map_result = json::object();
      const std::map<t_const_value*, t_const_value*, t_const_value::value_compare>& map_val = tvalue->get_map();
      for (auto& pair : map_val) {
        // Convert key to string for JSON object keys
        std::string key;
        if (pair.first->get_type() == t_const_value::CV_STRING) {
          key = pair.first->get_string();
        } else if (pair.first->get_type() == t_const_value::CV_INTEGER) {
          key = std::to_string(pair.first->get_integer());
        }
        map_result[key] = const_value_to_json(pair.second);
      }
      result = map_result;
      break;
    }
    case t_const_value::CV_LIST: {
      json list_result = json::array();
      const std::vector<t_const_value*>& list_val = tvalue->get_list();
      for (auto item : list_val) {
        list_result.push_back(const_value_to_json(item));
      }
      result = list_result;
      break;
    }
    default:
      result = nullptr;
  }

  return result;
}
