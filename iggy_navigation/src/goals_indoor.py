#!/usr/bin/env python

""" Code modified by: Dominic Larkin 5FEB2016
    
    Command a robot to move autonomously among a number of goal locations defined in the map frame.
    Attempt to move to each location in succession.  Keep track of success rate, time elapsed, and 
    total distance traveled. The goal locations are determined by adding distances to the current
    location of the robot on the map.
    based on nav_test.py - Version 0.1 2012-01-10. Modified for use with West Point Robotics.
    Originaly created for the Pi Robot Project: http://www.pirobot.org
    Copyright (c) 2012 Patrick Goebel.  All rights reserved.
    Original License:
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.


example of usage: rosrun igvc_run goal_pub_IGVC.py _waypoint_file:=igvc_run/src/waypoints3.csv

"""
import csv
import roslib
import rospy
import actionlib
import time
from actionlib_msgs.msg import *
from geometry_msgs.msg import Pose, PoseWithCovarianceStamped, Point, Quaternion, Twist
from move_base_msgs.msg import MoveBaseAction, MoveBaseGoal
from sensor_msgs.msg import NavSatFix
from nav_msgs.msg import Odometry 
from random import sample
from math import pow, sqrt
from LatLongUTMconversion import LLtoUTM, UTMtoLL
from marker_pub import make_waypoint_viz
from visualization_msgs.msg import Marker
from copy import deepcopy

class Navigator():

    def __init__(self):     
        rospy.init_node('nav_test2', anonymous=True)        
        rospy.on_shutdown(self.shutdown)    

        rospy.Subscriber('/odometry/filtered', Odometry, self.update_current_pose_map)

        # Publisher to manually control the robot (e.g. to stop it)
        self.cmd_vel_pub = rospy.Publisher('cmd_vel', Twist, queue_size=1) 
        self.vis_pub = rospy.Publisher( "visualization_marker", Marker, queue_size=1 )  

        # Subscribe to the move_base action server
        self.goal = MoveBaseGoal()
        self.move_base = actionlib.SimpleActionClient("move_base", MoveBaseAction)        
        rospy.loginfo("Waiting for move_base action server...")        
        # Wait upto 60 seconds for the action server to become available
        self.move_base.wait_for_server(rospy.Duration(60))        
        rospy.loginfo("Connected to move base server")   
        # Goal state return values
        self.goal_states = ['PENDING', 'ACTIVE', 'PREEMPTED', 
                       'SUCCEEDED', 'ABORTED', 'REJECTED',
                       'PREEMPTING', 'RECALLING', 'RECALLED',
                       'LOST']      
        # A variable to hold the initial and current pose of the robot
        self.current_pose_map = Odometry()
        self.initial_pose = Odometry()

    def calculateDist(self, currX, currY, lastX, lastY, isFirst):
        distance = 0
        if isFirst != True:
            distance = sqrt(pow(currX - lastX, 2) + pow(currY - lastY, 2))             
        else:
            distance =  sqrt(pow(currX - self.initial_pose.pose.pose.position.x, 2) + pow(currY - self.initial_pose.pose.pose.position.y, 2))
        rospy.loginfo("curr , init (%.4f, %.4f), (%.4f, %.4f)"% (currX,currY,self.initial_pose.pose.pose.position.x,self.initial_pose.pose.pose.position.y))
        return distance

    def navigate(self):
        # Variables to keep track of success rate, running time, and distance traveled
        n_goals = 0
        n_successes = 0
        distance_traveled = 0
        running_time = 0
        start_time = rospy.Time.now()

        self.setInitialPose() # In the odom frame. TODO Should this be in map frame?
        # TODO Use Dynamic reconfig for this
        # How long in seconds should the robot pause at each location?
        rest_time = rospy.get_param("~rest_time", 0)

        # DML read the filename from rosparam server
        wp_file = rospy.get_param("~waypoint_file", "waypoint.csv")
        #wp_file = "params/waypoints3.csv"
        rospy.loginfo("Waypoing file is: %s",wp_file)
        # TODO getGoals in odom frame, make a list in odom frame. 
        goals=self.makeWaypointsIntoGoals(wp_file)
        i=0        
        firstRun=True
        # Begin the main loop and run through a sequence of locations
        while not rospy.is_shutdown():                        
            # Keep track of the distance traveled.
            # TODO update the tolerance and pause time.

            cur_coord = goals[i][0]
            last_coord = goals[i-1][0]
            distance = self.calculateDist(cur_coord.position.x, cur_coord.position.y, last_coord.position.x, last_coord.position.x, firstRun)
            # Set up the next goal location
            self.goal.target_pose.pose = cur_coord
            self.goal.target_pose.header.frame_id = 'map'
            self.goal.target_pose.header.stamp = rospy.Time.now()
            self.goal.target_pose.pose.orientation = self.initial_pose.pose.pose.orientation
            # Let the user know where the robot is going next
            rospy.loginfo("Going to %2d: (%.4f,%.4f) distance: %.4f" %(i,self.goal.target_pose.pose.position.x,self.goal.target_pose.pose.position.y,distance))

            # Start the robot toward the next location
            self.move_base.send_goal(self.goal)

            # Allow 5 minutes to get there
            finished_within_time = self.move_base.wait_for_result(rospy.Duration.from_sec(300)) 

            # Check for success or failure
            if not finished_within_time:
                self.move_base.cancel_goal()
                rospy.loginfo("Timed out achieving goal")
            else:
                state = self.move_base.get_state()
                print "GOAL STATE IS:",self.goal_states[state]
                if state == GoalStatus.SUCCEEDED:
                    rospy.loginfo("Goal succeeded!")
                    n_successes += 1
                    distance_traveled += distance
                    rospy.loginfo("State:" + str(state))
                else:
                  rospy.loginfo("Goal failed with error code: " + str(self.goal_states[state]))

            # How long have we been running?
            running_time = rospy.Time.now() - start_time
            running_time = running_time.secs / 60.0

            # Print a summary success/failure, distance traveled and time elapsed
            n_goals += 1
            rospy.loginfo("Success so far: " + str(n_successes) + "/" + 
                          str(n_goals) + " = " + 
                          str(100 * n_successes/n_goals) + "%")            
            rospy.loginfo("Running time: %.2f min Distance: %.3f"  % (running_time,distance_traveled))

            # Increment the counters
            i += 1
            if (i >= len(goals)):
                rospy.signal_shutdown("NO MORE GOALS TO ACHIEVE")        
            firstRun=False
            rospy.sleep(rest_time)

    def setInitialPose(self): 
        while (self.current_pose_map.header.seq==0):
            rospy.sleep(0.3)   
            rospy.loginfo("wainting for initial pose")                    
        # Make sure we have the initial pose
        rospy.loginfo("Waiting for initial pose")
        # Get the initial pose from the robot
        rospy.loginfo("Initial Pose from /odometry/gps recieved")
        rospy.loginfo("Establishing initial position wait 10 seconds. Do not move the robot.")
        xs=[]
        ys=[]
        time.sleep(2)
        for i in range(5): #TODO move back to 10
            xs.append(self.current_pose_map.pose.pose.position.x) #TODO add check for empty array
            ys.append(self.current_pose_map.pose.pose.position.y) #self.initial_pose.pose.pose.position.x
            rospy.sleep(0.3)
        easting = sum(xs) / float(len(xs))
        northing = sum(ys) / float(len(ys))
        self.initial_pose = deepcopy(self.current_pose_map) 
        rospy.loginfo("Initial pose at (%.4f, %.4f)" % (easting, northing) )
        print self.current_pose_map

    # Read from file the list of Lat,Long,Duration,Tolerance and put into a list of waypoints
    def loadWaypoints(self, filename):
        waypoints=[]
        with open(filename, mode='r') as infile:
            reader = csv.reader(infile)
            for row in reader:
                if row[0][0] != '#': # ignore commented out waypoints
                    waypoints.append((float(row[0]),float(row[1]),float(row[2]),float(row[3]),(row[4]),(row[5])))
        return waypoints

    # Turn list of waypoints into Goals. Goals are coordinates in the robot odom frame.
    # TODO Look into makeing this in the map frame if map and odom frame diverge.
    def makeWaypointsIntoGoals(self, filename):
        time.sleep(1)
        #while (not self.initial_utm):
        #    rospy.loginfo("Waiting for initial position")
        # Load and parse waypoints from file.
        wps = self.loadWaypoints(filename)
        goalstemp=[]
        for waypointX,waypointY,search_duration,rest_duration,unused,identity in wps:
            goal_pose=Pose()
            goal_pose.position=deepcopy(self.initial_pose.pose.pose.position) 
            goal_pose.position.x=(waypointX - self.initial_pose.pose.pose.position.x) # REP103 says x is east and y is north
            goal_pose.position.y=(waypointY - self.initial_pose.pose.pose.position.y)            
            self.vis_pub.publish( make_waypoint_viz(goal_pose,identity[-2:],int(identity[-2:]) ))
            rospy.loginfo("x,y for waypoint#: %s in odom frame: (%.4f, %.4f)"%(identity[-2:],goal_pose.position.x, goal_pose.position.y))
            goalstemp.append((goal_pose, search_duration, rest_duration))
        return goalstemp

    def update_current_pose_map(self, current_pose_map):
        #rospy.loginfo("Current Pose is: (%.4f,%.4f)" %((current_pose_map.pose.pose.position.x),(current_pose_map.pose.pose.position.y)))        
        self.current_pose_map = current_pose_map

    def shutdown(self):
        rospy.loginfo("Stopping the robot...")
        self.move_base.cancel_goal()
        rospy.sleep(2)
        self.cmd_vel_pub.publish(Twist())
        rospy.sleep(1)

if __name__ == '__main__':
    global nav
    try:
        nav = Navigator()
        nav.navigate()        
        rospy.spin()
    except rospy.ROSInterruptException:
        rospy.loginfo("Navigation test finished.")


'''
user1:iggy_navigation$ rosmsg show move_base_msgs/MoveBaseGoal 
geometry_msgs/PoseStamped target_pose
  std_msgs/Header header
    uint32 seq
    time stamp
    string frame_id
  geometry_msgs/Pose pose
    geometry_msgs/Point position
      float64 x
      float64 y
      float64 z
    geometry_msgs/Quaternion orientation
      float64 x
      float64 y
      float64 z
      float64 w

user1:iggy_navigation$ rosmsg show move_base_msgs/MoveBaseAction
move_base_msgs/MoveBaseActionGoal action_goal
  std_msgs/Header header
    uint32 seq
    time stamp
    string frame_id
  actionlib_msgs/GoalID goal_id
    time stamp
    string id
  move_base_msgs/MoveBaseGoal goal
    geometry_msgs/PoseStamped target_pose
      std_msgs/Header header
        uint32 seq
        time stamp
        string frame_id
      geometry_msgs/Pose pose
        geometry_msgs/Point position
          float64 x
          float64 y
          float64 z
        geometry_msgs/Quaternion orientation
          float64 x
          float64 y
          float64 z
          float64 w
move_base_msgs/MoveBaseActionResult action_result
  std_msgs/Header header
    uint32 seq
    time stamp
    string frame_id
  actionlib_msgs/GoalStatus status
    uint8 PENDING=0
    uint8 ACTIVE=1
    uint8 PREEMPTED=2
    uint8 SUCCEEDED=3
    uint8 ABORTED=4
    uint8 REJECTED=5
    uint8 PREEMPTING=6
    uint8 RECALLING=7
    uint8 RECALLED=8
    uint8 LOST=9
    actionlib_msgs/GoalID goal_id
      time stamp
      string id
    uint8 status
    string text
  move_base_msgs/MoveBaseResult result
move_base_msgs/MoveBaseActionFeedback action_feedback
  std_msgs/Header header
    uint32 seq
    time stamp
    string frame_id
  actionlib_msgs/GoalStatus status
    uint8 PENDING=0
    uint8 ACTIVE=1
    uint8 PREEMPTED=2
    uint8 SUCCEEDED=3
    uint8 ABORTED=4
    uint8 REJECTED=5
    uint8 PREEMPTING=6
    uint8 RECALLING=7
    uint8 RECALLED=8
    uint8 LOST=9
    actionlib_msgs/GoalID goal_id
      time stamp
      string id
    uint8 status
    string text
  move_base_msgs/MoveBaseFeedback feedback
    geometry_msgs/PoseStamped base_position
      std_msgs/Header header
        uint32 seq
        time stamp
        string frame_id
      geometry_msgs/Pose pose
        geometry_msgs/Point position
          float64 x
          float64 y
          float64 z
        geometry_msgs/Quaternion orientation
          float64 x
          float64 y
          float64 z
          float64 w



user1@ros20:~/catkin_ws/src/iggy/iggy_navigation$ rosmsg show nav_msgs/Odometry 
std_msgs/Header header
  uint32 seq
  time stamp
  string frame_id
string child_frame_id
geometry_msgs/PoseWithCovariance pose
  geometry_msgs/Pose pose
    geometry_msgs/Point position
      float64 x
      float64 y
      float64 z
    geometry_msgs/Quaternion orientation
      float64 x
      float64 y
      float64 z
      float64 w
  float64[36] covariance
geometry_msgs/TwistWithCovariance twist
  geometry_msgs/Twist twist
    geometry_msgs/Vector3 linear
      float64 x
      float64 y
      float64 z
    geometry_msgs/Vector3 angular
      float64 x
      float64 y
      float64 z
  float64[36] covariance
'''
