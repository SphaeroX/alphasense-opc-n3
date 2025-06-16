#!/usr/bin/env python3
"""Query data from InfluxDB and plot selected fields.

This script fetches measurements from an InfluxDB bucket for the
specified time range and generates a simple diagram using Matplotlib.
The InfluxDB connection details can be supplied via command line
arguments or environment variables. If those are missing, the script
falls back to the credentials defined in ``include/config.h.example``.

Example:
    python plot_influx.py --fields scd41_co2 weather_temperature \
        --range 1h --measurement full

The tool can also run in an interactive mode powered by ChatGPT. When
started without field arguments it will ask what should be displayed,
use ChatGPT to build the query and plotting code and then execute it.
The OpenAI API key is read from ``OPENAI_API_KEY``.
"""

from __future__ import annotations

import argparse
import os
import re
from typing import Dict, List

import pandas as pd
from influxdb_client import InfluxDBClient
import matplotlib.pyplot as plt
import openai

AVAILABLE_FIELDS = [
    "opc_pm1", "opc_pm2_5", "opc_pm10", "opc_temperature", "opc_humidity",
    "scd41_co2", "scd41_temperature", "scd41_humidity",
    "calc_pollen_count", "calc_pollen_level", "calc_co2_quality",
    "weather_temperature", "weather_humidity", "weather_apparent_temperature",
    "weather_is_day", "weather_rain", "weather_cloud_cover_pct",
    "weather_pressure_msl", "weather_surface_pressure",
    "weather_wind_speed_kmh", "weather_wind_dir_deg", "weather_wind_gusts_kmh",
    "air_ragweed_pollen", "air_olive_pollen", "air_mugwort_pollen",
    "air_grass_pollen", "air_birch_pollen", "air_alder_pollen", "air_dust",
    "air_carbon_monoxide", "air_pm2_5", "air_pm10", "air_european_aqi",
] + [f"opc_bin_{i:02d}" for i in range(24)]


def load_config(path: str) -> Dict[str, str]:
    """Extract InfluxDB settings from a config.h-style file."""
    result: Dict[str, str] = {}
    pattern = re.compile(r'#define\s+(INFLUXDB_\w+)\s+"([^"]*)"')
    try:
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                match = pattern.match(line.strip())
                if match:
                    result[match.group(1)] = match.group(2)
    except FileNotFoundError:
        pass
    return result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot measurements from InfluxDB")
    default_config = os.path.join(os.path.dirname(__file__), "..", "include", "config.h.example")
    parser.add_argument("--config", default=default_config,
                        help="Path to config.h with InfluxDB defaults")
    parser.add_argument("--url", help="InfluxDB server URL")
    parser.add_argument("--token", help="InfluxDB access token")
    parser.add_argument("--org", help="InfluxDB organization")
    parser.add_argument("--bucket", help="InfluxDB bucket name")
    parser.add_argument("--measurement", default="full",
                        help="Measurement name to query")
    parser.add_argument("--range", default="1h",
                        help="Time range to query, e.g. 1h or 30m")
    parser.add_argument("--fields", nargs='+',
                        help="Field names to include in the diagram")
    parser.add_argument("--interactive", action="store_true",
                        help="Query ChatGPT for fields and plot code")
    args = parser.parse_args()

    env = {
        "INFLUXDB_URL": os.getenv("INFLUXDB_URL"),
        "INFLUXDB_TOKEN": os.getenv("INFLUXDB_TOKEN"),
        "INFLUXDB_ORG": os.getenv("INFLUXDB_ORG"),
        "INFLUXDB_BUCKET": os.getenv("INFLUXDB_BUCKET"),
    }
    cfg = load_config(args.config)
    args.url = args.url or env["INFLUXDB_URL"] or cfg.get("INFLUXDB_URL", "http://localhost:8086")
    args.token = args.token or env["INFLUXDB_TOKEN"] or cfg.get("INFLUXDB_TOKEN", "")
    args.org = args.org or env["INFLUXDB_ORG"] or cfg.get("INFLUXDB_ORG", "")
    args.bucket = args.bucket or env["INFLUXDB_BUCKET"] or cfg.get("INFLUXDB_BUCKET", "sensors")
    if args.fields is None:
        args.interactive = True
    return args


def fetch_data(client: InfluxDBClient, bucket: str, measurement: str,
               fields: List[str], time_range: str) -> pd.DataFrame:
    filters = " or ".join([f'r._field == "{f}"' for f in fields])
    flux = f"""
from(bucket: \"{bucket}\")
  |> range(start: -{time_range})
  |> filter(fn: (r) => r._measurement == \"{measurement}\")
  |> filter(fn: (r) => {filters})
"""
    tables = client.query_api().query_data_frame(flux)
    if isinstance(tables, list):
        df = pd.concat(tables)
    else:
        df = tables
    return df


def plot(df: pd.DataFrame, fields: List[str]) -> None:
    if df.empty:
        print("No data found for the given parameters")
        return
    pivot = df.pivot(index="_time", columns="_field", values="_value")
    pivot[fields].plot(subplots=True, figsize=(10, 2 * len(fields)), grid=True)
    plt.tight_layout()
    plt.show()


def interactive_chat() -> tuple[str, str] | tuple[None, None]:
    """Use ChatGPT to obtain a Flux query and plotting code."""
    if not os.getenv("OPENAI_API_KEY"):
        print("OPENAI_API_KEY environment variable not set")
        return None, None

    system_prompt = (
        "You help build InfluxDB Flux queries and matplotlib code.\n"
        f"Available fields: {', '.join(AVAILABLE_FIELDS)}.\n"
        "Ask the user for missing details. When you have everything, respond\n"
        "with:\nQUERY:\n```flux\n<flux query>\n```\nCODE:\n```python\n<plotting code>\n```"
    )

    messages = [{"role": "system", "content": system_prompt}]
    while True:
        user_input = input("You: ")
        messages.append({"role": "user", "content": user_input})
        response = openai.ChatCompletion.create(model="gpt-3.5-turbo", messages=messages)
        assistant = response.choices[0].message.content.strip()
        print("ChatGPT:", assistant)
        messages.append({"role": "assistant", "content": assistant})
        if "```python" in assistant and "```flux" in assistant:
            break

    query_match = re.search(r"```flux\n(.*?)```", assistant, re.S)
    code_match = re.search(r"```python\n(.*?)```", assistant, re.S)
    query = query_match.group(1).strip() if query_match else None
    code = code_match.group(1).strip() if code_match else None
    return query, code


def main() -> None:
    args = parse_args()
    with InfluxDBClient(url=args.url, token=args.token, org=args.org) as client:
        if args.interactive:
            query, code = interactive_chat()
            if not query or not code:
                return
            tables = client.query_api().query_data_frame(query)
            df = pd.concat(tables) if isinstance(tables, list) else tables
            exec(code, {"df": df, "pd": pd, "plt": plt})
        else:
            df = fetch_data(client, args.bucket, args.measurement, args.fields, args.range)
            plot(df, args.fields)


if __name__ == "__main__":
    main()
