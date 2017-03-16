#!/usr/bin/python

'''
The Roboteq Controller has a script running on it that will enable and disable the ESTOP with no
interaction from this code. The ESTOP and manual driving modes are not effected by this script, they
are independent of Iggy's computer.


SYMBOLS USED FOR SERIAL COMMO:
~ Read configuration operation 
^ Write configuration operation 
! Run time commands
? Run time queries
% Maintenance Commans
_ Carriage return

+ Command acknowledgement
- Command not recognized or accepted

'''
import serial
import rospy
import time
import os
import sys
from std_msgs.msg import Bool
from std_msgs.msg import Float32
from geometry_msgs.msg import Twist

from geometry_msgs.msg import Vector3

# global variables
controlMode = 1 # Mode 0 = Estop; Mode 1 = Manual; Mode 2 = Autonomous

# Create a log file
#outFile = open("roboteqLog.txt", "wb")

# While data is in the buffer read it, then return it.
def getdata():
    info = ''
    while ser.inWaiting() > 0: 
        char = ser.read()
        #if char != '\r':
        info += str(char)
    #print "INFO: ",info
    #if len(info) == 7:
    #    return info[3:7]
    return info

def getBattVoltage():
    try:
        ser.write('?V 2\r') #pulse input of pulse-in 5 (switch - down)
        time.sleep(.005)
        raw = getdata()
        if len(raw)==6:
            battVolts = int(raw[2:5])/10.0
            return battVolts
    except (KeyboardInterrupt, SystemExit):
        raise
    except: # catch *all other* exceptions
        e = sys.exc_info()[0]
        print( "<p>ROBOTEQ Error in getBattVoltage: %s</p>" % e )
    return -999.999 # an error has occured and we return

# Updates the global variable for controlMode
def setControlMode():
    global controlMode # use the global controlMode
    try:
        getdata()   #clear buffer
        ser.write('?PI 4\r') #pulse input of pulse-in 4 (switch - up)
        time.sleep(.005)
        pi4 = getdata()
        ser.write('?PI 5\r') #pulse input of pulse-in 5 (switch - down)
        time.sleep(.005)
        pi5 = getdata()
        if len(pi4) == len(pi5) == 8: # process only if data is valid
            tot = int(pi4[3:7]) + int(pi5[3:7])
            if tot > 3500:
                controlMode = 0
            elif tot < 2500:    
                controlMode = 2
            else:
                controlMode = 1
    except (KeyboardInterrupt, SystemExit):
        raise
    except: # catch *all other* exceptions
        e = sys.exc_info()[0]
        print( "<p>ROBOTEQ Error in setControlMode: %s</p>" % e )

def moveCallback(data):
    global controlMode
    if (controlMode == 2):  # robot is in autonomous mode
        speed = data.linear.x *1000 #linear.x is value between -1 and 1 and input to wheels is between -1000 and 1000
                                    #1000 would give full speed range, but setting to lower value to better control robot
        turn = (data.angular.z + 0.009)*500*-1 
        # G - Go to Speed or to Relative Position
        print speed,turn
        cmd = '!G 1 ' + str(speed) + '\r'
        ser.write(cmd)
        cmd = '!G 2 ' + str(turn) + '\r'
        ser.write(cmd)

# Configures the roboteq motor controller to work with Iggy. Relying on the EEPROM has proven unreliable.
def initalizeController():
    # Below are the initial configurations for the Roboteq motor controller required by Iggy. See RoboteqIggySettings.pdf.
    configCmds=['^CPRI 1 1\r',      '^CPRI 2 0\r',      '^OVL 350\r',       '^UVL 180\r',
                '^MAC 1 20000\r',   '^MAC 2 20000\r',   '^MXRPM 1 3500\r',  '^MXRPM 2 3500\r',
                '^MXMD 1\r',        '^PMOD 0 1\r']

    # Send commands to Roboteq and exit if any fail
    for cmd in configCmds:
        ser.write(cmd)
        time.sleep(.01)
        result = getdata()
        if (result != '+\r'):
            print "ERROR: ROBOTEQ DRIVER FAILED TO SET CONFIGURATION WITH: ", cmd,"\n"
            print "ERROR: ROBOTEQ DRIVER CONFIGURATION FAILED. NOW EXITING ROBOTEQ DRIVER\n\n"
            exit()
        print "SUCCESSFULLY CONFIGURED THE ROBOTEQ MOTOR CONTROLLER\n"
    
if __name__ == "__main__":

    # configure the serial connections 
    try:
        ser = serial.Serial(
            port='/dev/roboteq',
            baudrate=115200, #8N1
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS
        )
    except:
        try:
            ser = serial.Serial(
                port='/dev/ttyACM0',
                baudrate=115200, #8N1
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS
            )
        except:
            raise

    if (ser.isOpen()):
        ser.close()
    ser.open()

    #TODO rename to iggy_roboteq and align the namespace
    rospy.init_node('iggy_roboteq', anonymous=True)
    auto_pub = rospy.Publisher("/autonomous", Bool, queue_size=1)
    volt_pub = rospy.Publisher("/voltage", Float32, queue_size=1)
    rospy.Subscriber("/cmd_vel", Twist, moveCallback)

    rate = rospy.Rate(30)
    while not rospy.is_shutdown():
        setControlMode()
        battVolt = Float32(getBattVoltage())
        if battVolt > 0:
            volt_pub.publish(battVolt)
        if (controlMode == 2):  #robot is in autonomous mode
            auto_pub.publish(True) # Turn on autonomous lights
        else:    
            auto_pub.publish(False)
        rate.sleep()

    ser.close()
