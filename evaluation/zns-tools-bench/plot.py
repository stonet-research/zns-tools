#!/usr/bin/env python3

import matplotlib.pyplot as plt
import matplotlib
import numpy as np

matplotlib.rcParams['ps.useafm'] = True
matplotlib.rcParams['pdf.use14corefonts'] = True
matplotlib.rcParams['text.usetex'] = True

text_font_size = 13
marker_font_size = 11
label_font_size = 12
axes_font_size = 12

plt.rc('font', size=text_font_size)         
plt.rc('axes', labelsize=axes_font_size)    
plt.rc('xtick', labelsize=label_font_size)    
plt.rc('ytick', labelsize=label_font_size)    
plt.rc('legend', fontsize=label_font_size)

def plot_runtime():
    fig, ax = plt.subplots()
    data = [1.46, 3.40, 349.32, 218075]

    ax.bar(np.arange(len(data)), data, color="#117733") 

    fig.tight_layout()
    ax.set_axisbelow(True)
    ax.yaxis.grid(which='major', linestyle='dashed', linewidth='1')
    plt.yscale("log")
    ax.xaxis.set_ticks(np.arange(len(data)))
    ax.xaxis.set_ticklabels(["100", "1K", "10K", "25K"])
    ax.set_ylim(bottom=0)
    ax.set_ylabel("runtime (seconds)")
    ax.set_xlabel("Number of files")
    plt.savefig(f'runtime.pdf', bbox_inches='tight')
    plt.clf()


if __name__ == "__main__":
    plot_runtime()
