/*
 * Entry points of the sub programs routed to by the `ac` master executable
 * (src-port/main.cpp). Each one is the original tool `main()` renamed, so the
 * tools keep parsing their own arguments 1:1.
 */

#pragma once

int map_extractor_main(int argc, char* arg[]);
int mmaps_generator_main(int argc, char** argv);
int vmap4_extractor_main(int argc, char** argv);
int vmap4_assembler_main(int argc, char* argv[]);
