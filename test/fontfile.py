"""Read the fonts in forth/font.fs, for the tests that check drawn text.

fonts() gives {name: [(width, rows)]}, one entry for each character from
32 up, where rows is 16 lists of 0 and 1, and 1 is ink.
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))


def fonts(path=None):
    text = open(path or f"{HERE}/../forth/font.fs").read()
    out = {}
    for name, body in re.findall(r"^header (\w+) create \1\n(.*?)(?=^header |\Z)", text, re.S | re.M):
        numbers = [int(v) for v in re.findall(r"#(\d+) ,", body)]
        height, first, count, stride = numbers[:4]
        starts = numbers[5:5 + count + 1]
        cells = [int(v, 16) for v in re.findall(r"\$([0-9a-f]+) px,", body)]
        rows = [[cells[y * stride + x // 64] >> (63 - x % 64) & 1 for x in range(64 * stride)]
                for y in range(height)]
        out[name] = [(starts[i + 1] - starts[i], [row[starts[i]:starts[i + 1]] for row in rows])
                     for i in range(count)]
    return out
