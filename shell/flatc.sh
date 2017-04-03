#!/bin/bash

cd ../master/fbs

counter=0
for i in *.fbs  
do  
    echo $i  
    #echo ${i%.*}".bfbs"  

    flatc -b --schema $i

    counter=$[counter+1]
done

echo "flatbuffers schema file complie finish,files:"$counter