import os
import sys
from config import *

# parse a vector declaration line that maps a vector id to the signal that it is recording
def parse_vec_declaration(line):
    
    words = line.split(" ")
    if words[0] != "vector":
        print "Invalid line no vector"
        return None, "Invalid line"
    
    vector_id = int(words[1])
    owner_node = int(words[2].split("[")[1].split("]")[0])
    owner_node_type = words[2].split(".")[1].split("[")[0]

    signal_info = words[3].split('"')[1].split("(")[0] if'"' in words[3] else words[3].split(":")[0] 
    dest_node = -1
    dest_node_type = None
    if words[3].endswith("router"):
        dest_node = int(words[4]) 
        dest_node_type = "router"
        # the actual node number int(words[5].split("[")[1].split("]")[0])
    elif not signal_info.startswith("completionTime"): # dont know what this is
        if words[4] == 'node':
            dest_node = int(words[5].split(")")[0])
        else: 
            dest_node = int(words[4].split(")")[0])
        dest_node_type = "host"


    return vector_id, (owner_node, owner_node_type, signal_info, dest_node, dest_node_type)


# parse a line that is guaranteed to comprise of 4 columns the first of which is the vector id, second is
# event ID (that we are ignoring right now), third is the timestamp at which the signal
# was recorded and the 4th is the value recorded
def parse_vec_data_line(columns):
    try:
        vec_id = int(columns[0])
        event_id = float(columns[1])
        time = float(columns[2])
        value = float(columns[3])
        return vec_id, (time, value)

    except:
        print "Bad line for reading vec data: ", columns
        return None, None, None

# parse the entire file starting from the declarations making a map of them
# and then parsing the data lines one by one
# everytime store the data in a map, so its easy to extract all the useful signals at the end in one shot
def parse_vec_file(filename, startTime, endTime):
    vec_id_map = dict()
    last_vec_id = -1 
    queueDataPoints = list()
    parameters = ""

    with open(filename) as f:
        for line in f:
            if line.startswith("param"):
                parameters += (line.split("**")[1])
            if line.startswith("vector"):
                vec_id, vec_info = parse_vec_declaration(line)
                vec_id_map[vec_id] = vec_info

            if line[0].isdigit():
                columns = line.split("\t")
                if len(columns) == 4:
                    cur_vec_id = columns[0]
                    if (cur_vec_id != last_vec_id and is_interesting(int(columns[0]), vec_id_map)):
                        cur_vec_id = last_vec_id
                    if cur_vec_id == last_vec_id:
                        data = parse_vec_data_line(columns)
                        if data[0] != None:
                            tv_val = data[1]
                            if((vec_id_map[data[0]][1] == "router") and (vec_id_map[data[0]][4] == "router")):  
                                if ((tv_val[0] >= startTime) and (tv_val[0] <= endTime)):
                                    queueDataPoints.append(tv_val[1])
    return queueDataPoints


# check if this field is actually one of the fields we want to process
def is_interesting(vec_id, vec_id_to_info_map):
    vec_info = vec_id_to_info_map[vec_id]

    for s in ["numInQueue"]:
        if vec_info[2].startswith(s):
            return True
    return False

if __name__ == "__main__":
    vec_filename = sys.argv[1]
    demand = sys.argv[2]
    scheme = sys.argv[3]
    startTime = float(sys.argv[4])
    endTime = float(sys.argv[5])
    out_file = open(sys.argv[6], 'w')
    queueDataPoints = parse_vec_file(vec_filename, startTime, endTime)
    for x in queueDataPoints:
        out_file.write(scheme + "," + demand + "," + str(x) + "\n")
    out_file.close()
    print max(queueDataPoints)
