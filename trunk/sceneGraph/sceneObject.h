//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _SCENEOBJECT_H_
#define _SCENEOBJECT_H_

#ifndef _NETOBJECT_H_
#include "sim/netObject.h"
#endif
#ifndef _COLLISION_H_
#include "collision/collision.h"
#endif
#ifndef _POLYHEDRON_H_
#include "collision/polyhedron.h"
#endif
#ifndef _ABSTRACTPOLYLIST_H_
#include "collision/abstractPolyList.h"
#endif
#ifndef _OBJECTTYPES_H_
#include "T3D/objectTypes.h"
#endif
#ifndef _COLOR_H_
#include "core/color.h"
#endif
#ifndef _BASEMATINSTANCE_H_
#include "materials/baseMatInstance.h"
#endif
#ifndef _LIGHTRECEIVER_H_
#include "lighting/lightReceiver.h"
#endif


//-------------------------------------- Forward declarations...
class SceneObject;
class SceneGraph;
class SceneState;
class Box3F;
class Point3F;
class Convex;
class LightInfo;
class Frustum;
struct ObjectRenderInst;

//--------------------------------------------------------------------------

// CodeReview - old note which was posing as documentation!
//    There are two indiscretions here.  First is the name, which refers rather
//    blatantly to the container bin system.  A hygiene issue.  Next is the
//    user defined U32, which is added solely for the zoning system.  This should
//    properly be split up into two structures, for the disparate purposes, especially
//    since it's not nice to force the container bin to use 20 bytes structures when
//    it could get away with a 16 byte version.

/// Reference to a scene object.
class SceneObjectRef
{
  public:
   SceneObject*    object;
   SceneObjectRef* nextInBin;
   SceneObjectRef* prevInBin;
   SceneObjectRef* nextInObj;

   U32             zone;
};

//----------------------------------------------------------------------------
class Container
{
   enum CastRayType
   {
      CollisionGeometry,
      RenderedGeometry,
   };

public:
   struct Link
   {
      Link* next;
      Link* prev;
      Link();
      void unlink();
      void linkAfter(Link* ptr);
   };

   struct CallbackInfo 
   {
      AbstractPolyList* polyList;
      Box3F boundingBox;
      SphereF boundingSphere;
      void *key;
   };

   static const U32 csmNumBins;
   static const F32 csmBinSize;
   static const F32 csmTotalBinSize;
   static const U32 csmRefPoolBlockSize;
   static U32    smCurrSeqKey;

private:
   Link mStart,mEnd;

   SceneObjectRef*         mFreeRefPool;
   Vector<SceneObjectRef*> mRefPoolBlocks;

   SceneObjectRef* mBinArray;
   SceneObjectRef  mOverflowBin;

   /// A vector that contains just the water and physical zone
   /// object types which is used to optimize searches.
   Vector<SceneObject*> mWaterAndZones;

public:
   Container();
   ~Container();

   /// @name Basic database operations
   /// @{

   ///
   typedef void (*FindCallback)(SceneObject*,void *key);
   void findObjects(U32 mask, FindCallback, void *key = NULL);
   void findObjects(const Box3F& box, U32 mask, FindCallback, void *key = NULL);
   void findObjects(const Frustum& frustum, U32 mask, FindCallback, void *key = NULL);
   void polyhedronFindObjects(const Polyhedron& polyhedron, U32 mask,FindCallback, void *key = NULL);

   void findObjectList( U32 mask, Vector<SceneObject*> *outFound );

   void findObjectList( const Box3F& box, U32 mask, Vector<SceneObject*> *outFound );

   void findObjectList( const Frustum &frustum, U32 mask, Vector<SceneObject*> *outFound );

   /// @}

   /// @name Line intersection
   /// @{

   /// Test against collision geometry -- fast.
   bool castRay(const Point3F &start, const Point3F &end, U32 mask, RayInfo* info);

   /// Test against rendered geometry -- slow.
   bool castRayRendered(const Point3F &start, const Point3F &end, U32 mask, RayInfo* info);

   /// Base cast ray code
   bool castRayBase(U32 type, const Point3F &start, const Point3F &end, U32 mask, RayInfo* info);

   bool collideBox(const Point3F &start, const Point3F &end, U32 mask, RayInfo* info);
   /// @}

   /// @name Poly list
   /// @{

   bool buildPolyList(const Box3F& box, U32 mask, AbstractPolyList*);   
   bool buildCollisionList(const Box3F& box, const Point3F& start, const Point3F& end, const VectorF& velocity,
      U32 mask,CollisionList* collisionList,FindCallback = 0,void *key = NULL,const Box3F *queryExpansion = 0);
   bool buildCollisionList(const Polyhedron& polyhedron,
                           const Point3F& start, const Point3F& end,
                           const VectorF& velocity,
                           U32 mask, CollisionList* collisionList,
                           FindCallback callback = 0, void *key = NULL);

   /// Test against rendered geometry -- slow.
   bool buildRenderedPolyList(const Box3F& box, U32 mask, AbstractPolyList*);

   /// @}

   ///
   bool addObject(SceneObject*);
   bool removeObject(SceneObject*);

   void addRefPoolBlock();
   SceneObjectRef* allocateObjectRef();
   void freeObjectRef(SceneObjectRef*);
   void insertIntoBins(SceneObject*);
   void removeFromBins(SceneObject*);

   /// checkBins makes sure that we're not just sticking the object right back
   /// where it came from.  The overloaded insertInto is so we don't calculate
   /// the ranges twice.
   void checkBins(SceneObject*);
   void insertIntoBins(SceneObject*, U32, U32, U32, U32);


private:
   Vector<SimObjectPtr<SceneObject>*>  mSearchList;///< Object searches to support console querying of the database.  ONLY WORKS ON SERVER
   S32                                 mCurrSearchPos;
   Point3F                             mSearchReferencePoint;
   void cleanupSearchVectors();

   void _findWaterAndZoneObjects( U32 mask, FindCallback, void *key = NULL );
   void _findWaterAndZoneObjects( const Box3F &box, U32 mask, FindCallback callback, void *key = NULL );   

public:
   void initRadiusSearch(const Point3F& searchPoint,
                         const F32      searchRadius,
                         const U32      searchMask);
   void initTypeSearch(const U32      searchMask);
   U32  containerSearchNext();
   F32  containerSearchCurrDist();
   F32  containerSearchCurrRadiusDist();
};


//----------------------------------------------------------------------------
/// For simple queries.  Simply creates a vector of the objects
class SimpleQueryList
{
  public:
   Vector<SceneObject*> mList;

  public:
   SimpleQueryList() 
   {
      VECTOR_SET_ASSOCIATION(mList);
   }

   void insertObject(SceneObject* obj) { mList.push_back(obj); }
   static void insertionCallback(SceneObject* obj, void *key);
};

class SceneObjectLightingPlugin 
{
public:
   virtual ~SceneObjectLightingPlugin() { }
   /// Reset light plugin to clean state.
   virtual void reset() {}

   // Called by statics
   virtual U32  packUpdate(SceneObject* obj, U32 checkMask, NetConnection *conn, U32 mask, BitStream *stream) = 0;
   virtual void unpackUpdate(SceneObject* obj, NetConnection *conn, BitStream *stream) = 0;     
};

//----------------------------------------------------------------------------

/// A 3D object.
///
/// @section SceneObject_intro Introduction
///
/// SceneObject exists as a foundation for 3D objects in Torque. It provides the
/// basic functionality for:
///      - A scene graph (in the Zones and Portals sections), allowing efficient
///        and robust rendering of the game scene.
///      - Various helper functions, including functions to get bounding information
///        and momentum/velocity.
///      - Collision detection, as well as ray casting.
///      - Lighting. SceneObjects can register lights both at lightmap generation time,
///        and dynamic lights at runtime (for special effects, such as from flame or
///        a projectile, or from an explosion).
///      - Manipulating scene objects, for instance varying scale.
///
/// @section SceneObject_example An Example
///
/// Melv May has written a most marvelous example object deriving from SceneObject.
/// Unfortunately this page is too small to contain it.
///
/// @see http://www.garagegames.com/index.php?sec=mg&mod=resource&page=view&qid=3217
///      for a copy of Melv's example.
class SceneObject : public NetObject, public LightReceiver, public Container::Link
{
   typedef NetObject Parent;
   friend class Container;
   friend class SceneGraph;
   friend class SceneState;

   //-------------------------------------- Public constants
public:
   enum 
   {
      MaxObjectZones = 128
   };
   
   enum TraversalState 
   {
      Pending = 0,
      Working = 1,
      Done = 2
   };
   
   enum SceneObjectMasks 
   {
      ScaleMask    = BIT(0),
      NextFreeMask = BIT(1)
   };

   //-------------------------------------- Public interfaces
   // C'tors and D'tors
private:
   SceneObject(const SceneObject&); ///< @deprecated disallowed

public:
   SceneObject();
   virtual ~SceneObject();

   /// Returns a value representing this object which can be passed to script functions.
   const char* scriptThis();

public:
   /// @name Collision and transform related interface
   ///
   /// The Render Transform is the interpolated transform with respect to the
   /// frame rate. The Render Transform will differ from the object transform
   /// because the simulation is updated in fixed intervals, which controls the
   /// object transform. The framerate is, most likely, higher than this rate,
   /// so that is why the render transform is interpolated and will differ slightly
   /// from the object transform.
   ///
   /// @{

   /// Disables collisions for this object including raycasts
   virtual void disableCollision();

   /// Enables collisions for this object
   virtual void enableCollision();

   /// Returns true if collisions are enabled
   bool isCollisionEnabled() const { return mCollisionCount == 0; }

   /// This gets called when an object collides with this object.
   /// @param   object   Object colliding with this object
   /// @param   vec   Vector along which collision occurred
   virtual void onCollision( SceneObject *object, const VectorF &vec ) {}

   /// Returns true if this object allows itself to be displaced
   /// @see displaceObject
   virtual bool isDisplacable() const;

   /// Returns the momentum of this object
   virtual Point3F getMomentum() const;

   /// Sets the momentum of this object
   /// @param   momentum   Momentum
   virtual void setMomentum(const Point3F &momentum);

   /// Returns the mass of this object
   virtual F32 getMass() const;

   /// Displaces this object by a vector
   /// @param   displaceVector   Displacement vector
   virtual bool displaceObject(const Point3F& displaceVector);

   /// Returns the transform which can be used to convert object space
   /// to world space
   virtual const MatrixF& getTransform() const      { return mObjToWorld; }

   /// Returns the transform which can be used to convert world space
   /// into object space
   const MatrixF& getWorldTransform() const { return mWorldToObj; }

   /// Returns the scale of the object
   const VectorF& getScale() const          { return mObjScale;   }

   /// Returns the bounding box for this object in local coordinates
   const Box3F&   getObjBox() const        { return mObjBox;      }

   /// Returns the bounding box for this object in world coordinates
   const Box3F&   getWorldBox() const      { return mWorldBox;    }

   virtual const Box3F& getZoneBox() const { return mWorldBox;    }

   /// Returns the bounding sphere for this object in world coordinates
   const SphereF& getWorldSphere() const   { return mWorldSphere; }

   /// Returns the center of the bounding box in world coordinates
   Point3F        getBoxCenter() const     { return (mWorldBox.minExtents + mWorldBox.maxExtents) * 0.5f; }

   /// Sets the Object -> World transform
   ///
   /// @param   mat   New transform matrix
   virtual void setTransform(const MatrixF & mat);

   /// Sets the scale for the object
   /// @param   scale   Scaling values
   virtual void setScale(const VectorF & scale);

   /// This sets the render transform for this object
   /// @param   mat   New render transform
   virtual void setRenderTransform(const MatrixF &mat);

   /// Returns the render transform
   const MatrixF& getRenderTransform() const      { return mRenderObjToWorld; }

   /// Returns the render transform to convert world to local coordinates
   const MatrixF& getRenderWorldTransform() const { return mRenderWorldToObj; }

   /// Returns the render world box
   const Box3F& getRenderWorldBox()  const      { return mRenderWorldBox;   }

   /// Builds a convex hull for this object.
   ///
   /// Think of a convex hull as a low-res mesh which covers, as tightly as
   /// possible, the object mesh, and is used as a collision mesh.
   /// @param   box
   /// @param   convex   Convex mesh generated (out)
   virtual void buildConvex(const Box3F& box,Convex* convex);

   /// Builds a list of polygons which intersect a bounding volume.
   ///
   /// This will use either the sphere or the box, not both, the
   /// SceneObject implementation ignores sphere.
   ///
   /// @see AbstractPolyList
   /// @param   polyList   Poly list build (out)
   /// @param   box        Box bounding volume
   /// @param   sphere     Sphere bounding volume
   virtual bool buildPolyList(AbstractPolyList* polyList, const Box3F &box, const SphereF &sphere);

   /// Same as buildPolyList but operates against the rendered geometry.
   /// @see buildPolyList
   virtual bool buildRenderedPolyList(AbstractPolyList* polyList, const Box3F &box, const SphereF &sphere);

   /// Casts a ray and obtain collision information, returns true if RayInfo is modified.
   ///
   /// @param   start   Start point of ray
   /// @param   end   End point of ray
   /// @param   info   Collision information obtained (out)
   virtual bool castRay(const Point3F &start, const Point3F &end, RayInfo* info);

   /// Casts a ray against rendered geometry, returns true if RayInfo is modified.
   ///
   /// @param   start   Start point of ray
   /// @param   end     End point of ray
   /// @param   info    Collision information obtained (out)
   virtual bool castRayRendered(const Point3F &start, const Point3F &end, RayInfo* info);

   virtual bool collideBox(const Point3F &start, const Point3F &end, RayInfo* info);

   /// Returns the position of the object.
   virtual Point3F getPosition() const;

   /// Returns the render-position of the object.
   ///
   /// @see getRenderTransform
   Point3F getRenderPosition() const;

   /// Sets the position of the object
   void setPosition(const Point3F &pos);

   /// Gets the velocity of the object
   virtual Point3F getVelocity() const;

   /// Sets the velocity of the object
   /// @param  v  Velocity
   virtual void setVelocity(const Point3F &v);

   /// Applies an impulse force to this object
   /// @param   pos   Position where impulse came from in world space
   /// @param   vec   Velocity vector (Impulse force F = m * v)
   virtual void applyImpulse(const Point3F& pos,const VectorF& vec) { }

   /// Applies a radial impulse to the object
   /// using the impulse origin and force.
   /// @param origin Point of origin of the radial impulse.
   /// @param radius The radius of the impulse area.
   /// @param magnitude The strength of the impulse.
   virtual void applyRadialImpulse( const Point3F &origin, F32 radius, F32 magnitude );

   /// Returns the distance from this object to a point
   /// @param pnt Point
   virtual F32 distanceTo( const Point3F &pnt ) const;

   /// @}

   /// @name Mounted objects
   /// @{

   /// Mount an object to a mount point
   /// @param   obj   Object to mount
   /// @param   node   Mount node ID
   virtual void mountObject( SceneObject *obj, U32 node ) {}

   /// Remove an object mounting
   /// @param   obj   Object to unmount
   virtual void unmountObject( SceneObject *obj ) {}

   /// Unmount this object from it's mount
   virtual void unmount() {};      

   /// Callback when this object is mounted. This should be overridden to
   /// set maskbits or do other object type specific work.
   /// @param obj Object we are mounting to.
   /// @param node Node we are unmounting from.
   virtual void onMount( SceneObject *obj, S32 node ) {}

   /// Callback when this object is unmounted. This should be overridden to
   /// set maskbits or do other object type specific work.
   /// @param obj Object we are unmounting from.
   /// @param node Node we are unmounting from.
   virtual void onUnmount( SceneObject *obj, S32 node ) {}

   // Returns mount point to world space transform at tick time.
   virtual void getMountTransform( U32 index, MatrixF *mat ) {}

   // Returns mount point to world space transform at render time.
   virtual void getRenderMountTransform( U32 index, MatrixF *mat ) {}

   /// Return the object that this object is mounted to
   virtual SceneObject* getObjectMount() { return mMount.object; }

   /// Return object link of next object mounted to this object's mount
   virtual SceneObject* getMountLink() { return mMount.link; }

   /// Returns object list of objects mounted to this object.
   virtual SceneObject* getMountList() { return mMount.list; }

   /// Returns the mount id that this is mounted to
   virtual U32 getMountNode() { return mMount.node; }

   /// Returns true if this object is mounted to anything at all
   virtual bool isMounted() { return mMount.object != 0; }

   /// Returns the number of object mounted along with this
   virtual S32 getMountedObjectCount();

   /// Returns the object mounted at a position in the mount list
   /// @param   idx   Position on the mount list
   virtual SceneObject* getMountedObject(S32 idx);

   /// Returns the node the object at idx is mounted to
   /// @param   idx   Index
   virtual S32 getMountedObjectNode(S32 idx);

   /// Returns the object a object on the mount list is mounted to
   /// @param   node
   virtual SceneObject* getMountNodeObject(S32 node);

   /// @}

public:
  /// @name Zones
  ///
  /// A zone is a portalized section of an InteriorInstance, and an InteriorInstance can manage more than one zone.
  /// There is always at least one zone in the world, zone 0, which represents the whole world. Any
  /// other zone will be in an InteriorInstance. Torque keeps track of the zones containing an object
  /// as it moves throughout the world. An object can exists in multiple zones at once.
  /// @{

   /// Returns true if this object is managing zones.
   ///
   /// This is only true in the case of InteriorInstances which have zones in them.
   bool isManagingZones() const;

   /// Gets the index of the first zone this object manages in the collection of zones.
   U32  getZoneRangeStart() const                         { return mZoneRangeStart;   }

   /// Gets the number of zones containing this object.
   U32  getNumCurrZones() const                           { return mNumCurrZones;     }

   /// Returns the nth zone containing this object.
   U32  getCurrZone(const U32 index) const;

   /// If an object exists in multiple zones, this method will give you the
   /// number and indices of these zones (storing them in the provided variables).
   ///
   /// @param   obj      Object in question.
   /// @param   zones    Indices of zones containing the object. (out)
   /// @param   numZones Number of elements in the returned array.  (out)
   virtual bool getOverlappingZones(SceneObject* obj, U32* zones, U32* numZones);

   /// Returns the zone containing p.
   ///
   /// @param   p   Point to test.
   virtual U32  getPointZone(const Point3F& p);

   /// This is called on a zone managing object to scope all the zones managed.
   ///
   /// @param   rootPosition   Camera position
   /// @param   rootDistance   Camera visible distance
   /// @param   zoneScopeState Array of booleans which line up with the collection of zones, marked true if that zone is scoped (out)
   virtual bool scopeObject(const Point3F&        rootPosition,
                            const F32             rootDistance,
                            bool*                 zoneScopeState);
   /// @}

   /// Called when the SceneGraph is ready for the registration of RenderImages.
   ///
   /// @see SceneState
   ///
   /// @param   state               SceneState
   /// @param   stateKey            State key of the current SceneState
   /// @param   startZone           Base zone index
   /// @param   modifyBaseZoneState If true, the object needs to modify the zone state.
   virtual bool prepRenderImage( SceneState *state, 
                                 const U32 stateKey, 
                                 const U32 startZone,
                                 const bool modifyBaseZoneState = false);

   /// Adds object to the client or server container depending on the object
   void addToScene();

   /// Removes the object from the client/server container
   void removeFromScene();

   ///
   bool isRenderEnabled() const;

   //-------------------------------------- Derived class interface
   // Overrides
protected:
   bool onAdd();
   void onRemove();

   // Overrideables
protected:
   /// Called when this is added to the SceneGraph.
   ///
   /// @param   graph   SceneGraph this is getting added to
   virtual bool onSceneAdd(SceneGraph *graph);

   /// Called when this is removed from the SceneGraph
   virtual void onSceneRemove();

   /// Called when the size of the object changes
   virtual void onScaleChanged();

   /// Called when the zone list is changed.
   virtual void onRezone() {}

   /// @name Portals
   /// @{

   /// This is used by a portal controlling object to transform the base-modelview
   /// used by the scenegraph for rendering to the modelview it needs to render correctly.
   ///
   /// @see MirrorSubObject
   ///
   /// @param   portalIndex   Index of portal in the list of portals controlled by the object.
   /// @param   oldMV         Current modelview matrix used by the SceneGraph (in)
   /// @param   newMV         New modelview to be used by the SceneGraph (out)
   virtual void transformModelview(const U32 portalIndex, const MatrixF& oldMV, MatrixF* newMV);

   /// Used to transform the position of a point based on a portal.
   ///
   /// @param   portalIndex   Index of a portal to transform by.
   /// @param   point         Point to transform.
   virtual void transformPosition(const U32 portalIndex, Point3F& point);

   /// Returns a new view frustum for the portal.
   ///
   /// @param   portalIndex   Which portal in the list of portals the object controls
   /// @param   oldFrustum    Current frustum.
   /// @param   nearPlane     Near clipping plane.
   /// @param   farPlane      Far clipping plane.
   /// @param   oldViewport   Current viewport.
   /// @param   newFrustum    New view frustum to use. (out)
   /// @param   newViewport   New viewport to use. (out)
   /// @param   flippedMatrix Should the object should use a flipped matrix to calculate viewport and frustum?
   virtual bool computeNewFrustum(const U32      portalIndex,
                                  const Frustum  &oldFrustum,
                                  const F64      nearPlane,
                                  const F64      farPlane,
                                  const RectI&   oldViewport,
                                  F64            *newFrustum,
                                  RectI&         newViewport,
                                  const bool     flippedMatrix);

   /// Called before things are to be rendered from the portals point of view, to set up
   /// everything the portal needs to render correctly.
   ///
   /// @param   portalIndex   Index of portal to use.
   /// @param   pCurrState    Current SceneState
   /// @param   pParentState  SceneState used before this portal was activated
   virtual void openPortal(const U32   portalIndex,
                           SceneState* pCurrState,
                           SceneState* pParentState);

   /// Called after rendering of a portal is complete, this resets the states
   /// the previous call to openPortal() changed.
   ///
   /// @param   portalIndex    Index of portal to use.
   /// @param   pCurrState     Current SceneState
   /// @param   pParentState   SceneState used before this portal was activated
   virtual void closePortal(const U32   portalIndex,
                            SceneState* pCurrState,
                            SceneState* pParentState);
public:

   /// Returns the plane of the portal in world space.
   ///
   /// @param   portalIndex   Index of portal to use.
   /// @param   plane         Plane of the portal in world space (out)
   virtual void getWSPortalPlane(const U32 portalIndex, PlaneF *plane);

   /// @}

protected:
   /// Sets the mLastState and mLastStateKey.
   ///
   /// @param   state   SceneState to set as the last state
   /// @param   key     Key to set as the last state key
   void          setLastState(SceneState *state, U32 key);

   /// Returns true if the provided SceneState and key are set as this object's
   /// last state and key.
   ///
   /// @param   state    SceneState in question
   /// @param   key      State key in question
   bool          isLastState(SceneState *state, U32 key) const;


   /// @name Traversal State
   ///
   /// The SceneGraph traversal is recursive and the traversal state of an object
   /// can be one of three things:
   ///           - Pending - The object has not yet been examined for zone traversal.
   ///           - Working - The object is currently having its zones traversed.
   ///           - Done    - The object has had all of its zones traversed or doesn't manage zones.
   ///
   /// @note These states were formerly referred to as TraverseColor, with White, Black, and
   ///       Gray; this was changed in Torque 1.2 by Pat "KillerBunny" Wilson. This code is
   ///       only used internal to this class
   /// @{

   // These two replaced by TraversalState because that makes more sense -KB
   //void          setTraverseColor(TraverseColor);
   //TraverseColor getTraverseColor() const;
   // ph34r teh muskrat! - Travis Colure

   /// This sets the traversal state of the object.
   ///
   /// @note This is used internally; you should not normally need to call it.
   /// @param   s   Traversal state to assign
   void           setTraversalState( TraversalState s );

   /// Returns the traversal state of this object
   TraversalState getTraversalState() const;

   /// @}

   /// @name Lighting
   /// @{

protected:

   SceneObjectLightingPlugin* mLightPlugin;

public:

   void setLightingPlugin(SceneObjectLightingPlugin* plugin) { mLightPlugin = plugin; }
   SceneObjectLightingPlugin* getLightingPlugin() { return mLightPlugin; }

   /// @}   

public:
   
   // TODO: figure out why ShapeBase cannot access mMount 
   // with this in a protected section...

   /// Mounted objects
   struct MountInfo {
      SceneObject* list;              ///< Objects mounted on this object
      SceneObject* object;            ///< Object this object is mounted on.
      SceneObject* link;              ///< Link to next object mounted to this object's mount
      U32 node;                       ///< Node point we are mounted to.
   } mMount;
protected:

   /// @name Transform and Collision Members
   /// @{

   ///
   Container* mContainer;

   MatrixF mObjToWorld;   ///< Transform from object space to world space
   MatrixF mWorldToObj;   ///< Transform from world space to object space (inverse)
   Point3F mObjScale;     ///< Object scale

   Box3F   mObjBox;       ///< Bounding box in object space
   Box3F   mWorldBox;     ///< Bounding box in world space
   SphereF mWorldSphere;  ///< Bounding sphere in world space

   MatrixF mRenderObjToWorld;    ///< Render matrix to transform object space to world space
   MatrixF mRenderWorldToObj;    ///< Render matrix to transform world space to object space
   Box3F   mRenderWorldBox;      ///< Render bounding box in world space
   SphereF mRenderWorldSphere;   ///< Render bounxing sphere in world space

   /// Regenerates the world-space bounding box and bounding sphere.
   void resetWorldBox();

   /// Regenerates the render-world-space bounding box and sphere.
   void resetRenderWorldBox();

   /// Regenerates the object-space bounding box from the world-space
   /// bounding box, the world space to object space transform, and
   /// the object scale.
   void resetObjectBox();

   SceneObjectRef* mZoneRefHead;
   SceneObjectRef* mBinRefHead;

   U32 mBinMinX;
   U32 mBinMaxX;
   U32 mBinMinY;
   U32 mBinMaxY;

   /// @}

   /// @name Container Interface
   ///
   /// When objects are searched, we go through all the zones and ask them for
   /// all of their objects. Because an object can exist in multiple zones, the
   /// container sequence key is set to the id of the current search. Then, while
   /// searching, we check to see if an object's sequence key is the same as the
   /// current search key. If it is, it will NOT be added to the list of returns
   /// since it has already been processed.
   ///
   /// @{

   U32  mContainerSeqKey;  ///< Container sequence key

   /// Returns the container sequence key
   U32  getContainerSeqKey() const        { return mContainerSeqKey; }

   /// Sets the container sequence key
   void setContainerSeqKey(const U32 key) { mContainerSeqKey = key;  }
   /// @}

public:

   /// Returns a pointer to the container that contains this object
   Container* getContainer()         { return mContainer;       }

protected:
   S32 mCollisionCount;

   bool mGlobalBounds;

public:
   const bool isGlobalBounds() const
   {
      return mGlobalBounds;
   }

   /// If global bounds are set to be true, then the object is assumed to
   /// have an infinitely large bounding box for collision and rendering
   /// purposes.
   ///
   /// They can't be toggled currently.
   void setGlobalBounds()
   { 
      if(mContainer)
         mContainer->removeFromBins(this);

      mGlobalBounds = true;
      mObjBox.minExtents.set(-1e10, -1e10, -1e10);
      mObjBox.maxExtents.set( 1e10,  1e10,  1e10);

      if(mContainer)
         mContainer->insertIntoBins(this);
   }


   /// @name Rendering Members
   /// @{
   
public:

   SceneGraph* getSceneGraph() const { return mSceneManager; }

protected:
   SceneGraph* mSceneManager;      ///< SceneGraph that controls this object
   U32         mZoneRangeStart;    ///< Start of range of zones this object controls, 0xFFFFFFFF == no zones

   U32         mNumCurrZones;     ///< Number of zones this object exists in

   TraversalState mTraversalState;  ///< State of this object in the SceneGraph traversal - DON'T MESS WITH THIS
   SceneState*    mLastState;       ///< Last SceneState that was used to render this object.
   U32            mLastStateKey;    ///< Last state key that was used to render this object.

   /// @}

   /// @name Persist and console
   /// @{
public:
   static void initPersistFields();
   void inspectPostApply();
   DECLARE_CONOBJECT(SceneObject);

   /// @}

   /// Triggered when a scene object onAdd is called.  Allows other systems to plug into SceneObject
   static Signal<void(SceneObject*)> smSceneObjectAdd;

   /// Triggered when a SceneObject onRemove is called.
   static Signal<void(SceneObject*)> smSceneObjectRemove;
   protected:
	   U8              mSelectionFlags;
public:
	enum { 
		SELECTED      = BIT(0), 
		PRE_SELECTED  = BIT(1), 
	};
	void            setSelectionFlags(U8 flags) { mSelectionFlags = flags; }
	U8              getSelectionFlags() const { return mSelectionFlags; }
	bool            needsSelectionHighlighting() const { return (mSelectionFlags != 0); }
};


//--------------------------------------------------------------------------
extern Container gServerContainer;
extern Container gClientContainer;

//--------------------------------------------------------------------------
//-------------------------------------- Inlines
//
inline bool SceneObject::isManagingZones() const
{
   return mZoneRangeStart != 0xFFFFFFFF;
}

inline void SceneObject::setLastState(SceneState* state, U32 key)
{
   mLastState    = state;
   mLastStateKey = key;
}

inline bool SceneObject::isLastState(SceneState* state, U32 key) const
{
   return (mLastState == state && mLastStateKey == key);
}

inline void SceneObject::setTraversalState( TraversalState s ) {
   mTraversalState = s;
}

inline SceneObject::TraversalState SceneObject::getTraversalState() const {
   return mTraversalState;
}

inline U32 SceneObject::getCurrZone(const U32 index) const
{
   // Not the most efficient way to do this, walking the list,
   //  but it's an uncommon call...
   SceneObjectRef* walk = mZoneRefHead;
   for (U32 i = 0; i < index; i++) 
   {
      walk = walk->nextInObj;
      AssertFatal(walk!=NULL, "Error, too few object refs!");
   }
   AssertFatal(walk!=NULL, "Error, too few object refs!");

   return walk->zone;
}

//--------------------------------------------------------------------------
inline SceneObjectRef* Container::allocateObjectRef()
{
   if (mFreeRefPool == NULL) 
   {
      addRefPoolBlock();
   }
   AssertFatal(mFreeRefPool!=NULL, "Error, should always have a free reference here!");

   SceneObjectRef* ret = mFreeRefPool;
   mFreeRefPool = mFreeRefPool->nextInObj;

   ret->nextInObj = NULL;
   return ret;
}

inline void Container::freeObjectRef(SceneObjectRef* trash)
{
   trash->object = NULL;
   trash->nextInBin = NULL;
   trash->prevInBin = NULL;
   trash->nextInObj = mFreeRefPool;
   mFreeRefPool     = trash;
}

inline void Container::findObjectList( U32 mask, Vector<SceneObject*> *outFound )
{
   for ( Link* itr = mStart.next; itr != &mEnd; itr = itr->next )
   {
      SceneObject *ptr = static_cast<SceneObject*>( itr );
      if ( ( ptr->getType() & mask ) != 0 )
         outFound->push_back( ptr );
   }
}

inline void Container::findObjects(U32 mask, FindCallback callback, void *key)
{
   for (Link* itr = mStart.next; itr != &mEnd; itr = itr->next) {
      SceneObject* ptr = static_cast<SceneObject*>(itr);
      if ((ptr->getType() & mask) != 0 && !ptr->mCollisionCount)
         (*callback)(ptr,key);
   }
}

#endif  // _SCENEOBJECT_H_

