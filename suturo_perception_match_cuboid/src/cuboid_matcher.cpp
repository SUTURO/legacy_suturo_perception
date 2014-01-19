#include <suturo_perception_match_cuboid/cuboid_matcher.h>

#define MIN_ANGLE 5 // the minimum angle offset between to norm vectors
                    // if this threshold is not reached, no rotation will be made on this axis

CuboidMatcher::CuboidMatcher()
{
    input_cloud_ = pcl::PointCloud<pcl::PointXYZRGB>::Ptr(new pcl::PointCloud<pcl::PointXYZRGB>);
    debug = true;
}
std::vector<DetectedPlane> *CuboidMatcher::getDetectedPlanes()
{
  return &detected_planes_;
}

void CuboidMatcher::setDebug(bool b)
{
  debug = b;
}

void CuboidMatcher::setInputCloud(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud)
{
  pcl::copyPointCloud<pcl::PointXYZRGB, pcl::PointXYZRGB>(*cloud, *input_cloud_);
}

void CuboidMatcher::setSaveIntermediateResults(bool b)
{
  save_intermediate_results_ = b;
}

Eigen::Vector3f CuboidMatcher::reduceNormAngle(Eigen::Vector3f v1, Eigen::Vector3f v2)
{
  float dotproduct = v1.dot(v2);
  if(acos(dotproduct)> M_PI/2)
  {
    std::cout << "NORM IS ABOVE 90 DEG! TURN IN THE OTHER DIRECTION" << std::endl;
    v2 = -v2;
  }
  return v2;
}

int CuboidMatcher::transformationCount()
{
  return transformations_.size();
}

void CuboidMatcher::segmentPlanes()
{
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
  pcl::copyPointCloud<pcl::PointXYZRGB, pcl::PointXYZRGB>(*input_cloud_, *cloud);
  // Running index for the plane vectors
  int planeIdx = 0;

  // For each Extraction
  for (int i = 0; i < 2; i++) // Hack, extract two planes
  {
    DetectedPlane dp;
    detected_planes_.push_back(dp); // TODO boost pointer

    // Create the segmentation object
    pcl::SACSegmentation<pcl::PointXYZRGB> seg;
    // Optional
    seg.setOptimizeCoefficients (true);
    // Mandatory
    seg.setModelType (pcl::SACMODEL_PERPENDICULAR_PLANE ); // TODO edit
    seg.setMethodType (pcl::SAC_RANSAC);
    seg.setMaxIterations (1000);
    seg.setDistanceThreshold (0.005); // Tolerance is 0.5 cm

    seg.setInputCloud (cloud);
    seg.segment (*detected_planes_.at(planeIdx).getInliers(),
        *detected_planes_.at(planeIdx).getCoefficients());

    // Create the filtering object
    pcl::ExtractIndices<pcl::PointXYZRGB> extract;
    // Extract the inliers
    extract.setInputCloud (cloud);
    extract.setIndices (detected_planes_.at(planeIdx).getInliers());
    extract.setNegative (false);
    extract.filter (*detected_planes_.at(planeIdx).getPoints());

    // Create the filtering object
    extract.setNegative (true);
    extract.filter (*cloud);

    // Calculate the centroid of each object
    Eigen::Vector4f centroid;
    computeCentroid(detected_planes_.at(planeIdx).getPoints(), centroid);
    detected_planes_.at(planeIdx).setCentroid(centroid);

    planeIdx ++ ;
  }

  for (int i = 0; i < detected_planes_.size(); i++)
  {
    for (int j = 0; j < detected_planes_.size(); j++)
    {
      if(i==j) continue;

      if(debug)
        std::cout << "Angle between Normal " << i << " and Normal " << j;

      // TODO check angle between planes. The should be near 0°, 90°, 180° or 270°
      // for cuboids

      float angle = detected_planes_.at(i).angleBetween(
          detected_planes_.at(j).getCoefficientsAsVector3f());

      if(debug)
      {
        std::cout << ": " << angle << " RAD, " << ((angle * 180) / M_PI) << " DEG";
        std::cout << std::endl;
      }

    }
  }

  for (int i = 0; i < detected_planes_.size(); i++)
  {
    // fill the transformed_normals vector for the first time
    transformed_normals_.push_back(detected_planes_.at(i).getCoefficientsAsVector3f());
  }

}
void CuboidMatcher::computeCentroid(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_in, Eigen::Vector4f &centroid)
{

  pcl::PointCloud<pcl::PointXYZRGB>::Ptr hull_points (new pcl::PointCloud<pcl::PointXYZRGB> ());

  pcl::ConvexHull<pcl::PointXYZRGB> hull;
  hull.setInputCloud(cloud_in);
  hull.reconstruct (*hull_points);

  // Centroid calulcation
  pcl::compute3DCentroid (*hull_points, centroid);  
}

// Rotate the Vector 'normal_to_rotate' into 'base_normal'
// Returns a rotation matrix which can be used for the transformation
// The matrix also includes an empty translation vector
// By passing store_transformation=true, the transformation
// will be saved in the class attribute 'transformations'
Eigen::Matrix< float, 4, 4 > CuboidMatcher::rotateAroundCrossProductOfNormals(
    Eigen::Vector3f base_normal,
    Eigen::Vector3f normal_to_rotate, bool store_transformation=true)
{
    float costheta = normal_to_rotate.dot(base_normal) / (normal_to_rotate.norm() * base_normal.norm() );

    Eigen::Vector3f axis;
    Eigen::Vector3f firstAxis = normal_to_rotate.cross(base_normal);
    firstAxis.normalize();
    axis=firstAxis;
    float c = costheta;
    std::cout << "rotate COSTHETA: " << acos(c) << " RAD, " << ((acos(c) * 180) / M_PI) << " DEG" << std::endl;
    float s = sqrt(1-c*c);
    float CO = 1-c;

    float x = axis(0);
    float y = axis(1);
    float z = axis(2);
    
    Eigen::Matrix< float, 4, 4 > rotationBox;
    rotationBox(0,0) = x*x*CO+c;
    rotationBox(1,0) = y*x*CO+z*s;
    rotationBox(2,0) = z*x*CO-y*s;

    rotationBox(0,1) = x*y*CO-z*s;
    rotationBox(1,1) = y*y*CO+c;
    rotationBox(2,1) = z*y*CO+x*s;

    rotationBox(0,2) = x*z*CO+y*s;
    rotationBox(1,2) = y*z*CO-x*s;
    rotationBox(2,2) = z*z*CO+c;
   // Translation vector
    rotationBox(0,3) = 0;
    rotationBox(1,3) = 0;
    rotationBox(2,3) = 0;

    // The rest of the 4x4 matrix
    rotationBox(3,0) = 0;
    rotationBox(3,1) = 0;
    rotationBox(3,2) = 0;
    rotationBox(3,3) = 1;

    if(store_transformation)
      transformations_.push_back(rotationBox);

    return rotationBox;
}

Eigen::Matrix< float, 3, 3 > CuboidMatcher::removeTranslationVectorFromMatrix(Eigen::Matrix<float,4,4> m)
{
  Eigen::Matrix< float, 3, 3 > result;
  result.setZero();
  result(0,0) = m(0,0);
  result(1,0) = m(1,0);
  result(2,0) = m(2,0);

  result(0,1) = m(0,1);
  result(1,1) = m(1,1);
  result(2,1) = m(2,1);

  result(0,2) = m(0,2);
  result(1,2) = m(1,2);
  result(2,2) = m(2,2);
  return result;
}

void CuboidMatcher::updateTransformedNormals(Eigen::Matrix<float,4,4> transformation)
{
  std::vector<Eigen::Vector3f> tmp;

  // Rotate the normal of the second plane with the first rotation matrix
  for (int i = 0; i < transformed_normals_.size(); i++) {
    // Rotate every normal in the vector and push it
    // to the temporary result list
    tmp.push_back(
        removeTranslationVectorFromMatrix(transformation) * transformed_normals_.at(i));
  }

  transformed_normals_ = tmp;
}

Eigen::Vector3f CuboidMatcher::getVector3fFromVector4f(Eigen::Vector4f vec)
{
  Eigen::Vector3f ret(
      vec(0),
      vec(1),
      vec(2)
      );
  return ret;
}

void CuboidMatcher::computeCuboidCornersWithMinMax3D(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_in, pcl::PointCloud<pcl::PointXYZRGB>::Ptr corner_points)
{
  pcl::PointXYZRGB min_pt, max_pt;
  pcl::getMinMax3D(*cloud_in, min_pt, max_pt);
  // Compute the bounding box edge points
  pcl::PointXYZRGB pt1;
  pt1.x = min_pt.x; pt1.y = min_pt.y; pt1.z = min_pt.z;

  pcl::PointXYZRGB pt2;
  pt2.x = max_pt.x; pt2.y = max_pt.y; pt2.z = max_pt.z;
  pcl::PointXYZRGB pt3;
  pt3.x = max_pt.x,pt3.y = min_pt.y,pt3.z = min_pt.z;
  pcl::PointXYZRGB pt4;
  pt4.x = min_pt.x,pt4.y = max_pt.y,pt4.z = min_pt.z;
  pcl::PointXYZRGB pt5;
  pt5.x = min_pt.x,pt5.y = min_pt.y,pt5.z = max_pt.z;

  pcl::PointXYZRGB pt6;
  pt6.x = min_pt.x,pt6.y = max_pt.y,pt6.z = max_pt.z;
  pcl::PointXYZRGB pt7;
  pt7.x = max_pt.x,pt7.y = max_pt.y,pt7.z = min_pt.z;
  pcl::PointXYZRGB pt8;
  pt8.x = max_pt.x,pt8.y = min_pt.y,pt8.z = max_pt.z;

  corner_points->push_back(pt1);
  corner_points->push_back(pt2);
  corner_points->push_back(pt3);
  corner_points->push_back(pt4);
  corner_points->push_back(pt5);
  corner_points->push_back(pt6);
  corner_points->push_back(pt7);
  corner_points->push_back(pt8);
}

Cuboid CuboidMatcher::computeCuboidFromBorderPoints(pcl::PointCloud<pcl::PointXYZRGB>::Ptr corner_points)
{
  Cuboid c;

  // Get the "width": (minx,miny) -> (maxx,miny)
  c.length1 = pcl::distances::l2(corner_points->points.at(0).getVector4fMap(),corner_points->points.at(2).getVector4fMap());
  // Get the "height": (minx,miny) -> (minx,maxy)
  c.length2 = pcl::distances::l2(corner_points->points.at(0).getVector4fMap(),corner_points->points.at(3).getVector4fMap());
  // Get the "depth": (minx,miny,minz) -> (minx,maxy,maxz)
  c.length3 = pcl::distances::l2(corner_points->points.at(0).getVector4fMap(),corner_points->points.at(4).getVector4fMap());

  c.volume = c.length1 * c.length2 * c.length3;
  Eigen::Vector4f centroid;
  CuboidMatcher::computeCentroid(corner_points, centroid);
  c.center = getVector3fFromVector4f(centroid);
  return c;
}


bool CuboidMatcher::execute(Cuboid &c)
{
  segmentPlanes();
  if(detected_planes_.size() < 2)
  {
    std::cout << "Couldn't detect atleast two planes with RANSAC" << std::endl;
    return false;
  }

  pcl::PointCloud<pcl::PointXYZRGB>::Ptr rotated_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
  pcl::copyPointCloud<pcl::PointXYZRGB, pcl::PointXYZRGB>(*input_cloud_, *rotated_cloud);


  // rotate until two axis are aligned
  //   TODO: use the face with the smallest angle to the x-y plane

  // M
  Eigen::Vector3f plane_normal = detected_planes_.at(0).getCoefficientsAsVector3f();

  // Compute the necessary rotation to align a face of the object with the camera's
  // imaginary image plane
  // N
  Eigen::Vector3f camera_normal;
  camera_normal(0)=0;
  camera_normal(1)=0;
  camera_normal(2)=1;

  camera_normal = CuboidMatcher::reduceNormAngle(plane_normal, camera_normal);
  
  // TODO check if angle is below treshold. skip transformation then ...

  Eigen::Matrix< float, 4, 4 > rotationBox = 
    rotateAroundCrossProductOfNormals(camera_normal, plane_normal);
  
  std::cout << "Transformed first time" << std::endl;
  pcl::transformPointCloud (*rotated_cloud, *rotated_cloud, rotationBox);   

  // Update all normal vectors
  updateTransformedNormals(rotationBox);


  // Now align the second normal (which has been rotated) with the x-z plane
  Eigen::Vector3f xz_plane;
  xz_plane(0)=0;
  xz_plane(1)=1;
  xz_plane(2)=0;

  // TODO this behaviour differs from the original one. Does it give other results?
  xz_plane = CuboidMatcher::reduceNormAngle(transformed_normals_.at(1), xz_plane);

  // TODO check if angle is below treshold. skip transformation then ...
  Eigen::Matrix< float, 4, 4 > secondRotation = 
    rotateAroundCrossProductOfNormals(xz_plane, transformed_normals_.at(1));

  std::cout << "Transformed second time" << std::endl;
  pcl::transformPointCloud (*rotated_cloud, *rotated_cloud, secondRotation);   

  // calculate bb
  // Compute the bounding box for the rotated object
  pcl::PointXYZRGB min_pt, max_pt;
  pcl::getMinMax3D(*rotated_cloud, min_pt, max_pt);

  // Compute the bounding box edge points
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr bounding_box(new pcl::PointCloud<pcl::PointXYZRGB>);
  computeCuboidCornersWithMinMax3D(rotated_cloud, bounding_box);

  // transform bb corners back to the object by inversing the last transformations
  // which were necessary to align the object with the xy and xz plane
  // TODO: calculate orientation
  for(int i = transformations_.size()-1; i >= 0; i--)
    pcl::transformPointCloud (*bounding_box, *bounding_box, transformations_.at(i).transpose());   

  // calculate stats of the bounding box
  // We do this after the backtransformaton, to get the proper centroid of it
  c = computeCuboidFromBorderPoints(bounding_box);
  if(debug)
  {
    std::cout << "Cuboid statistics" << std::endl;
    std::cout << "Width: " << c.length1 << " Height: " << c.length2 << " Depth: " << c.length3 << " Volume: " << c.volume << " m^3" << std::endl;
  }
  return true;
}