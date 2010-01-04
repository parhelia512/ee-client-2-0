//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _REFLECTIONMANAGER_H_
#define _REFLECTIONMANAGER_H_

#ifndef _GFXDEVICE_H_
#include "gfx/gfxDevice.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _UTIL_DELEGATE_H_
#include "core/util/delegate.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _TSINGLETON_H_
#include "core/util/tSingleton.h"
#endif
#ifndef _REFLECTOR_H_
#include "sceneGraph/reflector.h"
#endif

class PlatformTimer;

enum ReflectMode
{
   ReflectNever = 0,
   ReflectDynamic,
   ReflectAlways        
};

typedef Delegate<bool(bool)> ReflectDelegate;     
class SceneObject;

struct Reflector
{
   SceneObject *object;
   ReflectDelegate updateFn;
   F32 priority;
   U32 maxRateMs;
   F32 maxDist;
   U32 lastUpdateMs;
   F32 score;
   bool updated;
   bool tried;
   bool hasTexture;
};

typedef Vector<Reflector> ReflectorVec;

GFX_DeclareTextureProfile( ReflectRenderTargetProfile );
GFX_DeclareTextureProfile( RefractTextureProfile );

class ReflectionManager
{
public:

   ReflectionManager();
   virtual ~ReflectionManager();     

   /// Called to change the reflection texture format.
   void setReflectFormat( GFXFormat format ) { mReflectFormat = format; }

   /// Returns the current reflection format.
   GFXFormat getReflectFormat() const { return mReflectFormat; }

   /// Doll out callbacks to registered objects based on 
   /// scoring and elapsed time.  This should be called 
   /// once for each viewport that renders.
   void update(   F32 timeSlice, 
                  const Point2I &resolution, 
                  const CameraQuery &query );

   void registerReflector( ReflectorBase *reflector );
   void unregisterReflector( ReflectorBase *reflector );

   GFXTexHandle allocRenderTarget( const Point2I &size );  

   GFXTextureObject* getRefractTex();

protected:

   bool _handleDeviceEvent( GFXDevice::GFXDeviceEventType evt );

protected:
   
   static U32 smFrameReflectionMS;

   /// A timer used for tracking update time.
   PlatformTimer *mTimer;

   ReflectorList mReflectors;

   /// Refraction texture copied from the backbuffer once per frame that
   /// gets used by all WaterObjects.
   GFXTexHandle mRefractTex;

   /// RefractTex has dimensions equal to the active render target scaled in
   /// both x and y by this float.
   F32 mRefractTexScale;

   /// The texture format to use for reflection and
   /// refraction texture sources.
   GFXFormat mReflectFormat;

   /// Set when the refaction texture is dirty
   /// and requires an update.
   bool mUpdateRefract;
};


/// Returns the ReflectionManager singleton.
#define REFLECTMGR Singleton<ReflectionManager>::instance()

#endif // _REFLECTIONMANAGER_H_