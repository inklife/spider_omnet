#!/bin/bash
PATH_NAME="../benchmarks/circulations/"

num_nodes=("2" "3" "4" "5" "10" "0" "0" "40" "60" "80" "100" "200" "400" "600" "800" "1000" \
    "40" "60" "80" "100" "200" "400" "600" "800" "1000")

balance=100

prefix=("two_node" "three_node" "four_node" "five_node" \
    "hotnets" "lnd_dec4_2018" "lnd_dec28_2018" \
    "sw_40_routers" "sw_60_routers" "sw_80_routers"  \
    "sw_100_routers" "sw_200_routers" "sw_400_routers" "sw_600_routers" \
    "sw_800_routers" "sw_1000_routers"\
    "sf_40_routers" "sf_60_routers" "sf_80_routers"  \
    "sf_100_routers" "sf_200_routers" "sf_400_routers" "sf_600_routers" \
    "sf_800_routers" "sf_1000_routers")

arraylength=${#prefix[@]}
PYTHON="/usr/bin/python"
mkdir -p ${PATH_NAME}

# generate the files
#for (( i=0; i<${arraylength}; i++ ));
array=( 7 16 )
for i in "${array[@]}"
do 
    # generate the graph first to ned file
    workloadname="${prefix[i]}_circ"
    network="${PATH_NAME}${workloadname}_net"
    topofile="${PATH_NAME}${prefix[i]}_topo.txt"
    workload="${PATH_NAME}$workloadname"
    inifile="${PATH_NAME}${workloadname}_default.ini"

    if [ $i -le 3 ]; then
        graph_type="simple_topologies"
    elif [ $i == 4 ]; then
        graph_type="hotnets_topo"
    elif [ $i == 5 ]; then
        graph_type="lnd_dec4_2018"
    elif [ $i == 6 ]; then
        graph_type="lnd_dec28_2018"
    elif [ $i -le 15 ]; then
        graph_type="small_world"
    else
        graph_type="scale_free"
    fi

    echo $network
    echo $topofile
    echo $inifile
    echo $graph_type

    
    $PYTHON create_topo_ned_file.py $graph_type\
            --network-name $network\
            --topo-filename $topofile\
            --num-nodes ${num_nodes[i]}\
            --balance-per-channel $balance\
            --separate-end-hosts


    # create transactions corresponding to this experiment run
    $PYTHON create_workload.py $workload uniform\
            --graph-topo custom\
            --payment-graph-dag-percentage 0\
            --topo-filename $topofile\
            --experiment-time 300\
            --balance-per-channel $balance\
            --generate-json-also\

    # create the ini file
    $PYTHON create_ini_file.py \
            --network-name $network\
            --topo-filename ${topofile:3}\
            --workload-filename ${workload:3}_workload.txt\
            --ini-filename $inifile
done 

