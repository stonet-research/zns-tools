#!/usr/bin/env python3

import matplotlib.pyplot as plt
import math
import numpy as np
import matplotlib.patches as mpatches

plt.rc('font', size=12)          # controls default text sizes
plt.rc('axes', titlesize=12)     # fontsize of the axes title
plt.rc('axes', labelsize=12)    # fontsize of the x and y labels
plt.rc('xtick', labelsize=12)    # fontsize of the tick labels
plt.rc('ytick', labelsize=12)    # fontsize of the tick labels
plt.rc('legend', fontsize=12)    # legend fontsize

zns_single_zone_iops = [154590.113663, 152024.832506, 149454.951502, 152415.186160, 151644.945168, 148789.840339, 154365.121163, 153500.149995]
f2fs_single_stream_iodepth_iops = [122028.632379, 122248.591714, 128583.647212, 125201.659945, 126796.840105, 130497.616746, 131037.632079, 128273.257558]
f2fs_single_stream_concur_iops = [131843.056436, 136349.526327, 135921.650850, 133116.032439, 134818.113044, 139030.937320, 137183.438503, 137339.665567]

if __name__ == '__main__':
    fig, ax = plt.subplots()

    xticks=np.arange(0,8,1)
    queue_depths = np.arange(8)

    zns_data = [val / 1000 for val in zns_single_zone_iops]
    f2fs_iodepth_data = [val / 1000 for val in f2fs_single_stream_iodepth_iops]
    f2fs_concur_data = [val / 1000 for val in f2fs_single_stream_concur_iops]

    ax.plot(xticks, zns_data, markersize=4, marker='x', label="ZNS")
    ax.plot(xticks, f2fs_iodepth_data, markersize=4, marker='o', label="f2fs-single-thread")
    ax.plot(xticks, f2fs_concur_data, markersize=4, marker='v', label="f2fs-concurrent")
    
    fig.tight_layout()
    ax.grid(which='major', linestyle='dashed', linewidth='1')
    ax.set_axisbelow(True)
    ax.legend(loc='lower right')
    ax.xaxis.set_ticks(np.arange(0,8,1))
    ax.xaxis.set_ticklabels(queue_depths)
    ax.set_ylim(bottom=0)
    ax.set_ylabel("KIOPS")
    ax.set_xlabel("Outstanding I/Os")
    plt.savefig(f"figs/f2fs-write_single_stream.pdf", bbox_inches="tight")
    plt.savefig(f"figs/f2fs-write_single_stream.png", bbox_inches="tight")
    plt.clf()
           
