# Template-Based Code Generation Exploration

## Overview

This document explores how Thrift could adopt a template-based code generation approach similar to protobuf compiler plugins.

## Current vs. Template-Based Approaches

### Current Programmatic Approach

**Advantages:**
- Full control over code generation logic
- Type-safe (C++ compiler catches errors)
- No external dependencies on template engines
- Direct access to AST for complex logic
- Performance (no template parsing overhead)

**Disadvantages:**
- 68,755+ lines of generator code across 31+ languages
- Code generation logic mixed with output formatting
- Hard to visualize generated code structure
- Difficult for non-C++ developers to contribute generators
- High duplication between similar languages
- Tedious string concatenation and indentation management

### Template-Based Approach

**Advantages:**
- Clear separation between logic and presentation
- Generated code structure is immediately visible
- Easier for language experts (non-C++ devs) to contribute
- Less code duplication (shared template helpers)
- More maintainable for simple patterns
- Can leverage existing template engines (Jinja2, Mustache, etc.)

**Disadvantages:**
- Complex logic harder to express in templates
- Additional dependency on template engine
- Potential performance overhead
- Less type safety (template errors found at runtime)
- Debugging can be harder

## Template-Based Architecture Proposal

```
┌─────────────────┐
│  Thrift IDL     │
│  (.thrift file) │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Parser         │
│  (Lex/Yacc)     │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  AST            │
│  (t_program)    │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────────┐
│  Template Engine                    │
│  ┌─────────────────────────────┐   │
│  │  Data Preparation Layer     │   │
│  │  (Extract data from AST)    │   │
│  └──────────┬──────────────────┘   │
│             │                       │
│             ▼                       │
│  ┌─────────────────────────────┐   │
│  │  Template Files (.j2)       │   │
│  │  ├── struct.java.j2         │   │
│  │  ├── service.java.j2        │   │
│  │  └── enum.java.j2           │   │
│  └─────────────────────────────┘   │
└────────┬────────────────────────────┘
         │
         ▼
┌─────────────────┐
│  Generated Code │
└─────────────────┘
```

## Example: Java Class Template

### Programmatic (Current)
```cpp
// t_java_generator.cc:2341
void t_java_generator::generate_java_struct(t_struct* tstruct, bool is_exception) {
    string f_struct_name = package_dir_ + "/" + tstruct->get_name() + ".java";
    ofstream_with_content_based_conditional_update f_struct;
    f_struct.open(f_struct_name);

    f_struct << autogen_comment() << java_package() << java_suppressions();
    f_struct << "public class " << tstruct->get_name();

    if (is_exception) {
        f_struct << " extends TException";
    }
    f_struct << " implements org.apache.thrift.TBase<"
             << tstruct->get_name() << ", "
             << tstruct->get_name() << "._Fields>";

    scope_up(f_struct);

    // Generate fields
    const vector<t_field*>& members = tstruct->get_members();
    for (auto member : members) {
        f_struct << indent() << "private "
                 << type_name(member->get_type()) << " "
                 << member->get_name() << ";" << endl;
    }

    // ... 200+ more lines of string concatenation
}
```

### Template-Based (Proposed)

**Template File: `templates/java/struct.java.j2`**
```jinja2
{{ autogen_comment }}
{{ package_declaration }}

{{ imports }}

{{ suppress_warnings }}
public class {{ struct.name }}
{%- if struct.is_exception %} extends TException{% endif %}
    implements org.apache.thrift.TBase<{{ struct.name }}, {{ struct.name }}._Fields> {

    private static final org.apache.thrift.protocol.TStruct STRUCT_DESC =
        new org.apache.thrift.protocol.TStruct("{{ struct.name }}");

    {# Field declarations #}
    {% for field in struct.fields %}
    private {{ field.type_name }} {{ field.name }};
    {% endfor %}

    {# Constructor #}
    public {{ struct.name }}() {
    }

    public {{ struct.name }}(
        {% for field in struct.fields %}
        {{ field.type_name }} {{ field.name }}{% if not loop.last %},{% endif %}
        {% endfor %}
    ) {
        this();
        {% for field in struct.fields %}
        this.{{ field.name }} = {{ field.name }};
        {% if field.type.is_binary %}
        if (this.{{ field.name }} != null) {
            this.{{ field.name }} = org.apache.thrift.TBaseHelper.copyBinary({{ field.name }});
        }
        {% endif %}
        {% endfor %}
    }

    {# Getters and setters #}
    {% for field in struct.fields %}
    public {{ field.type_name }} get{{ field.name|capitalize }}() {
        return this.{{ field.name }};
    }

    public {{ struct.name }} set{{ field.name|capitalize }}({{ field.type_name }} {{ field.name }}) {
        this.{{ field.name }} = {{ field.name }};
        return this;
    }

    {% endfor %}

    {# Read method #}
    {% include "java/read_method.j2" %}

    {# Write method #}
    {% include "java/write_method.j2" %}
}
```

**Data Preparation: `java_template_generator.cc`**
```cpp
class t_java_template_generator : public t_generator {
public:
    void generate_struct(t_struct* tstruct) {
        // Prepare data for template
        json data;
        data["struct"]["name"] = tstruct->get_name();
        data["struct"]["is_exception"] = tstruct->is_xception();

        // Extract fields
        for (auto field : tstruct->get_members()) {
            json field_data;
            field_data["name"] = field->get_name();
            field_data["type_name"] = type_name(field->get_type());
            field_data["type"]["is_binary"] = field->get_type()->is_binary();
            data["struct"]["fields"].push_back(field_data);
        }

        // Render template
        string output = template_engine_->render("java/struct.java.j2", data);

        // Write to file
        write_file(package_dir_ + "/" + tstruct->get_name() + ".java", output);
    }
};
```

## Template Engine Options

### 1. Jinja2 (Python)
- **Pros**: Very powerful, well-documented, widely used
- **Cons**: Requires Python runtime or C++ bindings
- **Example**: inja (C++ Jinja2 implementation)

### 2. Mustache
- **Pros**: Logic-less, language-agnostic, simple
- **Cons**: Limited control flow, harder for complex cases
- **Example**: mstch (C++ implementation)

### 3. Embedded Templates (C++20)
- **Pros**: No external dependencies, compile-time checked
- **Cons**: Verbose, less familiar syntax
- **Example**: Custom template engine using constexpr

### 4. Handlebars
- **Pros**: Good balance of logic and simplicity
- **Cons**: Fewer C++ implementations
- **Example**: handlebars.cpp

## Hybrid Approach

Best of both worlds: **Templates for simple patterns, code for complex logic**

```cpp
class t_java_hybrid_generator : public t_generator {
public:
    void generate_struct(t_struct* tstruct) {
        if (is_simple_struct(tstruct)) {
            // Use template for simple structs
            render_template("java/simple_struct.j2", prepare_data(tstruct));
        } else {
            // Use programmatic generation for complex cases
            generate_struct_programmatic(tstruct);
        }
    }

    void generate_service(t_service* tservice) {
        // Always use templates for service interfaces (straightforward)
        render_template("java/service_interface.j2", prepare_service_data(tservice));

        // Use code for complex async service implementations
        generate_async_service_programmatic(tservice);
    }
};
```

## Migration Strategy

### Phase 1: Proof of Concept
1. Create template generator for ONE simple language (e.g., JSON output)
2. Implement basic template engine integration
3. Compare output with existing generator
4. Measure performance impact

### Phase 2: Pilot Language
1. Choose a medium-complexity language (e.g., Python or Ruby)
2. Implement full template-based generator
3. Maintain backward compatibility
4. Run comprehensive tests

### Phase 3: Template Library
1. Extract common patterns into shared template helpers
2. Create template library for common constructs:
   - Field serialization
   - Constructor generation
   - Getters/setters
   - Equals/hashCode methods

### Phase 4: Gradual Migration
1. Convert simple generators first
2. Keep complex generators (C++, Java) programmatic initially
3. Allow generators to use hybrid approach
4. Document template best practices

## Implementation Example: Simple Template Generator

### Directory Structure
```
compiler/cpp/src/thrift/generate/
├── t_template_generator.h          # Base class for template-based generators
├── t_template_generator.cc
├── template_engine.h                # Template engine abstraction
├── template_engine.cc
└── templates/
    ├── python/
    │   ├── struct.py.j2
    │   ├── service.py.j2
    │   ├── enum.py.j2
    │   └── helpers.j2
    ├── ruby/
    │   ├── struct.rb.j2
    │   └── service.rb.j2
    └── shared/
        ├── apache_header.j2
        └── autogen_comment.j2
```

### Base Template Generator Class

```cpp
// t_template_generator.h
class t_template_generator : public t_generator {
public:
    t_template_generator(t_program* program,
                        const std::string& template_dir)
        : t_generator(program)
        , template_dir_(template_dir)
        , engine_(std::make_unique<TemplateEngine>(template_dir)) {
    }

protected:
    // Helper to render a template with data
    std::string render(const std::string& template_name,
                      const json& data) {
        return engine_->render(template_name, data);
    }

    // Helper to prepare common data
    json prepare_base_data() {
        json data;
        data["program_name"] = program_->get_name();
        data["namespace"] = get_namespace();
        data["includes"] = get_includes();
        return data;
    }

    // Convert t_type to template-friendly format
    json type_to_json(t_type* type);

    // Convert t_field to template-friendly format
    json field_to_json(t_field* field);

private:
    std::string template_dir_;
    std::unique_ptr<TemplateEngine> engine_;
};
```

## Benefits Analysis

### Quantitative Benefits

**Current Approach:**
- Average generator size: ~2,200 lines of C++
- Time to add new language: 2-4 weeks (for experienced C++ dev)
- Code duplication factor: ~40% (similar code across generators)

**Template Approach (Estimated):**
- Template size: ~500 lines of templates + 500 lines of C++ glue
- Time to add new language: 3-5 days (for language expert, not C++ expert)
- Code duplication factor: ~15% (shared template helpers)

### Qualitative Benefits

1. **Maintainability**: Easier to update code patterns across all languages
2. **Accessibility**: Language experts can contribute without deep C++ knowledge
3. **Visibility**: Generated code structure is visible in templates
4. **Testing**: Templates easier to unit test than complex C++ methods
5. **Documentation**: Templates serve as documentation of output format

## Challenges

1. **Performance**: Template parsing/rendering overhead (mitigated by caching)
2. **Debugging**: Harder to debug template rendering vs. C++ code
3. **Complex Logic**: Some generators have complex conditional logic
4. **Migration Effort**: 68,755 lines of existing code to potentially convert
5. **Tooling**: Need good template testing and validation tools
6. **Backward Compatibility**: Must maintain identical output initially

## Recommended Approach

**Start Small, Validate, Scale:**

1. **Create a proof-of-concept** template generator for a simple language
2. **Measure and compare**:
   - Output correctness (diff with existing generator)
   - Performance (compilation time)
   - Maintainability (ease of making changes)
3. **If successful**, migrate one production language (e.g., Python)
4. **Extract patterns** into shared template library
5. **Gradually migrate** other generators based on complexity
6. **Keep hybrid option** for generators with complex logic

## Next Steps

Would you like me to:

1. **Implement a proof-of-concept** template-based generator for a simple language (JSON, Python, or Ruby)?
2. **Integrate a template engine** (inja/Jinja2 for C++) into the compiler?
3. **Create comparison benchmarks** between programmatic vs. template approaches?
4. **Design the template generator API** in detail?
5. **Extract common patterns** from existing generators that would benefit from templates?

Let me know which direction you'd like to explore!
