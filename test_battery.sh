#!/bin/bash
# Test script for Razer Battery Monitor
# Run with: ./test_battery.sh

echo "=== Razer Battery Monitor Test ==="
echo ""
echo "This script will test the battery monitor with sudo privileges."
echo "Make sure your Razer Viper V2 Pro is connected (wireless dongle or USB)."
echo ""
echo "Starting test in 3 seconds..."
sleep 3

cd "$(dirname "$0")"

# Kill any existing instances
pkill -f RazerBatteryMonitor 2>/dev/null
sleep 1

# Run with sudo and capture output
echo "Running RazerBatteryMonitor..."
sudo ./RazerBatteryMonitor 2>&1 | tee /tmp/razer_battery_test.log

echo ""
echo "Test complete. Check /tmp/razer_battery_test.log for full output."

