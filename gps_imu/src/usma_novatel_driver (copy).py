#!/usr/bin/python
import time
import serial
import rospy
from sensor_msgs.msg import NavSatFix
from sensor_msgs.msg import Imu
from geometry_msgs.msg import Vector3
from geometry_msgs.msg import Twist
from usma_novatel_parser import *

# configure the serial connections 
ser = serial.Serial(
    port='/dev/ttyUSB0',
    baudrate=9600, #8N1
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS
)

if (ser.isOpen()):
    ser.close()
ser.open()

# Create a log file
outFile = open("novatelLog.txt", "wb")
# Send commands to CNS-5000 to start the logs
ser.write('unlogall\r\n')
#ser.write('LOG COM1 RAWIMUSA ONTIME 0.5\r\n')
#ser.write('LOG COM1 BESTGPSPOSA ONTIME 0.5\r\n')
##TODO write code to align the INS using coarse method on page 38 of the CNS 5000 manul.
#align = input("What directions  in degrees is the robot facing:")
#command = 'SETINITAZIMUTH ' + str(align) + ' 10\r\n'
#ser.write(command)
ser.write('LOG COM1 TERRASTARINFO\r\n')
while ser.inWaiting() > 0: # While data is in the buffer
    velodyne_output = ser.readline() # Read data a line of data from buffer
    outFile.write(velodyne_output) # Option to log data to file
    print velodyne_output
