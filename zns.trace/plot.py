#! /usr/bin/python3

import sys, getopt
import os
import glob
import math
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt

plt.rc('font', size=12)          # controls default text sizes
plt.rc('axes', titlesize=12)     # fontsize of the axes title
plt.rc('axes', labelsize=12)     # fontsize of the x and y labels
plt.rc('xtick', labelsize=12)    # fontsize of the tick labels
plt.rc('ytick', labelsize=12)    # fontsize of the tick labels
plt.rc('legend', fontsize=12)    # legend fontsize

ZONE_SIZE = 0
NR_ZONES = 0
LAT_CONV=10**3 # Latency is stored in nsec, convert to μsec (change to 10**6 for msec)

def main(argv):
    try:
        opts, args = getopt.getopt(argv,"hs:z:",["zone_size=","nr_zones"])
    except getopt.GetoptError:
        print('Error. Usage: plot.py -s [ZONE_SIZE (in 512B sectors)] [-z NR_ZONES]')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print('Error. Usage: plot.py -s [ZONE_SIZE (in 512B sectors)] [-z NR_ZONES]')
            sys.exit()
        elif opt in ("-s", "--zone_size"):
            global ZONE_SIZE
            ZONE_SIZE = int(arg)
        elif opt in ("-z", "--nr_zones"):
            global NR_ZONES
            NR_ZONES = int(arg)

def plot_z_op_map(data, type):
    """
    This is intended to plot the data for z_data_map and z_rw_ctr_map as these depend on being
    indexed by the zone index and the operation type (read or write)
    """

    dimension = math.floor(NR_ZONES ** 0.5)
    remainder = NR_ZONES - (dimension ** 2)
    
    if remainder == 0:
        read_data = np.zeros(shape=(dimension, dimension))
        write_data = np.zeros(shape=(dimension, dimension))
        perfect_square = True
    else:
        read_data = np.zeros(shape=(dimension + 1, dimension))
        write_data = np.zeros(shape=(dimension + 1, dimension))
        perfect_square = False

        for val in range(dimension - remainder):
            read_data[-1, -1 - val] = None
            write_data[-1, -1 - val] = None
    
    for key, entry in data.items():
        x_ind = key % dimension 
        read_data[math.floor(key/dimension)][x_ind] = entry["read"]
        write_data[math.floor(key/dimension)][x_ind] = entry["write"]
 
    if type == "z_data_map":
        ax = sns.heatmap(read_data, linewidth=0.5, xticklabels=False, cmap="rocket_r", cbar_kws={'format': '%d Blocks (512B)'})
    else:
        ax = sns.heatmap(read_data, linewidth=0.5, xticklabels=False, cmap="rocket_r")
    plt.ylim(0, dimension + remainder)
    plt.xlim(0, dimension)

    ticks = np.arange(0, dimension)
    ax.set_yticks(np.arange(0.5, dimension))

    ticks = [f"zones {tick*dimension+1}-{(tick+1)*dimension}" for tick in ticks]

    # fix last tick labe if NR_ZONES is not perfect square
    # e.g. if 32 zones, dimension will be 6x6=36 but last tick should say zones 30-32
    # if perfect_square == False:
    #     ticks[-1] = f"zones {dimension*(dimension-1)+1}-{(dimension**2-((dimension**2)%NR_ZONES))}"

    ax.set_yticklabels(ticks)
    plt.yticks(rotation=0)
    plt.savefig(f"{file_path}/figs/{file_name}/read-{type}-heatmap.pdf", bbox_inches="tight")
    plt.title(f"{type} read")
    plt.savefig(f"{file_path}/figs/{file_name}/read-{type}-heatmap.png", bbox_inches="tight")
    plt.clf()

    if type == "z_data_map":
        ax = sns.heatmap(write_data, linewidth=0.5, xticklabels=False, cmap="rocket_r", cbar_kws={'format': '%d LBAs'})
    else:
        ax = sns.heatmap(write_data, linewidth=0.5, xticklabels=False, cmap="rocket_r")
    plt.ylim(0, dimension + remainder)
    plt.xlim(0, dimension)

    ticks = np.arange(0, dimension)
    ax.set_yticks(np.arange(0.5, dimension))

    ticks = [f"zones {tick*dimension+1}-{(tick+1)*dimension}" for tick in ticks]

    # if perfect_square == False:
        # ticks[-1] = f"zones {dimension*(dimension-1)+1}-{(dimension**2-((dimension**2)%NR_ZONES))}"

    ax.set_yticklabels(ticks)
    plt.yticks(rotation=0)
    plt.savefig(f"{file_path}/figs/{file_name}/write-{type}-heatmap.pdf", bbox_inches="tight")
    plt.title(f"{type} write")
    plt.savefig(f"{file_path}/figs/{file_name}/write-{type}-heatmap.png", bbox_inches="tight")
    plt.clf()

def plot_z_reset_ctr_map(data):
    dimension = math.floor(NR_ZONES ** 0.5)
    remainder = NR_ZONES - (dimension ** 2)
    
    if remainder == 0:
        plt_data = np.zeros(shape=(dimension, dimension))
        # perfect_square = True
    else:
        dimension += 1
        plt_data = np.zeros(shape=(dimension, dimension))
        plt_data[plt_data == 0] = -1
        # perfect_square = False
        difference = abs(NR_ZONES - dimension ** 2)

        for val in range(difference):
            plt_data[-1, -1 - val] = None
    
    for key, entry in data.items():
        x_ind = key % dimension 
        plt_data[math.floor(key/dimension)][x_ind] = entry

    print(plt_data)
    
    cmap = sns.color_palette('rocket_r', as_cmap=True).copy()
    cmap.set_under('#88CCEE')
    ax = sns.heatmap(plt_data, linewidth=0.1, xticklabels=False, cmap=cmap, mask=(plt_data == None), yticklabels=False, clip_on=False, cbar_kws={'shrink': 0.8, 'extend': 'min', 'extendrect': True}, square=True, cbar=True, vmin=0)
    ax.set_facecolor("white")
    plt.ylim(0, dimension)
    plt.xlim(0, dimension)

    # ticks = np.arange(0, dimension)
    # ax.set_yticks(np.arange(0.5, dimension))

    # ticks = [f"zones {tick*dimension+1}-{(tick+1)*dimension}" for tick in ticks]

    # if perfect_square == False:
    #     ticks[-1] = f"zones {dimension*(dimension-1)+1}-{(dimension**2-((dimension**2)%NR_ZONES))}"

    # ax.set_yticklabels(ticks)
    plt.yticks(rotation=0)
    plt.savefig(f"{file_path}/figs/{file_name}/z_reset_ctr_map-heatmap.pdf", bbox_inches="tight")
    plt.title("z_reset_ctr_map")
    plt.savefig(f"{file_path}/figs/{file_name}/z_reset_ctr_map-heatmap.png", bbox_inches="tight")
    plt.clf()

def plot_z_reset_lat_map(data):
    dimension = math.floor(NR_ZONES ** 0.5)
    remainder = NR_ZONES - (dimension ** 2)
    
    if remainder == 0:
        plt_data = np.zeros(shape=(dimension, dimension))
        perfect_square = True
    else:
        plt_data = np.zeros(shape=(dimension + 1, dimension))
        perfect_square = False

        for val in range(dimension - remainder):
            plt_data[-1, -1 - val] = None
    
    for key, entry in data.items():
        total = 0
        counter = 0
        for k, val in entry.items():
            total += val
            counter += 1

        x_ind = key % dimension 
        if LAT_CONV == 10**3:
            plt_data[math.floor(key/dimension)][x_ind] = total/counter/10**3 # convert to μsec
        elif LAT_CONV == 10**6:
            plt_data[math.floor(key/dimension)][x_ind] = total/counter/10**6 # convert to msec

    if LAT_CONV == 10**3:
        ax = sns.heatmap(plt_data, linewidth=0.5, xticklabels=False, cmap="rocket_r", cbar_kws={'format': '%d μsec'})
    else:
        ax = sns.heatmap(plt_data, linewidth=0.5, xticklabels=False, cmap="rocket_r", cbar_kws={'format': '%d msec'})
    plt.ylim(0, dimension + remainder)
    plt.xlim(0, dimension)

    ticks = np.arange(0, dimension)
    ax.set_yticks(np.arange(0.5, dimension))

    ticks = [f"zones {tick*dimension+1}-{(tick+1)*dimension}" for tick in ticks]

    # if perfect_square == False:
        # ticks[-1] = f"zones {dimension*(dimension-1)+1}-{(dimension**2-((dimension**2)%NR_ZONES))}"

    ax.set_yticklabels(ticks)
    plt.yticks(rotation=0)
    plt.savefig(f"{file_path}/figs/{file_name}/z_reset_lat_map-heatmap.pdf", bbox_inches="tight")
    plt.title("z_reset_ctr_map")
    plt.savefig(f"{file_path}/figs/{file_name}/z_reset_lat_map-heatmap.png", bbox_inches="tight")
    plt.clf()

def plot_avg_io_size(data, counter):
    """
    Calculate and plot the average I/O size per zone.
    """

    dimension = math.floor(NR_ZONES ** 0.5)
    remainder = NR_ZONES - (dimension ** 2)
    
    if remainder == 0:
        read_data = np.zeros(shape=(dimension, dimension))
        write_data = np.zeros(shape=(dimension, dimension))
        perfect_square = True
    else:
        read_data = np.zeros(shape=(dimension + 1, dimension))
        write_data = np.zeros(shape=(dimension + 1, dimension))
        perfect_square = False

        for val in range(dimension - remainder):
            read_data[-1, -1 - val] = None
            write_data[-1, -1 - val] = None

    for key, entry in data.items():
        x_ind = key % dimension 
        if entry["read"] != 0:
            read_data[math.floor(key/dimension)][x_ind] = int(entry["read"])/int(counter[key]["read"])
        if entry["write"] != 0:
            write_data[math.floor(key/dimension)][x_ind] = int(entry["write"])/int(counter[key]["write"])

    ax = sns.heatmap(read_data, linewidth=0.5, xticklabels=False, cmap="rocket_r", cbar_kws={'format': '%d LBAs'})
    plt.ylim(0, dimension + remainder)
    plt.xlim(0, dimension)

    ticks = np.arange(0, dimension)
    ax.set_yticks(np.arange(0.5, dimension))

    ticks = [f"zones {tick*dimension+1}-{(tick+1)*dimension}" for tick in ticks]

    # if perfect_square == False:
    #     ticks[-1] = f"zones {dimension*(dimension-1)+1}-{(dimension**2-((dimension**2)%NR_ZONES))}"

    ax.set_yticklabels(ticks)
    plt.yticks(rotation=0)
    plt.savefig(f"{file_path}/figs/{file_name}/read-avg_io_size-heatmap.pdf", bbox_inches="tight")
    plt.title(f"avg I/O size read")
    plt.savefig(f"{file_path}/figs/{file_name}/read-avg_io_size-heatmap.png", bbox_inches="tight")
    plt.clf()

    ax = sns.heatmap(write_data, linewidth=0.5, xticklabels=False, cmap="rocket_r", cbar_kws={'format': '%d LBAs'})
    plt.ylim(0, dimension + remainder)
    plt.xlim(0, dimension)

    ticks = np.arange(0, dimension)
    ax.set_yticks(np.arange(0.5, dimension))

    ticks = [f"zones {tick*dimension+1}-{(tick+1)*dimension}" for tick in ticks]

    # if perfect_square == False:
    #     ticks[-1] = f"zones {dimension*(dimension-1)+1}-{(dimension**2-((dimension**2)%NR_ZONES))}"

    ax.set_yticklabels(ticks)
    plt.yticks(rotation=0)
    plt.savefig(f"{file_path}/figs/{file_name}/write-avg_io_size-heatmap.pdf", bbox_inches="tight")
    plt.title(f"avg I/O size write")
    plt.savefig(f"{file_path}/figs/{file_name}/write-avg_io_size-heatmap.png", bbox_inches="tight")
    plt.clf()

if __name__ == "__main__":
    main(sys.argv[1:])
    file_path = '/'.join(os.path.abspath(__file__).split('/')[:-1])

    if ZONE_SIZE == 0:
        print("Error. Invalid Zone Size")
        sys.exit()
    if NR_ZONES == 0:
        print("Error. Invalid number of Zones")
        sys.exit()

    for file in glob.glob(f"{file_path}/data/*"):
        file_name = file.split('/')[-1]

        # Don't replot existing figures
        # if os.path.isdir(f"{file_path}/figs/{file_name}/"):
        #     # pass
        # else:
        os.makedirs(f"{file_path}/figs/{file_name}", exist_ok=True)
        with open(f"{file_path}/data/{file_name}") as data_file:
            # bpftrace sorts based on the value in ascending order, 
            # therefore our dict will be out of order, which will be
            # irrelevant for plotting if we use a their keys as indices
            data = dict()
            data["z_data_map"] = dict()
            data["z_rw_ctr_map"] = dict()
            data["z_reset_ctr_map"] = dict()
            data["z_reset_lat_map"] = dict()
            for line in data_file:
                line = line[1:]
                if "logging" in line:
                    pass
                elif "reset_all_ctr" in line:
                    data["reset_all_ctr"] = int(line.split(" ")[-1])
                elif "z_data_map" in line:
                    line = line[11:]
                    line = line.split(",")
                    # index zone starting with 1 not 0
                    zone_index = math.floor(int(line[0])/ZONE_SIZE)
                    if zone_index not in data["z_data_map"]:
                        data["z_data_map"][zone_index] = dict()
                        # Initialize if they are never used
                        data["z_data_map"][zone_index]["read"] = 0
                        data["z_data_map"][zone_index]["write"] = 0

                    operation = int(line[1].split("]")[0].strip())
                    if operation == 1:
                        OP = "write"
                    elif operation == 2:
                        OP = "read"
                    else:
                        print('Error. Invalid opeartion in z_data_map')
                        sys.exit()
                    data["z_data_map"][zone_index][OP] = line[1].split(" ")[2].strip()
                elif "z_rw_ctr_map" in line:
                    line = line[13:]
                    line = line.split(",")
                    zone_index = math.floor(int(line[0])/ZONE_SIZE)
                    if zone_index not in data["z_rw_ctr_map"]:
                        data["z_rw_ctr_map"][zone_index] = dict()
                        # Initialize if they are never used
                        data["z_rw_ctr_map"][zone_index]["read"] = 0
                        data["z_rw_ctr_map"][zone_index]["write"] = 0

                    operation = int(line[1].split("]")[0].strip())
                    if operation == 1:
                        OP = "write"
                    elif operation == 2:
                        OP = "read"
                    else:
                        print('Error. Invalid opeartion in z_data_map')
                        sys.exit()
                    data["z_rw_ctr_map"][zone_index][OP] = line[1].split(" ")[2].strip()
                elif "z_reset_ctr_map" in line:
                    line = line[16:]
                    line = line.split(",")
                    zone_index = math.floor(int(line[0].split("]")[0])/ZONE_SIZE)
                    data["z_reset_ctr_map"][zone_index] = line[0].split(" ")[1].strip()
                elif "z_reset_lat_map" in line:
                    line = line[16:]
                    line = line.split(",")
                    zone_index = math.floor(int(line[0].split("]")[0])/ZONE_SIZE)
                    if zone_index not in data["z_reset_lat_map"]:
                        data["z_reset_lat_map"][zone_index] = dict()

                    reset_cnt = line[1].split("]")[0].strip()
                    if reset_cnt not in data["z_reset_lat_map"][zone_index]:
                        data["z_reset_lat_map"][zone_index][reset_cnt] = dict()
                    lat = int(line[1].split(":")[1].strip())
                    data["z_reset_lat_map"][zone_index][reset_cnt] = lat

            # plot_z_op_map(data["z_data_map"], "z_data_map")
            # plot_z_op_map(data["z_rw_ctr_map"], "z_rw_ctr_map")
            plot_z_reset_ctr_map(data["z_reset_ctr_map"])
            # plot_z_reset_lat_map(data["z_reset_lat_map"])
            # plot_avg_io_size(data["z_data_map"], data["z_rw_ctr_map"])
