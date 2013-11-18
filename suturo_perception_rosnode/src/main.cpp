#include "ros/ros.h"
#include <boost/signals2/mutex.hpp>

#include "suturo_perception.h"
#include "PerceivedObject.h"
#include "Point.h"
#include "suturo_perception_msgs/GetClusters.h"


class SuturoPerceptionROSNode
{
public:
  /*
   * Constructor
   */
  SuturoPerceptionROSNode(ros::NodeHandle& n) : nh(n)
  {
    clusterService = nh.advertiseService("GetClusters", 
      &SuturoPerceptionROSNode::getClusters, this);
    objectID = 0;
    //processing = false;
  }

  /*
   * Receive callback for the /camera/depth_registered/points subscription
   */
   void receive_cloud(const sensor_msgs::PointCloud2ConstPtr& inputCloud)
   {
    // process only one cloud
    ROS_INFO("Receiving cloud");
    if(processing)
    {
      ROS_INFO("processing...");
      pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_in (new pcl::PointCloud<pcl::PointXYZRGB>());
      pcl::fromROSMsg(*inputCloud,*cloud_in);

      sp.process_cloud(cloud_in);
      processing = false;

      ROS_INFO("Received a new point cloud: size = %du",cloud_in->points.size());
    }
  }

  /*
   * Implementation of the GetClusters Service.
   *
   * This method will subscribe to the /camera/depth_registered/points topic, 
   * wait for the processing of a single point cloud, and return the result from
   * the calulations as a list of PerceivedObjects.
   */
  bool getClusters(suturo_perception_msgs::GetClusters::Request &req,
    suturo_perception_msgs::GetClusters::Response &res)
  {
    ros::Subscriber sub;
    processing = true;

    // signal failed call, if request string does not match
    if (req.s.compare("get") != 0)
    {
      return false;
    }

    // Subscribe to the depth information topic
    sub = nh.subscribe("/camera/depth_registered/points", 1, 
      &SuturoPerceptionROSNode::receive_cloud, this);

    ROS_INFO("Waiting for processed cloud");
    ros::Rate r(20); // 20 hz
    while(processing)
    {
      ros::spinOnce();
      r.sleep();
    }
    ROS_INFO("Cloud processed. Lock buffer and return the results");

    mutex.lock();
    perceivedObjects = sp.getPerceivedObjects();
    res.perceivedObjs = *convertPerceivedObjects(&perceivedObjects);
    mutex.unlock();

    return true;
  }

private:
  bool processing; // processing flag
  SuturoPerception sp;
  std::vector<PerceivedObject> perceivedObjects;
  ros::NodeHandle nh;
  boost::signals2::mutex mutex;
  // ID counter for the perceived objects
  int objectID;
  ros::ServiceServer clusterService;  
  
  std::vector<suturo_perception_msgs::PerceivedObject> *convertPerceivedObjects(std::vector<PerceivedObject> *objects) {
    std::vector<suturo_perception_msgs::PerceivedObject> *result = new std::vector<suturo_perception_msgs::PerceivedObject>();
    for (std::vector<PerceivedObject>::iterator it = objects->begin(); it != objects->end(); ++it) {
      suturo_perception_msgs::PerceivedObject *msgObj = new suturo_perception_msgs::PerceivedObject();
      msgObj->c_id = it->c_id;
      msgObj->c_volume = it->c_volume;
      msgObj->c_centroid.x = it->c_centroid.x;
      msgObj->c_centroid.y = it->c_centroid.y;
      msgObj->c_centroid.z = it->c_centroid.z;
    }
    return result;
  }
};


int main (int argc, char** argv)
{
  ros::init(argc, argv, "suturo_perception");
  ros::NodeHandle nh;
  SuturoPerceptionROSNode spr(nh);
  ROS_INFO("suturo_perception READY");
    //sp.sayHi();
  ros::MultiThreadedSpinner spinner(2);
  spinner.spin();
  return (0);
}
