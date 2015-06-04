#include <ros/ros.h>
#include <igvc_roboteq/oop_igvc_odometry.h>
#include <geometry_msgs/Vector3.h>
#include <nav_msgs/Odometry.h>
#include <time.h>
#include <tf/transform_broadcaster.h>


class IgvcOdometry
{
public:
  IgvcOdometry()
  {
    _PreviousLeftEncoderCounts = 0;
    _PreviousRightEncoderCounts = 0; 
    //math for measured --> 70.16cm per rotation /32 = .021927 meters per count
    //math from calc --> (3.14159265 * 0.2286) / 32;  //meters per count (pie * diameter) / #black+white
    DistancePerCount = 0.021927;  //meters per count
    axisWidth = 0.6; //0.6 = wheel between tires
    heading = 0;
    x = 0;
    y = 0;

    //Topic you want to publish
    pub_ = n_.advertise<geometry_msgs::Vector3>("/published_topic", 1);

    //Topic you want to subscribe
    sub_ = n_.subscribe("/subscribed_topic", 1, &IgvcOdometry::callback, this);
  }

  void callback(const geometry_msgs::Vector3::ConstPtr& ticks)
  {
    float deltaLeft;
    float deltaRight;
    float deltaTime;
    float changeHeading;
    ros::Time last_time_encoder = current_time_encoder;
    current_time_encoder = ros::Time::now();
    deltaTime = (current_time_encoder - last_time_encoder).toSec();

    //ROS_INFO("vx %f, vy %f, deltaTime %f", vx, vy, deltaTime);

    if (deltaTime > 0.05) {

        deltaLeft = ticks->x - _PreviousLeftEncoderCounts;
        deltaRight = ticks->y - _PreviousRightEncoderCounts;

        geometry_msgs::Vector3 output;
        //.... do something with the input and generate the output...

        float vRightLast = vRight;
        float vLeftLast = vLeft;
        vRight = deltaRight * DistancePerCount / deltaTime;
        vLeft = deltaLeft * DistancePerCount / deltaTime;

//    vRight = aRight*deltaTime + vRightLast
    



        if (fabs(deltaLeft - deltaRight) < 1.0e-6) { // basically going straight
            float new_x = x + deltaLeft * cos(heading);
            float new_y = y + deltaRight * sin(heading);
            //float heading = heading;
            changeHeading = 1;
        } else {
            float R = axisWidth * (deltaLeft + deltaRight) / (2 * (deltaRight - deltaLeft));
            //R = turn radius for circular traj of robot center
            float wd = (deltaRight - deltaLeft) / axisWidth;  

            float new_x = x + R * sin(wd + heading) - R * sin(heading);
            float new_y = y - R * cos(wd + heading) + R * cos(heading);
            //heading = boundAngle(heading + wd);
            changeHeading = 0;
        }
            float aRight = (vRight - vRightLast)/deltaTime;
            float aLeft = (vLeft - vLeftLast)/deltaTime;
                                   
            float A = (aRight + aLeft)/2;
            float B = (vRightLast + vLeftLast)/2;
            float C = (aRight + aLeft)/(2*axisWidth);
            float D = (vRightLast - vLeftLast)/axisWidth;
            float vx = (A*deltaTime + B) * cos(C*deltaTime*deltaTime + D*deltaTime + heading);
            float vy = (A*deltaTime + B) * sin(C*deltaTime*deltaTime + D*deltaTime + heading);
            float newHeading = (C*deltaTime*deltaTime + D*deltaTime)*changeHeading + heading;
            float deltaHeading = (heading - newHeading)/ deltaTime;

    //pub things
        geometry_msgs::Quaternion odom_quat = tf::createQuaternionMsgFromYaw(heading);

        //send the transform
/*        geometry_msgs::TransformStamped odom_trans;
        odom_trans.header.stamp = current_time_encoder;
        odom_trans.header.frame_id = "wheels_odom";
        odom_trans.child_frame_id = "base_link";

        odom_trans.transform.translation.x = x;
        odom_trans.transform.translation.y = y;
        odom_trans.transform.translation.z = 0.0;
        odom_trans.transform.rotation = odom_quat;
        odom_broadcaster.sendTransform(odom_trans);
*/
        //header
        nav_msgs::Odometry odom;
        odom.header.stamp = current_time_encoder;
        odom.header.frame_id = "wheels_odom";
        //set the position
        odom.pose.pose.position.x = x;
        odom.pose.pose.position.y = y;
        odom.pose.pose.position.z = 0.0;
        odom.pose.pose.orientation = odom_quat;
        //set the velocity
        odom.child_frame_id = "base_link";
        odom.twist.twist.linear.x = vx;
        odom.twist.twist.linear.y = vy;
        odom.twist.twist.angular.z = deltaHeading;

    
        pub_.publish(odom);
        _PreviousLeftEncoderCounts = ticks->x;
        _PreviousRightEncoderCounts = ticks->y;

    }

  }

private:
    ros::Time current_time_encoder;
    float heading;
    float DistancePerCount;
    float axisWidth;

    float x;
    float y;
    float vRight, vLeft;
    long _PreviousLeftEncoderCounts;
    long _PreviousRightEncoderCounts; 

    ros::NodeHandle n_;
    ros::Publisher pub_;
    ros::Subscriber sub_;

};//End of class IgvcOdometry

int main(int argc, char **argv)
{
  //Initiate ROS
  ros::init(argc, argv, "subscribe_and_publish");

  //Create an object of class IgvcOdometry that will take care of everything
  IgvcOdometry SAPObject;

  ros::spin();

  return 0;
}
