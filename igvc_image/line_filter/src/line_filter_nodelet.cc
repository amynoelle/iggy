/*
  Author: Stuart Baker
  Revision Date: 2013-06-02
  File Name: line_filter_nodelet.cc
  Description: A ROS nodelet to filter an incoming image to mask everything but the white lines.
*/

// General ROS includes
#include <ros/ros.h>
#include <nodelet/nodelet.h>

// OpenCV includes
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

// Dynamic reconfigure includes
#include <dynamic_reconfigure/server.h>
#include <dynamic_reconfigure/config_tools.h>
#include <dynamic_reconfigure/ConfigDescription.h>
#include <dynamic_reconfigure/ParamDescription.h>
#include <dynamic_reconfigure/Group.h>
#include <dynamic_reconfigure/config_init_mutex.h>
#include </home/igvc/catkin_ws/src/igvc_image/line_filter/cfg/cpp/line_filter/LineFilterConfig.h> //need to figure out why a relative path does not work

namespace enc = sensor_msgs::image_encodings;
namespace igvc_image_pipeline
{
  class LineFilter : public nodelet::Nodelet
  {
    public:
      LineFilter() {}
      ~LineFilter()
      {
        cvDestroyAllWindows();
      }

    private:
      ros::NodeHandle nh_;
      ros::NodeHandle private_nh_;

      dynamic_reconfigure::Server<line_filter::LineFilterConfig> reconfigure_server_;
      dynamic_reconfigure::Server<line_filter::LineFilterConfig>::CallbackType f_;

      boost::shared_ptr<image_transport::ImageTransport> it_;
      image_transport::Subscriber image_sub_;
      image_transport::Publisher threshold_image_pub_;
      image_transport::Publisher filter_image_pub_;

      int hue_lower_, hue_upper_, saturation_lower_, saturation_upper_, value_lower_, value_upper_,
        blur_kernel_size_, erosion_type_, erosion_element_, erosion_size_, dilation_type_, dilation_element_,
        dilation_size_, morphology_operation_, morphology_element_, morphology_size_, canny_thresh1_, canny_thresh2_, canny_apeture_size_, hough_rho_, hough_theta_, hough_threshold_, hough_min_line_length_, hough_min_line_gap_;

      bool threshold_image_debug_, threshold_image_debug_last_, erosion_bool_, dilation_bool_,
        morphology_bool_;

      // Image processing matricies
      cv_bridge::CvImage threshold_line_image, filtered_line_image, filtered_image_;
      cv::Mat element_;

      virtual void onInit();
      void imageCb(const sensor_msgs::ImageConstPtr&);
      void configCb(line_filter::LineFilterConfig&, uint32_t);

  }; // class LineFilter
} // namespace igvc_image_pipeline

/*

Split for header

*/

// #include <line_filter_nodelet.h>
#include <pluginlib/class_list_macros.h>

namespace igvc_image_pipeline
{
  void LineFilter::onInit()
  {
    nh_ = getNodeHandle();
    private_nh_ = getNodeHandle();

    hue_lower_ = 0;
    hue_upper_ = 180;
    saturation_lower_ = 0;
    saturation_upper_ = 75;
    value_lower_ = 75;
    value_upper_ = 255;

    blur_kernel_size_ = 5;
    threshold_image_debug_ = false;
    threshold_image_debug_last_ = false;
    erosion_bool_ = false;
    erosion_element_ = 2;
    erosion_size_ = 1;
    dilation_bool_ = false;
    dilation_element_ = 0;
    dilation_size_ = 3;
    morphology_bool_ = false;
    morphology_operation_ = 0;
    morphology_element_ = 1;
    morphology_size_ = 8;
    
    canny_thresh1_ = 50;
    canny_thresh2_ = 250;
    canny_apeture_size_ = 3;
    hough_rho_ = 1;
    hough_theta_ = CV_PI/180;
    hough_threshold_ = 50;
    hough_min_line_length_ = 20; 
    hough_min_line_gap_ = 10;
    
    it_.reset(new image_transport::ImageTransport(nh_));

    filter_image_pub_ = it_->advertise("out", 1);
    image_sub_ = it_->subscribe("in", 1, &LineFilter::imageCb, this);

    f_ = boost::bind(&LineFilter::configCb, this, _1, _2);
    reconfigure_server_.setCallback(f_);

    cvStartWindowThread();
  }


// Callback for the dynamic reconfigure server. Adjusts the ersoion/dilation values.
  void LineFilter::configCb(line_filter::LineFilterConfig &config, uint32_t level)
  {
    hue_lower_ = config.hue_lower;
    hue_upper_ = config.hue_upper;
    saturation_lower_ = config.saturation_lower;
    saturation_upper_ = config.saturation_upper;
    value_lower_ = config.value_lower;
    value_upper_ = config.value_upper;

    threshold_image_debug_ = config.threshold_image_debug;

    blur_kernel_size_ = config.blur_kernel_size;

    erosion_bool_ = config.erosion_bool;
    erosion_element_ = config.erosion_element;
    erosion_size_ = config.erosion_size;

    dilation_bool_ = config.dilation_bool;
    dilation_element_ = config.dilation_element;
    dilation_size_ = config.dilation_size;

    morphology_bool_ = config.morphology_bool;
    morphology_operation_ = config.morphology_operation + 2;
    morphology_element_ = config.morphology_element;
    morphology_size_ = config.dilation_size;
    
    //canny_thresh1_ = config.canny_thresh1;
    //canny_thresh2_ = config.canny_thresh2;
    //canny_apeture_size_ = config.canny_apeture_size;
    
    //hough_rho_ = config.hough_rho;
    //hough_theta_ = config.hough_theta;
    //hough_threshold_ = config.hough_threshold;
    //hough_min_line_length_ = config.hough_min_line_gap; 
    //hough_min_line_gap_ = config.hough_min_line_length;

    if (threshold_image_debug_ != threshold_image_debug_last_)
    {
      if (threshold_image_debug_)
      {
        cv::namedWindow("Raw Threshold Image");
        cv::waitKey(3);
      }
      else if (!threshold_image_debug_)
      {
        // cvDestroyWindow("Raw Threshold Image");
        cv::waitKey(3);
      }
      threshold_image_debug_last_ = threshold_image_debug_;
    }
  }  // Dynamic reconfigure callback

// Callback for the image pipeline
  void LineFilter::imageCb(const sensor_msgs::ImageConstPtr& msg)
  {
    cv_bridge::CvImagePtr line_image_ptr(new cv_bridge::CvImage());
    cv_bridge::CvImagePtr converted_line_image_ptr(new cv_bridge::CvImage());
    cv_bridge::CvImagePtr threshold_line_image_ptr(new cv_bridge::CvImage());
    cv_bridge::CvImagePtr blur_line_image_ptr(new cv_bridge::CvImage());
    cv_bridge::CvImagePtr erosion_input_ptr_(new cv_bridge::CvImage());
    cv_bridge::CvImagePtr erosion_output_ptr_(new cv_bridge::CvImage());
    cv_bridge::CvImagePtr dilation_input_ptr_(new cv_bridge::CvImage());
    cv_bridge::CvImagePtr dilation_output_ptr_(new cv_bridge::CvImage());
    cv_bridge::CvImagePtr morph_input_ptr_(new cv_bridge::CvImage());
    cv_bridge::CvImagePtr morph_output_ptr_(new cv_bridge::CvImage());
    cv_bridge::CvImagePtr image_out_ptr_(new cv_bridge::CvImage());

    try
    {
      line_image_ptr = cv_bridge::toCvCopy(msg, enc::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }

    // Convert the BGR image to HSV
    cvtColor(line_image_ptr->image, converted_line_image_ptr->image, CV_BGR2HSV);

    // Threshold the image and publish it for tweaking
    cv::inRange(converted_line_image_ptr->image,
      cv::Scalar(hue_lower_, saturation_lower_, value_lower_),
      cv::Scalar(hue_upper_, saturation_upper_, value_upper_),
      threshold_line_image_ptr->image);

    if (threshold_image_debug_)
    {
      cv::imshow("Raw Threshold Image", threshold_line_image_ptr->image);
      cv::waitKey(3);
    }

     /// Applying Median blur
    for (int i = 1; i < blur_kernel_size_; i = i + 2)
    {
      medianBlur(threshold_line_image_ptr->image, blur_line_image_ptr->image, i);
    }

    // Erosion morphological filter
    if (erosion_bool_)
    {
      erosion_input_ptr_ = blur_line_image_ptr;

      if (erosion_element_ == 0)
      {
        erosion_type_ = cv::MORPH_RECT;
      }
      else if (erosion_element_ == 1)
      {
        erosion_type_ = cv::MORPH_CROSS;
      }
      else
      {
        erosion_type_ = cv::MORPH_ELLIPSE;
      }

      element_ = getStructuringElement(erosion_type_,
        cv::Size(2 * erosion_size_ + 1, 2 * erosion_size_ + 1 ),
        cv::Point( erosion_size_,
        erosion_size_ ));
      erode(erosion_input_ptr_->image, erosion_output_ptr_->image, element_);
    }

    // Dilation morphological filter
    if (dilation_bool_)
    {
      if (erosion_bool_)
      {
        dilation_input_ptr_ = erosion_output_ptr_;
      }
      else
      {
        dilation_input_ptr_ = blur_line_image_ptr;
      }

      if (dilation_element_ == 0)
      {
        dilation_type_ = cv::MORPH_RECT;
      }
      else if (dilation_element_ == 1)
      {
        dilation_type_ = cv::MORPH_CROSS;
      }
      else
      {
        dilation_type_ = cv::MORPH_ELLIPSE;
      }

      element_ = getStructuringElement(dilation_type_,
        cv::Size(2 * dilation_size_ + 1, 2 * dilation_size_ + 1),
        cv::Point(dilation_size_,
        dilation_size_ ));
      dilate(dilation_input_ptr_->image, dilation_output_ptr_->image, element_);
    }

    if (morphology_bool_)
    {
      if (erosion_bool_ && !dilation_bool_)
      {
        morph_input_ptr_ = erosion_output_ptr_;
      }
      else if (dilation_bool_)
      {
        morph_input_ptr_ = dilation_output_ptr_;
      }
      else
      {
        morph_input_ptr_ = blur_line_image_ptr;
      }

      // Other morphological filter
      // 2 - Opening, 3 - Closing, 4 - Gradient, 5 - Top Hat, 6 - Black Hat
      element_ = getStructuringElement(morphology_element_,
          cv::Size(2 * morphology_size_ + 1, 2 * morphology_size_ + 1),
          cv::Point(morphology_size_, morphology_size_));

      // Apply the specified morphology operation
      morphologyEx(morph_input_ptr_->image, morph_output_ptr_->image, morphology_operation_, element_);
    }

    if (erosion_bool_ && !dilation_bool_ && !morphology_bool_)
    {
      image_out_ptr_ = erosion_output_ptr_;
    }
    else if (dilation_bool_ && !morphology_bool_)
    {
      image_out_ptr_ = dilation_output_ptr_;
    }
    else if (morphology_bool_)
    {
      image_out_ptr_ = morph_output_ptr_;
    }
    else
    {
      image_out_ptr_ = blur_line_image_ptr;
    }
    
    cv::Canny(image_out_ptr_->image, image_out_ptr_->image, canny_thresh1_, canny_thresh2_, canny_apeture_size_);
    
    cv::vector<cv::Vec4i> lines;
    cv::HoughLinesP(image_out_ptr_->image, lines, hough_rho_, hough_theta_, hough_threshold_, hough_min_line_length_, hough_min_line_gap_);
    
    image_out_ptr_->image = cv::Mat::zeros(image_out_ptr_->image.size(), image_out_ptr_->image.type());
    
    for( size_t i =0; i< lines.size(); i++)
    {
      line(image_out_ptr_->image, cv::Point(lines[i][0],lines[i][1]),cv::Point(lines[i][2],lines[i][3]), cv::Scalar(0,0,255),3,8);
    }
    
    image_out_ptr_->header = line_image_ptr->header;
    image_out_ptr_->encoding = line_image_ptr->encoding; //"mono8";
    filter_image_pub_.publish(image_out_ptr_->toImageMsg());
  }  // Image callback
}  // namespace igvc_image_pipeline

PLUGINLIB_DECLARE_CLASS(igvc_image_pipeline, LineFilter, igvc_image_pipeline::LineFilter, nodelet::Nodelet);
