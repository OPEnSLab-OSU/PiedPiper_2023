from dataclasses import dataclass
import array

outFile = "PBINT.txt"

# "struct" for playback time
@dataclass
class playbackTime:
    hour: int
    minute: int
    minutes: int

# array storing playback times
playbackTimes: playbackTime = []
playbackTimesLen = 0

# prints playback times
def printPlaybackTimes():
    if (playbackTimesLen == 0):
        print("\nNo playback times were added! Device will wake once every 24 hours!")
        return
    
    playbackCount = 1
    
    print("\nPrinting playbacks:")
    for i in range(0, playbackTimesLen, 2):
        playbackStart = playbackTimes[i]
        playbackEnd = playbackTimes[i + 1]
        print("\tPlayback %d: %02d:%02d to %02d:%02d" % (playbackCount, playbackStart.hour, playbackStart.minute, playbackEnd.hour, playbackEnd.minute))
        playbackCount += 1


# parse input string returns True if input was valid, False if input was invalid
def parseInput(inStr):

    thisTime = inStr.split(':')

    if (len(thisTime) != 2):
        print("Input invalid")
        return False
    
    hour = thisTime[0]
    minute = thisTime[1]

    if not (hour.isdigit() and minute.isdigit()):
        print("Input invalid")
        return False
    
    hour = int(hour)
    minute = int(minute)

    if (hour < 0 or hour > 23) or (minute < 0 or minute > 59):
        print("Time must be between 00:00 and 23:59")
        return False
    
    minutes = hour * 60 + minute

    global playbackTimesLen
    global playbackTimes

    if playbackTimesLen > 0 and minutes <= playbackTimes[-1].minutes:
        print("Time must be after %02d:%02d" % (playbackTimes[-1].hour, playbackTimes[-1].minute))
        return False
        
    playbackTimes.append(playbackTime(hour, minute, minutes))
    playbackTimesLen += 1
    return True

# writes to output file
def outputPlaybackFile():
    with open(outFile, "w") as file:
        # for i in range(0, playbackTimesLen, 2):
        #     pbTimeStart = playbackTimes[i]
        #     pbTimeEnd = playbackTimes[i + 1]
        #     file.write(str(pbTimeStart.hour) + ':' + str(pbTimeStart.minute) + '\n')
        #     file.write(str(pbTimeEnd.hour) + ':' + str(pbTimeEnd.minute) + '\n')

        for pbTime in playbackTimes:
            #file.write(str(pbTime.minutes) + '\n')
            file.write(str(pbTime.hour) + ':' + str(pbTime.minute) + '\n')
    file.close()
    print ("\nPlaybacks written to %r\n" % (outFile))


# main
def main():
    
    print("\nEnter playback start/end time in \'hh:mm\' format, type \'end\' to write to file\n")

    while 1:
        promptInput: bool = True
        while 1:
            userInput = input("Enter 'end' or playback start time(i.e 23:59): ")
            if userInput == "end": 
                promptInput = False
                break
            if parseInput(userInput): break
        
        if (promptInput == False): break
        
        while 1:
            userInput = input("Enter playback end time(i.e 23:59): ")
            if (userInput == "end"): continue
            if parseInput(userInput): break

    printPlaybackTimes()

    outputPlaybackFile()