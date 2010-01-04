#ifndef _DDS_UTILS_H_
#define _DDS_UTILS_H_

struct DDSFile;

namespace DDSUtil
{
   bool squishDDS( DDSFile *srcDDS, const GFXFormat dxtFormat );
   void swizzleDDS( DDSFile *srcDDS, const Swizzle<U8, 4> &swizzle );
};

#endif