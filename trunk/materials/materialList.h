//-----------------------------------------------------------------------------
// Torque Shader Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _MATERIALLIST_H_
#define _MATERIALLIST_H_

#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _GFXTEXTUREHANDLE_H_
#include "gfx/gfxTextureHandle.h"
#endif
#ifndef _MATERIALFEATUREDATA_H_
#include "materials/materialFeatureData.h"
#endif

class Material;
class BaseMatInstance;
class Stream;
class GFXVertexFormat;


class MaterialList
{
public:
   MaterialList();
   MaterialList(U32 materialCount, const char **materialNames);
   virtual ~MaterialList();

   /// Note: this is not to be confused with MaterialList(const MaterialList&).  Copying
   ///  a material list in the middle of it's lifetime is not a good thing, so we force
   ///  it to copy at construction time by restricting the copy syntax to
   ///  ML* pML = new ML(&copy);
   explicit MaterialList(const MaterialList*);

   U32 getMaterialCount() const { return (U32)mMaterials.size(); }

   const Vector<String> &getMaterialNameList() const { return mMaterialNames; }
   const String &getMaterialName(U32 index) const { return mMaterialNames[index]; }
   GFXTexHandle &getMaterial(U32 index)
   {
      AssertFatal(index < (U32) mMaterials.size(), "MaterialList::getMaterial: index lookup out of range.");
      return mMaterials[index];
   }

   void setMaterialName(U32 index, String name);

   void set(U32 materialCount, const char **materialNames);
   U32  push_back(GFXTexHandle textureHandle, const String &filename);
   U32  push_back(const String &filename, Material* = 0);

   void load(U32 index, const String &path);
   bool load(const String &path);
   virtual void free();
   void clearMatInstList();

   typedef Vector<GFXTexHandle>::iterator iterator;
   typedef Vector<GFXTexHandle>::value_type value;

   GFXTexHandle& front() { return mMaterials.front(); }
   GFXTexHandle& first() { return mMaterials.first(); }
   GFXTexHandle& last()  { return mMaterials.last(); }
   bool       empty() const { return mMaterials.empty();   }
   U32        size() const { return (U32)mMaterials.size(); }
   iterator   begin() { return mMaterials.begin(); }
   iterator   end()   { return mMaterials.end(); }
   value operator[] (S32 index) { return getMaterial(U32(index)); }

   bool read(Stream &stream);
   bool write(Stream &stream);

   bool readText(Stream &stream, U8 firstByte);
   bool readText(Stream &stream);
   bool writeText(Stream &stream);

   void mapMaterials();

   /// Initialize material instances in material list.
   void initMatInstances(  const FeatureSet &features, 
                           const GFXVertexFormat *vertexFormat );

   BaseMatInstance * getMaterialInst( U32 texIndex ) const;
   void setMaterialInst( BaseMatInstance *matInst, U32 texIndex );

	// Needs to be accessible if were going to freely edit instances
	Vector<BaseMatInstance*> mMatInstList;

protected:
   Vector<String> mMaterialNames;
   Vector<GFXTexHandle> mMaterials;
	
   

   FeatureSet mFeatures;

   /// The vertex format on which the materials will render.
   const GFXVertexFormat *mVertexFormat;

   virtual void mapMaterial( U32 index );

private:
   enum Constants { BINARY_FILE_VERSION = 1 };
};

#endif
