#!/usr/bin/env python3

import matplotlib.pyplot as plt
import math
import numpy as np
import matplotlib.patches as mpatches
import os
import glob
import json

plt.rc('font', size=12)          # controls default text sizes
plt.rc('axes', titlesize=12)     # fontsize of the axes title
plt.rc('axes', labelsize=12)    # fontsize of the x and y labels
plt.rc('xtick', labelsize=12)    # fontsize of the tick labels
plt.rc('ytick', labelsize=12)    # fontsize of the tick labels
plt.rc('legend', fontsize=12)    # legend fontsize

zns_single_zone_data = dict()
zns_two_zone_data = dict()
zns_three_zone_data = dict()

def parse_fio_data(data_path, data):
    if not os.path.exists(f'{data_path}') or \
            os.listdir(f'{data_path}') == []: 
        print(f'No data in {data_path}')
        return 0 

    for file in glob.glob(f'{data_path}/*'): 
        with open(file, 'r') as f:
            for index, line in enumerate(f, 1):
                # Removing all fio logs in json file by finding first {
                if line.split()[0] == '{':
                    rows = f.readlines()
                    with open(os.path.join(os.getcwd(), 'temp.json'), 'w+') as temp:
                        temp.write(line)
                        temp.writelines(rows)
                    break
        with open(os.path.join(os.getcwd(), 'temp.json'), 'r') as temp:
            data[file] = dict()
            data[file] = json.load(temp)
            os.remove(os.path.join(os.getcwd(), 'temp.json'))

    return 1

def plot_throughput():
    xticks = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024]
    queue_depths = np.arange(11)
    zns_szd = [None] * len(queue_depths)
    zns_szd_stdev = [None] * len(queue_depths)
    zns_dzd = [None] * len(queue_depths)
    zns_dzd_stdev = [None] * len(queue_depths)
    zns_tzd = [None] * len(queue_depths)
    zns_tzd_stdev = [None] * len(queue_depths)

    for key, item in zns_single_zone_data.items():
        zns_szd[int(math.log2(int(item['jobs'][0]['job options']['iodepth'])))] = item['jobs'][0]['write']['iops']/1000
        zns_szd_stdev[int(math.log2(int(item['jobs'][0]['job options']['iodepth'])))] = item['jobs'][0]['write']['iops_stddev']/1000

    for key, item in zns_two_zone_data.items():
        zns_dzd[int(math.log2(int(item['jobs'][0]['job options']['iodepth'])))] = item['jobs'][0]['write']['iops']/1000
        zns_dzd_stdev[int(math.log2(int(item['jobs'][0]['job options']['iodepth'])))] = item['jobs'][0]['write']['iops_stddev']/1000

    for key, item in zns_three_zone_data.items():
        zns_tzd[int(math.log2(int(item['jobs'][0]['job options']['iodepth'])))] = item['jobs'][0]['write']['iops']/1000
        zns_tzd_stdev[int(math.log2(int(item['jobs'][0]['job options']['iodepth'])))] = item['jobs'][0]['write']['iops_stddev']/1000

    fig, ax = plt.subplots()

    ax.errorbar(queue_depths, zns_szd, yerr=zns_szd_stdev, markersize=4, capsize=3, marker='x', label='ZNS 1 Zone')
    ax.errorbar(queue_depths, zns_dzd, yerr=zns_dzd_stdev, markersize=4, capsize=3, marker='o', label='ZNS 2 Zones')
    ax.errorbar(queue_depths, zns_tzd, yerr=zns_tzd_stdev, markersize=4, capsize=3, marker='o', label='ZNS 3 Zones')

    fig.tight_layout()
    ax.grid(which='major', linestyle='dashed', linewidth='1')
    ax.set_axisbelow(True)
    ax.legend(loc='lower right')
    ax.xaxis.set_ticks(queue_depths)
    ax.xaxis.set_ticklabels(xticks)
    ax.set_ylim(bottom=0)
    ax.set_ylabel('KIOPS')
    ax.set_xlabel('Outstanding I/Os')
    plt.savefig(f'figs/zns_zone_iops.pdf', bbox_inches='tight')
    plt.savefig(f'figs/zns_zone_iops.png', bbox_inches='tight')
    plt.clf()

if __name__ == '__main__':
    file_path = '/'.join(os.path.abspath(__file__).split('/')[:-1])

    parse_fio_data(f'{file_path}/zns_single_zone_iodepth/', zns_single_zone_data)
    parse_fio_data(f'{file_path}/zns_two_zone_iodepth/', zns_two_zone_data)
    parse_fio_data(f'{file_path}/zns_three_zone_iodepth/', zns_three_zone_data)

    plot_throughput()
