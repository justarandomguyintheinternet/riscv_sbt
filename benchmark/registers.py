import json
from pathlib import Path
from statistics import median
import argparse

parser = argparse.ArgumentParser(description='register usage distribution tool')
parser.add_argument('--input', action="store", dest='input', help='Input file which is output from benchmark.py.')
parser.add_argument('--output', action="store", dest='output', default='run', help='Output file name.')
args = parser.parse_args()

INPUT = Path(__file__).parent / args.input
OUTPUT = Path(__file__).parent / (args.output + ".dat")

REG_NAMES = [
    "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
]

def main():
    with INPUT.open() as f:
        data = json.load(f)

    # one list of per-register fractions per binary
    fractions = []
    for binary in data["median"]["binaries"]:
        accesses = binary["register_accesses"][1:]  # drop x0
        total = sum(accesses)
        if total == 0:
            continue
        fractions.append([100.0 * count / total for count in accesses])

    medians = [median(reg) for reg in zip(*fractions)]

    # normalize back to 100, as medians dont sum up to it
    scale = 100.0 / sum(medians)
    medians = [frac * scale for frac in medians]

    width = max(len(name) for name in REG_NAMES) + 2
    with OUTPUT.open("w") as f:
        f.write(f"{'reg':<{width}}frac\n")
        for name, frac in zip(REG_NAMES, medians):
            f.write(f"{name:<{width}}{frac:.2f}\n")

    print(f"wrote {OUTPUT}")

if __name__ == "__main__":
    main()
