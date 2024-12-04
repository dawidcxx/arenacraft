#!/bin/bash

# Make sure we are runnning with CWD set to source root
[[ -f .devcontainer/tools/init_db.sh ]] || { echo "FAIL: init_db.sh must be run from source root"; exit 1; }

mysql -h db -u root -pabc123 < ./sql/create/create_mysql.sql
mysql -h db -u root -pabc123 auth < ./sql/base/auth_database.sql
mysql -h db -u root -pabc123 characters < ./sql/base/characters_database.sql

echo "Database schema init complete";

echo "Would you like to import the world database as well? [y/N]";

read -r import_world
case "$import_world" in
    [Yy]|[Yy][Ee][Ss])
        import_world=true
        ;;
    *)
        import_world=false
        ;;
esac

if [[ $import_world = true ]]; then
    cd /tmp

    latest_release=$(
        curl -X 'GET' 'https://codeberg.org/api/v1/repos/ProjectSkyfire/database/releases' \
             -H 'accept: application/json' \
             --silent \
        | jq -r '{url: .[0].assets[0].browser_download_url, name:.[0].assets[0].name }'
    )
    name=$(echo $latest_release | jq -r '.name')
    url=$(echo $latest_release | jq -r '.url')
    echo "Downloading latest release: $name ($url)"
    
    wget $url -O $name
    unzip -o $name  

    output_directory_name="${name%.zip}"
    cd $output_directory_name
    cd main_db/world
    sql_files=$(ls *.sql)

    for sql_file in $sql_files; do
        echo "Importing $sql_file"
        mysql -h db -u root -pabc123 world < $sql_file
    done

    echo "World database import complete";

fi