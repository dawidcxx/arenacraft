#!/bin/bash

# Make sure we are runnning with CWD set to source root
[[ -f .devcontainer/tools/init_data.sh ]] || { echo "FAIL: init_data.sh must be run from source root"; exit 1; }

# Require the following programs to be present
# - map_extractor
# - vmap4_extractor
# - vmap4_assembler
# - mmaps_generator
[[ -x $(which map_extractor)  ]] || { echo "FAIL: map_extractor not found"; exit 1; }
[[ -x $(which vmap4_extractor) ]] || { echo "FAIL: vmap4_extractor not found"; exit 1; }
[[ -x $(which vmap4_assembler) ]] || { echo "FAIL: vmap4_assembler not found"; exit 1; }
[[ -x $(which mmaps_generator) ]] || { echo "FAIL: mmaps_generator not found"; exit 1; }

cd /opt/game
start_time=$(date +%s)

# Provide hint to the caller if the game client is not added
if [ ! -f "/opt/game/Wow.exe" ] && [ ! -f "/opt/game/Wow-64.exe" ]; then
    echo "Missing wow files. Please drop your client to {SOURCE_ROOT}/.devcontainer/game";
    echo "Failed to extract client data, exitting..";
    exit 1;
fi

echo "Upon completaion your files will be stored {SOURCE_ROOT}/.devcontainer/server/data";

echo "Would you also like to run mmaps_generator (takes most of the time) [y/N]";
read -r run_mmap
case "$run_mmap" in
    [Yy]|[Yy][Ee][Ss])
        run_mmap=true
        ;;
    *)
        run_mmap=false
        ;;
esac

echo "Press any key to continue...(or Ctrl+C to exit)";
read -n 1 
echo "This will take a while, please be patient.";

rm -rf Buildings cameras dbc db2 maps mmaps vmaps

map_extractor 
echo "Map extraction complete";
vmap4_extractor
echo "VMap extraction complete";
vmap4_assembler 
echo "VMap assembly complete";

if [[ $run_mmap = true ]]; then
    mkdir mmaps
    mmaps_generator --threads $(nproc)  
    echo "MMap generation complete"
fi

echo "Extraction complete, moving files to {SOURCE_ROOT}/.devcontainer/server/data";

sudo mkdir -p /usr/local/skyfire-server/data
sudo mv dbc /usr/local/skyfire-server/data/dbc
sudo mv db2 /usr/local/skyfire-server/data/db2
sudo mv cameras /usr/local/skyfire-server/data/cameras
sudo mv maps /usr/local/skyfire-server/data/maps
sudo mv vmaps /usr/local/skyfire-server/data/vmaps
[[ $run_mmap = true  ]] && sudo mv mmaps /usr/local/skyfire-server/data/mmaps

end_time=$(date +%s)
echo "init_data.sh complete. took $(($end_time - $start_time)) seconds"