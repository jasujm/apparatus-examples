import matplotlib.pyplot as plt
from matplotlib import rc
import numpy as np
import scipy.stats as ss

import sys


def main():
    with sys.stdin as f:
        walk = np.loadtxt(f, dtype=np.intc)
    steps = np.array([2 ** (n + 1) - 1 for n in range(walk.shape[0])])

    rc("text", usetex=True)

    fig1 = plt.figure(1)
    plt.title(f"Distance to the shore after $2^{{{len(steps)}}}$ steps")
    plt.xlabel(r"Coordinate $X_n$")
    plt.yticks([])
    plt.hist(walk[-1, :], 50)

    # Drop a few points at the beginning. For small number of steps it doesn't
    # follow the power law.
    avgs = np.mean(walk, 1)
    lr = ss.linregress(np.log(steps[8:]), np.log(avgs[8:]))
    print(lr)

    def plot_x_vs_t(n, scale):
        x = np.exp(lr.intercept) * np.power(n, lr.slope)
        plt.title(rf"Expected distance to the shore ({scale} scale)")
        plt.xlabel(r"Step $n$")
        plt.ylabel(r"Coordinate $\langle X_n \rangle$")
        plt.scatter(steps, avgs)
        plt.plot(n, x, label=rf"$\langle X_n \rangle \sim n^{{{lr.slope:.3f}}}$")
        plt.legend()

    fig2 = plt.figure(2)
    plot_x_vs_t(np.linspace(steps[0], steps[-1]), "linear")

    fig3 = plt.figure(3)
    plt.loglog()
    plot_x_vs_t(np.logspace(0, len(steps), base=2), "logarithmic")

    plt.show()


if __name__ == "__main__":
    main()
