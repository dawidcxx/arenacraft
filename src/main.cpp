/*
 * `ac` - master executable of the zig-build port.
 *
 * Architecture: this router validates the requested sub program with argparse
 * and forwards the remaining arguments 1:1 to the original entry points
 * (src/subcommands.h). The sub programs keep their own argument parsing,
 * so they behave exactly like the old standalone executables did - including
 * usage strings, since the forwarded argv[0] is the sub command name.
 *
 *   ./zig-out/bin/ac authserver <original authserver args>
 *   ./zig-out/bin/ac map_extractor <original map_extractor args>
 */

#include <argparse/argparse.hpp>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "subcommands.h"

extern "C" int mallctl(char const* name, void* oldp, size_t* oldlenp, void* newp, size_t newlen);

static void TuneAllocator()
{
    bool background_thread = true;
    mallctl("background_thread", nullptr, nullptr, &background_thread, sizeof(background_thread));

    ssize_t decay_ms = 2000;
    mallctl("dirty_decay_ms", nullptr, nullptr, &decay_ms, sizeof(decay_ms));
    mallctl("muzzy_decay_ms", nullptr, nullptr, &decay_ms, sizeof(decay_ms));
}

int main(int argc, char** argv)
{
    TuneAllocator();

    argparse::ArgumentParser program("ac", "1.0");
    program.add_description("AzerothCore unified executable (zig-build port)");
    program.add_argument("command").metavar("COMMAND").help(
        "sub program to run: authserver | worldserver | map_extractor | mmaps_generator | vmap4_extractor | vmap4_assembler");

    if (argc < 2)
    {
        std::cout << program << std::endl;
        return 0;
    }

    try
    {
        // only the sub command name is parsed here, everything after it is
        // forwarded untouched to the sub program's own parser; the vendored
        // argparse expects argv[0] (program name) as the first vector element
        program.parse_args(std::vector<std::string>{argv[0], argv[1]});
    }
    catch (const std::exception& err)
    {
        std::cerr << err.what() << std::endl;
        std::cerr << program << std::endl;
        return 1;
    }

    std::string const command = program.get<std::string>("command");

    using EntryPoint = int (*)(int, char**);
    std::map<std::string, EntryPoint> const entry_points = {
        {"authserver", &authserver_main},
        {"worldserver", &worldserver_main},
        {"map_extractor", &map_extractor_main},
        {"mmaps_generator", &mmaps_generator_main},
        {"vmap4_extractor", &vmap4_extractor_main},
        {"vmap4_assembler", &vmap4_assembler_main},
    };

    auto it = entry_points.find(command);
    if (it == entry_points.end())
    {
        std::cerr << "unknown command '" << command << "'" << std::endl;
        std::cerr << program << std::endl;
        return 1;
    }

    // shift argv so the sub program sees its own name as argv[0] and keep
    // the remaining arguments untouched (1:1 behavior with the old binaries)
    return it->second(argc - 1, argv + 1);
}
