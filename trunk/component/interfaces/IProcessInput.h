//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _I_PROCESSINPUT_H_
#define _I_PROCESSINPUT_H_

#include "platform/event.h"


// CodeReview : Input can come from a number of places to end up in 
// torque, but in the torque world we don't want to expose this 
// information to the user.  This interface bridges the gap between 
// other input devices working details and input as torque understands it.
// Thoughts?  [7/6/2007 justind]
class IProcessInput
{
public:
   virtual bool processInputEvent( InputEventInfo &inputEvent ) = 0;
};





#endif