//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "materials/materialList.h"

#include "materials/matInstance.h"
#include "materials/materialManager.h"
#include "materials/materialFeatureTypes.h"
#include "core/volume.h"
#include "console/simSet.h"


MaterialList::MaterialList()
{
   VECTOR_SET_ASSOCIATION(mMaterialNames);
   VECTOR_SET_ASSOCIATION(mMaterials);
}

MaterialList::MaterialList(const MaterialList* pCopy)
{
   VECTOR_SET_ASSOCIATION(mMaterialNames);
   VECTOR_SET_ASSOCIATION(mMaterials);

   mMaterialNames.setSize(pCopy->mMaterialNames.size());
   S32 i;
   for (i = 0; i < mMaterialNames.size(); i++)
   {
      mMaterialNames[i] = pCopy->mMaterialNames[i];
   }

   mMaterials.setSize(pCopy->mMaterials.size());
   for (i = 0; i < mMaterials.size(); i++) {
      constructInPlace(&mMaterials[i]);
      mMaterials[i] = pCopy->mMaterials[i];
   }

   clearMatInstList();
   mMatInstList.setSize(pCopy->mMaterials.size());
   for( i = 0; i < mMatInstList.size(); i++ )
   {
      if( i < pCopy->mMatInstList.size() && pCopy->mMatInstList[i] )
      {
         mMatInstList[i] = pCopy->mMatInstList[i]->getMaterial()->createMatInstance();
      }
      else
      {
         mMatInstList[i] = NULL;
      }
   }

}



MaterialList::MaterialList(U32 materialCount, const char **materialNames)
{
   VECTOR_SET_ASSOCIATION(mMaterialNames);
   VECTOR_SET_ASSOCIATION(mMaterials);

   set(materialCount, materialNames);
}


//--------------------------------------
void MaterialList::set(U32 materialCount, const char **materialNames)
{
   free();
   mMaterials.setSize(materialCount);
   mMaterialNames.setSize(materialCount);
   clearMatInstList();
   mMatInstList.setSize(materialCount);
   for(U32 i = 0; i < materialCount; i++)
   {
      // vectors DO NOT initialize classes so manually call the constructor
      constructInPlace(&mMaterials[i]);
      mMaterialNames[i] = materialNames[i];
      mMatInstList[i] = NULL;
   }
}


//--------------------------------------
MaterialList::~MaterialList()
{
   free();
}

//--------------------------------------
void MaterialList::setMaterialName(U32 index, String name)
{
   if (index < mMaterialNames.size())
      mMaterialNames[index] = name;
}


//--------------------------------------
void MaterialList::load(U32 index, const String &path)
{
   AssertFatal(index < size(), "MaterialList:: index out of range.");
   if (index < size())
   {
      GFXTexHandle &handle = mMaterials[index];
      if (handle.isNull())
      {
         const String &name = mMaterialNames[index];
         if (name.isNotEmpty())
         {
            if (path.isNotEmpty())
            {
               // Should we strip off the extension of the path here before trying
               // to load the texture? 
               String   fullPath = String::ToString("%s/%s", path.c_str(), name.c_str());
               handle.set(fullPath, &GFXDefaultStaticDiffuseProfile, avar("%s() - handle (line %d)", __FUNCTION__, __LINE__));
            }
            else
            {
               handle.set(name, &GFXDefaultStaticDiffuseProfile, avar("%s() - handle (line %d)", __FUNCTION__, __LINE__));
            }
         }
      }
   }
}


//--------------------------------------
bool MaterialList::load(const String &path)
{
   AssertFatal(mMaterials.size() == mMaterials.size(), "MaterialList::load: internal vectors out of sync.");

   for(S32 i=0; i < mMaterials.size(); i++)
      load(i,path);

   for(S32 i=0; i < mMaterials.size(); i++)
   {
      // TSMaterialList nulls out the names of IFL materials, so
      // we need to ignore empty names.
      const String &name = mMaterialNames[i];
      if (name.isNotEmpty() && !mMaterials[i])
         return false;
   }
   return true;
}

//--------------------------------------
void MaterialList::free()
{
   AssertFatal(mMaterials.size() == mMaterialNames.size(), "MaterialList::free: internal vectors out of sync.");
   for(S32 i=0; i < mMaterials.size(); i++)
   {
      mMaterialNames[i] = String();
      mMaterials[i] = NULL;
   }
   clearMatInstList();
   mMatInstList.setSize(0);
   mMaterialNames.setSize(0);
   mMaterials.setSize(0);
}


//--------------------------------------
U32 MaterialList::push_back(GFXTexHandle textureHandle, const String &filename)
{
   mMaterials.increment();
   mMaterialNames.increment();
   mMatInstList.push_back(NULL);

   // vectors DO NOT initialize classes so manually call the constructor
   constructInPlace(&mMaterials.last());
   mMaterials.last()    = textureHandle;
   mMaterialNames.last() = filename;

   // return the index
   return mMaterials.size()-1;
}

//--------------------------------------
U32 MaterialList::push_back(const String &filename, Material* material)
{
   mMaterials.increment();
   mMaterialNames.increment();
   if (material) 
      mMatInstList.push_back(material->createMatInstance());
   else
      mMatInstList.push_back(NULL);

   // vectors DO NOT initialize classes so manually call the constructor
   constructInPlace(&mMaterials.last());
   mMaterialNames.last() = filename;

   // return the index
   return mMaterials.size()-1;
}

//--------------------------------------
bool MaterialList::read(Stream &stream)
{
   free();

   // check the stream version
   U8 version;
   if ( stream.read(&version) && version != BINARY_FILE_VERSION)
      return readText(stream,version);

   // how many materials?
   U32 count;
   if ( !stream.read(&count) )
      return false;

   // pre-size the vectors for efficiency
   mMaterials.reserve(count);
   mMaterialNames.reserve(count);

   // read in the materials
   for (U32 i=0; i<count; i++)
   {
      // Load the bitmap name
      char buffer[256];
      stream.readString(buffer);
      if( !buffer[0] )
      {
         AssertWarn(0, "MaterialList::read: error reading stream");
         return false;
      }

      // Material paths are a legacy of Tribes tools,
      // strip them off...
      char *name = &buffer[dStrlen(buffer)];
      while (name != buffer && name[-1] != '/' && name[-1] != '\\')
         name--;

      // Add it to the list
      mMaterials.increment();
      mMaterialNames.increment();
      mMatInstList.push_back(NULL);
      // vectors DO NOT initialize classes so manually call the constructor
      constructInPlace(&mMaterials.last());
      mMaterialNames.last() = name;
   }

   return (stream.getStatus() == Stream::Ok);
}


//--------------------------------------
bool MaterialList::write(Stream &stream)
{
   AssertFatal(mMaterials.size() == mMaterialNames.size(), "MaterialList::write: internal vectors out of sync.");

   stream.write((U8)BINARY_FILE_VERSION);    // version
   stream.write((U32)mMaterials.size());     // material count

   for(S32 i=0; i < mMaterials.size(); i++)  // material names
      stream.writeString(mMaterialNames[i]);

   return (stream.getStatus() == Stream::Ok);
}


//--------------------------------------
bool MaterialList::readText(Stream &stream, U8 firstByte)
{
   free();

   if (!firstByte)
      return (stream.getStatus() == Stream::Ok || stream.getStatus() == Stream::EOS);

   char buf[1024];
   buf[0] = firstByte;
   U32 offset = 1;

   for(;;)
   {
      stream.readLine((U8*)(buf+offset), sizeof(buf)-offset);
      if(!buf[0])
         break;
      offset = 0;

      // Material paths are a legacy of Tribes tools,
      // strip them off...
      char *name = &buf[dStrlen(buf)];
      while (name != buf && name[-1] != '/' && name[-1] != '\\')
         name--;

      // Add it to the list
      mMaterials.increment();
      mMaterialNames.increment();
      mMatInstList.push_back(NULL);
      // vectors DO NOT initialize classes so manually call the constructor
      constructInPlace(&mMaterials.last());
      mMaterialNames.last() = name;
   }

   return (stream.getStatus() == Stream::Ok || stream.getStatus() == Stream::EOS);
}

bool MaterialList::readText(Stream &stream)
{
   U8 firstByte;
   stream.read(&firstByte);
   return readText(stream,firstByte);
}

//--------------------------------------
bool MaterialList::writeText(Stream &stream)
{
   AssertFatal(mMaterials.size() == mMaterialNames.size(), "MaterialList::writeText: internal vectors out of sync.");

   for(S32 i=0; i < mMaterials.size(); i++)
      stream.writeLine((U8*)(mMaterialNames[i].c_str()));
   stream.writeLine((U8*)"");

   return (stream.getStatus() == Stream::Ok);
}

//--------------------------------------------------------------------------
// Clear all materials in the mMatInstList member variable
//--------------------------------------------------------------------------
void MaterialList::clearMatInstList()
{
   // clear out old materials.  any non null element of the list should be pointing at deletable memory,
   // although multiple indexes may be pointing at the same memory so we have to be careful (see
   // comment in loop body)
   for (U32 i=0; i<mMatInstList.size(); i++)
   {
      if (mMatInstList[i])
      {
         BaseMatInstance* current = mMatInstList[i];
         delete current;
         mMatInstList[i] = NULL;

         // ok, since ts material lists can remap difference indexes to the same object (e.g. for ifls),
         // we need to make sure that we don't delete the same memory twice.  walk the rest of the list
         // and null out any pointers that match the one we deleted.
         for (U32 j=0; j<mMatInstList.size(); j++)
            if (mMatInstList[j] == current)
               mMatInstList[j] = NULL;
      }
   }
}

//--------------------------------------------------------------------------
// Map materials - map materials to the textures in the list
//--------------------------------------------------------------------------
void MaterialList::mapMaterials()
{
   mMatInstList.setSize( mMaterials.size() );

   for( U32 i=0; i<mMaterials.size(); i++ )
      mapMaterial( i );
}

/// Map the material name at the given index to a material instance.
///
/// @note The material instance that is created will <em>not be initialized.</em>

void MaterialList::mapMaterial( U32 i )
{
   AssertFatal( i < mMaterials.size(), "MaterialList::mapMaterialList - index out of bounds" );

   if( mMatInstList[i] != NULL )
      return;

   // lookup a material property entry
   const String &matName = getMaterialName(i);

   // JMQ: this code assumes that all materials have names, but ifls don't (they are nuked by tsmateriallist loader code).  what to do?
   if( matName.isEmpty() )
   {
      mMatInstList[i] = NULL;
      return;
   }

   String materialName = MATMGR->getMapEntry(matName);

   // IF we didn't find it, then look for a PolyStatic generated Material
   //  [a little cheesy, but we need to allow for user override of generated Materials]
   if ( materialName.isEmpty() )
      materialName = MATMGR->getMapEntry(String::ToString( "polyMat_%s", matName.c_str()) );

   if(materialName.isNotEmpty())
   {
      Material * mat = MATMGR->getMaterialDefinitionByName(materialName);
      if( mat )
         mMatInstList[i] = mat->createMatInstance();
      else
         mMatInstList[i] = MATMGR->createWarningMatInstance();
   }
   else
   {
      if( mMaterials[i].isValid() )
      {            
         if (Con::getBoolVariable("$Materials::createMissing", true))
         {
            // No Material found, create new "default" material with just a diffuseMap
            //static U32 defaultMatCount = 0;
            String newMatName = Sim::getUniqueName( "DefaultMaterial" );

            Material *newMat = MATMGR->allocateAndRegister( newMatName, mMaterialNames[i] );

            // Flag this as an autogenerated Material
            newMat->mAutoGenerated = true;

            // Overwrite diffuseMap in new material - hackish, but works
            newMat->mStages[0].setTex( MFT_DiffuseMap, mMaterials[i] );              
            newMat->mDiffuseMapFilename[0] = mMaterials[i]->mTextureLookupName;

            // Set up some defaults for transparent textures
            if (mMaterials[i]->mHasTransparency)
            {
               newMat->mTranslucent = true;
               newMat->mTranslucentBlendOp = Material::LerpAlpha;
               newMat->mTranslucentZWrite = true;
               newMat->mAlphaRef = 20;
            }

            // create a MatInstance for the new material            
            mMatInstList[i] = newMat->createMatInstance();

#ifndef TORQUE_SHIPPING
            Con::warnf("[MaterialList::mapMaterials] Creating missing material for texture: %s", mMaterials[i]->mTextureLookupName.c_str() );
#endif
         }
         else
         {
            Con::errorf("[MaterialList::mapMaterials] Unable to find material for texture: %s", mMaterials[i]->mTextureLookupName.c_str() );
            mMatInstList[i] = MATMGR->createWarningMatInstance();
         }
      }
      else
      {
         mMatInstList[i] = MATMGR->createWarningMatInstance();
      }
   }
}

void MaterialList::initMatInstances(   const FeatureSet &features, 
                                       const GFXVertexFormat *vertexFormat )
{
   mFeatures = features;
   mVertexFormat = vertexFormat;

   for( U32 i=0; i<mMatInstList.size(); i++ )
   {
      BaseMatInstance *matInst = mMatInstList[i];
      if ( matInst )
         matInst->init( mFeatures, mVertexFormat );
   }
}

//--------------------------------------------------------------------------
// Get material instance
//--------------------------------------------------------------------------
BaseMatInstance * MaterialList::getMaterialInst( U32 texIndex ) const
{
   if ( mMatInstList.size() == 0 )
      return NULL;

   return mMatInstList[texIndex];
}

//--------------------------------------------------------------------------
// Set material instance
//--------------------------------------------------------------------------
void MaterialList::setMaterialInst( BaseMatInstance *matInst, U32 texIndex )
{
   if( texIndex >= mMatInstList.size() )
   {
      AssertFatal( false, "Index out of range" );
   }
   mMatInstList[texIndex] = matInst;
}

