// Minimal.cpp
// Playground for experimenting with zig build options
// and reproduce linkeage errors

#include <boost/program_options.hpp>
#include <fmt/core.h>
#include <fmt/format.h>
#include <iostream>
#include <string>

struct Person
{
  std::string name;
  int         age;
};

// Specialize fmt::formatter for Person
template <> struct fmt::formatter<Person>
{
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.end(); }

  template <typename FormatContext> auto format(const Person& p, FormatContext& ctx) -> decltype(ctx.out())
  {
    return fmt::format_to(ctx.out(), "Person{{name: \"{}\", age: {}}}", p.name, p.age);
  }
};

int main(int argc, char* argv[])
{
  namespace po = boost::program_options;

  std::string name;
  int         age;
  bool        verbose = false;

  try
  {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "produce help message")(
        "name,n", po::value<std::string>(&name)->default_value("Alice"),
        "person's name")("age,a", po::value<int>(&age)->default_value(17),
                         "person's age")("verbose,v", po::bool_switch(&verbose), "enable verbose output");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
      std::cout << desc << std::endl;
      return 0;
    }

    if (verbose)
    {
      fmt::print("Verbose mode enabled\n");
      fmt::print("Creating person with name: {}, age: {}\n", name, age);
    }

    Person p{name, age};
    fmt::print("{}\n", p);
  }
  catch (const po::error& e)
  {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
