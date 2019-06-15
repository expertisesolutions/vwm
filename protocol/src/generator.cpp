///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#include <boost/program_options.hpp>
#include <pugixml.hpp>

#include <vwm/protocol/argument.hpp>

#include <iostream>
#include <fstream>

int main(int argc, char** argv)
{
  boost::program_options::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("input,i", boost::program_options::value<std::string>(), "input file")
    ("output,o", boost::program_options::value<std::string>(), "output file")
    ;
  
  boost::program_options::variables_map vm;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
  boost::program_options::notify(vm);    

  if (vm.count("help") || !vm.count("input") || !vm.count("output"))
  {
    std::cout << desc << "\n";
    return 1;
  }

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(vm["input"].as<std::string>().c_str());

  std::ofstream out (vm["output"].as<std::string>().c_str(), std::ios::out);

  out << "///////////////////////////////////////////////////////////////////////////////\n";
  out << "//\n";
  out << "// Copyright 2019 Felipe Magno de Almeida.\n";
  out << "// Distributed under the Boost Software License, Version 1.0. (See\n";
  out << "// accompanying file LICENSE_1_0.txt or copy at\n";
  out << "// http://www.boost.org/LICENSE_1_0.txt)\n";
  out << "// See http://www.boost.org/libs/foreach for documentation\n";
  out << "//\n\n";
  out << "#ifndef VWM_WAYLAND_SERVER_PROTOCOL_GENERATED_HPP\n";
  out << "#define VWM_WAYLAND_SERVER_PROTOCOL_GENERATED_HPP\n\n";
  out << "namespace vwm { namespace wayland { namespace generated {\n\n";

  out << "enum class interface_\n";
  out << "{\n";
  out << "  empty,\n";
    
  std::uint32_t interface_index = 0;
  for (auto&& interface_ : doc.child ("protocol").children ("interface"))
    {
      std::cout << "interface " << interface_.attribute("name").value() << " interface index " << interface_index << std::endl;
      out << "  " << interface_.attribute("name").value() << ",\n";
    }

  out << " interface_size\n};\n\n";
  
  out << "template <typename Base>\n";
  out << "struct server_protocol : Base\n";
  out << "{\n";
  out << "  using object_type = typename Base::object_type;\n\n";
  out << "  template <typename...Args>\n";
  out << "  server_protocol (Args&&... args) : Base(args...) {}\n";

  vwm::protocol::generate_process_message (out, doc);

  // events
  for (auto&& interface_ : doc.child ("protocol").children ("interface"))
  {
    unsigned int event_index = 0;
    for (auto&& member : interface_.children ())
    {
      if (!strcmp(member.name(), "event"))
      {
        out << "  void " << interface_.attribute("name").value() << "_" << member.attribute("name").value() << "(uint32_t obj";
        unsigned int arg_size = 0;
        {
          for (auto&& arg : member.children("arg"))
          {
            out << ", ";
            vwm::protocol::generate_arg_decl (out, arg.attribute("type").value(), arg_size++);
          }
        }
        out << ")\n";
        out << "  {\n";

        auto generate_values_suffix_code
          = [] (std::ostream& out, unsigned int fixed_size_values_index, unsigned int fixed_size_values_size
                , unsigned int argument_index, std::optional<unsigned int> new_id_subitem)
            {
              auto tabs = "      ";
              out << tabs << "values1.size += sizeof (values" << fixed_size_values_index << ");\n";
              if (fixed_size_values_index != fixed_size_values_size)
                out << tabs << "values1.size += vwm::wayland::marshall_size(arg" << argument_index << ");\n";
            };
        
        ////
        unsigned int fixed_size_values_size = 1;
        vwm::protocol::generate_values (out, member, arg_size, fixed_size_values_size
                                        , "    "
                                        , "      std::uint32_t from;\n"
                                          "      std::uint16_t opcode;\n"
                                          "      std::uint16_t size;\n"
                                        , "obj, " + std::to_string(event_index) + ", 0"
                                        , generate_values_suffix_code
                                        , true);

        // now lets send
        {
          unsigned int fixed_size_values_index = 1;
          unsigned int i = 0;
          bool is_last = false;
          for (auto&& arg : member.children("arg"))
          {
            auto type = arg.attribute("type").value();
            if (vwm::protocol::is_arg_fixed_size(type))
              ++i;
            else // not fixed
            {
              out << "    std::cout << \"sending \" << sizeof(values" << fixed_size_values_index << ") << std::endl;\n";
              out << "    send (this->get_fd(), &values" << fixed_size_values_index << ", sizeof(values" << fixed_size_values_index << "), 0);\n";
              out << "    vwm::wayland::marshall_send (this->get_fd(), arg" << i << ");\n";
              
              i++;

              is_last = (i == arg_size);
              if (!is_last)
                fixed_size_values_index++;
            }
          }

          if (!is_last)
          {
            out << "    std::cout << \"sending \" << sizeof(values" << fixed_size_values_index << ") << std::endl;\n";
            out << "    send (this->get_fd(), &values" << fixed_size_values_index << ", sizeof(values" << fixed_size_values_index << "), 0);\n";
          }
          out << "    std::cout << \"header has size \" << values1.size << \"\\n\";" << std::endl;
        }

        
        out << "  }\n";
        ++event_index;
      }
    }
  }
  
  out << "};\n\n} } }\n\n";
  out << "#endif\n\n";
}

