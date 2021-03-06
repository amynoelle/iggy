#!/usr/bin/env python

""" 
imu_complementary_filter
source: http://www.phidgets.com/docs/Compass_Primer
"""
import math
import rospy
import tf
import sys
from std_msgs.msg import Float32
from sensor_msgs.msg import NavSatFix, Imu
from geometry_msgs.msg import PoseWithCovarianceStamped, Quaternion, Twist, Vector3Stamped
from nav_msgs.msg import Odometry

class MagHeading():
    def __init__(self):
        self.mMagnetic=Vector3Stamped()
        self.yaw_offset = 0
        self.world_yaw = 0
        self.magnetic_filter_data = []
        self.magnetic_filter_size = 15 #10 TODO return to a deeper filter
        # Give the node a name
        rospy.init_node('imu_filtered', anonymous=False)

        # Publisher of type nav_msgs/Odometry
        self.magHdg_pub = rospy.Publisher('simpleup/compass', Float32, queue_size=1)
        self.world_imu_pub = rospy.Publisher ('simpleup/global', Imu, queue_size=1)
        #self.br = tf.TransformBroadcaster()

        # Subscribe to the gps positions
        rospy.Subscriber('xsens/magnetic', Vector3Stamped, self.update_magnetic_callback)
        rospy.Subscriber('xsens/imu/data', Imu, self.update_imu_callback)

    # Receive the updated magnetic Vector3Stamped to calculate world_yaw and publish a compass heading
    def update_magnetic_callback(self, msg):
        self.magnetic_filter_data.append(math.atan2(msg.vector.x,msg.vector.y)) # add heading in radians to filter
        if (len(self.magnetic_filter_data) > self.magnetic_filter_data):
            self.magnetic_filter_data.pop(0)
        # Set the world yaw based on magnetic field
        self.world_yaw = sum(self.magnetic_filter_data)/len(self.magnetic_filter_data)
        # publish the compass direction of the robot is facing for human usage only
        self.magHdg_pub.publish(self.calcBasicHeading(msg.vector.x,msg.vector.y))        
        
    def update_imu_callback(self, msg):
        worldImu=msg
        #subtract self.yaw_offset
        quat_offset = tf.transformations.quaternion_from_euler(0, 0, self.yaw_offset)
        worldImu.orientation.x -= quat_offset[0]
        worldImu.orientation.y -= quat_offset[1]
        worldImu.orientation.z -= quat_offset[2]
        worldImu.orientation.w -= quat_offset[3]
        self.world_imu_pub.publish(worldImu)

    def calcBasicHeading(self, x, y):
        headingDeg = math.atan2(y,x) * (180/math.pi)
        if (headingDeg<0):
                headingDeg = headingDeg+360
        return headingDeg
              
    def magnetic_filter(self):
        mags = []
        for i in range(15): # Collect 50 magnetic readings in 5 Seconds
            mags.append(math.atan2(self.mMagnetic.vector.x,self.mMagnetic.vector.y))
            rospy.sleep(0.1)
        mag = sum(mags) / float(len(mags)) # Average the 50 readings and use this as the offset
        rospy.loginfo(" ")
        rospy.loginfo(">>>>>>>>>>>>>>>>>>>>>>>> OK TO MOVE THE ROBOT  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<")
        return mag

    def mainLoop(self):
        worldImu= Imu()
        rate = rospy.Rate(30)
        pub_tf=rospy.get_param('~pub_tf',"false")
        imu_frame=rospy.get_param('~imu_frame','imu')
        world_frame=rospy.get_param('world_frame','odom')
        rospy.spin()

if __name__ == '__main__':
        mHead=MagHeading()
        mHead.mainLoop()

'''
rosmsg show geometry_msgs/Vector3Stamped 
std_msgs/Header header
  uint32 seq
  time stamp
  string frame_id
geometry_msgs/Vector3 vector
  float64 x
  float64 y
  float64 z


rosmsg show sensor_msgs/Imu
std_msgs/Header header
  uint32 seq
  time stamp
  string frame_id
geometry_msgs/Quaternion orientation
  float64 x
  float64 y
  float64 z
  float64 w
float64[9] orientation_covariance
geometry_msgs/Vector3 angular_velocity
  float64 x
  float64 y
  float64 z
float64[9] angular_velocity_covariance
geometry_msgs/Vector3 linear_acceleration
  float64 x
  float64 y
  float64 z
float64[9] linear_acceleration_covariance

'''
        

