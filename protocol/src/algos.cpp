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
#include <cassert>

namespace vwm { namespace protocol {

void generate_for_each_value (pugi::xml_object_range<pugi::xml_named_node_iterator> args
                              , std::function<value_generator_separation(pugi::xml_node argument)> embedded_generator)
{
  auto first = args.begin()
    , last = args.end();
  for (;first != last; ++first)
  {
    while (embedded_generator (*first) == value_generator_separation::separate);
  }
}

vwm::protocol::value_generator_separation generate_event_value_members (std::ostream& out, unsigned& i, bool& separated
                                                                        , std::string tabs, pugi::xml_node arg)
{
  auto type = arg.attribute("type").value();
  if (vwm::protocol::is_arg_fixed_size(type))
  {
    out << tabs << "  ";
    vwm::protocol::generate_arg_decl (out, type, i++, "_");
    out << ";\n";
    return vwm::protocol::value_generator_separation::continue_;
  }
  else if (separated)
    return separated = false,vwm::protocol::value_generator_separation::continue_;
  else
    return i++,separated = true,vwm::protocol::value_generator_separation::separate;
}

void generate_values_type_definition (std::ostream& out, pugi::xml_object_range<pugi::xml_named_node_iterator> args, std::string tabs
                                      , std::function<value_generator_separation(pugi::xml_node argument)> embedded_generator)
{
  unsigned i = 0;
  out << tabs << "struct values" << i << "\n";
  out << tabs << "{\n";

  generate_for_each_value
    (args
     , [&] (auto arg)
       {
         auto r = embedded_generator (arg);
         if (r == value_generator_separation::separate)
         {
           out << tabs << "};\n";
           out << tabs << "struct values" << ++i << "\n";
           out << tabs << "{\n";
         }
         return r;
       });
  out << tabs << "};\n";
}

void generate_request_case_value_definition (std::ostream& out
                                             , unsigned& value_index
                                             , unsigned& arg_index
                                             // , pugi::xml_named_node_iterator arg_first
                                             // , pugi::xml_named_node_iterator arg_last
                                             , std::string tabs
                                             , pugi::xml_node arg)
{
  auto type = arg.attribute("type").value();
  if (vwm::protocol::is_arg_fixed_size(type)
      && !(arg.attribute("interface").empty() && !strcmp(type, "new_id")))
  {
    out << tabs;
    generate_arg_decl (out, type, arg_index, "", "& ");
    out << " = values" << value_index << ".arg" << arg_index << ";\n";
    
    arg_index++;
  }
  else if (arg.attribute("interface").empty() && !strcmp(type, "new_id"))
  {
    // interface
    out << tabs;
    generate_arg_decl (out, "string", arg_index, "");
    out << ";\n";
    out << tabs << "vwm::wayland::unmarshall(arg" << arg_index << ", std::string_view{payload.data() + offset, payload.size() - offset}, *this);\n";
    out << tabs << "offset += vwm::wayland::marshall_size(arg" << arg_index << ");\n";
    
    arg_index++;
    value_index++;
    out << tabs << "struct values" << value_index << " values" << value_index << ";\n";
    out << tabs << "if constexpr (!std::is_empty<struct values" << value_index << ">::value)\n";
    out << tabs << "{\n";
    out << tabs << "  std::memcpy(&values" << value_index << ", payload.data() + offset, sizeof(values" << value_index << "));\n";
    out << tabs << "  offset += sizeof(values" << value_index << ");\n";
    out << tabs << "}\n";

    out << tabs;
    generate_arg_decl (out, "uint", arg_index, "", "& ");
    out << " = values" << value_index << ".arg" << arg_index << ";\n";
    arg_index++;

    out << tabs;
    generate_arg_decl (out, "new_id", arg_index, "", "& ");
    out << " = values" << value_index << ".arg" << arg_index << ";\n";

    arg_index++;
  }
  else
  {
    out << tabs;
    generate_arg_decl (out, type, arg_index, "");
    out << ";\n";
    out << tabs << "vwm::wayland::unmarshall(arg" << arg_index << ", std::string_view{payload.data() + offset, payload.size() - offset}, *this);\n";
    out << tabs << "offset += vwm::wayland::marshall_size(arg" << arg_index << ");\n";

    arg_index++;
    value_index++;
    out << tabs << "struct values" << value_index << " values" << value_index << ";\n";
    out << tabs << "if constexpr (!std::is_empty<struct values" << value_index << ">::value)\n";
    out << tabs << "{\n";
    out << tabs << "  std::memcpy(&values" << value_index << ", payload.data() + offset, sizeof(values" << value_index << "));\n";
    out << tabs << "  offset += sizeof(values" << value_index << ");\n";
    out << tabs << "}\n";
  }
}

void generate_request_argument (std::ostream& out, unsigned& arg_index
                                , std::string tabs, pugi::xml_node arg)
{
  auto type = arg.attribute("type").value();
  if (vwm::protocol::is_arg_fixed_size(type)
      && !(arg.attribute("interface").empty() && !strcmp(type, "new_id")))
  {
    out << ", arg" << arg_index;
    
    arg_index++;
  }
  else if (arg.attribute("interface").empty() && !strcmp(type, "new_id"))
  {
    // interface
    out << ", arg" << arg_index;
    
    arg_index++;

    out << ", arg" << arg_index;

    arg_index++;

    out << ", arg" << arg_index;

    arg_index++;
  }
  else
  {
    out << ", arg" << arg_index;

    arg_index++;
  }
}

void generate_event_value_argument (std::ostream& out, unsigned& value_index
                                    , unsigned& arg_index
                                    , bool& pred_taken
                                    , unsigned current_value_index
                                    , pugi::xml_node arg)
{
  auto comma_if = [&] { if (pred_taken) out << ", "; };
  auto pred = [&] { return current_value_index == value_index; };
  auto type = arg.attribute("type").value();
  if (vwm::protocol::is_arg_fixed_size(type))
  {
    if (pred())
    {
      comma_if();
      out << "arg" << arg_index;
      pred_taken = true;
    }
    
    arg_index++;
  }
  else
  {
    arg_index++;
    value_index++;
  }
}
    
value_generator_separation generate_request_case_values_type_definition (std::ostream& out, bool& separated
                                                                         , unsigned& i, pugi::xml_named_node_iterator arg_first
                                                                         , pugi::xml_named_node_iterator arg_last
                                                                         , std::string tabs
                                                                         , pugi::xml_node arg)
{
  auto type = arg.attribute("type").value();
  if (vwm::protocol::is_arg_fixed_size(type)
      && !(arg.attribute("interface").empty() && !strcmp(type, "new_id")))
  {
    out << tabs << "  ";
    generate_arg_decl (out, type, i++, "");
    out << ";\n";
    
    return vwm::protocol::value_generator_separation::continue_;
  }
  // special case when no interface parameter but a new_id, insert interface and version
  else if (arg.attribute("interface").empty() && !strcmp(type, "new_id"))
  {
    if (separated)
    {
      separated = false;
             
      out << tabs << "  ";
      generate_arg_decl (out, "uint", i++, "");
      out << ";\n";
      out << tabs << "  ";
      generate_arg_decl (out, "new_id", i++, "");
      out << ";\n";
      return vwm::protocol::value_generator_separation::continue_;
    }
    else
    {
      i++; // for interface
      separated = true;
      return vwm::protocol::value_generator_separation::separate;
    }
  }
  else
  {
    if (separated)
    {
      separated = false;
      return vwm::protocol::value_generator_separation::continue_;
    }
    else
    {
      ++i;
      separated = true;
      return vwm::protocol::value_generator_separation::separate;
    }
  }
}
    
void generate_process_message_request_case (std::ostream& out, pugi::xml_node member
                                            , unsigned int member_index
                                            , std::string interface_name)
{
  std::cout << "members " << member.name() << " member index " << member_index << std::endl;

  out << "          case " << member_index << ":{\n";
  auto tabs = "            ";
  out << tabs << "std::cout << \"op " << member.attribute("name").value() << "\\n\";\n";
  std::cout << "op " << member.attribute("name").value() << std::endl;

  auto args = member.children ("arg");
  {
    bool separated = false;
    unsigned int i = 0;
    auto arg_first = args.begin(), arg_last = args.end();

    generate_values_type_definition
      (out, args, "            "
       , [&] (pugi::xml_node arg) -> vwm::protocol::value_generator_separation
         {
           return generate_request_case_values_type_definition
             (out, separated, i, arg_first, arg_last, "            ", arg);
         });
    out << tabs << "unsigned offset = 0;\n";
    {
      unsigned value_index = 0, arg_index = 0;
      out << tabs << "struct values" << value_index << " values" << value_index << ";\n";
      out << tabs << "if constexpr (!std::is_empty<struct values" << value_index << ">::value)\n";
      out << tabs << "{\n";
      out << tabs << "  std::memcpy(&values" << value_index << ", payload.data() + offset, sizeof(values" << value_index << "));\n";
      out << tabs << "  offset += sizeof(values" << value_index << ");\n";
      out << tabs << "}\n";

      for (auto&& arg : args)
      {
        generate_request_case_value_definition
          (out, value_index, arg_index, "            ", arg);
      }
    }
  }

  out << "            this->" << interface_name << "_" << member.attribute("name").value() << "(object";
  {
    unsigned int arg_index = 0;
    for (auto&& arg : args)
    {
      generate_request_argument
        (out, arg_index, "            ", arg);
    }
  }
  out << ");\n";
  out << "            break;}\n";
}
    
void generate_process_message (std::ostream& out, std::vector<pugi::xml_document>const& docs)
{
  out << "  void process_message (uint32_t from, uint16_t op, std::string_view payload)\n";
  out << "  {\n";
  out << "    auto object = this->get_object(from);\n";
  out << "    switch (this->get_interface(object))\n";
  out << "    {\n";
  out << "      case interface_::empty: throw -1; break;\n";

  for (auto&& doc : docs)
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
    
// void generate_values (std::ostream& out, pugi::xml_node member
//                       , unsigned int arg_size
//                       , unsigned int& fixed_size_values_last
//                       , std::string tabs
//                       , std::string header_decl_prefix
//                       , std::string header_constr_prefix
//                       , std::function<void(std::ostream&, unsigned int, unsigned int, unsigned int, std::optional<unsigned int>)> generate_values_suffix_code
//                       , bool generate_constructors)
// {
//   unsigned int fixed_size_values_index = 1;
//   fixed_size_values_last = -1;
//   unsigned int first_from_value = 0;
//   //out << "    vwm::wayland::sbo<char> buffer;\n";
//   out << tabs << "struct values1\n";
//   out << tabs << "{\n";
//   out << header_decl_prefix;

//   auto args = member.children("arg");

//   unsigned int i = 0;
//   bool is_last = false;
//   auto generate_value_end =
//     [&] (std::function<void(std::ostream&, unsigned int, unsigned int, unsigned int
//                             , std::optional<unsigned int>)> generate_values_suffix_code
//          , bool composed = false)
//     {
//        out << tabs << "} values" << fixed_size_values_index;
//        is_last = (i == arg_size);
//        if (is_last)
//          fixed_size_values_last = fixed_size_values_index;
//        if (generate_constructors)
//          value_constructor(out, fixed_size_values_index, first_from_value, member
//                            , header_constr_prefix);
//        else
//          out << ";\n";

//        if (generate_values_suffix_code)
//        {
//          if (composed)
//            generate_values_suffix_code (out, fixed_size_values_index, fixed_size_values_last, i, 0);
//          else
//            generate_values_suffix_code (out, fixed_size_values_index, fixed_size_values_last, i, {});
//        }

//        i++;
//        first_from_value = i;
//        if (!is_last)
//        {
//          fixed_size_values_index++;
//          out << tabs << "struct values" << fixed_size_values_index << "\n";
//          out << tabs << "{\n";
//        }
//        else
//        {
//        }
//     };
  
//   for (auto&& arg : args)
//    {
//      auto type = arg.attribute("type").value();
//      if (vwm::protocol::is_arg_fixed_size(type)
//          && !(arg.attribute("interface").empty() && !strcmp(type, "new_id")))
//      {
//        out << tabs << "  ";
//        generate_arg_decl (out, type, i, "_");
//        out << ";\n";
//        i++;
//      }
//      else if (!strcmp(type, "new_id"))
//      {
//        generate_value_end (generate_values_suffix_code, true);
//        if (generate_values_suffix_code)
//        {
//          generate_values_suffix_code (out, fixed_size_values_index, fixed_size_values_last, i, 1);
//          generate_values_suffix_code (out, fixed_size_values_index, fixed_size_values_last, i, 2);
//        }
//      }
//      else // not fixed
//      {
//        generate_value_end (generate_values_suffix_code);
//      }
//    }

//   fixed_size_values_last = fixed_size_values_index;
//   if (!is_last)
//   {
//     out << tabs << "} values" << fixed_size_values_index;
//     if (generate_constructors)
//       vwm::protocol::value_constructor(out, fixed_size_values_index, first_from_value, member
//                                        , header_constr_prefix);
//     else
//       out << ";\n";
//     if (generate_values_suffix_code)
//       generate_values_suffix_code (out, fixed_size_values_index, fixed_size_values_last, i, {});
//   }
// }

} }
