#!/bin/bash

set -e
set -o pipefail

cd ../master/pb

counter=0
for i in *.proto  
do  
    echo compiling $i ...
    #echo ${i%.*}".pb"  

    # --descriptor_set_out
    protoc -o ${i%.*}".pb" $i

    counter=$[counter+1]
done

echo "protobuf schema file complie finish,files:"$counter