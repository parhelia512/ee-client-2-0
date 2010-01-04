//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "terrain/terrMaterial.h"
#include "console/consoleTypes.h"
#include "gfx/bitmap/gBitmap.h"


IMPLEMENT_CONOBJECT( TerrainMaterial );

TerrainMaterial::TerrainMaterial()
   :  mSideProjection( false ),
      mDiffuseSize( 500.0f ),
      mDetailSize( 5.0f ),
      mDetailStrength( 1.0f ),
      mDetailDistance( 50.0f ),
      mParallaxScale( 0.0f )
{
}

TerrainMaterial::~TerrainMaterial()
{
}

void TerrainMaterial::initPersistFields()
{
   addField( "diffuseMap", TypeStringFilename, Offset( mDiffuseMap, TerrainMaterial ) );
   addField( "diffuseSize", TypeF32, Offset( mDiffuseSize, TerrainMaterial ) );

   addField( "normalMap", TypeStringFilename, Offset( mNormalMap, TerrainMaterial ) );
   
   addField( "detailMap", TypeStringFilename, Offset( mDetailMap, TerrainMaterial ) );
   addField( "detailSize", TypeF32, Offset( mDetailSize, TerrainMaterial ) );

   addField( "detailStrength", TypeF32, Offset( mDetailStrength, TerrainMaterial ) );
   addField( "detailDistance", TypeF32, Offset( mDetailDistance, TerrainMaterial ) );
   addField( "useSideProjection", TypeBool, Offset( mSideProjection, TerrainMaterial ) );
   addField( "parallaxScale", TypeF32, Offset( mParallaxScale, TerrainMaterial ) );

   Parent::initPersistFields();

   // Gotta call this at least once or it won't get created!
   Sim::getTerrainMaterialSet();
}

bool TerrainMaterial::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   SimSet *set = Sim::getTerrainMaterialSet();

   // Make sure we have an internal name set.
   if ( !mInternalName || !mInternalName[0] )
      Con::warnf( "TerrainMaterial::onAdd() - No internal name set!" );
   else
   {
      SimObject *object = set->findObjectByInternalName( mInternalName );
      if ( object )
         Con::warnf( "TerrainMaterial::onAdd() - Internal name collision; '%s' already exists!", mInternalName );
   }

   set->addObject( this );

   return true;
}

TerrainMaterial* TerrainMaterial::getWarningMaterial()
{ 
   return findOrCreate( NULL );
}

TerrainMaterial* TerrainMaterial::findOrCreate( const char *nameOrPath )
{
   SimSet *set = Sim::getTerrainMaterialSet();
   
   if ( !nameOrPath || !nameOrPath[0] )
      nameOrPath = "warning_material";

   // See if we can just find it.
   TerrainMaterial *mat = dynamic_cast<TerrainMaterial*>( set->findObjectByInternalName( StringTable->insert( nameOrPath ) ) );
   if ( mat )
      return mat;

   // We didn't find it... so see if its a path to a
   // file.  If it is lets assume its the texture.
   if ( GBitmap::sFindFiles( nameOrPath, NULL ) )
   {
      mat = new TerrainMaterial();
      mat->setInternalName( nameOrPath );
      mat->mDiffuseMap = nameOrPath;
      mat->registerObject();
      Sim::getRootGroup()->addObject( mat );
      return mat;
   }

   // Ok... return a debug material then.
   mat = dynamic_cast<TerrainMaterial*>( set->findObjectByInternalName( StringTable->insert( "warning_material" ) ) );
   if ( !mat )
   {
      // This shouldn't happen.... the warning_texture should
      // have already been defined in script, but we put this
      // fallback here just in case it gets "lost".
      mat = new TerrainMaterial();
      mat->setInternalName( "warning_material" );
      mat->mDiffuseMap = "core/art/warnMat.png";
      mat->mDiffuseSize = 500;
      mat->mDetailMap = "core/art/warnMat.png";
      mat->mDetailSize = 5;
      mat->registerObject();
   }

   return mat;
}
