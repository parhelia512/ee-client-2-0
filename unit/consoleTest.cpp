//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/strings/stringFunctions.h"

#include "unit/test.h"
#include "console/console.h"

using namespace UnitTesting;

ConsoleFunction(unitTest_runTests, void, 1, 3, "([searchString[, bool skipInteractive]]) - run unit tests,"
                " or just the tests that prefix match against the searchString.")
{
   const char *searchString = (argc > 1 ? argv[1] : "");
   bool skip = (argc > 2 ? dAtob(argv[2]) : false);

   TestRun tr;
   tr.test(searchString, skip);
}