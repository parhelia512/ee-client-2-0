#ifndef _RGB2LUV_H_
#define _RGB2LUV_H_

#ifndef _COLOR_H_
#include "core/color.h"
#endif

namespace ConvertRGB
{
   ColorF toLUV( const ColorF &rgbColor );
   ColorF toLUVScaled( const ColorF &rgbColor );
   ColorF fromLUV( const ColorF &luvColor );
};

#endif