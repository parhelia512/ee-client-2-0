//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/consoleCallback.h"
#include "console/simBase.h"
#include "math/mathTypes.h"
#include "math/mPoint3.h"

// FIXME [tom, 3/14/2007] Temporarily commenting this out because it's broken and
// we don't currently need it. It will more then likely require fixing before we
// have a working build, but for now it's one less error to worry about.

 class CBTest : public SimObject
 {
    typedef SimObject Parent;
 public:
    DECLARE_CONOBJECT(CBTest);
    DECLARE_CONSOLE_CALLBACK(Point3F, onCollide, 
    (Point3F pos, Point3F normal, S32 b, F32 c));
 };
 
 IMPLEMENT_CONOBJECT(CBTest);
 
 IMPLEMENT_CONSOLE_CALLBACK(CBTest, Point3F, onCollide, 
                            (Point3F pos, Point3F normal, S32 b, F32 c), (pos, normal, b, c),
                            true, "Simple callback issued on collision events.")
 
 ConsoleFunction(testCB, void, 1, 1, "Test that callbacks can happen!")
 {
    CBTest *testObj = new CBTest();
    testObj->registerObject("testCallback");
    Point3F foo = testObj->onCollide(Point3F(1,2,3), Point3F(4,5,6), 1, 2.0);
 }
