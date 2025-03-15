#!/bin/bash

source_dir="$1"
target_file="$2"

rm -f $target_file

for file in $1/*.sql; do
    var_name=$(basename $file | sed 's/\.sql//g' | sed 's/[^a-zA-Z0-9]/_/g')
    echo "constexpr char migration_$var_name[] = {" >> $target_file
    echo "#embed \"$file\"" >> $target_file
    echo "};" >> $target_file
done

echo "std::array migrations = {" >> $target_file
for file in $1/*.sql; do
    var_name=$(basename $file | sed 's/\.sql//g' | sed 's/[^a-zA-Z0-9]/_/g')
    echo "std::string_view(migration_$var_name)," >> $target_file
done
echo "};" >> $target_file
