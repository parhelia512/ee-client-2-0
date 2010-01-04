//-----------------------------------------------------------------------------
// Collada-2-DTS
// Copyright (C) 2008 GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _COLLADA_APPMESH_H_
#define _COLLADA_APPMESH_H_

#ifndef _TDICTIONARY_H_
#include "core/tDictionary.h"
#endif
#ifndef _APPMESH_H_
#include "ts/loader/appMesh.h"
#endif
#ifndef _TSSHAPELOADER_H_
#include "ts/loader/tsShapeLoader.h"
#endif
#ifndef _COLLADA_APPNODE_H_
#include "ts/collada/colladaAppNode.h"
#endif
#ifndef _COLLADA_EXTENSIONS_H_
#include "ts/collada/colladaExtensions.h"
#endif

//-----------------------------------------------------------------------------
// Torque unifies the vert position, normal and UV values, so that a single index
// uniquely identifies all 3 elements. A triangle then contains just 3 indices,
// and from that we can get the 3 positions, 3 normals and 3 UVs.
//
// for i=1:3
//    index = indices[triangle.start + i]
//    points[index], normals[index], uvs[index]
//
// Collada does not use unified vertex streams, (each triangle needs 9 indices),
// so this structure is used to map a single VertTuple index to 3 indices into
// the Collada streams. The Collada (and Torque) primitive index is also stored
// because the Collada document may use different streams for different primitives.
//
// For morph geometry, we can use the same array of VertTuples to access the base
// AND all of the target geometries because they MUST have the same topology.
struct VertTuple
{
   S32 prim, vertex, normal, color, uv, uv2;
   VertTuple(): prim(-1), vertex(-1), normal(-1), color(-1), uv(-1), uv2(-1) {}
   bool operator==(const VertTuple& p) const 
   {
      return   prim == p.prim && 
               vertex == p.vertex && 
               color == p.color &&
               normal == p.normal && 
               uv == p.uv &&
               uv2 == p.uv2;
   }
};

class ColladaAppMesh : public AppMesh
{
   typedef AppMesh Parent;

protected:
   class ColladaAppNode* appNode;                     ///< Pointer to the node that owns this mesh
   const domInstance_geometry* instanceGeom;
   const domInstance_controller* instanceCtrl;
   ColladaExtension_geometry* geomExt;                ///< geometry extension

   Vector<VertTuple> vertTuples;                      ///<
   Map<StringTableEntry,U32> boundMaterials;          ///< Local map of symbols to materials

   static bool fixedSizeEnabled;                      ///< Set to true to fix the detail size to a particular value for all geometry
   static S32 fixedSize;                              ///< The fixed detail size value for all geometry

   //-----------------------------------------------------------------------

   /// Get the morph controller for this mesh (if any)
   const domMorph* getMorph()
   {
      if (instanceCtrl) {
         const domController* ctrl = daeSafeCast<domController>(instanceCtrl->getUrl().getElement());
         if (ctrl->getSkin())
            ctrl = daeSafeCast<domController>(ctrl->getSkin()->getSource().getElement());
         return ctrl ? ctrl->getMorph() : NULL;
      }
      return NULL;
   }

   S32 addMaterial(const char* symbol);

   bool checkGeometryType(const daeElement* element);
   void getPrimitives(const domGeometry* geometry);

   void getVertexData(  const domGeometry* geometry, F32 time, const MatrixF& objectOffset,
                        Vector<Point3F>& points, Vector<Point3F>& norms, Vector<ColorI>& colors, 
                        Vector<Point2F>& uvs, Vector<Point2F>& uv2s, bool appendValues);

   void getMorphVertexData(   const domMorph* morph, F32 time, const MatrixF& objectOffset,
                              Vector<Point3F>& points, Vector<Point3F>& norms, Vector<ColorI>& colors,
                              Vector<Point2F>& uvs, Vector<Point2F>& uv2s );

public:

   ColladaAppMesh(const domInstance_geometry* instance, ColladaAppNode* node);
   ColladaAppMesh(const domInstance_controller* instance, ColladaAppNode* node);
   ~ColladaAppMesh()
   {
      delete geomExt;
   }

   static void fixDetailSize(bool fixed, S32 size=2)
   {
      fixedSizeEnabled = fixed;
      fixedSize = size;
   }

   /// Get the name of this mesh
   ///
   /// @return A string containing the name of this mesh
   const char *getName(bool allowFixed=true);

   //-----------------------------------------------------------------------

   /// Get a floating point property value
   ///
   /// @param propName     Name of the property to get
   /// @param defaultVal   Reference to variable to hold return value
   ///
   /// @return True if a value was set, false if not
   bool getFloat(const char *propName, F32 &defaultVal)
   {
      return appNode->getFloat(propName,defaultVal);
   }

   /// Get an integer property value
   ///
   /// @param propName     Name of the property to get
   /// @param defaultVal   Reference to variable to hold return value
   ///
   /// @return True if a value was set, false if not
   bool getInt(const char *propName, S32 &defaultVal)
   {
      return appNode->getInt(propName,defaultVal);
   }

   /// Get a boolean property value
   ///
   /// @param propName     Name of the property to get
   /// @param defaultVal   Reference to variable to hold return value
   ///
   /// @return True if a value was set, false if not
   bool getBool(const char *propName, bool &defaultVal)
   {
      return appNode->getBool(propName,defaultVal);
   }

   /// Return true if this mesh is a skin
   bool isSkin()
   {
      if (instanceCtrl) {
         const domController* ctrl = daeSafeCast<domController>(instanceCtrl->getUrl().getElement());
         if (ctrl && ctrl->getSkin() &&
            (ctrl->getSkin()->getVertex_weights()->getV()->getValue().getCount() > 0))
            return true;
      }
      return false;
   }

   /// Get the skin data: bones, vertex weights etc
   void lookupSkinData();

   /// Check if the mesh visibility is animated
   ///
   /// @param appSeq   Start/end time to check
   ///
   /// @return True if the mesh visibility is animated, false if not
   bool animatesVis(const AppSequence* appSeq);

   /// Check if the material used by this mesh is animated
   ///
   /// @param appSeq   Start/end time to check
   ///
   /// @return True if the material is animated, false if not
   bool animatesMatFrame(const AppSequence* appSeq);

   /// Check if the mesh is animated
   ///
   /// @param appSeq   Start/end time to check
   ///
   /// @return True if the mesh is animated, false if not
   bool animatesFrame(const AppSequence* appSeq);

   /// Generate the vertex, normal and triangle data for the mesh.
   ///
   /// @param time           Time at which to generate the mesh data
   /// @param objectOffset   Transform to apply to the generated data (bounds transform)
   void lockMesh(F32 time, const MatrixF& objectOffset);

   /// Get the transform of this mesh at a certain time
   ///
   /// @param time   Time at which to get the transform
   ///
   /// @return The mesh transform at the specified time
   MatrixF getMeshTransform(F32 time);

   /// Get the visibility of this mesh at a certain time
   ///
   /// @param time   Time at which to get visibility info
   ///
   /// @return Visibility from 0 (invisible) to 1 (opaque)
   F32 getVisValue(F32 time);
};

#endif // _COLLADA_APPMESH_H_
