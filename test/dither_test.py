def reference(gray, w, h):
    """Atkinson's dither, a pixel at a time: rows of 1 for white."""
    err = [[0] * (w + 4) for _ in range(h + 2)]
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            old = gray[y * w + x] + err[y][x + 2]
            new = 255 if old > 127 else 0
            row.append(1 if new else 0)
            e = ((old - new + 1024) >> 3) - 128
            for dx, dy in ((1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2)):
                err[y + dy][x + 2 + dx] += e
        rows.append(row)
    return rows
