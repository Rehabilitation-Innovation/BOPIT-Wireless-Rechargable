#!/usr/bin/env python3
#BOPIT Game V70, 11April2022
# added check_serial function
# use with bopitV241testincrementV2.HMI on Nextion
# save as BOPIT_latest.py so /etc/rc.local starts the most current version
import os
import sys
import serial
import RPi.GPIO as io
from time import time, sleep
import logging
import random
import pygame as pg

import pigpio

p = pigpio.pi()

p.i2c_open(1, 0x1a)

logging.basicConfig(filename='bopit.log', format='%(asctime)s %(message)s', datefmt='%d%b%Y %H:%M:%S')

pg.mixer.init()
soundObj = []
soundObjWrong = []
soundDir = '/home/bopit/bopit/wavFiles/'
wavFiles  = ['pushFemale.wav', 'pullFemale.wav', 'twistFemale.wav']   #, 'yay.wav', 'woow.wav', 'feel_good.wav']

for waves in wavFiles:
    obj = pg.mixer.Sound(soundDir + waves)
    obj.set_volume(1)
    soundObj.append(obj) # only wave?

entrySound = pg.mixer.Sound(soundDir + 'fanfare.wav')
entrySound.set_volume(1)

timesUpSound = pg.mixer.Sound(soundDir + 'timesUp.wav')
timesUpSound.set_volume(1)

rightSound = pg.mixer.Sound(soundDir + 'bellRing.wav') #'yay.wav')
rightSound.set_volume(1)

wrongSound = pg.mixer.Sound(soundDir + 'sirenShort.wav')
wrongSound.set_volume(1)

afterPlaygameSound = pg.mixer.Sound(soundDir + 'yay.wav')
afterPlaygameSound.set_volume(1)

exitSound = pg.mixer.Sound(soundDir + 'exit_cue.wav')
exitSound.set_volume(1)

# timeout is for read, read buffer when requested number of bytes recieved or read when timeout expires
port = serial.Serial("/dev/ttyAMA0", baudrate=9600, timeout=0.05) 

eof = b"\xff\xff\xff" #the b converts to a byte object, eof must end each line sent to the touch screen
dbl_quote = '"'

pushPin_board = 13
pullPin_board = 19
twistPin_board = 26
pinList_board = [pushPin_board, pullPin_board, twistPin_board]

io.setmode(io.BOARD) # as opposed to BCM, broadcom numbering
io.setup(pushPin_board, io.IN, pull_up_down=io.PUD_UP)
io.setup(pullPin_board, io.IN, pull_up_down=io.PUD_UP)
io.setup(twistPin_board, io.IN, pull_up_down=io.PUD_UP)

shutdownPin = 40
#io.setup(shutdownPin, io.IN, pull_up_down=io.PUD_UP)
# add event after def game_shutdown

pins_randomList = [] # create shuffled list after selecting nTrys

# use dictionary so variables can be read(and changed) in functions
dct = {'score':0, 'timeouts':0, 'wrongHits':0, 'nTrys':0,
        't2hit':0, 'tTotal':time(), 'summary_str':'xx', 'end':0}

results = [[],[]] # 0 is time at hit, 1 is release

entrySound.play()
port.flushInput() # Serial port has a buffer so we need to clear it
port.write(str.encode("page "+"welcome")+eof) # move to the game starting screen when we start running

def check_serial():
    msg = ''
    while not msg:
        msg = port.readline().decode("iso-8859-1")
    print('msg',msg)
    return msg

def shuffle_pinList(nPins):
    for i in range(nPins):
        temp = [0,1,2] # pin indicies
        random.shuffle(temp)
        for j in range(len(temp)): #list of lists to flat list
            pins_randomList.append(temp[j])

def game_shutdown(channel):
    port.write(str.encode("page "+"shutdown2")+eof) # move to the game shutdown screen when ending
    exitSound.play()
    io.cleanup()
    print('shutting down in 5 ', dct['end']) # put audio here
    os.system("sudo shutdown -h now")

        
def button_handler(pinNumber_board, pin2hit, timer): # pinNumber_board = from board
    # tone1Sound.play()
    # print('gtime start in handler for pin {}, timer {:.3f} {:.3f}'.format(pin2hit, time() - timer, time() - dct['tTotal']))
    if pinNumber_board == pinList_board[pin2hit]: # pin2hit=randint 0,1,2
        print('Correct hit on {} {}, {:.3f} {:.3f}'.format(pinNumber_board, pinList_board[pin2hit], time() - timer, time() - dct['tTotal']))
        rightSound.play()
        dct['score'] += 1
        results[0].append(time() - timer)
    else:
        print('Wrong hit, hit {} instead of {}, {:.3f} {:.3f}'.format(pinNumber_board, pinList_board[pin2hit], time() - timer, time() - dct['tTotal']))
        wrongSound.play()
        dct['wrongHits'] += 1
        results[0].append(-time() + timer)
    first = True # print first loop only
    release_timer = time()
    sleep(0.05)
    while not io.input(pinNumber_board): # wait for release, button is low and must go high
        if (time() - release_timer > 1) and first:
            print('Release the Button! {:.3f} '.format(time() - dct['tTotal']))
            first = False
    # tone2Sound.play()
    sleep(0.05)
    # print('gtime  end  in handler for pin {}, timer {:.3f} {:.3f}'.format(pin2hit, time() - timer, time() - dct['tTotal']))
    
def playgame():
    print('play game starts here, welcome screen')
    results[0] = []
    results[1] = [] # 0 is time at hit, 1 is release
    dct['score'] = 0
    dct['timeouts'] = 0
    dct['wrongHits'] = 0
    dct['tTotal'] = time()
    nTry_now = 0
    
    ready_yet = 3 # change this in Nextion
    while ready_yet == 3:
        dct['t2hit'] = int(check_serial())
        dct['nTrys'] = int(check_serial())
        ready_yet = int(check_serial())

    shuffle_pinList(dct['nTrys'])

    print('start game here, t2hitButton nTrys', dct['t2hit'], dct['nTrys'])
    print('  ')
    while nTry_now < dct['nTrys'] and dct['end'] != "end":
        sleep(0.5)
        pin2hit = pins_randomList[nTry_now]
        soundObj[pin2hit].play() # blocking
        
        if pin2hit == 0: 
            print('Lighting push')
            p.i2c_write_byte(0, 1)
            p.i2c_write_byte(0, 5) # turn off 2 and 3
            p.i2c_write_byte(0, 6)
        elif pin2hit == 1:
            print('Lighting pull')
            p.i2c_write_byte(0, 2)
            p.i2c_write_byte(0, 4) # turn off 1 and 3
            p.i2c_write_byte(0, 6)
        elif pin2hit == 2:
            print('Lighting twist')
            p.i2c_write_byte(0, 3)
            p.i2c_write_byte(0, 4) # turn off 1 and 2
            p.i2c_write_byte(0, 5)

        #sleep(0.15)
        timer = time()
        nTry_now += 1
        print('new pin2hit is {}, t is {:.3f}'.format(pin2hit, time() - dct['tTotal']))
        pushStatePrev = io.input(pushPin_board)
        pullStatePrev = io.input(pullPin_board)
        twistStatePrev = io.input(twistPin_board)
        while dct['end'] != "end": #True: # and keep_playing
            end_game = port.readline() # read(3)
            dct['end'] = end_game.decode("iso-8859-1") # end this while loop
            if (time() - timer) > dct['t2hit']:
                print('too long, global timer'.format(time() - dct['tTotal']))
                timesUpSound.play()
                dct['timeouts'] += 1
                results[0].append(time() - timer)
                break
            if not io.input(pushPin_board) and pushStatePrev:
                button_handler(pushPin_board, pin2hit, timer)
                break
            elif not io.input(pullPin_board) and pullStatePrev:
                button_handler(pullPin_board, pin2hit, timer)
                break
            elif not io.input(twistPin_board) and twistStatePrev:
                button_handler(twistPin_board, pin2hit, timer)
                break               
            pushStatePrev = io.input(pushPin_board)
            pullStatePrev = io.input(pullPin_board)
            twistStatePrev = io.input(twistPin_board)
        results[1].append(time() - timer)
        # print('Score is {} of {}, Total in game is {}'.format(dct['score'], nTry_now, dct['nTrys']))
        dct['summary_str'] = '{:3d}  {:3d}   {:3d}   {:3d} '.format(dct['score'], nTry_now, dct['wrongHits'], dct['timeouts'])
        # on page game here
        port.write(str.encode("tScore.txt=" + dbl_quote + dct['summary_str'] + dbl_quote) + eof)
        print(dct['summary_str'])
        print(' ')

    sum = 0
    n = 0
    for i in results[0]:
        if 0 < i < dct['t2hit']: # only correct hits
            sum += i
            n += 1
    if n > 0:
        avg_str = ' avg {:.1f}s'.format(sum / n)
    else:
        avg_str = ' none right :(' # no correct, avoid div by zero

    print('Final score is {} of {}, avgTime is {} with {} wrongHits and {} timeouts'.format(dct['score'], dct['nTrys'], avg_str, dct['wrongHits'], dct['timeouts']))
    print(dct['summary_str'])
    # logging.warning(' end game' + dct['summary_str'] + avg_str)
    port.write(str.encode("tAvg.txt=" + dbl_quote + avg_str + dbl_quote) + eof)
    port.write(str.encode("tGameover.txt=" + dbl_quote + 'Game Over' + dbl_quote) + eof)

    # for i, res in enumerate(results[0]):
        # print('{} {:.2f} {:.2f}'.format(i, res, results[1][i]))
#io.add_event_detect(shutdownPin, io.FALLING, callback=game_shutdown, bouncetime=50)
    
# ======START GAME HERE========
try:
    while dct['end'] != "yes":
        print('main loop, playgame inside dct[end]',dct['end'])
        playgame()
        afterPlaygameSound.play() #yay.wav
        p.i2c_write_byte(0, 4) # turn all lights off
        p.i2c_write_byte(0, 5) 
        p.i2c_write_byte(0, 6)
        print('after playgame ')
        # sleep(0.1)
        # port.write(str.encode("tGameover.txt=" + dbl_quote + 'Game Over' + dbl_quote) + eof)
        while True: # keep checking the port, this stops the game from running until they press play again
            end_game = port.readline().decode("iso-8859-1")
            # print('checking dct[end]', dct['end'])
            if end_game == "nno": # 3 chars for consistency?
                dct['end'] = end_game
                print('dct[end] = nno, back to playgame')
                port.write(str.encode("page "+"speed")+eof)
                break
            elif end_game == "yes":
                dct['end'] = end_game
                print('dct[end] = yes, cleanup')
                break # find a better way?
finally:
    io.cleanup()
    print('finally io cleanup dict[end]',dct['end'])
