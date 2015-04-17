#include <string>
#include <math.h>
#include <stdio.h>

#include <ros/ros.h>
#include <tf/transform_listener.h>
#include <image_transport/image_transport.h>

#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

namespace enc = sensor_msgs::image_encodings;

static const char WINDOW[] = "Image window";

class ImageConverter
{
  ros::NodeHandle nh_;
  image_transport::ImageTransport it_;
  image_transport::Subscriber image_sub_;
  image_transport::Publisher image_pub_;

  int h1, s1, v1, h2, s2, v2;

public:
  /*          // Added for erosion/dialation test
            cv::Mat erosion_dst, dilation_dst;
            int erosion_elem;
            int erosion_size;
            int dilation_elem;
            int dilation_size;
            int const max_elem;
            int const max_kernel_size;
*/
  ImageConverter():
    it_(nh_),
    h1(0),
    s1(0),
    v1(175),
    h2(180),
    s2(75),
    v2(255)
/*,
            // Added for erosion/dialation test
            erosion_elem(0),
            erosion_size(0),
            dilation_elem(0),
            dilation_size(0),
            max_elem(2),
            max_kernel_size(21)
*/
  {
    image_pub_ = it_.advertise("filter_out", 1);
    //image_sub_ = it_.subscribe("image_raw", 1, &ImageConverter::imageCb, this);
    image_sub_ = it_.subscribe("/camera/right/image_raw", 1, &ImageConverter::imageCb, this);

    cv::namedWindow("Input Image", CV_WINDOW_AUTOSIZE);
    cv::namedWindow("Output Image", CV_WINDOW_AUTOSIZE);
        

  /*          // Added for erosion/dialation test
            /// Create Erosion Trackbar
            cv::createTrackbar( "Erosion Element:\n 0: Rect \n 1: Cross \n 2: Ellipse", "Output Image", &erosion_elem, max_elem);
            cv::createTrackbar( "Erosion Kernel size:\n 2n +1", "Output Image", &erosion_size, max_kernel_size);
            /// Create Dilation Trackbar
            cv::createTrackbar( "Dialation Element:\n 0: Rect \n 1: Cross \n 2: Ellipse", "Output Image", &dilation_elem, max_elem);
            cv::createTrackbar( "Dialation Kernel size:\n 2n +1", "Output Imagee", &dilation_size, max_kernel_size);
            cv::waitKey(3);
*/
  }

  ~ImageConverter()
  {
    cv::destroyWindow("Input Image");
    cv::destroyWindow("Output Image");
    cvDestroyAllWindows();
  }


/*

Split for header

*/
  

  void imageCb(const sensor_msgs::ImageConstPtr& msg)
  {
    cv_bridge::CvImageConstPtr original_ptr;
    cv_bridge::CvImageConstPtr line_image_ptr;
    
    // convert the ROS msg into an openCV image
    try
    {
      original_ptr = cv_bridge::toCvShare(msg, enc::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }

    // Create another copy of the msg as an openCV image
     try
     {
       line_image_ptr = cv_bridge::toCvCopy(msg, enc::BGR8);
     }
     catch (cv_bridge::Exception& e)
     {
       ROS_ERROR("cv_bridge exception: %s", e.what());
       return;
     }

    cv_bridge::CvImage thresh_line_image;
    cv_bridge::CvImage filtered_line_image;
    cv::Mat dist, out_image, debug_out;
    
    // new code
    // reduce resolution of image
     //cv::GaussianBlur(line_image_ptr->image, line_image_ptr->image, cv::Size(7,7), 0.0, 0.0, cv::BORDER_DEFAULT);

    // Convert the BGR image to HSV
    cvtColor(line_image_ptr->image, line_image_ptr->image, CV_BGR2HSV);

    // Threshold the image
    cv::inRange(line_image_ptr->image, cv::Scalar(h1, s1, v1), cv::Scalar(h2, s2, v2), thresh_line_image.image);
    
    // Canny edge detection
    cv::Canny(thresh_line_image.image, dist, 50, 250, 3);
    
    // Find the Hough lines
    cv::vector<cv::Vec4i> lines;
    cv::HoughLinesP(dist, lines, 1, CV_PI/180, 50, 20 , 7);    
    cv::cvtColor(dist, debug_out, CV_GRAY2BGR);    
    out_image = cv::Mat::zeros(dist.size(), debug_out.type());
    
    // Draw the Hough lines on the image
    for( size_t i =0; i< lines.size(); i++)
    {
      line(debug_out, cv::Point(lines[i][0],lines[i][1]),cv::Point(lines[i][2],lines[i][3]), cv::Scalar(0,0,255),3,8);
      line(out_image, cv::Point(lines[i][0],lines[i][1]),cv::Point(lines[i][2],lines[i][3]), cv::Scalar(255,255,255),3,8);
    }
    
     /// Applying Median blur
    int MAX_KERNEL_LENGTH = 10;
    for ( int i = 1; i < MAX_KERNEL_LENGTH; i = i + 2 )
    {
      medianBlur(thresh_line_image.image, filtered_line_image.image, i );
    }
/*
                  // Added for erosion/dialation test
                    int erosion_type;
                    if( erosion_elem == 0 ){ erosion_type = cv::MORPH_RECT; }
                    else if( erosion_elem == 1 ){ erosion_type = cv::MORPH_CROSS; }
                    else{ erosion_type = cv::MORPH_ELLIPSE; }
                    cv::Mat element = getStructuringElement( erosion_type, cv::Size( 2*erosion_size + 1, 2*erosion_size+1 ), cv::Point( erosion_size, erosion_size ) );
                    erode( filtered_line_image.image, erosion_dst, element );
                    int dilation_type;
                    if( dilation_elem == 0 ){ dilation_type = cv::MORPH_RECT; }
                    else if( dilation_elem == 1 ){ dilation_type = cv::MORPH_CROSS; }
                    else{ dilation_type = cv::MORPH_ELLIPSE; }
                    element = getStructuringElement( dilation_type, cv::Size( 2*dilation_size + 1, 2*dilation_size+1 ), cv::Point( dilation_size, dilation_size ) );
                    dilate( filtered_line_image.image, dilation_dst, element );
*/


    // Show the images in a window
    cv::imshow("Input Image", original_ptr->image);
//     cv::imshow("Converted Imagfe", line_image_ptr->image);
    //cv::imshow("Filtered_Line Image", debug_out);
    cv::imshow("Output Image", filtered_line_image.image);
    cv::waitKey(3);
    cv_bridge::CvImage out;
    out.header = original_ptr->header;
    out.encoding = original_ptr->encoding;
    out.image = out_image;
    
    // publish the output image    
    image_pub_.publish(out.toImageMsg());
  }
};

int main(int argc, char** argv)
{
  ros::init(argc, argv, "line_filter");
  ImageConverter ic;
  ros::spin();
  return 0;
}
