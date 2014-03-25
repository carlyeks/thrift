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

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <list>

#include <stdlib.h>
#include <sys/stat.h>
#include <sstream>
#include "t_generator.h"
#include "platform.h"

using std::map;
using std::ofstream;
using std::ostringstream;
using std::pair;
using std::string;
using std::stringstream;
using std::vector;

static const string endl = "\n";  // avoid ostream << std::endl flushes

/**
 * Graphviz code generator
 */
class t_scala_generator : public t_generator {
  public:
    t_scala_generator(
        t_program* program,
        const std::map<std::string, std::string>& parsed_options,
        const std::string& option_string)
      : t_generator(program)
    {
      (void) parsed_options;
      (void) option_string;
      out_dir_base_ = "gen-scala";
    }

    /**
     * Init and end of generator
     */
    void init_generator();
    void close_generator();

    /**
     * Program-level generation functions
     */
    void generate_typedef (t_typedef*  ttypedef);
    void generate_enum    (t_enum*     tenum);
    void generate_const   (t_const*    tconst);
    void generate_struct  (t_struct*   tstruct);
    void generate_service (t_service*  tservice);

  protected:
    /**
     * Helpers
     */
    void print_type(t_type* ttype, string struct_field_ref);
    void print_const_value(t_type* type, t_const_value* tvalue);

  private:
    std::ofstream f_out_;
    std::ofstream package_file_;
    std::string package_name_;
    std::string package_dir_;
};

void t_scala_generator::init_generator() {
  // Make output directory
  MKDIR(get_out_dir().c_str());
  package_name_ = program_->get_namespace("java");

  string dir = package_name_;
  string subdir = get_out_dir();
  string::size_type loc;
  while ((loc = dir.find(".")) != string::npos) {
    subdir = subdir + "/" + dir.substr(0, loc);
    MKDIR(subdir.c_str());
    dir = dir.substr(loc+1);
  }
  if (dir.size() > 0) {
    subdir = subdir + "/" + dir;
    MKDIR(subdir.c_str());
  }
  
  string package_file_name = subdir + "/" + dir + ".scala";
  package_file_.open(package_file_name.c_str());
  package_dir_ = subdir;
}

void t_scala_generator::close_generator() {
}

void t_scala_generator::generate_typedef (t_typedef* ttypedef) {
  indent(package_file_) << "type " << ttypedef->get_symbolic() << " = " << ttypedef->get_type()->get_name() << endl;
}

void t_scala_generator::generate_enum (t_enum* tenum) {
  string name = tenum->get_name();
  f_out_ << "node [fillcolor=white];" << endl;
  f_out_ << name << " [label=\"enum " << escape_string(name);

  vector<t_enum_value*> values = tenum->get_constants();
  vector<t_enum_value*>::iterator val_iter;
  for (val_iter = values.begin(); val_iter != values.end(); ++val_iter) {
    f_out_ << '|' << (*val_iter)->get_name();
    f_out_ << " = ";
    f_out_ << (*val_iter)->get_value();
  }

  f_out_ << "\"];" << endl;
}

void t_scala_generator::generate_const (t_const* tconst) {
  string name = tconst->get_name();

  f_out_ << "node [fillcolor=aliceblue];" << endl;
  f_out_ << "const_" << name << " [label=\"";

  f_out_ << escape_string(name);
  f_out_ << " = ";
  print_const_value( tconst->get_type(), tconst->get_value());
  f_out_ << " :: ";
  print_type(tconst->get_type(), "const_" + name);

  f_out_ << "\"];" << endl;
}

void t_scala_generator::generate_struct  (t_struct*   tstruct) {
  string name = tstruct->get_name();

  if (tstruct->is_xception()) {
    f_out_ << "node [fillcolor=lightpink];" << endl;
    f_out_ << name << " [label=\"";
    f_out_ << "exception " << escape_string(name);
  } else if (tstruct->is_union()) {
    f_out_ << "node [fillcolor=lightcyan];" << endl;
    f_out_ << name << " [label=\"";
    f_out_ << "union " << escape_string(name);
  } else {
    f_out_ << "node [fillcolor=beige];" << endl;
    f_out_ << name << " [label=\"";
    f_out_ << "struct " << escape_string(name);
  }

  vector<t_field*> members = tstruct->get_members();
  vector<t_field*>::iterator mem_iter = members.begin();
  for ( ; mem_iter != members.end(); mem_iter++) {
    string field_name = (*mem_iter)->get_name();

    // print port (anchor reference)
    f_out_ << "|<field_" << field_name << '>';

    // field name :: field type
    f_out_ << (*mem_iter)->get_name();
    f_out_ << " :: ";
    print_type((*mem_iter)->get_type(),
        name + ":field_" + field_name);
  }

  f_out_ << "\"];" << endl;
}

void t_scala_generator::print_type(t_type* ttype, string struct_field_ref) {
  if (ttype->is_container()) {
    if (ttype->is_list()) {
      f_out_ << "list\\<";
      print_type(((t_list*)ttype)->get_elem_type(), struct_field_ref);
      f_out_ << "\\>";
    } else if (ttype->is_set()) {
      f_out_ << "set\\<";
      print_type(((t_set*)ttype)->get_elem_type(), struct_field_ref);
      f_out_ << "\\>";
    } else if (ttype->is_map()) {
      f_out_ << "map\\<";
      print_type(((t_map*)ttype)->get_key_type(), struct_field_ref);
      f_out_ << ", ";
      print_type(((t_map*)ttype)->get_val_type(), struct_field_ref);
      f_out_ << "\\>";
    }
  } else if (ttype->is_base_type()) {
    f_out_ << (((t_base_type*)ttype)->is_binary() ? "binary" : ttype->get_name());
  } else {
    f_out_ << ttype->get_name();
  }
}

/**
 * Prints out an string representation of the provided constant value
 */
void t_scala_generator::print_const_value(t_type* type, t_const_value* tvalue) {
  bool first = true;
  switch (tvalue->get_type()) {
    case t_const_value::CV_INTEGER:
      f_out_ << tvalue->get_integer();
      break;
    case t_const_value::CV_DOUBLE:
      f_out_ << tvalue->get_double();
      break;
    case t_const_value::CV_STRING:
      f_out_ << "\\\"" <<  get_escaped_string(tvalue) << "\\\"";
      break;
    case t_const_value::CV_MAP:
      {
        f_out_ << "\\{ ";
        map<t_const_value*, t_const_value*> map_elems = tvalue->get_map();
        map<t_const_value*, t_const_value*>::iterator map_iter;
        for (map_iter = map_elems.begin(); map_iter != map_elems.end(); map_iter++) {
          if (!first) {
            f_out_ << ", ";
          }
          first = false;
          print_const_value( ((t_map*)type)->get_key_type(), map_iter->first);
          f_out_ << " = ";
          print_const_value( ((t_map*)type)->get_val_type(), map_iter->second);
        }
        f_out_ << " \\}";
      }
      break;
    case t_const_value::CV_LIST:
      {
        f_out_ << "\\{ ";
        vector<t_const_value*> list_elems = tvalue->get_list();;
        vector<t_const_value*>::iterator list_iter;
        for (list_iter = list_elems.begin(); list_iter != list_elems.end(); list_iter++) {
          if (!first) {
            f_out_ << ", ";
          }
          first = false;
          if (type->is_list()) {
            print_const_value( ((t_list*)type)->get_elem_type(), *list_iter);
          } else {
            print_const_value( ((t_set*)type)->get_elem_type(), *list_iter);
          } 
        }
        f_out_ << " \\}";
      }
      break;
    case t_const_value::CV_IDENTIFIER:
      f_out_ << escape_string(type->get_name()) << "." << escape_string(tvalue->get_identifier_name());
      break;
    default:
      f_out_ << "UNKNOWN";
      break;
  }
}

void t_scala_generator::generate_service (t_service*  tservice) {
  string service_name = get_service_name(tservice);
  f_out_ << "subgraph cluster_" << service_name << " {" << endl;
  f_out_ << "node [fillcolor=bisque];" << endl;
  f_out_ << "style=dashed;" << endl;
  f_out_ << "label = \"" << escape_string(service_name) << " service\";" << endl;

  // TODO: service extends

  vector<t_function*> functions = tservice->get_functions();
  vector<t_function*>::iterator fn_iter = functions.begin();
  for ( ; fn_iter != functions.end(); fn_iter++) {
    string fn_name = (*fn_iter)->get_name();

    f_out_ << "function_" << fn_name;
    f_out_ << "[label=\"<return_type>function " << escape_string(fn_name);
    f_out_ << " :: ";
    print_type((*fn_iter)->get_returntype(), "function_" + fn_name + ":return_type");

    vector<t_field*> args = (*fn_iter)->get_arglist()->get_members();
    vector<t_field*>::iterator arg_iter = args.begin();
    for ( ; arg_iter != args.end(); arg_iter++) {
      f_out_ << "|<param_" << (*arg_iter)->get_name() << ">";
      f_out_ << (*arg_iter)->get_name();
      if ((*arg_iter)->get_value() != NULL) {
        f_out_ << " = ";
        print_const_value((*arg_iter)->get_type(), (*arg_iter)->get_value());
      }
      f_out_ << " :: ";
      print_type((*arg_iter)->get_type(),
          "function_" + fn_name + ":param_" + (*arg_iter)->get_name());

    }
    // end of node
    f_out_ << "\"];" << endl;
  }

  f_out_ << " }" << endl;
}

THRIFT_REGISTER_GENERATOR(scala, "Scala", "")

