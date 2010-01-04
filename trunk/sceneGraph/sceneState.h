//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SCENESTATE_H_
#define _SCENESTATE_H_

#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _MRECT_H_
#include "math/mRect.h"
#endif
#ifndef _MMATRIX_H_
#include "math/mMatrix.h"
#endif
#ifndef _MATHUTIL_FRUSTUM_H_
#include "math/util/frustum.h"
#endif
#ifndef _COLOR_H_
#include "core/color.h"
#endif
#ifndef _SCENEGRAPH_H_
#include "sceneGraph/sceneGraph.h"
#endif
#ifndef _LIGHTMANAGER_H_
#include "lighting/lightManager.h"
#endif

class SceneObject;
class InteriorInstance;
class RenderPassManager;

//--------------------------------------------------------------------------
//-------------------------------------- SceneState
//


/// The SceneState describes the state of the scene being rendered. It keeps track
/// of the information that objects need to render properly with regard to the
/// camera position, any fog information, viewing frustum, the global environment
/// map for reflections, viewable distance and portal information.
class SceneState
{
   friend class SceneGraph;

public:

   struct ZoneState 
   {
      bool  render;
      Frustum frustum;
      RectI viewport;

      bool   clipPlanesValid;
   };

   struct InteriorListElem
   {
      InteriorInstance *  obj;
      U32            stateKey;
      U32            startZone;
      U32            detailLevel;
      const MatrixF      * worldXform;
   };

   /// Sets up the clipping planes using the parameters from a ZoneState.
   ///
   /// @param   zone   ZoneState to initalize clipping to
   void setupClipPlanes( ZoneState *zone );

   /// Used to represent a portal which inserts a transformation into the scene.
   struct TransformPortal {
      SceneObject* owner;
      U32          portalIndex;
      U32          globalZone;
      Point3F      traverseStart;
      bool         flipCull;
   };

  public:

   /// Constructor
   SceneState( SceneState *parent,
               SceneGraph *mgr,
               ScenePassType passType,
               U32 numZones,
               const Frustum &frustum,
               const RectI &viewport,
               bool usePostEffects,
               bool invert );

   ~SceneState();

   /// Sets the active portal.
   ///
   /// @param   owner Object which owns the portal (portalized object)
   /// @param   idx   Index of the portal in the list of portal planes
   void setPortal(SceneObject *owner, const U32 idx);

   SceneGraph* getSceneManager() const { return mSceneManager; }

   LightManager* getLightManager() const { return mLightManager; }

   RenderPassManager* getRenderPass() const { return mSceneManager->getRenderPass(); }

   /// Returns the actual camera position.
   /// @see getDiffuseCameraPosition
   const Point3F& getCameraPosition() const { return mFrustum.getPosition(); }

   /// Returns the camera transform this SceneState is using.
   const MatrixF& getCameraTransform() const { return mFrustum.getTransform(); }

   /// Returns the minimum distance something must be from the camera to not be culled.
   F32 getNearPlane()  const { return mFrustum.getNearDist();   }

   /// Returns the maximum distance something can be from the camera to not be culled.
   F32 getFarPlane() const { return mFrustum.getFarDist();    }

   /// Returns the type of scene rendering pass that we're doing.
   ScenePassType getScenePassType() const { return mScenePassType; }

   /// Returns true if this is a diffuse scene rendering pass.
   bool isDiffusePass() const { return mScenePassType == SPT_Diffuse; }

   /// Returns true if this is a reflection scene rendering pass.
   bool isReflectPass() const { return mScenePassType == SPT_Reflect; }

   /// Returns true if this is a shadow scene rendering pass.
   bool isShadowPass() const { return mScenePassType == SPT_Shadow; }

   /// Returns true if this is not one of the other rendering passes.
   bool isOtherPass() const { return mScenePassType >= SPT_Other; }

   /// If true then bin based post effects are disabled 
   /// during rendering with this scene state.
   bool usePostEffects() const { return mUsePostEffects; }

   /// Returns the pixel size of the radius projected to 
   /// the screen at a desired distance.
   /// 
   /// Internally this uses the stored world to screen scale
   /// and viewport extents.  This allows the projection to
   /// be overloaded in special cases like when rendering
   /// shadows or reflections.
   ///
   /// @see getWorldToScreenScale
   /// @see getViewportExtent
   F32 projectRadius( F32 dist, F32 radius ) const;

   /// Returns the possibly overloaded world to screen scale.
   /// @see projectRadius
   const Point2F& getWorldToScreenScale() const { return mWorldToScreenScale; }

   /// Set a new world to screen scale to overload
   /// future screen metrics operations.
   void setWorldToScreenScale( const Point2F &scale ) { mWorldToScreenScale = scale; }

   /// Returns the possibly overloaded viewport extents.
   const Point2I& getViewportExtent() const { return mViewportExtent; }

   /// Set a new viewport extent to overload future
   /// screen metric operations.
   void setViewportExtent( const Point2I &extent ) { mViewportExtent = extent; }

   /// Returns the camera position used during the 
   /// diffuse rendering pass which may be different
   /// from the actual camera position.
   ///
   /// This is useful when doing level of detail 
   /// calculations that need to be relative to the
   /// diffuse pass.
   ///
   /// @see getCameraPosition
   Point3F getDiffuseCameraPosition() const { return mDiffuseCameraTransform.getPosition(); }
   const MatrixF& getDiffuseCameraTransform() const { return mDiffuseCameraTransform; }

   /// Set a new diffuse camera transform.
   /// @see getDiffuseCameraTransform
   void setDiffuseCameraTransform( const MatrixF &mat ) { mDiffuseCameraTransform = mat; }

   /// Returns the frustum.
   const Frustum& getFrustum() const { return mFrustum; }

   /// Set a different frustum for culling.
   void setFrustum( const Frustum &frustum ) { mFrustum = frustum; }

   /// Returns the base ZoneState.
   ///
   /// @see ZoneState
   const ZoneState& getBaseZoneState() const;

   /// Returns the base ZoneState as a non-const reference.
   ///
   /// @see getBaseZoneState
   ZoneState& getBaseZoneStateNC();

   /// Returns the ZoneState for a particular zone ID.
   ///
   /// @param   zoneId   ZoneId
   const ZoneState& getZoneState(const U32 zoneId) const;

   /// Returns the ZoneState for a particular zone ID as a non-const reference.
   ///
   /// @see getZoneState
   /// @param   zoneId   ZoneId
   ZoneState&       getZoneStateNC(const U32 zoneId);

   /// Adds a new transform portal to the SceneState.
   ///
   /// @param   owner                 SceneObject owner of the portal (portalized object).
   /// @param   portalIndex           Index of the portal in the list of portal planes.
   /// @param   globalZone            Index of the zone this portal is in in the list of ZoneStates.
   /// @param   traversalStartPoint   Start point of the zone traversal.
   ///                                @see SceneGraph::buildSceneTree
   ///                                @see SceneGraph::findZone
   ///
   /// @param   flipCull              If true, the portal plane will be flipped
   void insertTransformPortal(SceneObject* owner, U32 portalIndex,
                              U32 globalZone,     const Point3F& traversalStartPoint,
                              const bool flipCull);

   /// This enables terrain to be drawn inside interiors.
   void enableTerrainOverride();

   /// Returns true if terrain is allowed to be drawn inside interiors.
   bool isTerrainOverridden() const;

   /// Sorts the list of images, builds the translucency BSP tree,
   /// sets up the portal, then renders all images in the state.
   void renderCurrentImages();

   /// Returns true if the object in question is going to be rendered
   /// as opposed to being culled out.
   ///
   /// @param   obj   Object in question.
   bool isObjectRendered(const SceneObject *obj);

   /// Returns true if the culling on this state has been flipped
   bool isInvertedCull() const { return mFlipCull; }

   /// 
   void objectAlwaysRender( bool alwaysRender ) { mAlwaysRender = alwaysRender; }

  protected:

   Vector<ZoneState>         mZoneStates;             ///< Collection of ZoneStates in the scene.

   /// Builds the BSP tree of translucent images.
   void buildTranslucentBSP();

   bool mTerrainOverride;                             ///< If true, terrain is allowed to render inside interiors

   Vector<SceneState*>       mSubsidiaries;           ///< Transform portals which have been processed by the scene traversal process
                                                      ///
                                                      ///  @note Closely related.  Transform portals are turned into sorted mSubsidiaries
                                                      ///        by the traversal process...

   Vector<TransformPortal>   mTransformPortals;       ///< Collection of TransformPortals

   ZoneState mBaseZoneState;                          ///< ZoneState of the base zone of the scene

   ///
   MatrixF mDiffuseCameraTransform;

   /// The world to screen space scalar used for LOD calculations.
   Point2F mWorldToScreenScale;

   /// The viewport extents used for LOD calculations.
   /// @see 
   Point2I mViewportExtent;

   /// The camera frustum used in culling.
   Frustum  mFrustum;

   SceneState* mParent;
   SceneGraph * mSceneManager;
   LightManager* mLightManager;                               ///< This is used for portals, if this is not NULL, then this SceneState belongs to a portal, and not the main SceneState
   
   /// The type of scene render pass we're doing.
   ScenePassType mScenePassType;

   SceneObject* mPortalOwner;                         ///< SceneObject which owns the current portal
   U32          mPortalIndex;                         ///< Index the current portal is in the list of portals

   Vector< InteriorListElem > mInteriorList;

   bool mAlwaysRender;

   /// Forces bin based post effects to be disabled
   /// during rendering with this scene state.
   bool mUsePostEffects;

   bool    mFlipCull;                                 ///< If true the portal clipping plane will be reversed

  public:
   bool    mRenderLightmappedMeshes;                  ///< If true (default) lightmapped meshes should be rendered
   bool    mRenderNonLightmappedMeshes;               ///< If true (default) non-lightmapped meshes should be rendered


   void insertInterior( InteriorListElem &elem )
   {
      mInteriorList.push_back( elem );
   }
};

inline F32 SceneState::projectRadius( F32 dist, F32 radius ) const
{
   // We fixup any negative or zero distance 
   // so we don't get a divide by zero.
   dist = dist > 0.0f ? dist : 0.001f;
   return ( radius / dist ) * mWorldToScreenScale.y;
}

inline const SceneState::ZoneState& SceneState::getBaseZoneState() const
{
   return mBaseZoneState;
}

inline SceneState::ZoneState& SceneState::getBaseZoneStateNC()
{
   return mBaseZoneState;
}

inline const SceneState::ZoneState& SceneState::getZoneState(const U32 zoneId) const
{
   AssertFatal(zoneId < (U32)mZoneStates.size(), "Error, out of bounds zone!");
   return mZoneStates[zoneId];
}

inline SceneState::ZoneState& SceneState::getZoneStateNC(const U32 zoneId)
{
   AssertFatal(zoneId < (U32)mZoneStates.size(), "Error, out of bounds zone!");
   return mZoneStates[zoneId];
}

inline void SceneState::enableTerrainOverride()
{
   mTerrainOverride = true;
}

inline bool SceneState::isTerrainOverridden() const
{
   return mTerrainOverride;
}


#endif  // _H_SCENESTATE_

