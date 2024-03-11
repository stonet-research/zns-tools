import matplotlib.pyplot as plt
import math

# Color rules (based on https://personal.sron.nl/~pault/)
GREEN = "#117733"
TEAL = "#44AA99"
CYAN = "#88CCEE"
OLIVE = "#999933"
SAND = "#DDCC77"
ROSE = "#CC6677"
BLUE = "#88CCEE"
MAGENTA = "#AA4499"
GREY = GRAY = "#DDDDDD"


def set_font(size):
    text_font_size = size
    marker_font_size = size
    label_font_size = size
    axes_font_size = size

    plt.rc('text', usetex=True)
    plt.rc('pdf', use14corefonts=True, fonttype=42)
    plt.rc('ps', useafm=True)
    plt.rc('font', size=text_font_size, weight="bold",
           family='serif', serif='cm10')
    plt.rc('axes', labelsize=axes_font_size, labelweight="bold")
    plt.rc('xtick', labelsize=label_font_size)
    plt.rc('ytick', labelsize=label_font_size)
    plt.rc('legend', fontsize=label_font_size)
    plt.rcParams['text.latex.preamble'] = [r"\usepackage{amsmath}"]
    plt.rcParams['text.latex.preamble'] = r'\boldmath'


set_font(21)


def bold(text):
    return r'\textbf{' + text + r'}'


def stdef(vals):
    avg = sum(vals) / len(vals)
    sumderiv = 0
    for val in vals:
        sumderiv = sumderiv + val * val
    sumderivavg = sumderiv / (len(vals) - 1)
    return math.sqrt(sumderivavg - (avg*avg))


# HORIZONTAL BAR
fig, ax = plt.subplots()

TAGS = [bold(x) for x in ['1', '10', '25', '50', '100']]


Y = [[0.54, 0.48, 0.55, 0.54, 0.56, 0.59, 0.52, 0.53, 0.54, 0.56, 0.53],
     [3.86, 4.10, 4.05, 3.90, 4.09, 3.99, 3.87, 3.91, 4.17, 4.11, 4.04],
     [26.33, 26.87, 26.11, 25.78, 26.92, 24.71, 26.48, 27.66, 24.21, 23.34, 25.52],
     [57.01, 53.85, 53.44, 56.16, 54.50, 55.41, 54.41, 54.20, 54.57, 54.97],
     [104.15, 103.25, 100.74, 103.01, 106.41, 101.05, 102.16, 102.97, 104.00, 104.49, 105.43]]
# final
YERR = [stdef(y) for y in Y]
Y = [(sum(y) / len(y)) for y in Y]
print('zenfs', Y)

HBAR = ax.bar([x for x, _ in enumerate(Y)],
              Y,
              yerr=YERR,
              color=TEAL,
              #               hatch=lbaf0_pattern,
              width=0.5,
              label=bold('ZenFS'),
              align='center',
              edgecolor="black",
              ecolor="black")

Y_LIM_MIN = 0
Y_LIM_MAX = 150
# YERR = None
COLOR = CYAN
HATCH = r'\\'
YLAB = bold('Runtime (seconds)')


# labs
ax.set_xticks([x for x in range(len(Y))], labels=TAGS)
ax.set_yticks([y*25 for y in range(7)])

plt.ylabel(YLAB)
plt.xlabel(bold('Number of files (thousands)'))
ax.yaxis.grid()  # horizontal lines
plt.ylim(Y_LIM_MIN, Y_LIM_MAX)

plt.savefig(f'runtime.pdf', bbox_inches='tight')
