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
controlMode = 0 # Mode 0 = Estop; Mode 1 = Manual; Mode 2 = Autonomous

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
        if len(raw) > 4:
            battVolts = int(raw[2:-1])/10.0
            return battVolts
    except (KeyboardInterrupt, SystemExit):
        raise
    except: # catch *all other* exceptions
        e = sys.exc_info()[0]
        #rospy.loginfo( "ERROR: ROBOTEQ Error in getBattVoltage: %s", e )
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
            #rospy.loginfo("ROBOTEQ Controller in mode:%d tot is:%d",controlMode,tot)  
            if tot > 3500:
                controlMode = 0
            elif tot < 2500:  
                controlMode = 2
            else:
                controlMode = 1             
        else:
            #print ("ROBOTEQ DRIVER PWM for Pin 4 an 5 not LEN of 8 4:%d 5:%d\n",len(pi4),len(pi5))
            pass
    except (KeyboardInterrupt, SystemExit):
        raise
    except: # catch *all other* exceptions
        e = sys.exc_info()[0]
        rospy.loginfo( "<p>ROBOTEQ Error in setControlMode: %s</p>", e )

def sendStop():
        cmd = '!G 1 0\r'
        ser.write(cmd)
        cmd = '!G 2 0\r'
        ser.write(cmd)

def moveCallback(data):
    global controlMode
    if (controlMode == 2):  # robot is in autonomous mode
        #rospy.loginfo("ROBOTEQCB Controller inside if mode:%d",controlMode)  
        speed = data.linear.x *-1000 #linear.x is value between -1 and 1 and input to wheels is between -1000 and 1000
                                    #1000 would give full speed range, but setting to lower value to better control robot
        turn = (data.angular.z + 0.009)*500 
        # G - Go to Speed or to Relative Position
        #print speed,turn
        cmd = '!G 1 ' + str(speed) + '\r'
        ser.write(cmd)
        cmd = '!G 2 ' + str(turn) + '\r'
        ser.write(cmd)

# Configures the roboteq motor controller to work with Iggy. Relying on the EEPROM has proven unreliable.
def initalizeController():    
    ser.write('^ECHOF 1\r') # Turn on the ESTOP while resetting the configuration for the roboteq device
    time.sleep(.01)
    result = getdata()   
    ser.write('!R 0\r') # Stop any scripts running on the roboteq device
    time.sleep(.01)
    result = getdata()
    # Below are the initial configurations for the Roboteq motor controller required by Iggy. See RoboteqIggySettings.pdf.
    configCmds=['^CPRI 1 0\r',      '^CPRI 2 1\r',      '^OVL 350\r',       '^UVL 180\r',
                '^MAC 1 20000\r',   '^MAC 2 20000\r',   '^MXRPM 1 3500\r',  '^MXRPM 2 3500\r',
                '^MXMD 1\r',        '^PMOD 0 1\r']
    # Send commands to Roboteq and exit if any fail
    for cmd in configCmds:
        result = getdata()
        ser.write(cmd)
        time.sleep(.01)
        result = getdata()
        if (result != '+\r'): # If setting the configuration fails
            ser.write('!E 1\r') # Turn on the ESTOP
            ser.write('!R 0\r') # Stop any scripts
            rospy.loginfo("ERROR: ROBOTEQ DRIVER FAILED TO SET CONFIGURATION WITH: ", cmd,"\n")
            rospy.loginfo("ERROR: ROBOTEQ DRIVER CONFIGURATION FAILED. NOW EXITING ROBOTEQ DRIVER\n\n")
            exit()

    ser.write('!R 1\r') # Restart any scripts running on the roboteq device 
    time.sleep(.01)
    result = getdata()
    rospy.loginfo("SUCCESSFULLY CONFIGURED THE ROBOTEQ MOTOR CONTROLLER\n")
    
if __name__ == "__main__":
    global controlMode

    # configure the serial connections 
    try:
        ser = serial.Serial(
            port='/dev/roboteq',
            baudrate=115200, #8N1
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout=1
        )
    except:
        try:
            ser = serial.Serial(
                port='/dev/ttyACM0',
                baudrate=115200, #8N1
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                bytesize=serial.EIGHTBITS,
                timeout=1
            )
        except:
            raise
    if (ser.isOpen()):
        ser.close()
    ser.open()

    #TODO rename to iggy_roboteq and align the namespace
    rospy.init_node('iggy_roboteq', anonymous=True)
    auto_pub = rospy.Publisher("/autonomous", Bool, queue_size=1)
    #29.2 volts when batteries are below 2 bars but still above 1 bar.
    volt_pub = rospy.Publisher("/voltage", Float32, queue_size=1)
    rospy.Subscriber("/cmd_vel", Twist, moveCallback)

    initalizeController()

    rate = rospy.Rate(30)
    lastControlMode = controlMode
    while not rospy.is_shutdown():
        setControlMode()
        #rospy.loginfo("ROBOTEQ Controller in mode:%d",controlMode)  
        battVolt = getBattVoltage()
        if battVolt > 0.01:            
            volt_pub.publish(Float32(battVolt))
        if (controlMode == 2):  #robot is in autonomous mode
            auto_pub.publish(True) # Turn on autonomous lights
        else:    
            if(lastControlMode == 2):
                sendStop()
            auto_pub.publish(False)
        lastControlMode = controlMode
        rate.sleep()

    ser.close()

