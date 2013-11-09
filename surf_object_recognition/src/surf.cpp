// TODO: Use KD-Tree
// https://code.google.com/p/find-object/source/browse/trunk/find_object/example/main.cpp
#include <stdio.h>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/core/version.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp> 
#include <boost/timer.hpp>      // For boost::timer class

// OpenCV 2.4 moved the SURF and SIFT libraries to a new nonfree/ directory
#if CV_MINOR_VERSION > 3
	#include <opencv2/nonfree/features2d.hpp>
#else
	#include <opencv2/features2d/features2d.hpp>
#endif

using namespace cv;

void readme();

/** @function main */
int main( int argc, char** argv )
{
  if( argc != 3 )
  { readme(); return -1; }

  Mat img_object = imread( argv[1], CV_LOAD_IMAGE_GRAYSCALE );
  Mat img_scene = imread( argv[2], CV_LOAD_IMAGE_GRAYSCALE );

  if( !img_object.data || !img_scene.data )
  { std::cout<< " --(!) Error reading images " << std::endl; return -1; }

	// Start the execution timer
	boost::timer t;

  //-- Step 1: Detect the keypoints using SURF Detector
  int minHessian = 400;

  SurfFeatureDetector detector( minHessian );

  std::vector<KeyPoint> keypoints_object, keypoints_scene;

  detector.detect( img_object, keypoints_object );
	double keypointObjectElapsedTime = t.elapsed();
  detector.detect( img_scene, keypoints_scene );
	double keypointSceneElapsedTime = t.elapsed();


  //-- Step 2: Calculate descriptors (feature vectors)
  SurfDescriptorExtractor extractor;

  Mat descriptors_object, descriptors_scene;

  extractor.compute( img_object, keypoints_object, descriptors_object );
	double descriptorObjectElapsedTime = t.elapsed();
  extractor.compute( img_scene, keypoints_scene, descriptors_scene );
	double descriptorSceneElapsedTime = t.elapsed();

  //-- Step 3: Matching descriptor vectors using FLANN matcher
  FlannBasedMatcher matcher;
  std::vector< DMatch > matches;
  matcher.match( descriptors_object, descriptors_scene, matches );
	double matcherElapsedTime = t.elapsed();

  double max_dist = 0; double min_dist = 100;

  //-- Quick calculation of max and min distances between keypoints
  for( int i = 0; i < descriptors_object.rows; i++ )
  { double dist = matches[i].distance;
    if( dist < min_dist ) min_dist = dist;
    if( dist > max_dist ) max_dist = dist;
  }

  //-- Draw only "good" matches (i.e. whose distance is less than 2*min_dist )
  std::vector< DMatch > good_matches;

  for( int i = 0; i < descriptors_object.rows; i++ )
  { if( matches[i].distance < 2*min_dist )
     { good_matches.push_back( matches[i]); }
  }
  Mat img_matches;
  drawMatches( img_object, keypoints_object, img_scene, keypoints_scene,
               good_matches, img_matches, Scalar::all(-1), Scalar::all(-1),
               vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS );

  //-- Localize the object
  std::vector<Point2f> obj;
  std::vector<Point2f> scene;

  for( int i = 0; i < good_matches.size(); i++ )
  {
    //-- Get the keypoints from the good matches
    obj.push_back( keypoints_object[ good_matches[i].queryIdx ].pt );
    scene.push_back( keypoints_scene[ good_matches[i].trainIdx ].pt );
  }
	double goodMatchesElapsedTime = t.elapsed();

	std::vector<uchar> outlier_mask;  // Used for homography
  Mat H = findHomography( obj, scene, CV_RANSAC, 1.0, outlier_mask);
	double homographyElapsedTime = t.elapsed();
	int inliers=0, outliers=0;
	for(unsigned int k=0; k<obj.size();++k)
	{
		if(outlier_mask.at(k))
		{
			++inliers;
		}
		else
		{
			++outliers;
		}
	}

	// The homography must contain atleast 70% of the extracted keypoints in the homography
	// for a "good match"
	// If the inlier to outlier ratio is above this threshold, we will draw the bounding box
	// and count the object as recognized
	float inlierRatio = 0.7;
	if( static_cast<float>(inliers/outliers) >= inlierRatio){
		std::cout << "Recognized the object in the scene" << std::endl;
		//-- Get the corners from the image_1 ( the object to be "detected" on the left)
		std::vector<Point2f> obj_corners(4);
		obj_corners[0] = cvPoint(0,0);
		obj_corners[1] = cvPoint( img_object.cols, 0 );
		obj_corners[2] = cvPoint( img_object.cols, img_object.rows );
		obj_corners[3] = cvPoint( 0, img_object.rows );
		std::vector<Point2f> scene_corners(4);

		perspectiveTransform( obj_corners, scene_corners, H);

		//-- Draw lines between the corners (the mapped object in the scene - image_2 )
		line( img_matches, scene_corners[0] + Point2f( img_object.cols, 0), scene_corners[1] + Point2f( img_object.cols, 0), Scalar(0, 255, 0), 4 );
		line( img_matches, scene_corners[1] + Point2f( img_object.cols, 0), scene_corners[2] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
		line( img_matches, scene_corners[2] + Point2f( img_object.cols, 0), scene_corners[3] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
		line( img_matches, scene_corners[3] + Point2f( img_object.cols, 0), scene_corners[0] + Point2f( img_object.cols, 0), Scalar( 0, 255, 0), 4 );
	}
	double totalElapsedTime = t.elapsed();
	std::cout << std::endl << "#### Time measurements #### " << std::endl;
	std::cout << "The program ran in " << totalElapsedTime << "seconds" << std::endl;
	std::cout << "Keypoints calculated at " << keypointObjectElapsedTime << "seconds for the object and at "<< keypointSceneElapsedTime << " seconds for the scene " << std::endl;
	std::cout << "Descriptors calculated at " << descriptorObjectElapsedTime << "seconds for the object and at "<< descriptorSceneElapsedTime << " seconds for the scene " << std::endl;

	std::cout << keypoints_object.size() << " detected keypoints in the object frame " << std::endl;
	std::cout << keypoints_scene.size() << " detected keypoints in the scene frame " << std::endl;
  printf("-- Max dist between matches: %f \n", max_dist );
  printf("-- Min dist between matches: %f \n", min_dist );
	std::cout << good_matches.size() << " good matches" << std::endl;
	std::cout << "found " << inliers << " Inliers in the Homography and " << outliers << " Outliers" << std::endl;

  //-- Show detected matches
  imshow( "Good Matches & Object detection", img_matches );

  waitKey(0);
  return 0;
  }

  /** @function readme */
  void readme()
  { std::cout << " Usage: ./surf_keypoints <img1> <img2>" << std::endl; }
