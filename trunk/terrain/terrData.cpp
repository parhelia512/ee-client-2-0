//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "terrain/terrData.h"

#include "terrain/terrCollision.h"
#include "terrain/terrCell.h"
#include "terrain/terrRender.h"
#include "terrain/terrMaterial.h"
#include "terrain/terrCellMaterial.h"

#include "gui/worldEditor/terrainEditor.h"

#include "math/mathIO.h"
#include "core/stream/fileStream.h"
#include "core/stream/bitStream.h"
#include "console/consoleTypes.h"
#include "sim/netConnection.h"
#include "core/util/safeDelete.h"
#include "T3D/objectTypes.h"
#include "renderInstance/renderPassManager.h"
#include "sceneGraph/sceneState.h"
#include "materials/materialManager.h"
#include "gfx/gfxTextureManager.h"
#include "core/resourceManager.h"
#include "T3D/physics/physicsPlugin.h"
#include "T3D/physics/physicsStatic.h"


using namespace Torque;

IMPLEMENT_CO_NETOBJECT_V1(TerrainBlock);


Signal<void(U32,TerrainBlock*,const Point2I& ,const Point2I&)> TerrainBlock::smUpdateSignal;

F32 TerrainBlock::smLODScale = 1.0f;
F32 TerrainBlock::smDetailScale = 1.0f;


//RBP - Global function declared in Terrdata.h
TerrainBlock* getTerrainUnderWorldPoint(const Point3F & wPos)
{
	// Cast a ray straight down from the world position and see which
	// Terrain is the closest to our starting point
	Point3F startPnt = wPos;
	Point3F endPnt = wPos + Point3F(0.0f, 0.0f, -10000.0f);

	S32 blockIndex = -1;
	F32 nearT = 1.0f;

	SimpleQueryList queryList;
	gServerContainer.findObjects( TerrainObjectType, SimpleQueryList::insertionCallback, &queryList);

	for (U32 i = 0; i < queryList.mList.size(); i++)
	{
		Point3F tStartPnt, tEndPnt;
		TerrainBlock* terrBlock = dynamic_cast<TerrainBlock*>(queryList.mList[i]);
		terrBlock->getWorldTransform().mulP(startPnt, &tStartPnt);
		terrBlock->getWorldTransform().mulP(endPnt, &tEndPnt);

		RayInfo ri;
		if (terrBlock->castRayI(tStartPnt, tEndPnt, &ri, true))
		{
			if (ri.t < nearT)
			{
				blockIndex = i;
				nearT = ri.t;
			}
		}
	}

	if (blockIndex > -1)
		return (TerrainBlock*)(queryList.mList[blockIndex]);

	return NULL;
}

ConsoleFunction(getTerrainUnderWorldPoint, S32, 2, 4, "(Point3F x/y/z) Gets the terrain block that is located under the given world point.\n"
                                                      "@param x/y/z The world coordinates (floating point values) you wish to query at. " 
                                                      "These can be formatted as either a string (\"x y z\") or separately as (x, y, z)\n"
                                                      "@return Returns the ID of the requested terrain block (0 if not found).\n\n")
{
   Point3F pos;
   if(argc == 2)
      dSscanf(argv[1], "%f %f %f", &pos.x, &pos.y, &pos.z);
   else if(argc == 4)
   {
      pos.x = dAtof(argv[1]);
      pos.y = dAtof(argv[2]);
      pos.z = dAtof(argv[3]);
   }

   else
   {
      Con::errorf("getTerrainUnderWorldPoint(Point3F): Invalid argument count! Valid arguments are either \"x y z\" or x,y,z\n");
      return 0;
   }

   TerrainBlock* terrain = getTerrainUnderWorldPoint(pos);
   if(terrain != NULL)
   {
      return terrain->getId();
   }

   return 0;

}


TerrainBlock::TerrainBlock()
 : mSquareSize( 1.0f ),
   mScreenError( 16 ),   
   mDetailsDirty( false ),
   mLayerTexDirty( false ),
   mLightMap( NULL ),
   mLightMapSize( 256 ),
   mTile( false ),
   mMaxDetailDistance( 0.0f ),
   mCell( NULL ),
   mCRC( 0 ),
   mHasRendered( false ),
   mBaseTexSize( 1024 ),
   mBaseMaterial( NULL ),
   mDefaultMatInst( NULL ),
   mBaseTexScaleConst( NULL ),
   mBaseTexIdConst( NULL ),
   mPhysicsRep( NULL )  
{
   mTypeMask = TerrainObjectType | StaticObjectType | StaticRenderedObjectType | ShadowCasterObjectType;
   mNetFlags.set(Ghostable | ScopeAlways);
}


extern Convex sTerrainConvexList;

TerrainBlock::~TerrainBlock()
{
   // Kill collision
   sTerrainConvexList.nukeList();

   // Delete opacity sources.  This will also delete the opacity
   // maps held by the sources.

   // CLIPMAP_REMOVAL
   /*
   for( U32 i = 0; i < mOpacitySources.size(); ++ i )
      SAFE_DELETE( mOpacitySources[ i ] );
   mOpacitySources.clear();
   mOpacityMaps.clear();
   */

   SAFE_DELETE(mLightMap);
   mLightMapTex = NULL;

#ifdef TORQUE_TOOLS
   TerrainEditor* editor = dynamic_cast<TerrainEditor*>(Sim::findObject("ETerrainEditor"));
   if (editor)
      editor->detachTerrain(this);
#endif
}

void TerrainBlock::_onTextureEvent( GFXTexCallbackCode code )
{
   if ( code == GFXZombify )
   {
      if (  mBaseTex.isValid() &&
            mBaseTex->isRenderTarget() )
         mBaseTex = NULL;

      mLightMapTex = NULL;
   }
}

bool TerrainBlock::_setSquareSize( void *object, const char *data )
{
   TerrainBlock *terrain = static_cast<TerrainBlock*>( object );

   F32 newSqaureSize = dAtof( data );
   if ( !mIsEqual( terrain->mSquareSize, newSqaureSize ) )
   {
      terrain->mSquareSize = newSqaureSize;

      if ( terrain->isServerObject() && terrain->isProperlyAdded() )
         terrain->_updateBounds();

      terrain->setMaskBits( HeightMapChangeMask | SizeMask );
   }

   return false;
}

bool TerrainBlock::_setBaseTexSize( void *object, const char *data )
{
   TerrainBlock *terrain = static_cast<TerrainBlock*>( object );

   // NOTE: We're limiting the base texture size to 
   // 2048 as anything greater in size becomes too
   // large to generate for many cards.
   //
   // If you want to remove this limit feel free, but
   // prepare for problems if you don't ship the baked
   // base texture with your installer.
   //

   S32 texSize = mClamp( dAtoi( data ), 0, 2048 );
   if ( terrain->mBaseTexSize != texSize )
   {
      terrain->mBaseTexSize = texSize;
      terrain->setMaskBits( MaterialMask );
   }

   return false;
}

void TerrainBlock::setFile( const FileName &terrFileName )
{
   if ( terrFileName == mTerrFileName )
      return;

   Resource<TerrainFile> file = ResourceManager::get().load( terrFileName );
   setFile( file );

   setMaskBits( FileMask | HeightMapChangeMask );
}

void TerrainBlock::setFile( Resource<TerrainFile> terr )
{
   mFile = terr;
   mTerrFileName = terr.getPath();
}

bool TerrainBlock::save(const char *filename)
{
   return mFile->save(filename);
}

bool TerrainBlock::_setTerrainFile( void *object, const char *data )
{
   static_cast<TerrainBlock*>( object )->setFile( FileName( data ) );
   return false;
}

void TerrainBlock::_updateBounds()
{
   if ( !mFile )
      return; // quick fix to stop crashing when deleting terrainblocks

   // Setup our object space bounds.
   mBounds.minExtents.set( 0.0f, 0.0f, 0.0f );
   mBounds.maxExtents.set( getWorldBlockSize(), getWorldBlockSize(), 0.0f );
   getMinMaxHeight( &mBounds.minExtents.z, &mBounds.maxExtents.z );

   // If we aren't tiling go ahead and set our mObjBox to be equal to mBounds
   // This will get overridden with global bounds if tiling is on but it is useful to store
   if ( !mTile )
   {
      if (  mObjBox.maxExtents != mBounds.maxExtents || 
            mObjBox.minExtents != mBounds.minExtents )
      {
         mObjBox = mBounds;
         resetWorldBox();
      }
   }
}

void TerrainBlock::setHeight( const Point2I &pos, F32 height )
{
   U16 ht = floatToFixed( height );
   mFile->setHeight( pos.x, pos.y, ht );

   // Note: We do not update the grid here as this could
   // be called several times in a loop.  We depend on the
   // caller doing a grid update when he is done.
}

void TerrainBlock::updateGridMaterials( const Point2I &minPt, const Point2I &maxPt )
{
   if ( mCell )
   {
      // Tell the terrain cell that something changed.
      const RectI gridRect( minPt, maxPt - minPt );
      mCell->updateGrid( gridRect, true );
   }

   // We mark us as dirty... it will be updated
   // before the next time we render the terrain.
   mLayerTexDirty = true;

   // Signal anyone that cares that the opacity was changed.
   smUpdateSignal.trigger( LayersUpdate, this, minPt, maxPt );
}


Point2I TerrainBlock::getGridPos( const Point3F &worldPos ) const
{
   Point3F terrainPos = worldPos;
   getWorldTransform().mulP( terrainPos );

   F32 squareSize = ( F32 ) getSquareSize();
   F32 halfSquareSize = squareSize / 2.0;

   F32 x = ( terrainPos.x + halfSquareSize ) / squareSize;
   F32 y = ( terrainPos.y + halfSquareSize ) / squareSize;

   Point2I gridPos( ( S32 ) mFloor( x ), ( S32 ) mFloor( y ) );
   return gridPos;
}

void TerrainBlock::updateGrid( const Point2I &minPt, const Point2I &maxPt, bool updateClient )
{
   // On the client we just signal everyone that the height
   // map has changed... the server does the actual changes.
   if ( isClientObject() )
   {
      PROFILE_SCOPE( TerrainBlock_updateGrid_Client );

      // This depends on the client getting this call 'after' the server.
      // Which is currently the case.
      _updateBounds();

      smUpdateSignal.trigger( HeightmapUpdate, this, minPt, maxPt );

      // Tell the terrain cell that the height changed.
      const RectI gridRect( minPt, maxPt - minPt );
      mCell->updateGrid( gridRect );

      // Give the plugin a chance to update the
      // collision representation.
      if ( mPhysicsRep )
         mPhysicsRep->update();

      return;
   }

   // Now on the server we rebuild the 
   // affected area of the grid map.
   mFile->updateGrid( minPt, maxPt );

   // Fix up the bounds.
   _updateBounds();

   // Give the plugin a chance to update the
   // collision representation.
   if ( mPhysicsRep )
      mPhysicsRep->update();   

   // Signal again here for any server side observers.
   smUpdateSignal.trigger( HeightmapUpdate, this, minPt, maxPt );

   // If this is a server object and the client update
   // was requested then try to use the local connection
   // pointer to do it.
   if ( updateClient && getClientObject() )
      ((TerrainBlock*)getClientObject())->updateGrid( minPt, maxPt, false );
}

bool TerrainBlock::getHeight( const Point2F &pos, F32 *height ) const
{
   F32 invSquareSize = 1.0f / mSquareSize;
   F32 xp = pos.x * invSquareSize;
   F32 yp = pos.y * invSquareSize;
   S32 x = S32(xp);
   S32 y = S32(yp);
   xp -= (F32)x;
   yp -= (F32)y;

   const U32 blockMask = mFile->mSize - 1;

   // If we disable repeat, then skip non-primary block
   if ( !mTile && ( x & ~blockMask || y & ~blockMask ) )
      return false;
   
   x &= blockMask;
   y &= blockMask;
   
   const TerrainSquare *sq = mFile->findSquare( 0, x, y );
   if ( sq->flags & TerrainSquare::Empty )
      return false;

   F32 zBottomLeft = fixedToFloat( mFile->getHeight( x, y ) );
   F32 zBottomRight = fixedToFloat( mFile->getHeight( x + 1, y ) );
   F32 zTopLeft = fixedToFloat( mFile->getHeight( x, y + 1 ) );
   F32 zTopRight = fixedToFloat( mFile->getHeight( x + 1, y + 1 ) );

   if ( sq->flags & TerrainSquare::Split45 )
   {
      if (xp>yp)
         // bottom half
         *height = zBottomLeft + xp * (zBottomRight-zBottomLeft) + yp * (zTopRight-zBottomRight);
      else
         // top half
         *height = zBottomLeft + xp * (zTopRight-zTopLeft) + yp * (zTopLeft-zBottomLeft);
   }
   else
   {
      if (1.0f-xp>yp)
         // bottom half
         *height = zBottomRight + (1.0f-xp) * (zBottomLeft-zBottomRight) + yp * (zTopLeft-zBottomLeft);
      else
         // top half
         *height = zBottomRight + (1.0f-xp) * (zTopLeft-zTopRight) + yp * (zTopRight-zBottomRight);
   }

   return true;
}

bool TerrainBlock::getNormal( const Point2F &pos, Point3F *normal, bool normalize, bool skipEmpty ) const
{
   F32 invSquareSize = 1.0f / mSquareSize;
   F32 xp = pos.x * invSquareSize;
   F32 yp = pos.y * invSquareSize;
   S32 x = S32(xp);
   S32 y = S32(yp);
   xp -= (F32)x;
   yp -= (F32)y;
   
   const U32 blockMask = mFile->mSize - 1;

   // If we disable repeat, then skip non-primary block
   if ( !mTile && ( x & ~blockMask || y & ~blockMask ) )
      return false;
   
   x &= blockMask;
   y &= blockMask;
   
   const TerrainSquare *sq = mFile->findSquare( 0, x, y );
   if ( skipEmpty && sq->flags & TerrainSquare::Empty )
      return false;

   F32 zBottomLeft = fixedToFloat( mFile->getHeight( x, y ) );
   F32 zBottomRight = fixedToFloat( mFile->getHeight( x + 1, y ) );
   F32 zTopLeft = fixedToFloat( mFile->getHeight( x, y + 1 ) );
   F32 zTopRight = fixedToFloat( mFile->getHeight( x + 1, y + 1 ) );

   if ( sq->flags & TerrainSquare::Split45 )
   {
      if (xp>yp)
         // bottom half
         normal->set(zBottomLeft-zBottomRight, zBottomRight-zTopRight, mSquareSize);
      else
         // top half
         normal->set(zTopLeft-zTopRight, zBottomLeft-zTopLeft, mSquareSize);
   }
   else
   {
      if (1.0f-xp>yp)
         // bottom half
         normal->set(zBottomLeft-zBottomRight, zBottomLeft-zTopLeft, mSquareSize);
      else
         // top half
         normal->set(zTopLeft-zTopRight, zBottomRight-zTopRight, mSquareSize);
   }

   if (normalize)
      normal->normalize();

   return true;
}

bool TerrainBlock::getNormalAndHeight( const Point2F &pos, Point3F *normal, F32 *height, bool normalize ) const
{
   F32 invSquareSize = 1.0f / mSquareSize;
   F32 xp = pos.x * invSquareSize;
   F32 yp = pos.y * invSquareSize;
   S32 x = S32(xp);
   S32 y = S32(yp);
   xp -= (F32)x;
   yp -= (F32)y;

   const U32 blockMask = mFile->mSize - 1;

   // If we disable repeat, then skip non-primary block
   if ( !mTile && ( x & ~blockMask || y & ~blockMask ) )
      return false;
   
   x &= blockMask;
   y &= blockMask;
   
   const TerrainSquare *sq = mFile->findSquare( 0, x, y );
   if ( sq->flags & TerrainSquare::Empty )
      return false;

   F32 zBottomLeft  = fixedToFloat( mFile->getHeight(x, y) );
   F32 zBottomRight = fixedToFloat( mFile->getHeight(x + 1, y) );
   F32 zTopLeft     = fixedToFloat( mFile->getHeight(x, y + 1) );
   F32 zTopRight    = fixedToFloat( mFile->getHeight(x + 1, y + 1) );

   if ( sq->flags & TerrainSquare::Split45 )
   {
      if (xp>yp)
      {
         // bottom half
         normal->set(zBottomLeft-zBottomRight, zBottomRight-zTopRight, mSquareSize);
         *height = zBottomLeft + xp * (zBottomRight-zBottomLeft) + yp * (zTopRight-zBottomRight);
      }
      else
      {
         // top half
         normal->set(zTopLeft-zTopRight, zBottomLeft-zTopLeft, mSquareSize);
         *height = zBottomLeft + xp * (zTopRight-zTopLeft) + yp * (zTopLeft-zBottomLeft);
      }
   }
   else
   {
      if (1.0f-xp>yp)
      {
         // bottom half
         normal->set(zBottomLeft-zBottomRight, zBottomLeft-zTopLeft, mSquareSize);
         *height = zBottomRight + (1.0f-xp) * (zBottomLeft-zBottomRight) + yp * (zTopLeft-zBottomLeft);
      }
      else
      {
         // top half
         normal->set(zTopLeft-zTopRight, zBottomRight-zTopRight, mSquareSize);
         *height = zBottomRight + (1.0f-xp) * (zTopLeft-zTopRight) + yp * (zTopRight-zBottomRight);
      }
   }

   if (normalize)
      normal->normalize();

   return true;
}


bool TerrainBlock::getNormalHeightMaterial(  const Point2F &pos, 
                                             Point3F *normal, 
                                             F32 *height, 
                                             U8 *matIndex ) const
{
   F32 invSquareSize = 1.0f / mSquareSize;
   F32 xp = pos.x * invSquareSize;
   F32 yp = pos.y * invSquareSize;
   S32 x = S32(xp);
   S32 y = S32(yp);
   xp -= (F32)x;
   yp -= (F32)y;

   const U32 blockMask = mFile->mSize - 1;

   // If we disable repeat, then skip non-primary block
   if ( !mTile && ( x & ~blockMask || y & ~blockMask ) )
      return false;
   
   x &= blockMask;
   y &= blockMask;
   
   const TerrainSquare *sq = mFile->findSquare( 0, x, y );
   if ( sq->flags & TerrainSquare::Empty )
      return false;

   F32 zBottomLeft  = fixedToFloat( mFile->getHeight(x, y) );
   F32 zBottomRight = fixedToFloat( mFile->getHeight(x + 1, y) );
   F32 zTopLeft     = fixedToFloat( mFile->getHeight(x, y + 1) );
   F32 zTopRight    = fixedToFloat( mFile->getHeight(x + 1, y + 1) );

   *matIndex = mFile->getLayerIndex( x, y );

   if ( sq->flags & TerrainSquare::Split45 )
   {
      if (xp>yp)
      {
         // bottom half
         normal->set(zBottomLeft-zBottomRight, zBottomRight-zTopRight, mSquareSize);
         *height = zBottomLeft + xp * (zBottomRight-zBottomLeft) + yp * (zTopRight-zBottomRight);
      }
      else
      {
         // top half
         normal->set(zTopLeft-zTopRight, zBottomLeft-zTopLeft, mSquareSize);
         *height = zBottomLeft + xp * (zTopRight-zTopLeft) + yp * (zTopLeft-zBottomLeft);
      }
   }
   else
   {
      if (1.0f-xp>yp)
      {
         // bottom half
         normal->set(zBottomLeft-zBottomRight, zBottomLeft-zTopLeft, mSquareSize);
         *height = zBottomRight + (1.0f-xp) * (zBottomLeft-zBottomRight) + yp * (zTopLeft-zBottomLeft);
      }
      else
      {
         // top half
         normal->set(zTopLeft-zTopRight, zBottomRight-zTopRight, mSquareSize);
         *height = zBottomRight + (1.0f-xp) * (zTopLeft-zTopRight) + yp * (zTopRight-zBottomRight);
      }
   }

   normal->normalize();

   return true;
}

U32 TerrainBlock::getMaterialCount() const
{
   return mFile->mMaterials.size();
}

void TerrainBlock::addMaterial( const String &name, U32 insertAt )
{
   TerrainMaterial *mat = TerrainMaterial::findOrCreate( name );

   if ( insertAt == -1 )
   {
      mFile->mMaterials.push_back( mat );
      mFile->_initMaterialInstMapping();
   }
   else
   {

      // TODO: Insert and reindex!        

   }

   mDetailsDirty = true;
   mLayerTexDirty = true;
}

void TerrainBlock::removeMaterial( U32 index )
{
   mFile->mMaterials.erase( index );
   mFile->_initMaterialInstMapping();

   // TODO: Reindex!
}

void TerrainBlock::updateMaterial( U32 index, const String &name )
{
   if ( index >= mFile->mMaterials.size() )
      return;

   mFile->mMaterials[ index ] = TerrainMaterial::findOrCreate( name );
   mFile->_initMaterialInstMapping();

   mDetailsDirty = true;
   mLayerTexDirty = true;
}

TerrainMaterial* TerrainBlock::getMaterial( U32 index ) const
{
   if ( index >= mFile->mMaterials.size() )
      return NULL;

   return mFile->mMaterials[ index ];
}

void TerrainBlock::deleteAllMaterials()
{
   mFile->mMaterials.clear();
   mFile->mMaterialInstMapping.clearMatInstList();
}

const char* TerrainBlock::getMaterialName( U32 index ) const
{
   if ( index < mFile->mMaterials.size() )
      return mFile->mMaterials[ index ]->getInternalName();

   return NULL;
}

void TerrainBlock::setLightMap( GBitmap *newLightMap )
{
   SAFE_DELETE( mLightMap );
   mLightMap = newLightMap;
   mLightMapTex = NULL;
}

void TerrainBlock::clearLightMap()
{
   if ( !mLightMap )
      mLightMap = new GBitmap( mLightMapSize, mLightMapSize, 0, GFXFormatR8G8B8 );

   mLightMap->fillWhite();
   mLightMapTex = NULL;
}

GFXTextureObject* TerrainBlock::getLightMapTex()
{
   if ( mLightMapTex.isNull() && mLightMap )
   {
      mLightMapTex.set( mLightMap, 
                        &GFXDefaultStaticDiffuseProfile, 
                        false, 
                        "TerrainBlock::getLightMapTex()" );
   }

   return mLightMapTex;
}

void TerrainBlock::onEditorEnable()
{
}

void TerrainBlock::onEditorDisable()
{
}

bool TerrainBlock::onAdd()
{
   if(!Parent::onAdd())
      return false;

   if ( mTerrFileName.isEmpty() )
   {
      mTerrFileName = Con::getVariable( "$Client::MissionFile" );
      Vector<String> materials;
      materials.push_back( "warning_material" );
      TerrainFile::create( &mTerrFileName, 256, materials );
   }

   Resource<TerrainFile> terr = ResourceManager::get().load( mTerrFileName );
   if(terr == NULL)
   {
      if(isClientObject())
         NetConnection::setLastError("You are missing a file needed to play this mission: %s", mTerrFileName.c_str());
      return false;
   }

   setFile( terr );

   if ( terr->mNeedsResaving )
   {
      if (Platform::messageBox("Update Terrain File", "You appear to have a Terrain file in an older format. Do you want Torque to update it?", MBOkCancel, MIQuestion) == MROk)
      {
         terr->save(terr->mFilePath.getFullPath());
         terr->mNeedsResaving = false;
      }
   }

   if (terr->mFileVersion != TerrainFile::FILE_VERSION || terr->mNeedsResaving)
   {
      Con::errorf(" *********************************************************");
      Con::errorf(" *********************************************************");
      Con::errorf(" *********************************************************");
      Con::errorf(" PLEASE RESAVE THE TERRAIN FILE FOR THIS MISSION!  THANKS!");
      Con::errorf(" *********************************************************");
      Con::errorf(" *********************************************************");
      Con::errorf(" *********************************************************");
   }

   _updateBounds();
   
   if (mTile)
      setGlobalBounds();

   resetWorldBox();
   setRenderTransform(mObjToWorld);

   if (isClientObject())
   {
      if ( mCRC != terr.getChecksum() )
      {
         NetConnection::setLastError("Your terrain file doesn't match the version that is running on the server.");
         return false;
      }

      clearLightMap();

      // Init the detail layer rendering helper.
      _updateMaterials();
      _updateLayerTexture();

      // If the cached base texture is older that the terrain file or
      // it doesn't exist then generate and cache it.
      String baseCachePath = _getBaseTexCacheFileName();
      if ( Platform::compareModifiedTimes( baseCachePath, mTerrFileName ) < 0 )
         _updateBaseTexture( true );

      // The base texture should have been cached by now... so load it.
      mBaseTex.set( baseCachePath, &GFXDefaultStaticDiffuseProfile, "TerrainBlock::mBaseTex" );

      GFXTextureManager::addEventDelegate( this, &TerrainBlock::_onTextureEvent );
      LightManager::smActivateSignal.notify( this, &TerrainBlock::_onLMActivate );

      // Build the terrain quadtree.
      _rebuildQuadtree();
   }
   else
      mCRC = terr.getChecksum();

   addToScene();

   if ( gPhysicsPlugin )
      mPhysicsRep = gPhysicsPlugin->createStatic( this );

   return true;
}

String TerrainBlock::_getBaseTexCacheFileName() const
{
   Torque::Path basePath( mTerrFileName );
   basePath.setFileName( basePath.getFileName() + "_basetex" );
   basePath.setExtension( "dds" );
   return basePath.getFullPath();
}

void TerrainBlock::_rebuildQuadtree()
{
   SAFE_DELETE( mCell );

   // Recursively build the cells.
   mCell = TerrCell::init( this );

   // Build the shared PrimitiveBuffer.
   mCell->createPrimBuffer( &mPrimBuffer );
}

void TerrainBlock::onRemove()
{
   removeFromScene();

   SAFE_DELETE( mPhysicsRep );

   if ( isClientObject() )
   {
      mBaseTex = NULL;
      mLayerTex = NULL;
      SAFE_DELETE( mBaseMaterial );
      SAFE_DELETE( mDefaultMatInst );
      SAFE_DELETE( mCell );
      mPrimBuffer = NULL;
      mBaseShader = NULL;
      GFXTextureManager::removeEventDelegate( this, &TerrainBlock::_onTextureEvent );
      LightManager::smActivateSignal.remove( this, &TerrainBlock::_onLMActivate );
   }

   Parent::onRemove();
}

bool TerrainBlock::prepRenderImage(SceneState* state, const U32 stateKey,
                                   const U32 /*startZone*/, const bool /*modifyBaseState*/)
{
   PROFILE_SCOPE(TerrainBlock_prepRenderImage);

   if (isLastState(state, stateKey))
      return false;

   setLastState(state, stateKey);

   // This should be sufficient for most objects that don't manage zones, and
   //  don't need to return a specialized RenderImage...
   bool render = true;
   if (state->isTerrainOverridden() == false)
      render = state->isObjectRendered(this);

   // small hack to reduce "stutter" in framerate if terrain is suddenly seen (ie. from an interior)
   if( !mHasRendered )
   {
      mHasRendered = true;
      render = true;
      state->enableTerrainOverride();
   }

   if (render == true)
      _renderBlock( state );

   return false;
}

void TerrainBlock::setTransform(const MatrixF & mat)
{
   Parent::setTransform( mat );

   if ( mPhysicsRep )
      mPhysicsRep->setTransform( mat );

   setRenderTransform( mat );
   setMaskBits( TransformMask );
}

void TerrainBlock::setScale( const VectorF &scale )
{
   // We disable scaling... we never scale!
   Parent::setScale( VectorF::One );   
}

void TerrainBlock::initPersistFields()
{
   addGroup( "Media" );
      
      addProtectedField( "terrainFile", TypeStringFilename, Offset( mTerrFileName, TerrainBlock ), 
         &TerrainBlock::_setTerrainFile, &defaultProtectedGetFn,
         "The source terrain data file." );

   endGroup( "Media" );

   addGroup( "Misc" );
   
      addProtectedField( "squareSize", TypeF32, Offset( mSquareSize, TerrainBlock ),
         &TerrainBlock::_setSquareSize, &defaultProtectedGetFn,
         "Indicates the spacing between points on the XY plane on the terrain." );

      addField( "tile", TypeBool, Offset( mTile, TerrainBlock ), "Toggles infinite tiling of terrain." );

      addProtectedField( "baseTexSize", TypeS32, Offset( mBaseTexSize, TerrainBlock ),
         &TerrainBlock::_setBaseTexSize, &defaultProtectedGetFn,
         "Size of base texture size per meter." );

      addField( "screenError", TypeS32, Offset( mScreenError, TerrainBlock ), "Not yet implemented." );

   endGroup( "Misc" );

   Parent::initPersistFields();

   Con::addVariable( "$TerrainBlock::debugRender", TypeBool, &smDebugRender );
   Con::addVariable( "$TerrainBlock::lodScale", TypeF32, &smLODScale );
   Con::addVariable( "$TerrainBlock::detailScale", TypeF32, &smDetailScale );
}

void TerrainBlock::inspectPostApply()
{
   Parent::inspectPostApply();
   setMaskBits( MiscMask );
}

U32 TerrainBlock::packUpdate(NetConnection *, U32 mask, BitStream *stream)
{
   if ( stream->writeFlag( mask & TransformMask ) )
      mathWrite( *stream, getTransform() );

   if ( stream->writeFlag( mask & FileMask ) )
   {
      stream->write( mTerrFileName );
      stream->write( mCRC );
   }

   if ( stream->writeFlag( mask & SizeMask ) )
   {
      stream->write( mSquareSize );
      stream->writeFlag( mTile );
   }

   if ( stream->writeFlag( mask & MaterialMask ) )
      stream->write( mBaseTexSize );

   stream->writeFlag( mask & HeightMapChangeMask );

   if ( stream->writeFlag( mask & MiscMask ) )
      stream->write( mScreenError );

   return 0;
}

void TerrainBlock::unpackUpdate(NetConnection *, BitStream *stream)
{
   if ( stream->readFlag() ) // TransformMask
   {
      MatrixF mat;
      mathRead( *stream, &mat );
      setTransform( mat );
   }

   if ( stream->readFlag() ) // FileMask
   {
      FileName terrFile;
      stream->read( &terrFile );
      stream->read( &mCRC );

      if ( isProperlyAdded() )
         setFile( terrFile );
      else
         mTerrFileName = terrFile;
   }

   if ( stream->readFlag() ) // SizeMask
   {
      stream->read( &mSquareSize );
      mTile = stream->readFlag();
   }

   if ( stream->readFlag() ) // MaterialMask
   {
      stream->read( &mBaseTexSize );

      if ( isProperlyAdded() )
         _updateBaseTexture( false );
   }

   if ( stream->readFlag() && isProperlyAdded() ) // HeightMapChangeMask
   {
      _updateBounds();
      _rebuildQuadtree();
      mDetailsDirty = true;
      mLayerTexDirty = true;
   }

   if ( stream->readFlag() ) // MiscMask
      stream->read( &mScreenError );
}

void TerrainBlock::getMinMaxHeight( F32 *minHeight, F32 *maxHeight ) const 
{
   // We can get the bound height from the last grid level.
   const TerrainSquare *sq = mFile->findSquare( mFile->mGridLevels, 0, 0 );
   *minHeight = fixedToFloat( sq->minHeight );
   *maxHeight = fixedToFloat( sq->maxHeight );
}

//-----------------------------------------------------------------------------
// Console Methods
//-----------------------------------------------------------------------------

ConsoleMethod(TerrainBlock, save, bool, 3, 3, "(string fileName) - saves the terrain block's terrain file to the specified file name.")
{
   char filename[256];
   dStrcpy(filename,argv[2]);
   char *ext = dStrrchr(filename, '.');
   if (!ext || dStricmp(ext, ".ter") != 0)
      dStrcat(filename, ".ter");
   return static_cast<TerrainBlock*>(object)->save(filename);
}

ConsoleFunction(getTerrainHeight, F32, 2, 3, "(Point2 pos) - gets the terrain height at the specified position."
				"@param pos The world space point, minus the z (height) value\n Can be formatted as either (\"x y\") or (x,y)\n"
				"@return Returns the terrain height at the given point as an F32 value.\n")
{
	Point2F pos;
	F32 height = 0.0f;

	if(argc == 2)
		dSscanf(argv[1],"%f %f",&pos.x,&pos.y);
	else if(argc == 3)
	{
		pos.x = dAtof(argv[1]);
		pos.y = dAtof(argv[2]);
	}

	TerrainBlock * terrain = getTerrainUnderWorldPoint(Point3F(pos.x, pos.y, 5000.0f));
	if(terrain)
		if(terrain->isServerObject())
		{
			Point3F offset;
			terrain->getTransform().getColumn(3, &offset);
			pos -= Point2F(offset.x, offset.y);
			terrain->getHeight(pos, &height);
		}
		return height;
}

ConsoleFunction(getTerrainHeightBelowPosition, F32, 2, 4, "(Point3F pos) - gets the terrain height at the specified position."
				"@param pos The world space point. Can be formatted as either (\"x y z\") or (x,y,z)\n"
				"@note This function is useful if you simply want to grab the terrain height underneath an object.\n"
				"@return Returns the terrain height at the given point as an F32 value.\n")
{
	Point3F pos;
	F32 height = 0.0f;

   if(argc == 2)
      dSscanf(argv[1], "%f %f %f", &pos.x, &pos.y, &pos.z);
   else if(argc == 4)
   {
      pos.x = dAtof(argv[1]);
      pos.y = dAtof(argv[2]);
      pos.z = dAtof(argv[3]);
   }

   else
   {
      Con::errorf("getTerrainHeightBelowPosition(Point3F): Invalid argument count! Valid arguments are either \"x y z\" or x,y,z\n");
      return 0;
   }

	TerrainBlock * terrain = getTerrainUnderWorldPoint(pos);
	
	Point2F nohghtPos(pos.x, pos.y);

	if(terrain)
	{
		if(terrain->isServerObject())
		{
			Point3F offset;
			terrain->getTransform().getColumn(3, &offset);
			nohghtPos -= Point2F(offset.x, offset.y);
			terrain->getHeight(nohghtPos, &height);
		}
	}
	
	return height;
}
