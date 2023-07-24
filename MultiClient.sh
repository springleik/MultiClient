#! /usr/bin/bash

g++ MultiClient.cpp -pthread -o MultiClient
arm-buildroot-linux-uclibcgnueabihf-g++ MultiClient.cpp -o MultiClientX
