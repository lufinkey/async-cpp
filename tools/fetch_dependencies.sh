#!/bin/sh

# enter base directory
base_dir=$(dirname "${BASH_SOURCE[0]}")
cd "$base_dir/../"
base_dir="$PWD"

# create external dir
mkdir -p external || exit $?

# clone dtl
if [ ! -e "external/dtl/.git" ]; then
    echo "fetching dtl"
    git clone --recursive "https://github.com/cubicdaiya/dtl" "external/dtl" || exit $?
fi
