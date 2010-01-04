//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SFXLISTENER_H_
#define _SFXLISTENER_H_

#ifndef _MPOINT3_H_
   #include "math/mPoint3.h"
#endif
#ifndef _MMATH_H_
   #include "math/mMatrix.h"
#endif
#ifndef _TVECTOR_H_
   #include "core/util/tVector.h"
#endif


class SFXSource;



///
class SFXListener
{
   protected:

      /// The current position and rotation.
      MatrixF  mTransform;

      /// The velocity.
      VectorF  mVelocity;

      /// Used to sort sources by attenuated volume and channel priority.
      static S32 QSORT_CALLBACK sourceCompare( const void* item1, const void* item2 );

   public:

      /// The constructor.
      SFXListener();

      /// The non-virtual destructor... because you 
      /// shouldn't need to overload this class.
      ~SFXListener();

      ///
      void setTransform( const MatrixF& transform ) { mTransform = transform; }

      ///
      const MatrixF& getTransform() const { return mTransform; }

      ///
      void setVelocity( const VectorF& velocity ) { mVelocity = velocity; }

      ///
      const VectorF& getVelocity() const { return mVelocity; }

      ///
      void sortSources( Vector< SFXSource* >& sources );
};

#endif // _SFXLISTENER_H_
