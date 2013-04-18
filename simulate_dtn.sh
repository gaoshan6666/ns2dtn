#!/bin/bash

for i in 1; do
    new_seed=`expr 12345670 + 2 \* $i - 1`
    ns create-traffic-file.tcl -seed $new_seed -nn 40 -bundlesize 10000 -lifetime 750.0 > trafficgen.tcl
    sleep 5
    nice -n 4 ns bundle-test-large-scen.tcl -seed $new_seed -nn 40 -simtime 5000.0 > dtn.txt
    grep 'Destination' dtn.txt > temp.txt
    awk '{print $6" "$10" "$14" "$18}' temp.txt > bundle_delays.tr
    sed -i 's/, / /g' bundle_delays.tr
    grep 'Source' dtn.txt > temp.txt
    awk '{print $2" "$10" "$14" "$18}' temp.txt > receipt_delays.tr
    sed -i 's/, / /g' receipt_delays.tr
    rm temp.txt
    mv *.tr *.txt trafficgen.tcl Run$i
done