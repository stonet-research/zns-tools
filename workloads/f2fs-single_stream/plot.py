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

f2fs_iodepth = dict()
f2fs_concurrent = dict()

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
    xticks = [1, 2, 3, 4, 5, 6, 7, 8]
    queue_depths = np.arange(8)
    f2fs_iod = [None] * len(queue_depths)
    f2fs_iod_stdev = [None] * len(queue_depths)
    f2fs_concur = [None] * len(queue_depths)
    f2fs_concur_stdev = [None] * len(queue_depths)

    for key, item in f2fs_iodepth.items():
        f2fs_iod[(int(item['jobs'][0]['job options']['iodepth'])) - 1] = item['jobs'][0]['write']['iops']/1000
        f2fs_iod_stdev[int(item['jobs'][0]['job options']['iodepth']) - 1] = item['jobs'][0]['write']['iops_stddev']/1000

    for key, item in f2fs_concurrent.items():
        f2fs_concur[int(item['jobs'][0]['job options']['thread']) - 1] = item['jobs'][0]['write']['iops']/1000
        f2fs_concur_stdev[int(item['jobs'][0]['job options']['thread']) - 1] = item['jobs'][0]['write']['iops_stddev']/1000

    fig, ax = plt.subplots()

    ax.errorbar(queue_depths, f2fs_iod, yerr=f2fs_iod_stdev, markersize=4, capsize=3, marker='x', label='F2FS single file (io_uring)')
    ax.errorbar(queue_depths, f2fs_concur, yerr=f2fs_concur_stdev, markersize=4, capsize=3, marker='o', label='F2FS concurrent files (psync)')

    # Plotting horizontal lines for max throughput on ZNS
    plt.axhline(y = 257364.462305/1000, color = 'r', linestyle = ':', label = "ZNS 1 Zone")
    plt.axhline(y = 459717.450472/1000, color = 'gray', linestyle = 'dashed', label = "ZNS 2 Zones")
    plt.axhline(y = 378353.052926/1000, color = 'green', linestyle = 'dashdot', label = "ZNS 3 Zones")

    fig.tight_layout()
    # ax.grid(which='major', linestyle='dashed', linewidth='1')
    ax.set_axisbelow(True)
    ax.legend(loc='upper right')
    ax.xaxis.set_ticks(queue_depths)
    ax.xaxis.set_ticklabels(xticks)
    ax.set_ylim(bottom=0)
    ax.set_ylabel('KIOPS')
    ax.set_xlabel('Outstanding I/Os')
    plt.savefig(f'figs/f2fs_single_write.pdf', bbox_inches='tight')
    plt.savefig(f'figs/f2fs_single_write.png', bbox_inches='tight')
    plt.clf()

if __name__ == '__main__':
    file_path = '/'.join(os.path.abspath(__file__).split('/')[:-1])

    parse_fio_data(f'{file_path}/f2fs_single_stream_iodepth/', f2fs_iodepth)
    parse_fio_data(f'{file_path}/f2fs_single_stream_concur/', f2fs_concurrent)

    plot_throughput()
