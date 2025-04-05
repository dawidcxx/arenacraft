git submodule foreach --recursive | grep "modules/" | while read -r line; do
    submodule=$(echo $line | awk '{print $2}')
    #remove quotes from submodule for example 'my/path' to my/path
    submodule=$(echo $submodule | sed "s/'//g")

    # clean submodule
    git submodule deinit -f -- $submodule
    git rm -f $submodule
    rm -rf .git/modules/$submodule
    

done
