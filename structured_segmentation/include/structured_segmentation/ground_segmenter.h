/*
Copyright 2011, 2012 Massachusetts Institute of Technology

This work is sponsored by the Department of the Air Force under Air Force 
Contract #FA8721-05-C-0002. Opinions, interpretations, conclusions and 
recommendations are those of the authors and are not necessarily endorsed by the 
United States Government.
*/

#ifndef STRUCTURED_SEGMENTATION__GROUND_SEGMENTER_H
#define STRUCTURED_SEGMENTATION__GROUND_SEGMENTER_H
#include <ros/ros.h>
#include <nodelet/nodelet.h>

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <cv_bridge/cv_bridge.h>

#include "pcl_ros/point_cloud.h"
#include "pcl/conversions.h"
#include <sensor_msgs/PointCloud2.h>

#include "structured_segmentation/typedefs.h"

#include <dynamic_reconfigure/server.h>
#include "structured_segmentation/GroundSegmenterConfig.h"

namespace structured_segmentation
{

  static const double ANG_RES = 0.1728;     // Degrees
  static const unsigned int WIDTH = floor(360 / ANG_RES);      // Number of bins
  static const unsigned int START_RING = 6;
  static const unsigned int START_INDEX = 1041;
  static const double Z_FLOOR = -0.5;

  class GroundSegmenter: public nodelet::Nodelet
  {
    public:
      GroundSegmenter():
        display_(true),
        mingraddist_(0.3),
        maxgrad_(0.3),
        maxdh_(0.05),
        input_cloud_ptr_(new PointCloud),
        ground_cloud_ptr_(new PointCloud),
        not_ground_cloud_ptr_(new PointCloud)
    {
      mingraddist_sqr_ = mingraddist_ * mingraddist_;
    }

      ~GroundSegmenter() {};

      void initialize(void);
      void getPoint(int ring, int index, PointT& point);
      void onConfigure(structured_segmentation::GroundSegmenterConfig& config, uint32_t level);

    private:
      ros::NodeHandle nh_;
      ros::NodeHandle private_nh_;
      ros::Subscriber input_;
      ros::Publisher output_ground_pub_;
      ros::Publisher output_not_ground_pub_;

      dynamic_reconfigure::Server<structured_segmentation::GroundSegmenterConfig>* server_;

      // Parameters.
      bool display_;
      double mingraddist_;
      double mingraddist_sqr_;
      double maxgrad_;
      double maxdh_;

      boost::shared_ptr<PointCloud> input_cloud_ptr_;
      boost::shared_ptr<PointCloud> ground_cloud_ptr_;
      boost::shared_ptr<PointCloud> not_ground_cloud_ptr_;

      virtual void onInit(void);
      void calc2DCloud(const PointCloudPtr& input);
      void segment(const PointCloudPtr& input);
      void calculateGradientField(boost::shared_ptr<cv::Mat> &gradient_matrix);
      void calculateGround(const boost::shared_ptr<cv::Mat> &gradient_matrix,
                          boost::shared_ptr<cv::Mat> &ground_matrix);
      float calcGrad(size_t ring, size_t index);
  };
}

#endif