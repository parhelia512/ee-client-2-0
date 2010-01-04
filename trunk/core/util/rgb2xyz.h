#ifndef _RGB2XYZ_H_
#define _RGB2XYZ_H_

#include "core/color.h"

namespace ConvertRGB
{
   ColorF toXYZ( const ColorF &rgbColor );
   ColorF fromXYZ( const ColorF &xyzColor );
};

#endif