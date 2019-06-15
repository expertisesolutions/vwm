///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#include <ostream>

#include <pugixml.hpp>

#include <vwm/protocol/argument.hpp>

#include <iostream>

#include <cstring>
#include <algorithm>
#include <map>

namespace vwm { namespace protocol {

void generate_process_message_request_case (std::ostream& out, pugi::xml_node member
                                            , unsigned int member_index
                                            , std::string interface_name)
{
  std::cout << "members " << member.name() << " member index " << member_index << std::endl;

  out << "          case " << member_index << ":{\n";
  auto tabs = "            ";
  out << tabs << "std::cout << \"op " << member.attribute("name").value() << "\\n\";\n";
  out << tabs << "unsigned offset = 0;\n";

  auto args = member.children ("arg");
  unsigned int argument_count = std::distance (args.begin(), args.end());
  auto arg_first = args.begin(), arg_last = args.end();
  bool contains_variable_sized_argument
    = std::find_if
    (arg_first, arg_last
     , [] (auto node) -> bool
       {
         auto type = node.attribute("type").value();
         return !strcmp(type, "new_id")
           || !strcmp(type, "string")
           || !strcmp(type, "array");
       }) != arg_last;
  bool contains_fd
    = std::find_if
    (arg_first, arg_last
     , [] (auto node) -> bool
       {
         auto type = node.attribute("type").value();
         return !strcmp(type, "fd");
       }) != arg_last;

  if (!contains_variable_sized_argument && !contains_fd)
  {
    if (argument_count != 0)
    {
      out << tabs << "struct arguments\n";
      out << tabs << "{\n";

      unsigned int i = 0;
      for (auto&& arg : member.children("arg"))
      {
        out << tabs << "  ";
        vwm::protocol::generate_arg_decl (out, arg.attribute("type").value(), i++);
        out << ";\n";
      }

      out << tabs << "} arguments;\n\n";
      out << tabs << "if (payload.size() == sizeof(arguments))\n";
      out << tabs << "{\n";
      out << tabs << "  std::memcpy(&arguments, payload.data(), sizeof(arguments));\n";
    }
    else
    {
      out << tabs << "if (payload.size() == 0)\n";
      out << tabs << "{\n";
    }
    out << tabs << "  this->" << interface_name << "_"
        << member.attribute("name").value() << "(object";
    if (argument_count != 0)
    {
      unsigned int i = 0;
      for (auto&& arg : member.children("arg"))
      {
        static_cast<void>(arg);
        out << ", arguments.arg" << i++;
      }
    }
    out << ");\n";
    out << tabs << "}\n";
    out << tabs << "else\n";
    out << tabs << "{\n";
    out << tabs << "  std::cout << \"payload size \" << payload.size() << \" arguments has size ";
    if (argument_count != 0)
      out << "\" << sizeof(arguments) << std::endl;\n";
    else
      out << " 0\" << std::endl;\n";
    out << tabs << "  throw -1; // for now, should just call a function instead of throwing\n";
    out << tabs << "}\n";
  }
  else
  {
    std::map<unsigned, unsigned> argument_values_map;
    auto generate_values_suffix_code
      = [&] (std::ostream& out, unsigned int fixed_size_values_index, unsigned int fixed_size_values_size
             , unsigned int argument_index, std::optional<unsigned int> new_id_subitem)
    {
      auto tab = "            ";
      auto memcpy_lambda
        = [&] ()
        {
          out << tab << "if constexpr (!std::is_empty<struct values" << fixed_size_values_index << ">::value)\n";
          out << tab << "{\n";
          out << tab << "  std::memcpy(&values" << fixed_size_values_index << ", payload.data() + offset, sizeof(struct values" << fixed_size_values_index
              << "));\n";
          out << tab << "}\n";
        };
      if (new_id_subitem)
      {
        if (*new_id_subitem == 0)
        {
          //out << "interface" << std::endl;
          memcpy_lambda();
          out << tab << "std::string_view arg" << argument_index << "_interface;\n";
          out << tab << "vwm::wayland::unmarshall_copy (std::string_view{payload.data() + offset, payload.size() - offset}, arg"
              << argument_index << "_interface);\n";
          out << tab << "offset += vwm::wayland::unmarshall_size (std::string_view{payload.data() + offset, payload.size() - offset}, arg"
              << argument_index << "_interface);\n";
          out << tab << "std::uint32_t arg" << argument_index << "_version = 0;\n";
          out << tab << "if (payload.size() < offset + sizeof(std::uint32_t)))\n";
          out << tab << "{\n";
          out << tab << "  throw -1;\n";
          out << tab << "}\n";
          out << tab << "else\n";
          out << tab << "  std::memcpy(&"", payload.data());\n";
        }
        else if (*new_id_subitem == 1)
        {
          
        }
        //   out << "version" << std::endl;
        // else if (*new_id_subitem == 2)
        //   out << "new_id" << std::endl;
      }
      else
      {
        argument_values_map.insert(std::make_pair(argument_index, fixed_size_values_index));
        memcpy_lambda();
      }
    };
    
    unsigned int fixed_size_values_last;
    generate_values (out, member, argument_count, fixed_size_values_last, "            ", "", "", generate_values_suffix_code, false);
    
    //bool variable_sized_offset = false;
    //std::size_t offset = 0;
  }

  out << "            break;}\n";
}
    
void generate_process_message (std::ostream& out, pugi::xml_document& doc)
{
  out << "  void process_message (uint32_t from, uint16_t op, std::string_view payload)\n";
  out << "  {\n";
  out << "    auto object = this->get_object(from);\n";
  out << "    switch (object.interface_)\n";
  out << "    {\n";
  out << "      case interface_::empty: throw -1; break;\n";

  for (auto&& interface_ : doc.child ("protocol").children ("interface"))
  {
      std::cout << "interface " << interface_.attribute("name").value() << std::endl;

      unsigned int member_index = 0;

      out << "      case interface_::" << interface_.attribute("name").value() << ":\n" << std::endl;
      out << "        switch (op)\n";
      out << "        {\n";
      
      for (auto&& member : interface_.children ("request"))
      {
        generate_process_message_request_case (out, member, member_index, interface_.attribute("name").value());
        member_index++;
      }

      out << "          default: break;\n";
      out << "        };\n";
      out << "      break;\n";
    }
  out << "    };\n";
  out << "  }\n";
}
    
void generate_values (std::ostream& out, pugi::xml_node member
                      , unsigned int arg_size
                      , unsigned int& fixed_size_values_last
                      , std::string tabs
                      , std::string header_decl_prefix
                      , std::string header_constr_prefix
                      , std::function<void(std::ostream&, unsigned int, unsigned int, unsigned int, std::optional<unsigned int>)> generate_values_suffix_code
                      , bool generate_constructors)
{
  unsigned int fixed_size_values_index = 1;
  unsigned int first_from_value = 0;
  //out << "    vwm::wayland::sbo<char> buffer;\n";
  out << tabs << "struct values1\n";
  out << tabs << "{\n";
  out << header_decl_prefix;

  auto args = member.children("arg");
  auto arg_first = args.begin(), arg_last = args.end();

  bool has_interface_param
    = std::find_if (arg_first, arg_last
                    , [] (auto arg)
                      {
                        return !strcmp(arg.attribute("name").value(), "interface");
                      }) != arg_last;

  unsigned int i = 0;
  bool is_last = false;
  auto generate_value_end =
    [&] (std::function<void(std::ostream&, unsigned int, unsigned int, unsigned int
                            , std::optional<unsigned int>)> generate_values_suffix_code
         , bool composed = false)
    {
       out << tabs << "} values" << fixed_size_values_index;
       if (generate_constructors)
         value_constructor(out, fixed_size_values_index, first_from_value, member
                           , header_constr_prefix);
       else
         out << ";\n";

       if (generate_values_suffix_code)
       {
         if (composed)
           generate_values_suffix_code (out, fixed_size_values_index, fixed_size_values_last, i, 0);
         else
           generate_values_suffix_code (out, fixed_size_values_index, fixed_size_values_last, i, {});
       }

       i++;
       first_from_value = i;
       is_last = (i == arg_size);
       if (!is_last)
       {
         fixed_size_values_index++;
         out << tabs << "struct values" << fixed_size_values_index << "\n";
         out << tabs << "{\n";
       }
    };
  
  for (auto&& arg : args)
   {
     auto type = arg.attribute("type").value();
     if (vwm::protocol::is_arg_fixed_size(type)
         && !(!has_interface_param && !strcmp(type, "new_id")))
     {
       out << tabs << "  ";
       generate_arg_decl (out, type, i, "_");
       out << ";\n";
       i++;
     }
     else if (!strcmp(type, "new_id"))
     {
       generate_value_end (generate_values_suffix_code, true);
       if (generate_values_suffix_code)
       {
         generate_values_suffix_code (out, fixed_size_values_index, fixed_size_values_last, i, 1);
         generate_values_suffix_code (out, fixed_size_values_index, fixed_size_values_last, i, 2);
       }
     }
     else // not fixed
     {
       generate_value_end (generate_values_suffix_code);
     }
   }

  fixed_size_values_last = fixed_size_values_index;
  if (!is_last)
  {
    out << tabs << "} values" << fixed_size_values_index;
    if (generate_constructors)
      vwm::protocol::value_constructor(out, fixed_size_values_index, first_from_value, member
                                       , header_constr_prefix);
    else
      out << ";\n";
    if (generate_values_suffix_code)
      generate_values_suffix_code (out, fixed_size_values_index, fixed_size_values_last, i, {});
  }
}

} }
