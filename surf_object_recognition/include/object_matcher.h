#ifndef OBJECT_MATCHER_H
#define OBJECT_MATCHER_H

#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/core/version.hpp>
#include <opencv2/highgui/highgui.hpp>

// OpenCV 2.4 moved the SURF and SIFT libraries to a new nonfree/ directory
#if CV_MINOR_VERSION > 3
	#include <opencv2/nonfree/features2d.hpp>
#else
	#include <opencv2/features2d/features2d.hpp>
#endif

#include "matching_strategy.h"

using namespace cv;

class ObjectMatcher
{
private:
	cv::Ptr<cv::FeatureDetector> detector;
	cv::Ptr<cv::DescriptorExtractor> extractor;
	MatchingStrategy* matching_strategy;
	// result will be put into good_matches by reference
	// void match(Mat &descriptors_object, Mat &descriptors_scene, std::vector<DMatch> &good_matches);
	
public:
		ObjectMatcher();
		ObjectMatcher(cv::Ptr<cv::FeatureDetector> detector, cv::Ptr<cv::DescriptorExtractor> extractor);
		/**
		 * Try to recognize the Object given in train_image in the test_image
		 * @param headless If this is set to true, no Windows will be opened to display the matches.
		 * @returns True, if Object has been recognized
		 */
		bool execute(std::string train_image, std::string test_image, bool headless);
};

#endif

