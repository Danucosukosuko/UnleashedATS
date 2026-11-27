# UnleashedATS bootloader: the ESP-IDF bootloader the Arduino core ships,
# plus the option to start the recovery app ("test" partition) when the
# encoder button is held at power on. Only the bootloader of this project
# is used; the app is a placeholder.
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(unleashedats_bootloader)
