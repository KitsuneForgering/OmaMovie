#!/usr/bin/env bash
# Prints the CPU frequency policy for a benchmark report. On intel_pstate the governor is
# usually "powersave"; the energy/performance preference and the power profile set the clocks,
# and they changed per-frame timings by up to 1.9x between otherwise identical runs
# (Docs/Research/evidence/2026-10-19-evaluate-lookup-scaling.md).
policy=/sys/devices/system/cpu/cpu0/cpufreq
read_or() { cat "$1" 2>/dev/null || echo n/a; }
profile=$(powerprofilesctl get 2>/dev/null || echo n/a)
echo "cpufreq: driver $(read_or $policy/scaling_driver), governor $(read_or $policy/scaling_governor)," \
    "epp $(read_or $policy/energy_performance_preference), power profile $profile," \
    "AC $(cat /sys/class/power_supply/A*/online 2>/dev/null | head -1 || echo n/a)"
