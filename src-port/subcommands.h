/*
 * Entry points of the sub programs routed to by the `ac` master executable
 * (src-port/main.cpp). Each one is the original tool/server `main()` renamed,
 * so the programs keep parsing their own arguments 1:1.
 */

#pragma once

// tools
int map_extractor_main(int argc, char* arg[]);
int mmaps_generator_main(int argc, char** argv);
int vmap4_extractor_main(int argc, char** argv);
int vmap4_assembler_main(int argc, char* argv[]);

// servers
int authserver_main(int argc, char** argv);
int worldserver_main(int argc, char** argv);
