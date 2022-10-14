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
zns_single_iops_stdev = [5488.530381, 7544.770434, 8465.096610, 8595.080944, 5759.664473, 8823.541888, 4246.134996, 4855.189992]
f2fs_single_stream_iodepth_iops = [122028.632379, 122248.591714, 128583.647212, 125201.659945, 126796.840105, 130497.616746, 131037.632079, 128273.257558]
f2fs_single_stream_iodepth_stdev = [12368.192458, 11794.008125, 7516.438832, 6229.119071, 6707.977135, 6472.080003, 5949.793649, 6534.907876]
f2fs_single_stream_concur_iops = [131843.056436, 136349.526327, 135921.650850, 133116.032439, 134818.113044, 139030.937320, 137183.438503, 137339.665567]
f2fs_single_stream_concur_stdev = [4591.014908, 3018.424039, 3192.285968, 3194.415281, 3642.581020, 2896.757127, 5354.782823, 4379.984932]

if __name__ == '__main__':
    fig, ax = plt.subplots()

    xticks = np.arange(1,9,1)
    queue_depths = np.arange(8)

    zns_data = [val / 1000 for val in zns_single_zone_iops]
    zns_stdev = [val / 1000 for val in zns_single_iops_stdev]
    f2fs_iodepth_data = [val / 1000 for val in f2fs_single_stream_iodepth_iops]
    f2fs_iodepth_stdev = [val / 1000 for val in f2fs_single_stream_iodepth_stdev]
    f2fs_concur_data = [val / 1000 for val in f2fs_single_stream_concur_iops]
    f2fs_concur_stdev = [val / 1000 for val in f2fs_single_stream_concur_stdev]

    ax.errorbar(xticks, zns_data, yerr=zns_stdev, markersize=4, capsize=3, marker='x', label="ZNS")
    ax.errorbar(xticks, f2fs_iodepth_data, yerr=f2fs_iodepth_stdev, markersize=4, capsize=3, marker='o', label="f2fs-single-thread")
    ax.errorbar(xticks, f2fs_concur_data, yerr=f2fs_concur_stdev, markersize=4, capsize=3, marker='v', label="f2fs-concurrent")

    fig.tight_layout()
    ax.grid(which='major', linestyle='dashed', linewidth='1')
    ax.set_axisbelow(True)
    ax.legend(loc='lower right')
    ax.xaxis.set_ticks(np.arange(1,9,1))
    ax.xaxis.set_ticklabels(xticks)
    ax.set_ylim(bottom=0, top=170)
    ax.set_ylabel("KIOPS")
    ax.set_xlabel("Outstanding I/Os")
    plt.savefig(f"figs/f2fs-write_single_stream.pdf", bbox_inches="tight")
    plt.savefig(f"figs/f2fs-write_single_stream.png", bbox_inches="tight")
    plt.clf()
           
