import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation

N_WALKS = 10
N_STEPS = 500


def generate_walks():
    steps = np.concatenate(
        [
            np.zeros((1, N_WALKS), dtype=int),
            2 * np.random.randint(2, size=(N_STEPS, N_WALKS)) - 1,
        ]
    )
    return np.cumsum(steps, 0)


def main():
    walks = generate_walks()

    fig, ax = plt.subplots()
    ax.title.set_text("Simple random walks")

    x = np.arange(0, walks.shape[0])
    lines = ax.plot(x, walks)

    def animate(i):
        for j, line in enumerate(lines):
            line.set_data(x[:i], walks[:i, j])
        return lines

    ani = animation.FuncAnimation(
        fig, animate, frames=walks.shape[0], interval=10, blit=True
    )

    plt.show()

    ani.save("walks.mp4")


if __name__ == "__main__":
    main()
