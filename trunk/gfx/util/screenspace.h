#ifndef _SCREENSPACE_UTILS_H_
#define _SCREENSPACE_UTILS_H_

#include "math/mPoint3.h"
#include "math/mPoint4.h"
#include "math/mRect.h"

namespace ScreenSpace
{
void RenderTargetParameters(const Point3I &targetSize, const RectI &targetViewport, Point4F &rtParams);
};

#endif