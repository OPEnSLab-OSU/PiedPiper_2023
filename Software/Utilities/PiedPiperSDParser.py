# Pied Piper SD parser - parses all data on Pied Piper SD card into a meaningful format
#  - converts /LOG.txt into a graph to visualize periods when device was active
#  - converts raw sample and frequency data from detections to spectrograms with date and time, detection algorithm settings, correlation coefficient, temperature/humidity data as title

import pathlib
import os
import sys
import signal_processing as sp
import numpy as np
from datetime import datetime, timedelta


if __name__ == "__main__":

    file = None

    # store passed argument as path to SD directory
    if len(sys.argv) < 2:
        print("usage: python .\PiedPiperSDParser \"path_to_sd\" \"optional_path_to_output\"")
        exit(1)
    
    inDir = pathlib.Path(sys.argv[1])

    # check if argument to input directory is valid
    if not inDir.exists():
        print("Directory to SD invalid, pass absolute path to Pied Piper SD as argument!")
        exit(1)

    # if output directory wasn't specified, stored output in current working directory
    outDir = pathlib.Path.cwd()

    if len(sys.argv) < 3:
        print("Output directory not specified. Output will be saved to working directory.")
    else:
        outDir = pathlib.Path(sys.argv[2])

    # exit program if path to output directory is invalid
    if not outDir.exists():
       print("Path to output folder invalid, pass absolute path to output directory")
       exit(1)
    
    # paths to LOG.txt and DATA directory
    logDir = inDir.joinpath("LOG.txt")
    dataDir = inDir.joinpath("DATA")

    # check if LOG.txt exists
    if not logDir.exists():
        print("LOG.txt not present")
    else:
        print("Parsing LOG.txt...")
        dates = []
        file = open(logDir)
        # storing all dates in LOG.txt
        for dt in file.readlines():
            dates.append(datetime(int(dt[0:4]), int(dt[4:6]), int(dt[6:8]), int(dt[9:11]), int(dt[12:14]), int(dt[15:17])))

        # check that LOG.txt wasn't empty
        if (len(dates) > 0):
            # check difference between dates
            totalDays = (dates[-1] - dates[0]).days + 1
            # generate all dates between dates[0] and dates[-1] inclusive
            allDates = [dates[0] + timedelta(days=i) for i in range (totalDays)]

            # label all dates as "inactive"
            values = ["Inactive"] * totalDays
            _index = 0
            # find dates which were logged and label them as "active"
            for date in allDates:
                if (date in dates):
                    values[_index] = "Active"
                _index += 1

            # plot and save as LOG.png
            sp.plt.plot(allDates, values)
            sp.plt.xticks([allDates[0], allDates[len(allDates) >> 1], allDates[-1]])
            sp.plt.xlabel("Date")
            sp.plt.gca().invert_yaxis()
            sp.plt.savefig(outDir.joinpath("LOG.png"))
            sp.plt.close()

    # check if DATA directory exists
    if not dataDir.exists():
        print("DATA not present")
    else:
        print("Parsing detection data...")

        # go through each date in DATA
        for date in dataDir.iterdir():
            if not date.is_dir():
                continue
            outFile = date.parts[-1]
            # print(outFile)
            outFile = outDir.joinpath(outFile)
            if not outFile.exists():
                outFile.mkdir()
            # go through each time in date
            for time in date.iterdir():
                if not time.is_dir():
                    continue

                # form paths to expected files
                details = time.joinpath("DETS.TXT")         # RTC time of detection, correlation coefficient, temperature/humidity
                rawSamples = time.joinpath("RAW.TXT")       # samples which were used for computing frequency data
                processedFreqs= time.joinpath("PFD.TXT")    # processed frequency data
                photo = time.joinpath("PHOTO.JPG")          # photo from camera module
                
                _details1 = None
                _details2 = None
                _time = time.parts[-1]
                _sampleRate = None
                _windowSize = None
                
                # open and process detection details
                if details.exists():
                    file = open(details)
                    _details1 = file.readline().strip("\n").split(" ")
                    _details2 = file.readline().strip("\n").split(" ")
                    file.close()

                    _sampleRate = int(_details2[0])
                    _windowSize = int(_details2[1])
                
                # open and process raw samples data 
                if rawSamples.exists():
                    file = open(rawSamples)
                    rawSamples = list(map(int, file.readlines()))
                    file.close()
                    _title = _details1[0] + " RAW\nConfidence: " + _details1[1] + "    Temp/Humidity: "
                    sp.printSpecgram(sp.dataToSpecgram(rawSamples, _windowSize), _sampleRate, title=_title, show=False, save=True, fname=outFile.joinpath(_time + "_" + "RAW.png"))

                # open and process processed frequency data
                if processedFreqs.exists():
                    file = open(processedFreqs)
                    processedFreqs = list(map(int, file.readlines()))
                    file.close()
                    processedFreqs = np.split(np.array(processedFreqs), int(len(processedFreqs) / (_windowSize >> 1)), axis=0)
                    _title = _details1[0] + "    Noise Removal: (" + _details2[2] + "/" + _details2[3] + ")    Time/Freq Smoothing: " + "(" + _details2[4] + "/" + _details2[5] + ")\nConfidence: " + _details1[1] + "    Temp/Humidity: "
                    sp.printSpecgram(processedFreqs, _sampleRate, _windowSize, title=_title, show=False, save=True, fname=outFile.joinpath(_time + "_" + "PROCESSED.png"))

                # probably just copy photo to output directory
                # if photo.exists():

                # exit(0)

    print("Done...\nOutput saved to: %s" % outDir)

    exit(0)
