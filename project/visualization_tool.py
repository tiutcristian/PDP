#!/usr/bin/env python3
"""
Animate body positions from traj.csv (step,i,x,y,z).

Usage:
  python animate.py traj.csv
Options:
  python animate.py traj.csv --stride 5 --interval 30 --limit 500 --trail 0
"""

import argparse
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation


def load_csv(path: str):
    data = np.genfromtxt(path, delimiter=",", names=True)  # expects header
    steps = data["step"].astype(int)
    ids = data["i"].astype(int)

    uniq_steps = np.unique(steps)
    uniq_ids = np.unique(ids)

    step_to_t = {s: t for t, s in enumerate(uniq_steps)}
    id_to_n = {bid: n for n, bid in enumerate(uniq_ids)}

    T = len(uniq_steps)
    N = len(uniq_ids)
    pos = np.empty((T, N, 3), dtype=float)

    for s, bid, x, y, z in zip(steps, ids, data["x"], data["y"], data["z"]):
        pos[step_to_t[s], id_to_n[bid], :] = (x, y, z)

    return uniq_steps, uniq_ids, pos


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", help="Input traj.csv with columns: step,i,x,y,z")
    ap.add_argument("--stride", type=int, default=1, help="Use every k-th step (default 1)")
    ap.add_argument("--interval", type=int, default=30, help="ms between frames (default 30)")
    ap.add_argument("--limit", type=int, default=0, help="Show only first LIMIT bodies (0=all)")
    ap.add_argument("--trail", type=int, default=0, help="Trail length in frames (0=no trail)")
    args = ap.parse_args()

    steps, ids, pos = load_csv(args.csv)

    # Downsample steps
    pos = pos[::args.stride]
    steps = steps[::args.stride]

    if args.limit and args.limit < pos.shape[1]:
        pos = pos[:, :args.limit, :]
        ids = ids[:args.limit]

    T, N, _ = pos.shape

    # Axis limits with padding
    mins = pos.min(axis=(0, 1))
    maxs = pos.max(axis=(0, 1))
    span = np.maximum(maxs - mins, 1e-9)
    pad = 0.05 * span
    xlim = (mins[0] - pad[0], maxs[0] + pad[0])
    ylim = (mins[1] - pad[1], maxs[1] + pad[1])
    zlim = (mins[2] - pad[2], maxs[2] + pad[2])

    fig = plt.figure()
    ax = fig.add_subplot(111, projection="3d")
    ax.set_xlim(*xlim)
    ax.set_ylim(*ylim)
    ax.set_zlim(*zlim)
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.set_zlabel("z")
    ax.set_title("N-body positions over time")

    scat = ax.scatter(pos[0, :, 0], pos[0, :, 1], pos[0, :, 2], s=12)
    info = ax.text2D(0.02, 0.95, "", transform=ax.transAxes)

    trail_lines = []
    if args.trail > 0:
        for _ in range(N):
            (ln,) = ax.plot([], [], [], linewidth=1)
            trail_lines.append(ln)

    def update(frame):
        xyz = pos[frame]
        scat._offsets3d = (xyz[:, 0], xyz[:, 1], xyz[:, 2])

        if args.trail > 0:
            start = max(0, frame - args.trail)
            seg = pos[start:frame + 1]  # [L, N, 3]
            for n, ln in enumerate(trail_lines):
                ln.set_data(seg[:, n, 0], seg[:, n, 1])
                ln.set_3d_properties(seg[:, n, 2])

        info.set_text(f"step={steps[frame]}  bodies={N}")
        return (scat, info, *trail_lines)

    # IMPORTANT: keep a reference to the animation object
    anim = FuncAnimation(fig, update, frames=T, interval=args.interval, blit=False)
    # Save animation (choose one)
    anim.save("animation.gif", writer="pillow", fps=30)
    # anim.save("animation.mp4", writer="ffmpeg", fps=30)
    # plt.show()


if __name__ == "__main__":
    main()