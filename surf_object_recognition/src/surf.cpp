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
#include <boost/program_options.hpp>
#include <algorithm>
#include <iterator>

#include "object_matcher.h"
#include "simple_matcher.cpp"
#include "nndr_matcher.cpp"
#include "symmetry_nndr_matcher.cpp"
 
using namespace boost;
namespace po = boost::program_options;

// OpenCV 2.4 moved the SURF and SIFT libraries to a new nonfree/ directory
#if CV_MINOR_VERSION > 3
	#include <opencv2/nonfree/features2d.hpp>
#else
	#include <opencv2/features2d/features2d.hpp>
#endif

using namespace cv;
using namespace std;
namespace po = boost::program_options;

/** @function main */
int main( int argc, char** argv )
{
  bool headless_mode=false;
  std::string train_image;
  std::vector<std::string> test_images;
  int min_good_matches;
  std::string matcher;

  try
  {
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
      ("help", "produce help message")
      ("train-img,t", po::value<std::vector<std::string> >()->required(), "The training image, that should be matched in the test images")
      ("test-img,i", po::value<std::vector<std::string> >()->required(), "A set of images where train-img should be recognized")
      // zero_tokens tells program_options to not require an argument to a parameter switch
      // This gives flag-like behaviour
      ("headless,h", po::value<bool>()->zero_tokens(), "Headless mode -- Dont show images")
      ("min-good-matches,m", po::value<int>(&min_good_matches)->default_value(0), "The minimum amount of good matches which must be present to perform object recognition")
      ("matcher,a", po::value<std::string>(&matcher), "Choose the strategy for matching the extracted Descriptors. Possible Values: simple, nndr, symmetry_nndr")
    ;

    // Use positional_options to allow the passing of test-img filenames without giving
    // the parameters explicitly. Like surf_keypoints testimg1 testimg2 instead of
    // surf_keypoints -i testimg1 -i testimg2 etc.
    po::positional_options_description p;
    p.add("test-img", -1);

    // "HashMap" for parameters
    po::variables_map vm;
    // po::store(po::parse_command_line(argc, argv, desc), vm); // Without positional options
    po::store(po::command_line_parser(argc, argv).
          options(desc).positional(p).run(), vm); 

    if (vm.count("help")) {
      cout << "Usage: surf_keypoints [OPTIONS] test-img1 test-img2..." << endl << endl;
      cout << desc << "\n";
      return 1;
    }

    // Put notify after the help check, so help is display even
    // if required parameters are not given
    po::notify(vm);

    if(vm.count("train-img"))
    {
      // Take the first train-image, we are not supporting more right now
      train_image = vm["train-img"].as<std::vector<std::string> >().at(0);
    }
    if(vm.count("test-img"))
    {
      test_images = vm["test-img"].as<std::vector<std::string> >();
    }

    // Check for valid values for matcher
    if (vm.count("matcher") && 
        (!(matcher == "simple" || matcher == "nndr" || matcher == "symmetry_nndr")))
            throw po::validation_error(po::validation_error::invalid_option_value, "matcher");

    if(vm.count("headless"))
    {
      headless_mode = true;
      cout << "Running headless" << endl;
    }
    else
    {
      headless_mode = false;
      cout << "Running with GUI" << endl;
    }

  }
  catch(std::exception& e)
  {
    cout << "Usage: surf_keypoints [OPTIONS] test-img1 test-img2..." << endl << endl;
    std::cerr << "Error: " << e.what() << "\n";
    return false;
  }
  catch(...)
  {
    std::cerr << "Unknown error!" << "\n";
    return false;
  } 

  ObjectMatcher om; // Create ObjectMatcher with SURF as default
  // ObjectMatcher om(detector,extractor);
  std::cout << "Starting with min_good_matches=" << min_good_matches << std::endl;
  om.setMinGoodMatches(min_good_matches);

  // Set the desired matcher, if the user picked one
  if(matcher=="simple")
  {
    om.setMatcher(new SimpleMatcher);
  } else if(matcher=="nndr")
  {
    om.setMatcher(new NNDRMatcher);
  } else if(matcher=="symmetry_nndr")
  {
    om.setMatcher(new SymmetryNNDRMatcher);
  }

  int positives=0;
  // Run all test images against Training image
  for(int i=0;i<test_images.size();i++)
  {
    if(om.execute(train_image,test_images.at(i),headless_mode))
      positives++;
    cout << endl;
  }
  std::cout << "Found " << positives << " positives in " << test_images.size() << " test images" << std::endl;
  return 0;
}
// vim: tabstop=2 expandtab shiftwidth=2 softtabstop=2: 
