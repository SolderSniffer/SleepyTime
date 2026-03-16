# SleepyTime — EPD bringup sample
#
# Minimal test application for the SSD1681 1.54" EPD.
#

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(epd_sample)

target_sources(app PRIVATE src/main.c)
