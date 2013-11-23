#ifndef PERCEIVED_OBJECT_H
#define PERCEIVED_OBJECT_H

#include "point.h"

namespace suturo_perception_lib
{
  class PerceivedObject
  {
      public:
    int c_id;
    Point c_centroid;
    double c_volume;
    int c_shape;
  };
}

#endif
