//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "T3D/tsStatic.h"

#include "core/resourceManager.h"
#include "core/stream/bitStream.h"
#include "sceneGraph/sceneState.h"
#include "sceneGraph/sceneGraph.h"
#include "lighting/lightManager.h"
#include "math/mathIO.h"
#include "ts/tsShapeInstance.h"
#include "console/consoleTypes.h"
#include "T3D/shapeBase.h"
#include "sim/netConnection.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxTransformSaver.h"
#include "ts/tsRenderState.h"
#include "collision/boxConvex.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physicsStatic.h"
#include "materials/materialDefinition.h"
#include "materials/materialManager.h"
#include "materials/matInstance.h"
#include "materials/materialFeatureData.h"
#include "materials/materialFeatureTypes.h"

IMPLEMENT_CO_NETOBJECT_V1(TSStatic);

//--------------------------------------------------------------------------
TSStatic::TSStatic()
{
   overrideOptions = false;

   mNetFlags.set(Ghostable | ScopeAlways);

   mTypeMask |= StaticObjectType | StaticTSObjectType |
	   StaticRenderedObjectType | ShadowCasterObjectType;

   mShapeName        = "";
   mShapeInstance    = NULL;

   mPlayAmbient      = true;
   mAmbientThread    = NULL;

   mAllowPlayerStep = true;

   mConvexList = new Convex;

   mRenderNormalScalar = 0;

   mPhysicsRep = NULL;

   mCollisionType = CollisionMesh;
}

TSStatic::~TSStatic()
{
   delete mConvexList;
   mConvexList = NULL;
}

//--------------------------------------------------------------------------
static const EnumTable::Enums collisionTypeEnums[] =
{
	{ TSStatic::None,          "None"           },
   { TSStatic::Bounds,        "Bounds"         },
	{ TSStatic::CollisionMesh, "Collision Mesh" },
   { TSStatic::VisibleMesh,   "Visible Mesh"   },
};

static const EnumTable gCollisionTypeTable( 4, &collisionTypeEnums[0] );

void TSStatic::initPersistFields()
{
   addGroup("Media");
   addField("shapeName",   TypeFilename,  Offset(mShapeName,   TSStatic), "Name and path to model file.");
   addField("playAmbient", TypeBool,      Offset(mPlayAmbient, TSStatic), "Play the \"ambient\" animation.");
   endGroup("Media");

   addGroup("Lighting");
   addField("receiveSunLight", TypeBool, Offset(receiveSunLight, SceneObject), "Shape lighting affected by global Sun");
   addField("receiveLMLighting", TypeBool, Offset(receiveLMLighting, SceneObject), "Shape lighting affected by nearby lightmaps");
   //addField("useAdaptiveSelfIllumination", TypeBool, Offset(useAdaptiveSelfIllumination, SceneObject));
   addField("useCustomAmbientLighting", TypeBool, Offset(useCustomAmbientLighting, SceneObject), "Ambient light color (in low/no lighting condition))"
	   "which overrides other sources, such as Sun.");
   //addField("customAmbientSelfIllumination", TypeBool, Offset(customAmbientForSelfIllumination, SceneObject));
   addField("customAmbientLighting", TypeColorF, Offset(customAmbientLighting, SceneObject));
   addField("lightGroupName", TypeRealString, Offset(lightGroupName, SceneObject), "Groups shape in a set with other objects affected by a designated"
	   "light source.");
   endGroup("Lighting");

   addGroup("Collision");
   addField( "collisionType",   TypeEnum, Offset( mCollisionType,   TSStatic ), 1, &gCollisionTypeTable );
   addField( "allowPlayerStep", TypeBool, Offset( mAllowPlayerStep, TSStatic ), "Allow a player to collide with this object.");
   endGroup("Collision");

   addGroup("Debug");
   addField("renderNormals", TypeF32, Offset( mRenderNormalScalar, TSStatic ), "Debug rendering mode which highlights shape normals." );
   endGroup("Debug");

   Parent::initPersistFields();
}

void TSStatic::inspectPostApply()
{
   // Apply any transformations set in the editor
   Parent::inspectPostApply();

   if(isServerObject()) 
   {
      setMaskBits(AdvancedStaticOptionsMask);
      prepCollision();
   }
}

//--------------------------------------------------------------------------
bool TSStatic::onAdd()
{
   PROFILE_SCOPE(TSStatic_onAdd);

   if ( isServerObject() )
   {
      // Handle the old "usePolysoup" field
      SimFieldDictionary* fieldDict = getFieldDictionary();

      if ( fieldDict )
      {
         StringTableEntry slotName = StringTable->insert( "usePolysoup" );

         SimFieldDictionary::Entry * entry = fieldDict->findDynamicField( slotName );

         if ( entry )
         {
            // Was "usePolysoup" set?
            bool usePolysoup = dAtob( entry->value );

            // "usePolysoup" maps to the new VisibleMesh type
            if ( usePolysoup )
               mCollisionType = VisibleMesh;

            // Remove the field in favor on the new "collisionType" field
            fieldDict->setFieldValue( slotName, "" );
         }
      }
   }

   if ( !Parent::onAdd() )
      return false;

   // Setup the shape.
   if ( !_createShape() )
   {
      Con::errorf( "TSStatic::onAdd() - Shape creation failed!" );
      return false;
   }

   setRenderTransform(mObjToWorld);

   // Register for the resource change signal.
   ResourceManager::get().getChangedSignal().notify( this, &TSStatic::_onResourceChanged );

   addToScene();

   return true;
}

bool TSStatic::_createShape()
{
   // Cleanup before we create.
   mCollisionDetails.clear();
   mLOSDetails.clear();
   SAFE_DELETE( mPhysicsRep );
   SAFE_DELETE( mShapeInstance );
   mShape = NULL;

   if (!mShapeName || mShapeName[0] == '\0') 
   {
      Con::errorf( "TSStatic::_createShape() - No shape name!" );
      return false;
   }

   mShapeHash = _StringTable::hashString(mShapeName);

   mShape = ResourceManager::get().load(mShapeName);
   if ( bool(mShape) == false )
   {
      Con::errorf( "TSStatic::_createShape() - Unable to load shape: %s", mShapeName );
      return false;
   }

   if (  isClientObject() && 
         !mShape->preloadMaterialList(mShape.getPath()) && 
         NetConnection::filesWereDownloaded() )
      return false;

   mObjBox = mShape->bounds;
   resetWorldBox();

   mShapeInstance = new TSShapeInstance( mShape, isClientObject() );

   prepCollision();

   // Find the "ambient" animation if it exists
   S32 ambientSeq = mShape->findSequence("ambient");

   if ( ambientSeq > -1 && !mAmbientThread )
      mAmbientThread = mShapeInstance->addThread();

   if ( mAmbientThread )
      mShapeInstance->setSequence( mAmbientThread, ambientSeq, 0);

   return true;
}

void TSStatic::prepCollision()
{
   // Let the client know that the collision was updated
   setMaskBits( UpdateCollisionMask );

   // Allow the ShapeInstance to prep its collision if it hasn't already
   if ( mShapeInstance )
      mShapeInstance->prepCollision();

   // Cleanup any old collision data
   mCollisionDetails.clear();
   mLOSDetails.clear();

   if ( mPhysicsRep )
      SAFE_DELETE( mPhysicsRep );

   // Any detail or mesh that starts with these names is considered
   // to be a "collision" mesh ("LOS" allows for specific line of sight meshes)
   static const String sCollisionStr( "Collision" );
   static const String sLOSStr( "LOS" );

   if ( mCollisionType == None || mCollisionType == Bounds )
      mConvexList->nukeList();
   else if ( mCollisionType == CollisionMesh )
   {
      // Scan out the collision hulls...
      for (U32 i = 0; i < mShape->details.size(); i++)
      {
         const String &name = mShape->names[mShape->details[i].nameIndex];

         if (name.compare( sCollisionStr, sCollisionStr.length(), String::NoCase ) == 0)
         {
            mCollisionDetails.push_back(i);

            // The way LOS works is that it will check to see if there is a LOS detail that matches
            // the the collision detail + 1 + MaxCollisionShapes (this variable name should change in
            // the future). If it can't find a matching LOS it will simply use the collision instead.
            // We check for any "unmatched" LOS's further down
            mLOSDetails.increment();

            S32 number = 0;
            String::GetTrailingNumber( name, number );

            String   buff = String::ToString("LOS-%d", mAbs(number) + 1 + LOSOverrideOffset);
            S32 los = mShape->findDetail(buff);
            if (los == -1)
               mLOSDetails.last() = i;
            else
               mLOSDetails.last() = los;
         }
      }

      // Snag any "unmatched" LOS details
      for (U32 i = 0; i < mShape->details.size(); i++)
      {
         const String &name = mShape->names[mShape->details[i].nameIndex];

         if (name.compare( sLOSStr, sLOSStr.length(), String::NoCase ) == 0)
         {
            // See if we already have this LOS
            bool found = false;
            for (U32 j = 0; j < mLOSDetails.size(); j++)
            {
               if (mLOSDetails[j] == i)
               {
                  found = true;
                  break;
               }
            }

            if (!found)
               mLOSDetails.push_back(i);
         }
      }

      // Since it looks odd to continue to collide against a mesh with
      // no collision details under the current type be sure to nuke it
      if ( mCollisionDetails.size() == 0 )
         mConvexList->nukeList();
   }
   else  // VisibleMesh
   {
      // With the VisbileMesh we do our collision against the highest LOD
      // visible mesh
      if ( mShape->details.size() > 0 )
      {
         U32 highestDetail = 0;
         F32 highestSize = mShape->details[0].size;

         for ( U32 i = 1; i < mShape->details.size(); i++ )
         {
            // Make sure we skip any details that shouldn't be rendered
            if ( mShape->details[i].size < 0 )
               continue;

            // Also make sure we skip any collision details with a size
            const String &name = mShape->names[mShape->details[i].nameIndex];

            if ( name.compare( sCollisionStr, sCollisionStr.length(), String::NoCase ) == 0 )
               continue;

            if ( name.compare( sLOSStr, sLOSStr.length(), String::NoCase ) == 0 )
               continue;

            // Otherwise test against the current highest size
            if ( mShape->details[i].size > highestSize )
            {
               highestDetail = i;
               highestSize = mShape->details[i].size;
            }
         }

         mCollisionDetails.push_back( highestDetail );
         mLOSDetails.push_back( highestDetail );
      }

      // Since it looks odd to continue to collide against a mesh with
      // no collision details under the current type be sure to nuke it
      if ( mCollisionDetails.size() == 0 )
         mConvexList->nukeList();
   }

   if ( gPhysicsPlugin )
      mPhysicsRep = gPhysicsPlugin->createStatic( this );
}

void TSStatic::onRemove()
{
   SAFE_DELETE( mPhysicsRep );

   mConvexList->nukeList();

   removeFromScene();

   // Remove the resource change signal.
   ResourceManager::get().getChangedSignal().remove( this, &TSStatic::_onResourceChanged );

   delete mShapeInstance;
   mShapeInstance = NULL;

   mAmbientThread = NULL;

   Parent::onRemove();
}

void TSStatic::_onResourceChanged( ResourceBase::Signature sig, const Torque::Path &path )
{
   if (  sig != Resource<TSShape>::signature() ||
         path != Path( mShapeName ) )
      return;
   
   _createShape();
}

void TSStatic::interpolateTick( F32 delta )
{
}

void TSStatic::processTick()
{
   if ( mPlayAmbient && mAmbientThread && isServerObject() )
      mShapeInstance->advanceTime( getTickSec(), mAmbientThread );
}

void TSStatic::advanceTime( F32 timeDelta )
{
   if ( mPlayAmbient && mAmbientThread )
      mShapeInstance->advanceTime( timeDelta, mAmbientThread );
}

//--------------------------------------------------------------------------
bool TSStatic::prepRenderImage(SceneState* state, const U32 stateKey,
                                       const U32 /*startZone*/, const bool /*modifyBaseState*/)
{
   if (isLastState(state, stateKey))
      return false;

   setLastState(state, stateKey);

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...

   if (  !mShapeInstance || 
         ( !state->isObjectRendered(this) && !state->isReflectPass() ) )
         return false;

   Point3F cameraOffset;
   getRenderTransform().getColumn(3,&cameraOffset);
   cameraOffset -= state->getDiffuseCameraPosition();
   F32 dist = cameraOffset.len();
   if (dist < 0.01f)
      dist = 0.01f;

   F32 invScale = (1.0f/getMax(getMax(mObjScale.x,mObjScale.y),mObjScale.z));

   mShapeInstance->setDetailFromDistance( state, dist * invScale );
   if ( mShapeInstance->getCurrentDetail() < 0 )
      return false;

   GFXTransformSaver saver;
   
   // Set up our TS render state.
   TSRenderState rdata;
   rdata.setSceneState( state );
   rdata.setFadeOverride( 1.0f );

   LightManager *lm = gClientSceneGraph->getLightManager();
   if ( !state->isShadowPass() )
      lm->setupLights( this, getWorldSphere() );

   MatrixF mat = getRenderTransform();
   mat.scale( mObjScale );
   GFX->setWorldMatrix( mat );

   mShapeInstance->animate();
   mShapeInstance->render( rdata );

   lm->resetLights();

   if ( mRenderNormalScalar > 0 )
   {
      ObjectRenderInst *ri = state->getRenderPass()->allocInst<ObjectRenderInst>();
      ri->renderDelegate.bind( this, &TSStatic::_renderNormals );
      ri->type = RenderPassManager::RIT_Object;
      state->getRenderPass()->addInst( ri );
   }

   return false;
}

void TSStatic::_renderNormals( ObjectRenderInst *ri, SceneState *state, BaseMatInstance *overrideMat )
{
   PROFILE_SCOPE( TSStatic_RenderNormals );

   GFXTransformSaver saver;

   MatrixF mat = getRenderTransform();
   mat.scale( mObjScale );
   GFX->multWorld( mat );

   S32 dl = mShapeInstance->getCurrentDetail();
   mShapeInstance->renderDebugNormals( mRenderNormalScalar, dl );
}

void TSStatic::setScale( const VectorF &scale )
{
   Parent::setScale( scale );

   if ( mPhysicsRep )
      mPhysicsRep->setScale( scale );
}

void TSStatic::setTransform(const MatrixF & mat)
{
   Parent::setTransform(mat);

   if ( mPhysicsRep )
      mPhysicsRep->setTransform( mat );

   // Since this is a static it's render transform changes 1
   // to 1 with it's collision transform... no interpolation.
   setRenderTransform(mat);
}

U32 TSStatic::packUpdate(NetConnection *con, U32 mask, BitStream *stream)
{
   U32 retMask = Parent::packUpdate(con, mask, stream);

   mathWrite(*stream, getTransform());
   mathWrite(*stream, getScale());
   stream->writeString(mShapeName);

   if ( stream->writeFlag( mask & UpdateCollisionMask ) )
      stream->write( (U32)mCollisionType );

   stream->writeFlag(mAllowPlayerStep);

   stream->write( mRenderNormalScalar );

   stream->writeFlag( mPlayAmbient );

   if (mLightPlugin)
   {
      retMask |= mLightPlugin->packUpdate(this, AdvancedStaticOptionsMask, con, mask, stream);
   }

   return retMask;
}

void TSStatic::unpackUpdate(NetConnection *con, BitStream *stream)
{
   Parent::unpackUpdate(con, stream);

   MatrixF mat;
   Point3F scale;
   mathRead(*stream, &mat);
   mathRead(*stream, &scale);
   setScale(scale);
   setTransform(mat);

   mShapeName = stream->readSTString();

   if ( stream->readFlag() ) // UpdateCollisionMask
   {
      U32 collisionType = CollisionMesh;

      stream->read( &collisionType );

      // Handle it if we have changed CollisionType's
      if ( (CollisionType)collisionType != mCollisionType )
      {
         mCollisionType = (CollisionType)collisionType;

         if ( isProperlyAdded() && mShapeInstance )
            prepCollision();
      }
   }

   mAllowPlayerStep = stream->readFlag();

   stream->read( &mRenderNormalScalar );

   mPlayAmbient = stream->readFlag();

   if (mLightPlugin)
   {
      mLightPlugin->unpackUpdate(this, con, stream);
   }
}

//----------------------------------------------------------------------------
bool TSStatic::castRay(const Point3F &start, const Point3F &end, RayInfo* info)
{
   if ( mCollisionType == None )
      return false;

   if ( !mShapeInstance )
      return false;

   if ( mCollisionType == Bounds )
   {
      F32 st, et, fst = 0.0f, fet = 1.0f;
      F32 *bmin = &mObjBox.minExtents.x;
      F32 *bmax = &mObjBox.maxExtents.x;
      F32 const *si = &start.x;
      F32 const *ei = &end.x;

      for ( U32 i = 0; i < 3; i++ )
      {
         if (*si < *ei) 
         {
            if ( *si > *bmax || *ei < *bmin )
               return false;
            F32 di = *ei - *si;
            st = ( *si < *bmin ) ? ( *bmin - *si ) / di : 0.0f;
            et = ( *ei > *bmax ) ? ( *bmax - *si ) / di : 1.0f;
         }
         else 
         {
            if ( *ei > *bmax || *si < *bmin )
               return false;
            F32 di = *ei - *si;
            st = ( *si > *bmax ) ? ( *bmax - *si ) / di : 0.0f;
            et = ( *ei < *bmin ) ? ( *bmin - *si ) / di : 1.0f;
         }
         if ( st > fst ) fst = st;
         if ( et < fet ) fet = et;
         if ( fet < fst )
            return false;
         bmin++; bmax++;
         si++; ei++;
      }

      info->normal = start - end;
      info->normal.normalizeSafe();
      getTransform().mulV( info->normal );

      info->t = fst;
      info->object = this;
      info->point.interpolate( start, end, fst );
      info->material = NULL;
      return true;
   }
   else
   {
      RayInfo shortest = *info;
      RayInfo localInfo;
      shortest.t = 1e8f;

      for ( U32 i = 0; i < mLOSDetails.size(); i++ )
      {
         mShapeInstance->animate( mLOSDetails[i] );

         if ( mShapeInstance->castRayOpcode( mLOSDetails[i], start, end, &localInfo ) )
         {
            localInfo.object = this;

            if (localInfo.t < shortest.t)
               shortest = localInfo;
         }
      }

      if (shortest.object == this)
      {
         // Copy out the shortest time...
         *info = shortest;
         return true;
      }
   }

   return false;
}

bool TSStatic::castRayRendered(const Point3F &start, const Point3F &end, RayInfo *info)
{
   if ( !mShapeInstance )
      return false;

   // Cast the ray against the currently visible detail
   RayInfo localInfo;
   bool res = mShapeInstance->castRayOpcode( mShapeInstance->getCurrentDetail(), start, end, &localInfo );

   if ( res )
   {
      *info = localInfo;
      info->object = this;
      return true;
   }

   return false;
}


//----------------------------------------------------------------------------
bool TSStatic::buildPolyList(AbstractPolyList* polyList, const Box3F &box, const SphereF &)
{
   if ( mCollisionType == None )
      return false;

   if ( !mShapeInstance )
      return false;

   polyList->setTransform( &mObjToWorld, mObjScale );
   polyList->setObject( this );

   if ( mCollisionType == Bounds )
   {
      polyList->setObject( this );
      polyList->addBox( mObjBox );
   }
   else  // CollisionMesh || VisibleMesh
   {
      for (U32 i = 0; i < mCollisionDetails.size(); i++)
         mShapeInstance->buildPolyListOpcode( mCollisionDetails[i], polyList, box );
   }

   return true;
}

bool TSStatic::buildRenderedPolyList(AbstractPolyList* polyList, const Box3F &box, const SphereF &)
{
   if ( mShapeInstance )
   {
      polyList->setTransform( &mObjToWorld, mObjScale );
      polyList->setObject(this);

      // Add the currently rendered detail to the polyList
      mShapeInstance->buildPolyListOpcode( mShapeInstance->getCurrentDetail(), polyList, box );

      return true;
   }

   return false;
}

void TSStatic::buildConvex(const Box3F& box, Convex* convex)
{
   if ( mCollisionType == None )
      return;

   if ( mShapeInstance == NULL )
      return;

   // These should really come out of a pool
   mConvexList->collectGarbage();

   if ( mCollisionType == Bounds )
   {
      // Just return a box convex for the entire shape...
      Convex* cc = 0;
      CollisionWorkingList& wl = convex->getWorkingList();
      for (CollisionWorkingList* itr = wl.wLink.mNext; itr != &wl; itr = itr->wLink.mNext)
      {
         if (itr->mConvex->getType() == BoxConvexType &&
             itr->mConvex->getObject() == this)
         {
            cc = itr->mConvex;
            break;
         }
      }
      if (cc)
         return;

      // Create a new convex.
      BoxConvex* cp = new BoxConvex;
      mConvexList->registerObject(cp);
      convex->addToWorkingList(cp);
      cp->init(this);

      mObjBox.getCenter(&cp->mCenter);
      cp->mSize.x = mObjBox.len_x() / 2.0f;
      cp->mSize.y = mObjBox.len_y() / 2.0f;
      cp->mSize.z = mObjBox.len_z() / 2.0f;
   }
   else  // CollisionMesh || VisibleMesh
   {
      TSStaticPolysoupConvex::smCurObject = this;

      for (U32 i = 0; i < mCollisionDetails.size(); i++)
         mShapeInstance->buildConvexOpcode( mObjToWorld, mObjScale, mCollisionDetails[i], box, convex, mConvexList );

      TSStaticPolysoupConvex::smCurObject = NULL;
   }
}

//--------------------------------------------------------------------------
SceneObject* TSStaticPolysoupConvex::smCurObject = NULL;

TSStaticPolysoupConvex::TSStaticPolysoupConvex()
:  box( 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f ),
   normal( 0.0f, 0.0f, 0.0f, 0.0f ),
   idx( 0 ),
   mesh( NULL )
{
   mType = TSPolysoupConvexType;

   for ( U32 i = 0; i < 4; ++i )
   {
      verts[i].set( 0.0f, 0.0f, 0.0f );
   }
}

Point3F TSStaticPolysoupConvex::support(const VectorF& vec) const
{
   F32 bestDot = mDot( verts[0], vec );

   const Point3F *bestP = &verts[0];
   for(S32 i=1; i<4; i++)
   {
      F32 newD = mDot(verts[i], vec);
      if(newD > bestDot)
      {
         bestDot = newD;
         bestP = &verts[i];
      }
   }

   return *bestP;
}

Box3F TSStaticPolysoupConvex::getBoundingBox() const
{
   Box3F wbox = box;
   wbox.minExtents.convolve( mObject->getScale() );
   wbox.maxExtents.convolve( mObject->getScale() );
   mObject->getTransform().mul(wbox);
   return wbox;
}

Box3F TSStaticPolysoupConvex::getBoundingBox(const MatrixF& mat, const Point3F& scale) const
{
   AssertISV(false, "TSStaticPolysoupConvex::getBoundingBox(m,p) - Not implemented. -- XEA");
   return box;
}

void TSStaticPolysoupConvex::getPolyList(AbstractPolyList *list)
{
   // Transform the list into object space and set the pointer to the object
   MatrixF i( mObject->getTransform() );
   Point3F iS( mObject->getScale() );
   list->setTransform(&i, iS);
   list->setObject(mObject);

   // Add only the original collision triangle
   S32 base =  list->addPoint(verts[0]);
               list->addPoint(verts[2]);
               list->addPoint(verts[1]);

   list->begin(0, (U32)idx ^ (U32)mesh);
   list->vertex(base + 2);
   list->vertex(base + 1);
   list->vertex(base + 0);
   list->plane(base + 0, base + 1, base + 2);
   list->end();
}

void TSStaticPolysoupConvex::getFeatures(const MatrixF& mat,const VectorF& n, ConvexFeature* cf)
{
   cf->material = 0;
   cf->object = mObject;

   // For a tetrahedron this is pretty easy... first
   // convert everything into world space.
   Point3F tverts[4];
   mat.mulP(verts[0], &tverts[0]);
   mat.mulP(verts[1], &tverts[1]);
   mat.mulP(verts[2], &tverts[2]);
   mat.mulP(verts[3], &tverts[3]);

   // points...
   S32 firstVert = cf->mVertexList.size();
   cf->mVertexList.increment(); cf->mVertexList.last() = tverts[0];
   cf->mVertexList.increment(); cf->mVertexList.last() = tverts[1];
   cf->mVertexList.increment(); cf->mVertexList.last() = tverts[2];
   cf->mVertexList.increment(); cf->mVertexList.last() = tverts[3];

   //    edges...
   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+0;
   cf->mEdgeList.last().vertex[1] = firstVert+1;

   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+1;
   cf->mEdgeList.last().vertex[1] = firstVert+2;

   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+2;
   cf->mEdgeList.last().vertex[1] = firstVert+0;

   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+3;
   cf->mEdgeList.last().vertex[1] = firstVert+0;

   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+3;
   cf->mEdgeList.last().vertex[1] = firstVert+1;

   cf->mEdgeList.increment();
   cf->mEdgeList.last().vertex[0] = firstVert+3;
   cf->mEdgeList.last().vertex[1] = firstVert+2;

   //    triangles...
   cf->mFaceList.increment();
   cf->mFaceList.last().normal = PlaneF(tverts[2], tverts[1], tverts[0]);
   cf->mFaceList.last().vertex[0] = firstVert+2;
   cf->mFaceList.last().vertex[1] = firstVert+1;
   cf->mFaceList.last().vertex[2] = firstVert+0;

   cf->mFaceList.increment();
   cf->mFaceList.last().normal = PlaneF(tverts[1], tverts[0], tverts[3]);
   cf->mFaceList.last().vertex[0] = firstVert+1;
   cf->mFaceList.last().vertex[1] = firstVert+0;
   cf->mFaceList.last().vertex[2] = firstVert+3;

   cf->mFaceList.increment();
   cf->mFaceList.last().normal = PlaneF(tverts[2], tverts[1], tverts[3]);
   cf->mFaceList.last().vertex[0] = firstVert+2;
   cf->mFaceList.last().vertex[1] = firstVert+1;
   cf->mFaceList.last().vertex[2] = firstVert+3;

   cf->mFaceList.increment();
   cf->mFaceList.last().normal = PlaneF(tverts[0], tverts[2], tverts[3]);
   cf->mFaceList.last().vertex[0] = firstVert+0;
   cf->mFaceList.last().vertex[1] = firstVert+2;
   cf->mFaceList.last().vertex[2] = firstVert+3;

   // All done!
}

//------------------------------------------------------------------------
//These functions are duplicated in tsStatic, shapeBase, and interiorInstance.
//They each function a little differently; but achieve the same purpose of gathering
//target names/counts without polluting simObject.

ConsoleMethod( TSStatic, getTargetName, const char*, 3, 3, "")
{
	S32 idx = dAtoi(argv[2]);

	TSStatic *obj = dynamic_cast< TSStatic* > ( object );
	if(obj)
		return obj->getShape()->getTargetName(idx);

	return "";
}

ConsoleMethod( TSStatic, getTargetCount, S32, 2, 2, "")
{
	TSStatic *obj = dynamic_cast< TSStatic* > ( object );
	if(obj)
		return obj->getShape()->getTargetCount();

	return -1;
}

// This method is able to change materials per map to with others. The material that is being replaced is being mapped to
// unmapped_mat as a part of this transition
ConsoleMethod( TSStatic, changeMaterial, void, 5, 5, "(mapTo, fromMaterial, ToMaterial)")
{
	TSStatic *obj = dynamic_cast< TSStatic* > ( object );

	if(obj)
	{
		// Lets get ready to switch out materials
		Material *oldMat = dynamic_cast<Material*>(Sim::findObject(argv[3]));
		Material *newMat = dynamic_cast<Material*>(Sim::findObject(argv[4]));

		// if no valid new material, theres no reason for doing this
		if( !newMat )
			return;
		
		// Lets remap the old material off, so as to let room for our current material room to claim its spot
		if( oldMat )
			oldMat->mMapTo = String("unmapped_mat");

		newMat->mMapTo = argv[2];
		
		// Map the material in the in the matmgr
		MATMGR->mapMaterial( argv[2], argv[4] );
		
		U32 i = 0;
		// Replace instances with the new material being traded in. Lets make sure that we only
		// target the specific targets per inst, this is actually doing more than we thought
		for (; i < obj->getShape()->materialList->getMaterialNameList().size(); i++)
		{
			if( String(argv[2]) == obj->getShape()->materialList->getMaterialName(i))
			{
				delete [] obj->getShape()->materialList->mMatInstList[i];
				obj->getShape()->materialList->mMatInstList[i] = newMat->createMatInstance();
				break;
			}
		}

		// Finish up preparing the material instances for rendering
		const GFXVertexFormat *flags = getGFXVertexFormat<GFXVertexPNTTB>();
		FeatureSet features = MATMGR->getDefaultFeatures();
		obj->getShape()->materialList->getMaterialInst(i)->init( features, flags );
	}
}

ConsoleMethod( TSStatic, getModelFile, const char *, 2, 2, "getModelFile( String )")
{
	return object->getShapeFileName();
}