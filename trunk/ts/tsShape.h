//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TSSHAPE_H_
#define _TSSHAPE_H_

#ifndef __RESOURCE_H__
#include "core/resource.h"
#endif
#ifndef _TSMESH_H_
#include "ts/tsMesh.h"
#endif
#ifndef _TSINTEGERSET_H_
#include "ts/tsIntegerSet.h"
#endif
#ifndef _TSTRANSFORM_H_
#include "ts/tsTransform.h"
#endif
#ifndef _TSSHAPEALLOC_H_
#include "ts/tsShapeAlloc.h"
#endif
#ifndef _MATERIALLIST_H_
#include "materials/materialList.h"
#endif

#define DTS_EXPORTER_CURRENT_VERSION 124

class TSMaterialList;
class TSLastDetail;


/// TSShape stores generic data for a 3space model.
///
/// TSShape and TSShapeInstance act in conjunction to allow the rendering and
/// manipulation of a three dimensional model.
///
/// @note The material lists are the only thing that is not loaded in TSShape.
/// instead, they are loaded in TSShapeInstance because of a former restriction
/// on the resource manager where only one file could be opened at a time.
/// The resource manager restriction has been resolved, but the material
/// lists are still loaded in TSShapeInstance.
///
/// @see TSShapeInstance for a further discussion of the 3space system.
class TSShape
{
  public:
      enum
      {
         UniformScale   = BIT(0),
         AlignedScale   = BIT(1),
         ArbitraryScale = BIT(2),
         Blend          = BIT(3),
         Cyclic         = BIT(4),
         MakePath       = BIT(5),
         IflInit        = BIT(6),
         HasTranslucency= BIT(7),
         AnyScale       = UniformScale | AlignedScale | ArbitraryScale
      };

   /// Nodes hold the transforms in the shape's tree.  They are the bones of the skeleton.
   struct Node
   {
      S32 nameIndex;
      S32 parentIndex;

      // computed at runtime
      S32 firstObject;
      S32 firstChild;
      S32 nextSibling;
   };

   /// Objects hold renderable items (in particular meshes).
   ///
   /// Each object has a number of meshes associated with it.
   /// Each mesh corresponds to a different detail level.
   ///
   /// meshIndicesIndex points to numMeshes consecutive indices
   /// into the meshList and meshType vectors.  It indexes the
   /// meshIndexList vector (meshIndexList is merely a clearinghouse
   /// for the object's mesh lists).  Some indices may correspond to
   /// no mesh -- which means no mesh will be drawn for the part for
   /// the given detail level.  See comments on the meshIndexList
   /// for how null meshes are coded.
   ///
   /// @note Things are stored this way so that there are no pointers.
   ///       This makes serialization to disk dramatically simpler.
   struct Object
   {
      S32 nameIndex;
      S32 numMeshes;
      S32 startMeshIndex; ///< Index into meshes array.
      S32 nodeIndex;

      // computed at load
      S32 nextSibling;
      S32 firstDecal; // DEPRECATED
   };

   /// IFL Materials are used to animate material lists -- i.e., run through a series
   /// of frames of a material.
   ///
   /// They work by replacing a material in the material
   /// list so that it is transparent to the rest of the code.
   /// Offset time of each frame is stored in iflFrameOffsets vector, starting at index position
   /// firstFrameOffsetIndex..
   struct IflMaterial
   {
      S32 nameIndex; ///< File name with extension.
      S32 materialSlot;
      S32 firstFrame;
      S32 firstFrameOffTimeIndex;
      S32 numFrames;
   };

   /// A Sequence holds all the information necessary to perform a particular animation (sequence).
   ///
   /// Sequences index a range of keyframes. Keyframes are assumed to be equally spaced in time.
   ///
   /// Each node and object is either a member of the sequence or not.  If not, they are set to
   /// default values when we switch to the sequence unless they are members of some other active sequence.
   /// Blended sequences "add" a transform to the current transform of a node.  Any object animation of
   /// a blended sequence over-rides any existing object state.  Blended sequences are always
   /// applied after non-blended sequences.
   struct Sequence
   {
      S32 nameIndex;
      S32 numKeyframes;
      F32 duration;
      S32 baseRotation;
      S32 baseTranslation;
      S32 baseScale;
      S32 baseObjectState;
      S32 baseDecalState; // DEPRECATED
      S32 firstGroundFrame;
      S32 numGroundFrames;
      S32 firstTrigger;
      S32 numTriggers;
      F32 toolBegin;

      /// @name Bitsets
      /// These bitsets code whether this sequence cares about certain aspects of animation
      /// e.g., the rotation, translation, or scale of node transforms,
      /// or the visibility, frame or material frame of objects.
      /// @{

      TSIntegerSet rotationMatters;     ///< Set of nodes
      TSIntegerSet translationMatters;  ///< Set of nodes
      TSIntegerSet scaleMatters;        ///< Set of nodes
      TSIntegerSet visMatters;          ///< Set of objects
      TSIntegerSet frameMatters;        ///< Set of objects
      TSIntegerSet matFrameMatters;     ///< Set of objects
      TSIntegerSet decalMatters;        ///< DEPRECATED
      TSIntegerSet iflMatters;          ///< Set of IFLs
      /// @}

      S32 priority;
      U32 flags;
      U32 dirtyFlags; ///< determined at load time

      /// @name Flag Tests
      /// Each of these tests a different flag against the object's flag list
      /// to determine the attributes of the given object.
      /// @{

      bool testFlags(U32 comp) const      { return (flags&comp)!=0; }
      bool animatesScale() const          { return testFlags(AnyScale); }
      bool animatesUniformScale() const   { return testFlags(UniformScale); }
      bool animatesAlignedScale() const   { return testFlags(AlignedScale); }
      bool animatesArbitraryScale() const { return testFlags(ArbitraryScale); }
      bool isBlend() const                { return testFlags(Blend); }
      bool isCyclic() const               { return testFlags(Cyclic); }
      bool makePath() const               { return testFlags(MakePath); }
      /// @}

      /// @name IO
      /// @{

      void read(Stream *, bool readNameIndex = true);
      void write(Stream *, bool writeNameIndex = true);
      /// @}
   };

   /// Describes state of an individual object.  Includes everything in an object that can be
   /// controlled by animation.
   struct ObjectState
   {
      F32 vis;
      S32 frameIndex;
      S32 matFrameIndex;
   };

   /// When time on a sequence advances past a certain point, a trigger takes effect and changes
   /// one of the state variables to on or off. (State variables found in TSShapeInstance::mTriggerStates)
   struct Trigger
   {
      enum TriggerStates
      {
         StateOn = BIT(31),
         InvertOnReverse = BIT(30),
         StateMask = BIT(30)-1
      };

      U32 state; ///< One of TriggerStates
      F32 pos;
   };

   /// Details are used for render detail selection.
   ///
   /// As the projected size of the shape changes,
   /// a different node structure can be used (subShape) and a different objectDetail can be selected
   /// for each object drawn.   Either of these two parameters can also stay constant, but presumably
   /// not both.  If size is negative then the detail level will never be selected by the standard
   /// detail selection process.  It will have to be selected by name.  Such details are "utility
   /// details" because they exist to hold data (node positions or collision information) but not
   /// normally to be drawn.  By default there will always be a "Ground" utility detail.
   ///
   /// Note that this struct should always be 32bit aligned
   /// as its required by assembleShape/disassembleShape.
   struct Detail
   {
      S32 nameIndex;
      S32 subShapeNum;
      S32 objectDetailNum;
      F32 size;
      F32 averageError;
      F32 maxError;
      S32 polyCount;

      /// These values are new autobillboard settings stored
      /// as part of the Detail struct in version 26 and above.
      /// @{

      S32 bbDimension;     ///< The size of the autobillboard image.
      S32 bbDetailLevel;   ///< The detail to render as the autobillboard.
      U32 bbEquatorSteps;  ///< The number of autobillboard images to capture around the equator.
      U32 bbPolarSteps;    ///< The number of autobillboard images to capture along the pole.
      F32 bbPolarAngle;    ///< The angle in radians at which the top/bottom autobillboard images should be displayed.
      U32 bbIncludePoles;  ///< If non-zero then top and bottom images are generated for the autobillboard.

      /// @}
   };

   /// @name Collision Accelerators
   ///
   /// For speeding up buildpolylist and support calls.
   /// @{
   struct ConvexHullAccelerator {
      S32      numVerts;
      Point3F* vertexList;
      Point3F* normalList;
      U8**     emitStrings;
   };
   ConvexHullAccelerator* getAccelerator(S32 dl);
   /// @}


   /// @name Shape Vector Data
   /// @{

   Vector<Node> nodes;
   Vector<Object> objects;
   Vector<IflMaterial> iflMaterials;
   Vector<ObjectState> objectStates;
   Vector<S32> subShapeFirstNode;
   Vector<S32> subShapeFirstObject;
   Vector<S32> detailFirstSkin;
   Vector<S32> subShapeNumNodes;
   Vector<S32> subShapeNumObjects;
   Vector<Detail> details;
   Vector<Quat16> defaultRotations;
   Vector<Point3F> defaultTranslations;

   /// @}

   /// These are set up at load time, but memory is allocated along with loaded data
   /// @{

   Vector<S32> subShapeFirstTranslucentObject;
   Vector<TSMesh*> meshes;

   /// @}

   /// @name Alpha Vectors
   /// these vectors describe how to transition between detail
   /// levels using alpha. "alpha-in" next detail as intraDL goes
   /// from alphaIn+alphaOut to alphaOut. "alpha-out" current
   /// detail level as intraDL goes from alphaOut to 0.
   /// @note
   ///   - intraDL is at 1 when if shape were any closer to us we'd be at dl-1
   ///   - intraDL is at 0 when if shape were any farther away we'd be at dl+1
   /// @{

   Vector<F32> alphaIn;
   Vector<F32> alphaOut
      ;
   /// @}

   /// @name Resizeable vectors
   /// @{

   Vector<Sequence>                 sequences;
   Vector<Quat16>                   nodeRotations;
   Vector<Point3F>                  nodeTranslations;
   Vector<F32>                      nodeUniformScales;
   Vector<Point3F>                  nodeAlignedScales;
   Vector<Quat16>                   nodeArbitraryScaleRots;
   Vector<Point3F>                  nodeArbitraryScaleFactors;
   Vector<Quat16>                   groundRotations;
   Vector<Point3F>                  groundTranslations;
   Vector<Trigger>                  triggers;
   Vector<F32>                      iflFrameOffTimes;
   Vector<TSLastDetail*>            billboardDetails;
   Vector<ConvexHullAccelerator*>   detailCollisionAccelerators;
   Vector<String>                   names;

   /// @}

   TSMaterialList * materialList;

   /// @name Bounding
   /// @{

   F32 radius;
   F32 tubeRadius;
   Point3F center;
   Box3F bounds;

   /// @}

   // various...
   U32 mExporterVersion;
   F32 mSmallestVisibleSize;  ///< Computed at load time from details vector.
   S32 mSmallestVisibleDL;    ///< @see mSmallestVisibleSize
   S32 mReadVersion;          ///< File version that this shape was read from.
   U32 mFlags;                ///< hasTranslucancy, iflInit
   U32 data;                  ///< User-defined data storage.

   /// The GFX vertex format for all detail meshes in the shape.
   /// @see initVertexFeatures()
   GFXVertexFormat mVertexFormat;

   /// The GFX vertex size in bytes for all detail meshes in the shape.
   /// @see initVertexFeatures()
   U32 mVertSize;

   bool mSequencesConstructed;

   S8* mShapeData;

   // shape class has few methods --
   // just constructor/destructor, io, and lookup methods

   // constructor/destructor
   TSShape();
   ~TSShape();
   void init();
   void initMaterialList();    ///< you can swap in a new material list, but call this if you do
   bool preloadMaterialList(const Torque::Path &path); ///< called to preload and validate the materials in the mat list

   void setupBillboardDetails( const String &cachePath );

   /// Called from init() to calcuate the GFX vertex features for
   /// all detail meshes in the shape.
   void initVertexFeatures();

   bool getSequencesConstructed() const { return mSequencesConstructed; }
   void setSequencesConstructed(const bool c) { mSequencesConstructed = c; }

   /// @name Lookup Animation Info
   /// indexed by keyframe number and offset (which object/node/decal
   /// of the animated objects/nodes/decals you want information for).
   /// @{

   QuatF & getRotation(const Sequence & seq, S32 keyframeNum, S32 rotNum, QuatF *) const;
   const Point3F & getTranslation(const Sequence & seq, S32 keyframeNum, S32 tranNum) const;
   F32 getUniformScale(const Sequence & seq, S32 keyframeNum, S32 scaleNum) const;
   const Point3F & getAlignedScale(const Sequence & seq, S32 keyframeNum, S32 scaleNum) const;
   TSScale & getArbitraryScale(const Sequence & seq, S32 keyframeNum, S32 scaleNum, TSScale *) const;
   const ObjectState & getObjectState(const Sequence & seq, S32 keyframeNum, S32 objectNum) const;
   /// @}

   /// build LOS collision detail
   void computeAccelerator(S32 dl);
   bool buildConvexHull(S32 dl) const;
   void computeBounds(S32 dl, Box3F & bounds) const; // uses default transforms to compute bounding box around a detail level
                                                     // see like named method on shapeInstance if you want to use animated transforms

   /// @name Lookup Methods
   /// @{

   /// Returns index into the name vector that equals the passed name.
   S32 findName( const String &name ) const;
   
   /// Returns name string at the passed name vector index.
   const String& getName( S32 nameIndex ) const;
   
   /// Returns name string for mesh at the passed index.
   const String& getMeshName( S32 meshIndex ) const;
   
   /// Returns name string for node at the passed index.
   const String& getNodeName( S32 nodeIndex ) const;

	S32 getTargetCount() const;
	const String& getTargetName( S32 mapToNameIndex ) const;

   S32 findNode(S32 nameIndex) const;
   S32 findNode(const String &name) const { return findNode(findName(name)); }

   S32 findObject(S32 nameIndex) const;
   S32 findObject(const String &name) const { return findObject(findName(name)); }

   S32 findIflMaterial(S32 nameIndex) const;
   S32 findIflMaterial(const String &name) const { return findIflMaterial(findName(name)); }

   S32 findDetail(S32 nameIndex) const;
   S32 findDetail(const String &name) const { return findDetail(findName(name)); }

   S32 findSequence(S32 nameIndex) const;
   S32 findSequence(const String &name) const { return findSequence(findName(name)); }

   S32 getSubShapeForNode(S32 nodeIndex);
   S32 getSubShapeForObject(S32 objIndex);
   void getSubShapeDetails(S32 subShapeIndex, Vector<S32>& validDetails);

   void getNodeWorldTransform(S32 nodeIndex, MatrixF* mat) const;
   void getNodeKeyframe(S32 nodeIndex, const TSShape::Sequence& seq, S32 keyframe, MatrixF* mat) const;
   void getNodeObjects(S32 nodeIndex, Vector<S32>& nodeObjects);
   void getNodeChildren(S32 nodeIndex, Vector<S32>& nodeChildren);

   void getObjectDetails(S32 objIndex, Vector<S32>& objDetails);

   bool findMeshIndex(const String &meshName, S32& objIndex, S32& meshIndex);
   TSMesh* findMesh(const String &meshName);

   bool hasTranslucency() const { return (mFlags & HasTranslucency)!=0; }

   const GFXVertexFormat* getVertexFormat() const { return &mVertexFormat; }

   U32 getVertexSize() const { return mVertSize; }

   /// @}

   /// @name Alpha Transitions
   /// These control default values for alpha transitions between detail levels
   /// @{

   static F32 smAlphaOutLastDetail;
   static F32 smAlphaInBillboard;
   static F32 smAlphaOutBillboard;
   static F32 smAlphaInDefault;
   static F32 smAlphaOutDefault;
   /// @}

   /// don't load this many of the highest detail levels (although we always
   /// load one renderable detail if there is one)
   static S32 smNumSkipLoadDetails;

   /// by default we initialize shape when we read...
   static bool smInitOnRead;

   /// @name Version Info
   /// @{

   /// Most recent version...the one we write
   static S32 smVersion;
   /// Version currently being read, only valid during read
   static S32 smReadVersion;
   static const U32 smMostRecentExporterVersion;
   ///@}

   /// @name Persist Methods
   /// Methods for saving/loading shapes to/from streams
   /// @{

   void write(Stream *);
   bool read(Stream *);
   void readOldShape(Stream * s, S32 * &, S16 * &, S8 * &, S32 &, S32 &, S32 &);
   void writeName(Stream *, S32 nameIndex);
   S32  readName(Stream *, bool addName);

   /// Initializes our TSShape to be ready to receive put mesh data
   void createEmptyShape();

   void exportSequences(Stream *);
   bool importSequences(Stream *, const String& sequencePath);

   void readIflMaterials(const char* shapePath);
   /// @}

   /// @name Persist Helper Functions
   /// @{

   static TSShapeAlloc smTSAlloc;

   void fixEndian(S32 *, S16 *, S8 *, S32, S32, S32);
   /// @}

   /// @name Memory Buffer Transfer Methods
   /// uses TSShape::Alloc structure
   /// @{

   void assembleShape();
   void disassembleShape();
   ///@}

   /// mem buffer transfer helper (indicate when we don't want to include a particular mesh/decal)
   bool checkSkip(S32 meshNum, S32 & curObject, S32 skipDL);

   void fixupOldSkins(S32 numMeshes, S32 numSkins, S32 numDetails, S32 * detailFirstSkin, S32 * detailNumSkins);

   /// @name Shape Editing
   /// @{
   S32 addName(const String& name);
   bool removeName(const String& name);
   S32 addDetail(const String& dname, S32 size, S32 subShapeNum);

   S32 addBillboardDetail( const String& dname, 
                           S32 size, 
                           S32 numEquatorSteps, 
                           S32 numPolarSteps, 
                           S32 dl, 
                           S32 dim, 
                           bool includePoles, 
                           F32 polarAngle );

   static TSMesh* createMeshCube(const Point3F& center, const Point3F& extents);

   bool renameNode(const String& oldName, const String& newName);
   bool renameObject(const String& oldName, const String& newName);
   bool renameSequence(const String& oldName, const String& newName);

   bool setNodeTransform(const String& name, const Point3F& pos, const QuatF& rot);
   bool addNode(const String& name, const String& parentName, const Point3F& pos, const QuatF& rot);
   bool removeNode(const String& name);

   S32 addObject(const String& objName, S32 subShapeIndex);
   void addMeshToObject(S32 objIndex, S32 meshIndex, TSMesh* mesh);
   void removeMeshFromObject(S32 objIndex, S32 meshIndex);
   bool setObjectNode(const String& objName, const String& nodeName);
   bool removeObject(const String& objName);

   bool addMesh(TSShape* srcShape, const String& srcMeshName, const String& meshName);
   bool addMesh(TSMesh* mesh, const String& meshName);
   bool setMeshSize(const String& meshName, S32 size);
   bool removeMesh(const String& meshName);

   bool addSequence(const Torque::Path& path, const String& fromSeq, const String& name, S32 startFrame, S32 endFrame, S32* totalFrames=NULL);
   bool removeSequence(const String& name);

   bool addTrigger(const String& seqName, S32 keyframe, S32 state);
   bool removeTrigger(const String& seqName, S32 keyframe, S32 state);

   bool setSequenceBlend(const String& seqName, bool blend, const String& blendRefSeqName, S32 blendRefFrame);
   bool setSequenceGroundSpeed(const String& seqName, const Point3F& trans, const Point3F& rot);
   /// @}
};


/// Specialized material list for 3space objects
///
/// @note Reflectance amounts on 3space objects are determined by
///       the alpha channel of the base material texture

class TSMaterialList : public MaterialList
{
   typedef MaterialList Parent;

   Vector<U32> mFlags;
   Vector<U32> mReflectanceMaps;
   Vector<U32> mBumpMaps;
   Vector<U32> mDetailMaps;
   Vector<F32> mDetailScales;
   Vector<F32> mReflectionAmounts;

   bool mNamesTransformed;

   void allocate(U32 sz);

  public:

   enum
   {
      S_Wrap             = BIT(0),
      T_Wrap             = BIT(1),
      Translucent        = BIT(2),
      Additive           = BIT(3),
      Subtractive        = BIT(4),
      SelfIlluminating   = BIT(5),
      NeverEnvMap        = BIT(6),
      NoMipMap           = BIT(7),
      MipMap_ZeroBorder  = BIT(8),
      IflMaterial        = BIT(27),
      IflFrame           = BIT(28),
      AuxiliaryMap       = BIT(29) | BIT(30) | BIT(31) // DEPRECATED
   };

   TSMaterialList(U32 materialCount, const char **materialNames, const U32 * materialFlags,
                  const U32 * reflectanceMaps, const U32 * bumpMaps, const U32 * detailMaps,
                  const F32 * detailScales, const F32 * reflectionAmounts);
   TSMaterialList();
   TSMaterialList(const TSMaterialList*);
   ~TSMaterialList();
   void free();

   /// @name Lookups
   /// @{

   GFXTexHandle * getReflectionMap(U32 index) { return mReflectanceMaps[index] == 0xFFFFFFFF ? NULL : &getMaterial(mReflectanceMaps[index]); }
   F32 getReflectionAmount(U32 index) { return mReflectionAmounts[index]; }
   GFXTexHandle * getBumpMap(U32 index) { return mBumpMaps[index] == 0xFFFFFFFF ? NULL : &getMaterial(mBumpMaps[index]); }
   GFXTexHandle * getDetailMap(U32 index) { return mDetailMaps[index] == 0xFFFFFFFF ? NULL : &getMaterial(mDetailMaps[index]); }
   F32 getDetailMapScale(U32 index) { return mDetailScales[index]; }
   bool reflectionInAlpha(U32 index) { return mReflectanceMaps[index] == index; }
   bool isIFL( U32 index )
   { 
      if( index < mFlags.size() )
      {
         return mFlags[index] & IflMaterial;
      }
      else
      {
         return false;
      }
   }

   /// @}

   U32 getFlags(U32 index);
   void setFlags(U32 index, U32 value);

   void remap(U32 toIndex, U32 fromIndex); ///< support for ifl sequences

   bool setMaterial(U32 index, const Torque::Path &texturePath); // use to support reskinning

   /// pre-load only ... support for ifl sequences
   void push_back(const String &name, U32 flags,
                  U32 a=0xFFFFFFFF, U32 b=0xFFFFFFFF, U32 c=0xFFFFFFFF,
                  F32 dm=1.0f, F32 em=1.0f);
   void push_back(const char * name, U32 flags, Material* mat);

   /// @name IO
   /// Functions for reading/writing to/from streams
   /// @{

   bool write(Stream &);
   bool read(Stream &);
   /// @}

protected:
   virtual void mapMaterial( U32 index );
};

#define TSNode TSShape::Node
#define TSObject TSShape::Object
#define TSSequence TSShape::Sequence
#define TSDetail TSShape::Detail

inline QuatF & TSShape::getRotation(const Sequence & seq, S32 keyframeNum, S32 rotNum, QuatF * quat) const
{
   return nodeRotations[seq.baseRotation + rotNum*seq.numKeyframes + keyframeNum].getQuatF(quat);
}

inline const Point3F & TSShape::getTranslation(const Sequence & seq, S32 keyframeNum, S32 tranNum) const
{
   return nodeTranslations[seq.baseTranslation + tranNum*seq.numKeyframes + keyframeNum];
}

inline F32 TSShape::getUniformScale(const Sequence & seq, S32 keyframeNum, S32 scaleNum) const
{
   return nodeUniformScales[seq.baseScale + scaleNum*seq.numKeyframes + keyframeNum];
}

inline const Point3F & TSShape::getAlignedScale(const Sequence & seq, S32 keyframeNum, S32 scaleNum) const
{
   return nodeAlignedScales[seq.baseScale + scaleNum*seq.numKeyframes + keyframeNum];
}

inline TSScale & TSShape::getArbitraryScale(const Sequence & seq, S32 keyframeNum, S32 scaleNum, TSScale * scale) const
{
   nodeArbitraryScaleRots[seq.baseScale + scaleNum*seq.numKeyframes + keyframeNum].getQuatF(&scale->mRotate);
   scale->mScale = nodeArbitraryScaleFactors[seq.baseScale + scaleNum*seq.numKeyframes + keyframeNum];
   return *scale;
}

inline const TSShape::ObjectState & TSShape::getObjectState(const Sequence & seq, S32 keyframeNum, S32 objectNum) const
{
   return objectStates[seq.baseObjectState + objectNum*seq.numKeyframes + keyframeNum];
}

#endif
