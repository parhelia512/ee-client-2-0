
#include "tResponseCurve.h"

IMPLEMENT_CONOBJECT( SimResponseCurve );

SimResponseCurve::SimResponseCurve()
{

}

bool SimResponseCurve::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   return true;
}

void SimResponseCurve::onRemove()
{
   Parent::onRemove();
}

void SimResponseCurve::addPoint(F32 value, F32 time)
{
   mCurve.addPoint( value, time );
}

F32 SimResponseCurve::getValue(F32 time)
{
   return mCurve.getVal( time );
}

void SimResponseCurve::clear()
{
   mCurve.clear();
}

ConsoleMethod( SimResponseCurve, addPoint, void, 4, 4, "addPoint( F32 value, F32 time )" )
{
   object->addPoint( dAtof(argv[2]), dAtof(argv[3]) );
}

ConsoleMethod( SimResponseCurve, getValue, F32, 3, 3, "getValue( F32 time )" )
{
   return object->getValue( dAtof(argv[2]) );
}

ConsoleMethod( SimResponseCurve, clear, void, 2, 2, "clear()" )
{
   object->clear();
}