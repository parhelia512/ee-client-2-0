//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _MATTEXTURETARGET_H_
#define _MATTEXTURETARGET_H_

#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif
#ifndef _REFBASE_H_
#include "core/util/refBase.h"
#endif

class RectI;
class GFXTextureObject;
struct GFXSamplerStateDesc;
struct GFXShaderMacro;
class ConditionerFeature;


///
class MatTextureTarget : public WeakRefBase
{
protected:

   typedef Map<String,MatTextureTarget*> TexTargetMap;

   ///
   static TexTargetMap smRegisteredTargets;

   /// The target name we were registered with.
   String mRegTargetName;

public:

   /// @name Registration and Management
   /// @{ 

   ///
   static bool registerTarget( const String &name, MatTextureTarget *target );

   ///
   static void unregisterTarget( const String &name, MatTextureTarget *target );

   ///
   static MatTextureTarget* findTargetByName( const String &name );

   /// @}
   
public:

   virtual ~MatTextureTarget();

   ///
   virtual GFXTextureObject* getTargetTexture( U32 mrtIndex ) const = 0;

   ///
   //virtual const Point2I &getTargetSize() const = 0;

   ///
   virtual const RectI& getTargetViewport() const = 0;

   ///
   virtual void setupSamplerState( GFXSamplerStateDesc *desc ) const = 0;

   /// Returns the conditioner feature for the target.
   virtual ConditionerFeature* getTargetConditioner() const = 0;

   /// Adds the condition and uncondition shader macros
   /// from the ConditionerFeature to the incoming vector.
   void getTargetShaderMacros( Vector<GFXShaderMacro> *outMacros );

};

/// A weak reference to a texture target.
typedef WeakRefPtr<MatTextureTarget> MatTextureTargetRef;

#endif // _MATTEXTURETARGET_H_
