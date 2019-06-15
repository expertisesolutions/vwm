///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_PROTOCOL_ARGUMENT_HPP
#define VWM_PROTOCOL_ARGUMENT_HPP

#include <functional>

namespace vwm { namespace protocol {

struct uint_tag {};
struct int_tag {};
struct fixed_tag {};
struct string_tag {};
struct object_tag {};
struct new_id_tag {};
struct array_tag {};
struct fd_tag {};

template <typename F>
typename F::result_type select_arg (std::string type, F function)
{
  if (type == "uint")
  {
    return function(uint_tag{});
  }
  else if (type == "int")
  {
    return function(int_tag{});
  }
  else if (type == "fixed")
  {
    return function(fixed_tag{});
  }
  else if (type == "string")
  {
    return function(string_tag{});
  }
  else if (type == "object")
  {
    return function(object_tag{});
  }
  else if (type == "new_id")
  {
    return function(new_id_tag{});
  }
  else if (type == "array")
  {
    return function(array_tag{});
  }
  else if (type == "fd")
  {
    return function(fd_tag{});
  }
  else
  {
    throw -1;
  }
}

struct print_decl
{
  std::ostream* os;
  unsigned int i;
  std::string suffix, prefix;

  void operator()(uint_tag) const
  {
    *os << "std::uint32_t " << prefix << "arg" << i << suffix;
  }

  void operator()(int_tag) const
  {
    *os << "std::int32_t " << prefix << "arg" << i << suffix;
  }

  void operator()(fixed_tag) const
  {
    *os << "vwm::wayland::fixed " << prefix << "arg" << i << suffix;
  }

  void operator()(string_tag) const
  {
    *os << "std::string_view " << prefix << "arg" << i << suffix;
  }

  void operator()(object_tag) const
  {
    *os << "std::uint32_t " << prefix << "arg" << i << suffix;
  }

  void operator()(new_id_tag) const
  {
    *os << "std::uint32_t " << prefix << "arg" << i << suffix;
  }

  void operator()(array_tag) const
  {
    *os << "vwm::wayland::array " << prefix << "arg" << i << suffix;
  }

  void operator()(fd_tag) const
  {
    *os << "int " << prefix << "arg" << i << suffix;
  }

  typedef void result_type;
};

inline void generate_arg_decl (std::ostream& os, std::string type, unsigned int i, std::string suffix = "", std::string prefix = "")
{
  select_arg (type, print_decl{&os, i, suffix, prefix});
}

struct is_arg_fixed_size_visitor
{
  template <typename T>
  constexpr bool operator()(T) const { return true; }

  constexpr bool operator()(string_tag) const { return false; }

  constexpr bool operator()(array_tag) const { return false; }

  typedef bool result_type;
};

inline bool is_arg_fixed_size (std::string type)
{
  return select_arg (type, is_arg_fixed_size_visitor{});
}

struct is_arg_fd_visitor
{
  template <typename T>
  constexpr bool operator()(T) const { return false; }

  constexpr bool operator()(fd_tag) const { return true; }

  typedef bool result_type;
};

inline bool is_arg_fd (std::string type)
{
  return select_arg (type, is_arg_fd_visitor{});
}

void generate_values (std::ostream& out, pugi::xml_node member, unsigned int arg_size
                      , unsigned int& fixed_size_values_size
                      , std::string tabs
                      , std::string header_decl
                      , std::string header_constr
                      , std::function<void(std::ostream&, unsigned int, unsigned int, unsigned int, std::optional<unsigned int>)> generate_values_suffix_code
                      , bool generate_constructors = true);
enum class value_generator_separation
{
  continue_
  , separate
};

void generate_values_type_definition (std::ostream& out, pugi::xml_object_range<pugi::xml_named_node_iterator> args, std::string tabs
                                      , std::function<value_generator_separation(pugi::xml_node argument)> embedded_generator);
    
inline
void value_constructor (std::ostream& out, unsigned int fixed_size_values_index, unsigned int first_from_value
                        , pugi::xml_node member, std::string header_args)
{
  bool needs_comma = (fixed_size_values_index == 1) && !header_args.empty();
  out << "{";
  if (fixed_size_values_index == 1)
    out << header_args;
  {
    unsigned int i = first_from_value;
    auto arg = member.children("arg");
    auto arg_first = std::next(arg.begin(), first_from_value)
      , arg_last = arg.end();
    for (;arg_first != arg_last; ++arg_first, ++i)
    {
      auto type = (*arg_first).attribute("type").value();
      if (is_arg_fixed_size(type))
      {
        if (needs_comma)
          out << ", ";
        needs_comma = true;
        out << "arg" << i;
      }
      else // not fixed
        break;
    }
  }
  out << "};\n";
}

void generate_process_message (std::ostream& out, pugi::xml_document& doc);

    void generate_arguments_enumeration (std::ostream& out, pugi::xml_object_range<pugi::xml_named_node_iterator> args);
    
} }

#endif
