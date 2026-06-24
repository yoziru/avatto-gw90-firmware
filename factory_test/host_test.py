#!/usr/bin/env python3
"""
AVATTO GW90-TI Factory Test Host Script
========================================

Features:
  - Communicate with factory test firmware via serial port
  - Parse test result JSON
  - Generate test reports (CSV / screen output)
  - Support batch testing

Usage:
  python host_test.py --port COM3
  python host_test.py --port /dev/ttyUSB0 --output report.csv

Dependencies:
  pip install pyserial
"""

import serial
import json
import time
import argparse
import csv
import sys
import os
from datetime import datetime


# ANSI Colors
class Color:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    CYAN = '\033[96m'
    BOLD = '\033[1m'
    END = '\033[0m'


def colorize(text, color):
    """Add color to text (terminal only)"""
    if sys.stdout.isatty():
        return f"{color}{text}{Color.END}"
    return text


class FactoryTester:
    def __init__(self, port, baud=115200, timeout=60):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser = None
        self.results = []
        self.summary = None
        self.raw_log = []

    def connect(self):
        """Connect to serial port"""
        print(f"\nConnecting to serial port {self.port} @ {self.baud}...")
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
            time.sleep(0.5)
            # Clear input buffer
            self.ser.reset_input_buffer()
            print(colorize("  Serial port connected", Color.GREEN))
            return True
        except serial.SerialException as e:
            print(colorize(f"  Serial port connection failed: {e}", Color.RED))
            return False

    def disconnect(self):
        """Disconnect serial port"""
        if self.ser and self.ser.is_open:
            self.ser.close()

    def send_command(self, cmd):
        """Send command"""
        self.ser.write(f"{cmd}\n".encode())
        self.ser.flush()

    def run_test(self):
        """Execute test workflow"""
        self.results = []
        self.summary = None
        self.raw_log = []

        print(colorize("\n" + "=" * 60, Color.CYAN))
        print(colorize("  AVATTO GW90-TI Production Line Test", Color.BOLD))
        print(colorize("=" * 60, Color.CYAN))
        print(f"  Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"  Port: {self.port}")
        print()

        # Send start command
        self.send_command("CMD:START")

        # Read results
        start_time = time.time()
        btn_prompted = False

        while time.time() - start_time < self.timeout:
            if self.ser.in_waiting > 0:
                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue

                self.raw_log.append(line)

                # Parse RSP: line
                if line.startswith("RSP:"):
                    json_str = line[4:]
                    try:
                        data = json.loads(json_str)
                    except json.JSONDecodeError:
                        continue

                    # Button prompt
                    if data.get("action") == "PRESS_BTN_NOW":
                        if not btn_prompted:
                            print(colorize("\n  >>> Please press the device button <<<\n", Color.YELLOW))
                            btn_prompted = True
                        continue

                    # Test result
                    if "test" in data and "result" in data:
                        self.results.append(data)
                        passed = data["result"] == "PASS"
                        status = colorize("PASS", Color.GREEN) if passed else colorize("FAIL", Color.RED)
                        print(f"  [{status}] {data['test']:20s} | {data.get('detail', '')}")

                    # Summary result
                    if data.get("type") == "SUMMARY":
                        self.summary = data
                        break

                # Regular log line
                elif line.startswith("["):
                    # Progress info
                    print(f"  {colorize(line, Color.CYAN)}")

        # Print summary
        if self.summary:
            self._print_summary()
        else:
            print(colorize("\n  Timeout: No test summary received!", Color.RED))

        return self.summary

    def _print_summary(self):
        """Print test summary"""
        s = self.summary
        verdict = s.get("verdict", "UNKNOWN")
        total = s.get("total", 0)
        passed = s.get("pass", 0)
        failed = s.get("fail", 0)
        failures = s.get("failures", [])

        print()
        print(colorize("=" * 60, Color.CYAN))
        if verdict == "PASS":
            print(colorize(f"  *** TEST PASSED ***  ({passed}/{total})", Color.GREEN + Color.BOLD))
        else:
            print(colorize(f"  *** TEST FAILED ***  ({passed}/{total})", Color.RED + Color.BOLD))
            print(colorize(f"  Failures: {', '.join(failures)}", Color.RED))
        print(colorize("=" * 60, Color.CYAN))
        print()

    def get_mac(self):
        """Extract MAC address from results"""
        for r in self.results:
            if r.get("test") == "mac_addr" and r.get("result") == "PASS":
                return r.get("detail", "UNKNOWN")
        return "UNKNOWN"

    def save_report(self, filepath):
        """Save report to CSV"""
        file_exists = os.path.exists(filepath)

        with open(filepath, 'a', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)

            # Write headers
            if not file_exists:
                headers = ["Time", "MAC", "Verdict"]
                headers += [r["test"] for r in self.results]
                headers += ["Failures"]
                writer.writerow(headers)

            # Write data row
            row = [
                datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                self.get_mac(),
                self.summary.get("verdict", "UNKNOWN") if self.summary else "TIMEOUT"
            ]
            for r in self.results:
                row.append(f"{r['result']}: {r.get('detail', '')}")

            failures = self.summary.get("failures", []) if self.summary else ["TIMEOUT"]
            row.append(", ".join(failures) if failures else "NONE")
            writer.writerow(row)

        print(f"  Report saved: {filepath}")

    def save_raw_log(self, filepath):
        """Save raw serial log"""
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(f"# AVATTO GW90 Factory Test Log\n")
            f.write(f"# Time: {datetime.now().isoformat()}\n")
            f.write(f"# Port: {self.port}\n\n")
            for line in self.raw_log:
                f.write(line + "\n")


def main():
    parser = argparse.ArgumentParser(description='AVATTO GW90-TI Factory Test Host')
    parser.add_argument('--port', required=True, help='Serial port (COM3 or /dev/ttyUSB0)')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate (default 115200)')
    parser.add_argument('--output', default='factory_test_report.csv', help='CSV report output file')
    parser.add_argument('--log-dir', default='logs', help='Raw log save directory')
    parser.add_argument('--batch', action='store_true', help='Batch mode (auto-wait for next after each test)')
    parser.add_argument('--timeout', type=int, default=60, help='Single test timeout (seconds)')
    args = parser.parse_args()

    tester = FactoryTester(args.port, args.baud, args.timeout)

    if not tester.connect():
        sys.exit(1)

    # Create log directory
    os.makedirs(args.log_dir, exist_ok=True)

    try:
        test_count = 0
        while True:
            test_count += 1
            print(colorize(f"\n{'='*60}", Color.BOLD))
            print(colorize(f"  Testing device #{test_count}", Color.BOLD))
            print(colorize(f"{'='*60}", Color.BOLD))

            summary = tester.run_test()

            # Save report
            tester.save_report(args.output)

            # Save raw log
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            mac = tester.get_mac().replace(":", "")
            log_file = os.path.join(args.log_dir, f"test_{timestamp}_{mac}.log")
            tester.save_raw_log(log_file)

            if not args.batch:
                break

            # Batch mode: wait for operator to place next board
            print(colorize("\n  Waiting for next device...", Color.YELLOW))
            print("  (Press Enter to start next, type q to quit)")

            user_input = input("  > ").strip().lower()
            if user_input == 'q':
                break

            # Reconnect (device may have been power-cycled)
            tester.disconnect()
            time.sleep(2)
            if not tester.connect():
                print(colorize("  Serial reconnect failed, exiting", Color.RED))
                break

    except KeyboardInterrupt:
        print(colorize("\n\n  Test interrupted by user", Color.YELLOW))
    finally:
        tester.disconnect()
        print(f"\n  Tested {test_count} devices total")
        print(f"  Report: {args.output}")
        print()


if __name__ == '__main__':
    main()
