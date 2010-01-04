//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _LIGHTINFO_H_
#define _LIGHTINFO_H_

#ifndef _GFXSTRUCTS_H_
#include "gfx/gfxStructs.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif

struct SceneGraphData;
class LightManager;
class SimObject;
class BitStream;


/// The extended light info type wrapper object.
class LightInfoExType
{
protected:

   typedef HashTable<String,U32> TypeMap;

   /// Returns the map of all the info types.  We create
   /// it as a method static so that its available to other
   /// statics regardless of initialization order.
   static inline TypeMap& getTypeMap()
   {
      static TypeMap smTypeMap;
      return smTypeMap;
   }

   /// The info type index for this type.
   U32 mTypeIndex;

public:

   LightInfoExType( const char *type );

   inline LightInfoExType( const LightInfoExType &type )
      : mTypeIndex( type.mTypeIndex )
   {
   }

   inline operator U32 () const { return mTypeIndex; }
};

/// This is the base class for extended lighting info
/// that lies outside of the normal info stored in LightInfo.
class LightInfoEx
{
public:

   /// Basic destructor so we can delete the extended info
   /// without knowing the concrete type.
   virtual ~LightInfoEx() { }

   /// 
   virtual const LightInfoExType& getType() const = 0;

   /// Copy the values from the other LightInfoEx.
   virtual void set( const LightInfoEx *ex ) {}

   ///
   virtual void packUpdate( BitStream *stream ) const {}

   ///
   virtual void unpackUpdate( BitStream *stream ) {}
};


/// This is the base light information class that will be tracked by the 
/// engine.  Should basically contain a bounding volume and methods to interact
/// with the rest of the system (for example, setting GFX fixed function lights).
class LightInfo
{
public:

   enum Type 
   {
      Point    = 0,
      Spot     = 1,
      Vector   = 2,
      Ambient  = 3,
      Count    = 4,
   };

protected:

   Type mType;

   /// The primary light color.
   ColorF mColor;


   F32 mBrightness;

   ColorF mAmbient;

   MatrixF mTransform;

   Point3F mRange;

   F32 mInnerConeAngle;

   F32 mOuterConeAngle;

   bool mCastShadows;

   ::Vector<LightInfoEx*> mExtended;

   /// The priority of this light used for
   /// light and shadow scoring.
   F32 mPriority;

   /// A temporary which holds the score used
   /// when prioritizing lights for rendering.
   F32 mScore;

public:

   LightInfo();
   ~LightInfo();

   // Copies data passed in from light
   void set( const LightInfo *light );

   // Sets a fixed function GFXLight with our properties 
   void setGFXLight( GFXLightInfo *light );

   // Accessors
   Type getType() const { return mType; }
   void setType( Type val ) { mType = val; }

   const MatrixF& getTransform() const { return mTransform; }
   void setTransform( const MatrixF &xfm ) { mTransform = xfm; }

   Point3F getPosition() const { return mTransform.getPosition(); }
   void setPosition( const Point3F &pos ) { mTransform.setPosition( pos ); }

   VectorF getDirection() const { return mTransform.getForwardVector(); }
   void setDirection( const VectorF &val );

   const ColorF& getColor() const { return mColor; }
   void setColor( const ColorF &val ) { mColor = val; }

   F32 getBrightness() const { return mBrightness; }
   void setBrightness( F32 val ) { mBrightness = val; }

   const ColorF& getAmbient() const { return mAmbient; }
   void setAmbient( const ColorF &val ) { mAmbient = val; }

   const Point3F& getRange() const { return mRange; }
   void setRange( const Point3F &range ) { mRange = range; }
   void setRange( F32 range ) { mRange.set( range, range, range ); }

   F32 getInnerConeAngle() const { return mInnerConeAngle; }
   void setInnerConeAngle( F32 val ) { mInnerConeAngle = val; }

   F32 getOuterConeAngle() const { return mOuterConeAngle; }
   void setOuterConeAngle( F32 val ) { mOuterConeAngle = val; }

   bool getCastShadows() const { return mCastShadows; }
   void setCastShadows( bool castShadows ) { mCastShadows = castShadows; }

   void setPriority( F32 priority ) { mPriority = priority; }
   F32 getPriority() const { return mPriority; }

   void setScore( F32 score ) { mScore = score; }
   F32 getScore() const { return mScore; }

   /// Helper function for getting the extended light info.
   /// @see getExtended
   template <class ExClass>
   inline ExClass* getExtended() const { return (ExClass*)getExtended( ExClass::Type ); }

   /// Returns the extended light info for the selected type.
   LightInfoEx* getExtended( const LightInfoExType &type ) const;

   /// Adds the extended info to the light deleting the 
   /// existing extended info if it has one.
   void addExtended( LightInfoEx *lightInfoEx );   

   /// 
   void deleteAllLightInfoEx();

   ///
   void packExtended( BitStream *stream ) const;

   ///
   void unpackExtended( BitStream *stream );
};


///
class LightInfoList : public Vector<LightInfo*>
{
public:
   void registerLight( LightInfo *light );
   void unregisterLight( LightInfo *light );
};


/// When the scene is queried for lights, the light manager will get 
/// this interface to trigger a register light call.
class ISceneLight
{
public:

   virtual ~ISceneLight() {}

   /// Submit lights to the light manager passed in.
   virtual void submitLights( LightManager *lm, bool staticLighting ) = 0;

   ///
   virtual LightInfo* getLight() = 0;
};

#endif // _LIGHTINFO_H_
