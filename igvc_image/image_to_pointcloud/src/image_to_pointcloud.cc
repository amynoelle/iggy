/*
  Author: Stuart Baker
  Revision Date: 2013-06-02
  File Name: image_to_pointcloud.cc
  Description: A ROS nodelet to create a pointcloud from a given image.
                     Key assumptions:
                      - the camera is fixed in space (its transform doesn't change)
                      - the surface beneath the camera can be approximated as a flat plane
*/

// General C++ includes
#include <string>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
// General ROS includes
#include <ros/ros.h>
#include <nodelet/nodelet.h>
#include <tf/transform_listener.h>

// OpenCV includes
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

// PCL includes
// #include <sensor_msgs/PointCloud2.h>
// #include <pcl_ros/point_cloud.h>
// #include <pcl/ros/conversions.h>
// #include <pcl/point_cloud.h>
// #include <pcl/point_types.h>
// #include <pcl_conversions/pcl_conversions.h>

// Dynamic reconfigure includes
// #include <dynamic_reconfigure/server.h>
// #include <dynamic_reconfigure/config_tools.h>
// #include <dynamic_reconfigure/ConfigDescription.h>
// #include <dynamic_reconfigure/ParamDescription.h>
// #include <dynamic_reconfigure/Group.h>
// #include <dynamic_reconfigure/config_init_mutex.h>
// #include <line_filter/ImageToPointCloudConfig.h>

// Defines
#define _USE_MATH_DEFINES

namespace enc = sensor_msgs::image_encodings;
namespace igvc_image_pipeline
{
  class ImageToPointCloud : public nodelet::Nodelet
  {
    public:
      ImageToPointCloud() {}
      ~ImageToPointCloud() {}

    private:
      ros::NodeHandle nh_;
      ros::NodeHandle private_nh_;

      ros::Publisher cloud_pub_;

      boost::shared_ptr<image_transport::ImageTransport> it_;
      image_transport::Subscriber image_sub_;

      // dynamic_reconfigure::Server<line_filter::ImageToPointCloudConfig> reconfigure_server_;
      // dynamic_reconfigure::Server<line_filter::ImageToPointCloudConfig>::CallbackType f_;

      tf::Vector3 origin_;


      //pcl::PointCloud<pcl::PointXYZ> output_cloud_;
      int n;
      sensor_msgs::PointCloud output_cloud_;
      double x_offset_, x_meter_pixel_, y_meter_pixel_;
      int image_height_, image_width_;
      cv::Mat image_transform_;

      virtual void onInit();
      void computeTransform(void);
      // void configCb(line_filter::ImageToPointCloudConfig&, uint32_t);
      void imageCb(const sensor_msgs::ImageConstPtr&);
      void createPoint(int, int);
  }; // class ImageToPointCloud
} // namespace igvc_image_pipeline

/*

Split for header

*/

// #include <image_to_pointcloud.h>
#include <pluginlib/class_list_macros.h>

namespace igvc_image_pipeline
{
  void ImageToPointCloud::onInit()
  {
    // Establish the nodehandles
    nh_ = getNodeHandle();
    private_nh_ = getNodeHandle();

    // Setup the subscribers and publishers
    cloud_pub_ = nh_.advertise<sensor_msgs::PointCloud>("out",1);//<pcl::PointCloud<pcl::PointXYZ> > ("out", 1);

    // Setup the image transport subscriber
    it_.reset(new image_transport::ImageTransport(nh_));
    image_sub_ = it_->subscribe("filter_out", 1, &ImageToPointCloud::imageCb, this);

    computeTransform();

    // Setup the dynamic reconfigure server
    // f_ = boost::bind(&ImageToPointCloud::configCb, this, _1, _2);
    // reconfigure_server_.setCallback(f_);

    // Set the frames and grab the transform
    try
    {
      tf::TransformListener tf_listener_;
      tf::StampedTransform transform_;

      tf_listener_.waitForTransform("/camera_frame", "/base_footprint", ros::Time(0), ros::Duration(10.0) );
      tf_listener_.lookupTransform("/camera_frame", "/base_footprint", ros::Time(0), transform_);

      origin_ = transform_.getOrigin();
      origin_ *= -1.0;
    }
    catch (tf::TransformException ex)
    {
      ROS_ERROR("%s",ex.what());
    }

    output_cloud_.header.frame_id = "/base_footprint";
    //output_cloud_.is_dense = true;
  }  // On init function

  void ImageToPointCloud::computeTransform(void)
  {
    double x_image_distance_, y_image_distance_, x_image_min_, x_image_max_, y_image_near_max_,
                y_image_near_min_, y_image_far_max_, y_image_far_min_;

    // These values are all in the ROS coordinate frame: x forward, y left, z up
    // Set the base variables
    image_height_ = 480;//768;
    image_width_ = 640;//1024;

    // Calculate the x-direction values
    x_image_min_ = 0.266667;//0.701675;
    x_image_max_ = 2.5400;//4.9149;
    x_image_distance_ = x_image_max_ - x_image_min_;
    x_offset_ = 0.266667;//0.701675;
    x_meter_pixel_ = x_image_distance_ / image_height_;

    // Calculate the y-direction values
    y_image_far_max_ = 1.3462;//3.24485;
    y_image_far_min_ = -1.3462;//-3.1623;
    y_image_distance_ = y_image_far_max_ - y_image_far_min_;
    y_meter_pixel_ = y_image_distance_ / image_width_;

    y_image_near_max_ = 0.2032;//1.03505;
    y_image_near_min_ = -0.2032;//-0.94615;

  }


// Callback for the dynamic reconfigure server
  // void ImageToPointCloud::configCb(line_filter::ImageToPointCloudConfig &config, uint32_t level)
  // {

  // }  // Dynamic reconfigure callback

// Callback for the image pipeline
  void ImageToPointCloud::imageCb(const sensor_msgs::ImageConstPtr& msg)
  {
    // Create a local pointer to the source image
    cv_bridge::CvImageConstPtr source_image_ptr;
    try
    {
      source_image_ptr = cv_bridge::toCvShare(msg, enc::TYPE_32SC1);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }

    // Set the limits for iterating and the cycle throught the image to create a point cloud
    image_height_ = source_image_ptr->image.rows;
    image_width_ = source_image_ptr->image.cols;
    n=0;
    for (int i = 0; i < image_height_; ++i)
    {
      for (int j = 0; j < image_width_; ++j)
      {
        if (source_image_ptr->image.at<int>(i, j))
        {
          createPoint(i, j);
	  n++;
        }
      }
    }

    // Publish the generated point cloud and then clear it out for the next cycle
    
    //pcl_conversions::toPCL(output_cloud_.header); <-- this didnt work
    output_cloud_.header.stamp = ros::Time::now(); //I think the problem is our output_cloud_ is the wrong type. The examples i see show it
                                                   //as a sensor_msgs type so the headers.stamp might be affected by that difference.
    //It works if you comment it out! So if we do not need the time stamp then maybe we can go without it.  I am not sure if the subscriber is //using it
    cloud_pub_.publish(output_cloud_); 
    output_cloud_.points={};//clear();
    n=0;
  }  // Image callback

  // Function to create a point cloud point from a given pixel
  void ImageToPointCloud::createPoint(int i, int j)
  {
    //pcl::PointXYZ pixel_point;

    // Calculate the z distance
    output_cloud_.points[n].z=0;//pixel_point.z = 0;

    // Calculate the x distance
    /*pixel_point.x*/ output_cloud_.points[n].x = x_offset_ + (image_height_ - i) * x_meter_pixel_;

    // Calculate the y distance
    /*pixel_point.y*/ output_cloud_.points[n].y = (image_width_ / 2 - j) * y_meter_pixel_;

    //output_cloud_.push_back(pixel_point);
  }  // Create point function
}  // igvc_image_pipeline namespace

PLUGINLIB_DECLARE_CLASS(igvc_image_pipeline,
                                          ImageToPointCloud,
                                          igvc_image_pipeline::ImageToPointCloud,
                                          nodelet::Nodelet);
