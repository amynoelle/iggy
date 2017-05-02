#!/usr/bin/python
import time
import serial
import rospy
from sensor_msgs.msg import NavSatFix
from sensor_msgs.msg import Imu
from geometry_msgs.msg import Vector3
from geometry_msgs.msg import Twist
from usma_novatel_parser import *
from os.path import expanduser
from std_msgs.msg import String

# TODO rename these devices. raw_gps is not accurate name, this the cns5000 ins device.
ser = serial.Serial(
    port='/dev/raw_gps',
    #baudrate=115200, #8N1
    baudrate=9600, #8N1
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS
)

if (ser.isOpen()):
    ser.close()
ser.open()

timestr = time.strftime("%Y_%m_%d_%H_%M_%S")
# Create a log file
home = expanduser("~")
fName = home+"/Data/novatelLog"+timestr+".txt"
#outFile = open(fName, "wb")
rospy.init_node('kvh_cns5000_averaging', anonymous=True)
rate = rospy.Rate(5) 
try:
    while not rospy.is_shutdown():  
        while ser.inWaiting() > 0:
            kvh5000_output = ser.readline() # Read data a line of data from buffer
            print (kvh5000_output) # Option to log data to file
	    rate.sleep()

                     
except KeyboardInterrupt:

    #outFile.close() 
    ser.close()
    raise

