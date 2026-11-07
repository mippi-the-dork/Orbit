#!/usr/bin/env python3
"""Read Orbit Motion History CSV or ZIP captures using only Python's standard library.

This is offline analysis only: it does not change the plugin, game, or project.
Sampled velocities cannot prove contact impulses, monotonic energy loss, or sleep safety.
"""

from __future__ import annotations

import argparse
import csv
import io
import math
import sys
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

FLAGS = (
    "experimental_chaos_uniform_gravity",
    "experimental_chaos_point_gravity",
    "experimental_chaos_multi_point_gravity",
    "experimental_chaos_mixed_point_directional_gravity",
    "experimental_chaos_expanded_field_gravity",
)
WINDOWS = ((0, 5), (5, 10), (10, 20), (20, 30), (30, 40), (40, 50), (50, 60))
NEEDED = {"body", "t_sec", "velocity_x", "velocity_y", "velocity_z", "awake"}


def number(row: dict[str, str], key: str, fallback: float = math.nan) -> float:
    try:
        value = float(row.get(key, ""))
        return value if math.isfinite(value) else fallback
    except (TypeError, ValueError):
        return fallback


def is_true(value: object) -> bool:
    return str(value).strip().lower() in ("1", "true", "yes")


def parse_capture(stream, label: str) -> dict:
    reader = csv.DictReader(stream)
    missing = NEEDED - set(reader.fieldnames or ())
    if missing:
        raise ValueError(f"{label}: missing required CSV columns: {', '.join(sorted(missing))}")

    bodies: dict[str, dict] = {}
    flags = Counter()
    rows = 0
    multi_flag_rows = 0
    for row in reader:
        time = number(row, "pie_time_sec", number(row, "t_sec"))
        velocity = tuple(number(row, k) for k in ("velocity_x", "velocity_y", "velocity_z"))
        if not math.isfinite(time) or not all(math.isfinite(v) for v in velocity):
            continue
        speed = math.sqrt(sum(v * v for v in velocity))
        body_id = row.get("body_id", "").strip()
        body_name = (row.get("actor_label") or row.get("actor_name") or row.get("body") or "Unknown").strip()
        key = f"{body_name} [{body_id}]" if body_id else body_name
        state = bodies.setdefault(key, {
            "first": time, "last": time, "last_awake": True, "last_speed": 0.0,
            "first_sleep": None, "count": 0, "sums": defaultdict(float),
            "window_count": Counter(), "window_speed": Counter(),
            "window_tangent": Counter(), "window_angular": Counter(),
            "window_gravity": Counter(),
        })
        state["count"] += 1
        state["first"] = min(state["first"], time)
        state["last"] = max(state["last"], time)
        state["last_speed"] = speed
        state["last_awake"] = is_true(row.get("awake", ""))
        if not state["last_awake"] and state["first_sleep"] is None:
            state["first_sleep"] = time
        state["sums"]["speed"] += speed
        ang = number(row, "angular_speed")
        tang = number(row, "tangential_speed")
        grav = [number(row, k) for k in ("gravity_x", "gravity_y", "gravity_z")]
        gmag = math.sqrt(sum(g * g for g in grav)) if all(math.isfinite(g) for g in grav) else math.nan
        for index, (start, end) in enumerate(WINDOWS):
            if start <= time < end:
                state["window_count"][index] += 1
                state["window_speed"][index] += speed
                if math.isfinite(tang):
                    state["window_tangent"][index] += tang
                if math.isfinite(ang):
                    state["window_angular"][index] += ang
                if math.isfinite(gmag):
                    state["window_gravity"][index] += gmag
        active = [flag for flag in FLAGS if is_true(row.get(flag, ""))]
        flags[active[0] if len(active) == 1 else ("Standard / no solver flag" if not active else "INVALID: multiple solver flags")] += 1
        if len(active) > 1:
            multi_flag_rows += 1
        rows += 1
    if not rows:
        raise ValueError(f"{label}: no usable motion samples")
    return {"label": label, "rows": rows, "bodies": bodies, "flags": flags,
            "sim_end": max(body["last"] for body in bodies.values()),
            "invalid_flags": multi_flag_rows}


def captures(paths: list[Path]):
    for path in paths:
        if zipfile.is_zipfile(path):
            with zipfile.ZipFile(path) as z:
                for item in z.infolist():
                    if not item.is_dir() and item.filename.lower().endswith(".csv"):
                        with z.open(item) as handle:
                            stream = io.TextIOWrapper(handle, encoding="utf-8-sig", errors="replace", newline="")
                            yield parse_capture(stream, f"{path.name} / {item.filename}")
        else:
            with path.open(encoding="utf-8-sig", errors="replace", newline="") as handle:
                yield parse_capture(handle, path.name)


def report(capture: dict) -> list[dict[str, object]]:
    print(f"\n=== {capture['label']} ===")
    bodies = capture["bodies"]
    final_time = capture["sim_end"]
    asleep = sum(not b["last_awake"] for b in bodies.values() if final_time - b["last"] <= 1.0)
    tracked = sum(final_time - b["last"] <= 1.0 for b in bodies.values())
    print(f"Usable samples: {capture['rows']:,}; unique bodies: {len(bodies)}; ", end="")
    print(f"latest PIE time: {final_time:.2f}s; asleep at end among tracked: {asleep}/{tracked}")
    for key, count in capture["flags"].most_common():
        print(f"  Gravity path: {key}: {count:,} samples ({100*count/capture['rows']:.1f}%)")
    if capture["invalid_flags"]:
        print(f"  WARNING: {capture['invalid_flags']} samples contain multiple experimental mode flags")
    print("  Samples are not guaranteed to cover every body for the whole capture.")
    heading = ("Body", "Last s", "Awake", "Final speed", "10–20s", "20–30s", "30–40s", "40–50s", "50–60s")
    print(" | ".join(heading))
    print(" | ".join("---" for _ in heading))
    table_rows = []
    for name, b in sorted(bodies.items()):
        def avg(index):
            count = b["window_count"][index]
            return b["window_speed"][index] / count if count else math.nan
        windows = [avg(i) for i in range(2, 7)]
        def fmt(x):
            return f"{x:.2f}" if math.isfinite(x) else "n/a"
        print(" | ".join((name, f"{b['last']:.2f}", str(b["last_awake"]),
              fmt(b["last_speed"]), *(fmt(x) for x in windows))))
        row = {"capture": capture["label"], "body": name,
               "first_pie_sec": round(b["first"], 4), "last_pie_sec": round(b["last"], 4),
               "sample_count": b["count"], "awake_at_last_sample": int(b["last_awake"]),
               "last_linear_speed_cm_s": round(b["last_speed"], 5),
               "first_sampled_sleep_sec": b["first_sleep"]}
        for index, (start, end) in enumerate(WINDOWS):
            n = b["window_count"][index]
            row[f"mean_speed_{start}_{end}_cm_s"] = round(avg(index), 5) if n else ""
            row[f"samples_{start}_{end}"] = n
        table_rows.append(row)
    print("Note: speed is magnitude of velocity, not total mechanical energy; gravity can convert potential to kinetic energy.")
    print("Note: one source_center field cannot identify all contributing sources in an overlapping field.")
    return table_rows


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, nargs="+", help="Motion History .csv or .zip archive")
    parser.add_argument("--output-csv", type=Path, help="Optional per-body summary output CSV")
    args = parser.parse_args(argv)
    try:
        for path in args.capture:
            if not path.is_file():
                raise ValueError(f"Capture not found: {path}")
        all_rows = []
        for capture in captures(args.capture):
            all_rows.extend(report(capture))
        if not all_rows:
            raise ValueError("No CSV captures found")
        if args.output_csv:
            args.output_csv.parent.mkdir(parents=True, exist_ok=True)
            with args.output_csv.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(handle, fieldnames=all_rows[0].keys())
                writer.writeheader()
                writer.writerows(all_rows)
            print(f"Summary written: {args.output_csv}")
        return 0
    except (OSError, ValueError, zipfile.BadZipFile, csv.Error) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
