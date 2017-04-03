#!/bin/bash

cd ../master/fbs

counter=0
for i in *.fbs  
do  
    echo $i  
    #echo ${i%.*}".bfbs"  
    #echo ${i%.*}  

    fbname=${i%.*}".pb"  

    flatc -b --schema $i

    #let counter=counter + 1
    counter=$[counter+1]
done

echo "flatbuffers schema file complie finish,files:"$counter