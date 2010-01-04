//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "core/resourceManager.h"
#include "core/stream/bitStream.h"
#include "core/stream/fileStream.h"
#include "core/stream/memStream.h"
#include "core/fileObject.h"

#include "ts/tsShapeConstruct.h"
#include "ts/tsShapeInstance.h"
#include "console/consoleTypes.h"

#define MAX_PATH_LENGTH 256


//#define DEBUG_SPEW


static EnumTable::Enums gUpAxisEnums[] =
{
   { UPAXISTYPE_X_UP,   "X_AXIS" },
   { UPAXISTYPE_Y_UP,   "Y_AXIS" },
   { UPAXISTYPE_Z_UP,   "Z_AXIS" },
   { UPAXISTYPE_COUNT,  "DEFAULT" },
};
EnumTable gUpAxisEnumTable( UPAXISTYPE_COUNT+1, gUpAxisEnums );

static EnumTable::Enums gLodTypeEnums[] =
{
   { ColladaUtils::ImportOptions::DetectDTS,       "DetectDTS" },
   { ColladaUtils::ImportOptions::SingleSize,      "SingleSize" },
   { ColladaUtils::ImportOptions::TrailingNumber,  "TrailingNumber" },
};
EnumTable gLodTypeEnumTable( ColladaUtils::ImportOptions::NumLodTypes, gLodTypeEnums );

//-----------------------------------------------------------------------------

ResourceRegisterPostLoadSignal< TSShape > TSShapeConstructor::_smAutoLoad( &TSShapeConstructor::_onTSShapeLoaded );
ResourceRegisterUnloadSignal< TSShape > TSShapeConstructor::_smAutoUnload( &TSShapeConstructor::_onTSShapeUnloaded );

void TSShapeConstructor::_onTSShapeLoaded( Resource< TSShape >& resource )
{
   TSShapeConstructor* ctor = findShapeConstructor( resource.getPath().getFullPath() );
   if( ctor )
      ctor->_onLoad( resource );
}

void TSShapeConstructor::_onTSShapeUnloaded( const Torque::Path& path, TSShape* shape )
{
   TSShapeConstructor* ctor = findShapeConstructor( path.getFullPath() );
   if( ctor )
      ctor->_onUnload();
}

//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(TSShapeConstructor);

TSShapeConstructor::TSShapeConstructor()
 : mShapePath("")
{
   mShape = NULL;
}

TSShapeConstructor::~TSShapeConstructor()
{
}

bool TSShapeConstructor::addSequenceFromField( void* obj, const char* data )
{
   TSShapeConstructor *pObj = static_cast<TSShapeConstructor*>( obj );

   if ( data && data[0] )
      pObj->mSequences.push_back( FileName(data) );

   return false;
}

void TSShapeConstructor::initPersistFields()
{
   addGroup( "Media" );
   addField( "baseShape",     TypeStringFilename, Offset(mShapePath, TSShapeConstructor) );
   endGroup( "Media" );

   addGroup( "Collada" );
   addField( "upAxis",        TypeEnum, Offset(mOptions.upAxis, TSShapeConstructor), 1, &gUpAxisEnumTable, "Override for the COLLADA <up_axis> element" );
   addField( "unit",          TypeF32, Offset(mOptions.unit, TSShapeConstructor), 1, 0, "Override for the COLLADA <unit> element" );
   addField( "lodType",       TypeEnum, Offset(mOptions.lodType, TSShapeConstructor), 1, &gLodTypeEnumTable, "Controls how LOD sizes are determined" );
   addField( "singleDetailSize", TypeS32, Offset(mOptions.singleDetailSize, TSShapeConstructor), 1, 0, "Detail size for all meshes in the model" );
   addField( "matNamePrefix", TypeRealString, Offset(mOptions.matNamePrefix, TSShapeConstructor), 1, 0, "Prefix to apply to COLLADA material names" );
   addField( "alwaysImport",  TypeRealString, Offset(mOptions.alwaysImport, TSShapeConstructor), 1, 0, "TAB separated patterns of nodes to import even if in neverImport list" );
   addField( "neverImport",   TypeRealString, Offset(mOptions.neverImport, TSShapeConstructor), 1, 0, "TAB separated patterns of nodes to ignore on loading" );
   addField( "ignoreNodeScale",  TypeBool, Offset(mOptions.ignoreNodeScale, TSShapeConstructor), 1, 0, "Ignore <scale> elements inside <node>s" );
   addField( "adjustCenter",  TypeBool, Offset(mOptions.adjustCenter, TSShapeConstructor), 1, 0, "Translate model so origin is at the center" );
   addField( "adjustFloor",   TypeBool, Offset(mOptions.adjustFloor, TSShapeConstructor), 1, 0, "Translate model so origin is at the bottom" );
   addField( "forceUpdateMaterials",   TypeBool, Offset(mOptions.forceUpdateMaterials, TSShapeConstructor), 1, 0, "Force update of materials.cs, even if Materials already exist" );
   endGroup( "Collada" );

   addGroup( "Sequences" );
   addProtectedField( "sequence", TypeStringFilename, NULL, &addSequenceFromField, &emptyStringProtectedGetFn, "" );
   endGroup( "Sequences" );

   Parent::initPersistFields();
}

TSShapeConstructor* TSShapeConstructor::findShapeConstructor(const FileName& path)
{
   SimGroup *group;
   if (Sim::findObject( "TSShapeConstructorGroup", group ))
   {
      // Find the TSShapeConstructor object for the given shape file
      for (S32 i = 0; i < group->size(); i++)
      {
         TSShapeConstructor* tss = dynamic_cast<TSShapeConstructor*>( group->at(i) );
         if ( tss->mShapePath.equal( path, String::NoCase ) )
            return tss;
      }
   }
   return NULL;
}

//-----------------------------------------------------------------------------
bool TSShapeConstructor::onAdd()
{
   if ( !Parent::onAdd() )
      return false;

   // Prevent multiple objects pointing at the same shape file
   TSShapeConstructor* tss = findShapeConstructor( mShapePath );
   if ( tss )
   {
      Con::errorf("TSShapeConstructor::onAdd failed: %s is already referenced by "
         "another TSShapeConstructor object (%s - %d)", mShapePath.c_str(),
         tss->getName(), tss->getId());
      return false;
   }

   // Add to the TSShapeConstructor group (for lookups)
   SimGroup *group;
   if ( !Sim::findObject( "TSShapeConstructorGroup", group ) )
   {
      group = new SimGroup();
      if ( !group->registerObject("TSShapeConstructorGroup") )
      {
         Con::errorf("TSShapeConstructor::onAdd failed: Could not register "
            "TSShapeConstructorGroup");
         return false;
      }
      Sim::getRootGroup()->addObject( group );
   }
   group->addObject( this );

   // This is only here for backwards compatibility!
   //
   // If we have no sequences, it may be using the older sequence# syntax.
   // Check for dynamic fields of that pattern and add them into the sequence vector.
   if ( mSequences.empty() )
   {
      for ( U32 idx = 0; idx < MaxLegacySequences; idx++ )
      {
         String field = String::ToString( "sequence%d", idx );
         const char *data = getDataField( StringTable->insert(field.c_str()), NULL );

         // Break at first field not used
         if ( !data || !data[0] )
            break;

         // By pushing the field thru Con::setData for TypeStringFilename
         // we get the default filename expansion.  If we didn't do this
         // then we would have unexpanded ~/ in the file paths.
         String expanded;
         Con::setData( TypeStringFilename, &expanded, 0, 1, &data );
         addSequenceFromField( this, expanded.c_str() );
      }
   }

   // If an instance of this shape has already been loaded, call onLoad now
   Resource<TSShape> shape = ResourceManager::get().find( mShapePath );
   if ( shape )
      _onLoad( shape );

   return true;
}

//-----------------------------------------------------------------------------

void TSShapeConstructor::_onLoad(TSShape* shape)
{
   #ifdef DEBUG_SPEW
   Con::printf( "[TSShapeConstructor] attaching to shape '%s'", mShapePath.c_str() );
   #endif
   
   mShape = shape;
   mChangeSet.clear();

   // Store initial (empty) sequence data
   mSeqData.clear();
   mSeqData.setSize(mShape->sequences.size());
   for ( U32 i = 0; i < mShape->sequences.size(); i++)
   {
      mSeqData[i].from = mShape->getName(mShape->sequences[i].nameIndex).c_str();
      mSeqData[i].from += "\t";
      mSeqData[i].total = mShape->sequences[i].numKeyframes;
      mSeqData[i].end = mSeqData[i].total - 1;
   }

   // Add sequences defined using field syntax
   for ( S32 i = 0; i < mSequences.size(); i++ )
   {
      if ( mSequences[i].isEmpty() )
         continue;

      char fileBuf[MAX_PATH_LENGTH];
      dStrcpy(fileBuf, mSequences[i]);

      // Determine if there is a sequence name after the filename, and if so,
      // split the filename from the sequence name.
      char* split = dStrchr(fileBuf, ' ');
      char* split2 = dStrchr(fileBuf, '\t');
      if (split2 && (!split || (split2 < split)))
         split = split2;

      char* destName = 0;
      if (split)
      {
         *split = '\0';
         destName = split + 1;
         while (*destName && dIsspace(*destName))
            destName++;
      }

      if (mShape->addSequence(fileBuf, String::EmptyString, destName, 0, -1))
      {
         const char* argv[] = { fileBuf, destName };
         mChangeSet.add("addSequence", 2, argv);

         // Store information about how this sequence was created
         mSeqData.increment();
         mSeqData.last().from = String::ToString("%s\t", fileBuf);
         mSeqData.last().total = mShape->sequences.last().numKeyframes;
         mSeqData.last().start = 0;
         mSeqData.last().end = mSeqData.last().total - 1;
      }
   }
   
   // Call script function
   if( isMethod( "onLoad" ) )
      Con::executef(this, "onLoad", Con::getIntArg(getId()));
}

//-----------------------------------------------------------------------------

void TSShapeConstructor::_onUnload()
{
   #ifdef DEBUG_SPEW
   Con::printf( "[TSShapeConstructor] detaching from '%s'", mShapePath.c_str() );
   #endif
   
   if( isMethod( "onUnload" ) )
      Con::executef( this, "onUnload", Con::getIntArg( getId() ) );
      
   mShape = NULL;
   mSeqData.clear();
}

//-----------------------------------------------------------------------------
// Storage

bool TSShapeConstructor::writeField(StringTableEntry fieldname, const char *value)
{
   // Ignore the sequence fields (these are written as 'addSequence' commands instead)
   if ( dStrnicmp( fieldname, "sequence", 8 ) == 0 )
      return false;

   return Parent::writeField( fieldname, value );
}

void TSShapeConstructor::write(Stream& stream, U32 tabStop, U32 flags)
{
   // Only output selected objects if they want that.
   if ((flags & SelectedOnly) && !isSelected())
      return;

   // Write the change-set into the onLoad method
   if ( !mChangeSet.empty() )
   {
      // Generate an optimized version of the change-set for writing
      ChangeSet output;
      ChangeSet::optimize(mChangeSet, output);

      // Remove all __backup__ sequences (used during Shape Editing)
      if (mShape)
      {
         for (S32 i = 0; i < mShape->sequences.size(); i++)
         {
            const char* seqName = mShape->getName(mShape->sequences[i].nameIndex);
            if (dStrStartsWith(seqName, "__backup__"))
               output.add("removeSequence", 1, &seqName);
         }
      }

      stream.writeText(avar("function %s::onLoad(%%this)\r\n{\r\n", getName()));
      output.write(stream);
      stream.writeText("}\r\n\r\n");
   }

   // Then write the object itself (singleton instead of new, so can't use Parent::write)
   stream.writeText(avar("singleton %s(%s)\r\n{\r\n", getClassName(), getName() ? getName() : ""));
   writeFields(stream, 1);
   stream.writeText("};\r\n");
}

//-----------------------------------------------------------------------------
// Console utility methods
bool parsePositionAndRotation(const char* str, Point3F& pos, QuatF& rot)
{
   pos.set(0,0,0);
   rot.set(0,0,0,1);

   AngAxisF aa;
   S32 count = dSscanf(str, "%g %g %g %g %g %g %g",
      &pos.x, &pos.y, &pos.z, &aa.axis.x, &aa.axis.y, &aa.axis.z, &aa.angle);

   if ((count != 3) && (count != 7))
   {
      Con::printf("Failed to parse pos and rot \"p.x p.y p.z aa.x aa.y aa.z aa.a\" from '%s'", str);
      return false;
   }

   rot.set(aa);
   return true;
}

// These macros take care of doing node, sequence and object lookups, including
// printing error messages to the console if the element cannot be found.

// Do a node lookup and allow the root node name ("")
#define GET_NODE_INDEX_ALLOW_ROOT(var, name, ret)                    \
   S32 var##Index = -1;                                              \
   if (name[0])                                                      \
   {                                                                 \
      var##Index = object->mShape->findNode(name);                   \
      if (var##Index < 0)                                            \
      {                                                              \
         Con::errorf("%s: Could not find node '%s'", argv[0], name); \
         return ret;                                                 \
      }                                                              \
   }                                                                 \
   TSShape::Node* var = var##Index < 0 ? NULL : &object->mShape->nodes[var##Index]; \
   TORQUE_UNUSED(var##Index);                                        \
   TORQUE_UNUSED(var)

// Do a node lookup, root node ("") is not allowed
#define GET_NODE_INDEX_NO_ROOT(var, name, ret)                       \
   S32 var##Index = object->mShape->findNode(name);                  \
   if (var##Index < 0)                                               \
   {                                                                 \
      Con::errorf("%s: Could not find node '%s'", argv[0], name);    \
      return ret;                                                    \
   }                                                                 \
   TSShape::Node* var = &object->mShape->nodes[var##Index];          \
   TORQUE_UNUSED(var##Index);                                        \
   TORQUE_UNUSED(var)

// Do an object lookup
#define GET_OBJECT(var, name, ret)                                   \
   S32 var##Index = object->mShape->findObject(name);                \
   if (var##Index < 0)                                               \
   {                                                                 \
      Con::errorf("%s: Could not find object '%s'", argv[0], name);  \
      return ret;                                                    \
   }                                                                 \
   TSShape::Object* var = &object->mShape->objects[var##Index];      \
   TORQUE_UNUSED(var##Index);                                        \
   TORQUE_UNUSED(var)

// Do a mesh lookup
#define GET_MESH(var, name, ret)                                     \
   TSMesh* var = object->mShape->findMesh(name);                     \
   if (!var)                                                         \
   {                                                                 \
      Con::errorf("%s: Could not find mesh '%s'", argv[0], name);    \
      return ret;                                                    \
   }

// Do a sequence lookup
#define GET_SEQUENCE(var, name, ret)                                 \
   S32 var##Index = object->mShape->findSequence(name);              \
   if (var##Index < 0)                                               \
   {                                                                 \
      Con::errorf("%s: Could not find sequence named '%s'", argv[0], name);   \
      return ret;                                                    \
   }                                                                 \
   TSShape::Sequence* var = &object->mShape->sequences[var##Index];  \
   TSShapeConstructor::SequenceData* var##Data = &object->mSeqData[var##Index];  \
   TORQUE_UNUSED(var##Index);                                        \
   TORQUE_UNUSED(var);                                               \
   TORQUE_UNUSED(var##Data);


// Helper macro to add a console command to the change set
#define ADD_TO_CHANGE_SET()   object->addToChangeSet(argv[0], argc-2, argv+2);

//-----------------------------------------------------------------------------
// CHANGE-SET MANAGEMENT
ConsoleMethod(TSShapeConstructor, popChangeCommands, void, 3, 3,
   "(int count) - only used by ShapeEditor for managing the undo stack")
{
   object->popFromChangeSet(dAtoi(argv[2]));
}

//-----------------------------------------------------------------------------
// DUMP
ConsoleMethod(TSShapeConstructor, dumpShape, void, 2, 3,
   "([string dump_filename])")
{
   TSShapeInstance* tsi = new TSShapeInstance(object->mShape, false);
   
   if( !tsi->getShape() )
   {
      Con::errorf( "TSShapeConstructor::dumpShape() - shape not loaded" );
      return;
   }

   if (argc == 2)
   {
      // Dump the constructed shape to a memory stream
      MemStream* dumpStream = new MemStream(8192);
      tsi->dump(*dumpStream);

      // Write stream to the console
      U32 end = dumpStream->getPosition();
      dumpStream->setPosition(0);
      while (dumpStream->getPosition() < end)
      {
         char line[1024];
         dumpStream->readLine((U8*)line, sizeof(line));
         Con::printf(line);
      }

      delete dumpStream;
   }
   else
   {
      // Dump constructed shape to file
      char filenameBuf[1024];
      Con::expandScriptFilename(filenameBuf, sizeof(filenameBuf), argv[2]);

      FileStream* dumpStream = new FileStream;
      if (dumpStream->open(filenameBuf, Torque::FS::File::Write))
      {
         tsi->dump(*dumpStream);
         dumpStream->close();
      }
      else
         Con::errorf("dumpShape failed: Could not open file '%s' for writing", filenameBuf);

      delete dumpStream;
   }

   delete tsi;
}

ConsoleMethod(TSShapeConstructor, saveShape, void, 3, 3,
   "(string filename)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::saveShape() - shape not loaded" );
      return;
   }

   char filenameBuf[1024];
   Con::expandScriptFilename(filenameBuf, sizeof(filenameBuf), argv[2]);

   FileStream* dtsStream = new FileStream;
   if (dtsStream->open(filenameBuf, Torque::FS::File::Write))
   {
      object->mShape->write(dtsStream);
      dtsStream->close();
   }
   else
   {
      Con::errorf("saveShape failed: Could not open '%s' for writing", filenameBuf);
   }
   delete dtsStream;
}

/* @todo:Would be useful for shape editor
ConsoleMethod(TSShapeConstructor, reload, void, 2, 2,
   "()")
{
   // Force reload of shape resource
   Resource<TSShape> shape = ResourceManager::get().find( object->mShapePath );
   if ( shape )
      shape->reload();
}
*/

//-----------------------------------------------------------------------------
// NODES
ConsoleMethod(TSShapeConstructor, getNodeCount, S32, 2, 2,
   "()")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getNodeCount() - shape not loaded" );
      return 0;
   }
   
   return object->mShape->nodes.size();
}

ConsoleMethod(TSShapeConstructor, getNodeIndex, S32, 3, 3,
   "(string node_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getNodeIndex() - shape not loaded" );
      return -1;
   }

   return object->mShape->findNode(argv[2]);
}

ConsoleMethod(TSShapeConstructor, getNodeName, const char*, 3, 3,
   "(int index)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getNodeName() - shape not loaded" );
      return "";
   }

   S32 index = dAtoi(argv[2]);
   if ((index < 0) || (index >= object->mShape->nodes.size()))
   {
      Con::errorf("getNodeName: index out of range (0-%d)", object->mShape->nodes.size()-1);
      return "";
   }
   return object->mShape->getName(object->mShape->nodes[index].nameIndex);
}

ConsoleMethod(TSShapeConstructor, getNodeParentName, const char*, 3, 3,
   "(string node_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getNodeParentName() - shape not loaded" );
      return "";
   }

   GET_NODE_INDEX_NO_ROOT(node, argv[2], "");

   if (node->parentIndex < 0)
      return "";
   else
      return object->mShape->getName(object->mShape->nodes[node->parentIndex].nameIndex);
}

ConsoleMethod(TSShapeConstructor, setNodeParent, bool, 4, 4,
  "(string node_name, string parent_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::setNodeParent() - shape not loaded" );
      return false;
   }

   GET_NODE_INDEX_NO_ROOT(node, argv[2], false);
   GET_NODE_INDEX_ALLOW_ROOT(parent, argv[3], false);

   node->parentIndex = parentIndex;
   ADD_TO_CHANGE_SET();

   return true;
}

ConsoleMethod(TSShapeConstructor, getNodeChildCount, S32, 3, 3,
   "(string node_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getNodeChildCount() - shape not loaded" );
      return 0;
   }

   GET_NODE_INDEX_ALLOW_ROOT(node, argv[2], 0);

   Vector<S32> nodeChildren;
   object->mShape->getNodeChildren(nodeIndex, nodeChildren);
   return nodeChildren.size();
}

ConsoleMethod(TSShapeConstructor, getNodeChildName, const char*, 4, 4,
   "(string node_name, int index)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getNodeChildName() - shape not loaded" );
      return "";
   }

   GET_NODE_INDEX_ALLOW_ROOT(node, argv[2], "");

   Vector<S32> nodeChildren;
   object->mShape->getNodeChildren(nodeIndex, nodeChildren);

   S32 childIndex = dAtoi(argv[3]);
   if ((childIndex < 0) || (childIndex >= nodeChildren.size()))
   {
      Con::errorf("getNodeChildName: Index out of range (0-%d)", nodeChildren.size()-1);
      return "";
   }
   return object->mShape->getName(object->mShape->nodes[nodeChildren[childIndex]].nameIndex);
}

ConsoleMethod(TSShapeConstructor, getNodeObjectCount, S32, 3, 3,
   "(string node_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getNodeObjectCount() - shape not loaded" );
      return 0;
   }

   GET_NODE_INDEX_ALLOW_ROOT(node, argv[2], 0);

   Vector<S32> nodeObjects;
   object->mShape->getNodeObjects(nodeIndex, nodeObjects);
   return nodeObjects.size();
}

ConsoleMethod(TSShapeConstructor, getNodeObjectName, const char*, 4, 4,
   "(string node_name, int object_index)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getNodeObjectName() - shape not loaded" );
      return "";
   }

   GET_NODE_INDEX_ALLOW_ROOT(node, argv[2], "");

   Vector<S32> nodeObjects;
   object->mShape->getNodeObjects(nodeIndex, nodeObjects);

   S32 index = dAtoi(argv[3]);
   if ((index < 0) || (index >= nodeObjects.size()))
   {
      Con::errorf("getNodeObjectName: Index out of range (0-%d)", nodeObjects.size()-1);
      return "";
   }
   return object->mShape->getName(object->mShape->objects[nodeObjects[index]].nameIndex);
}

ConsoleMethod(TSShapeConstructor, getNodeTransform, const char*, 3, 4,
   "(string node_name, [boolean isworld])")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getNodeTransform() - shape not loaded" );
      return "";
   }

   GET_NODE_INDEX_NO_ROOT(node, argv[2], "0 0 0 0 0 1 0");

   // Get the node transform
   Point3F pos;
   AngAxisF aa;

   if ((argc > 3) && dAtob(argv[3]))
   {
      // World transform
      MatrixF mat;
      object->mShape->getNodeWorldTransform(nodeIndex, &mat);
      pos = mat.getPosition();
      aa.set(mat);
   }
   else
   {
      // Local transform
      pos = object->mShape->defaultTranslations[nodeIndex];
      const Quat16& q16 = object->mShape->defaultRotations[nodeIndex];
      aa.set(q16.getQuatF());
   }

   char* returnBuffer = Con::getReturnBuffer(256);
   dSprintf(returnBuffer, 256, "%g %g %g %g %g %g %g",
      pos.x, pos.y, pos.z, aa.axis.x, aa.axis.y, aa.axis.z, aa.angle);

   return returnBuffer;
}

ConsoleMethod(TSShapeConstructor, setNodeTransform, bool, 3, 5,
   "(string node_name, string px py pz [rx ry rz ra], [boolean isworld])")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::setNodeTransform() - shape not loaded" );
      return false;
   }

   GET_NODE_INDEX_NO_ROOT(node, argv[2], false);

   // optional position and rotation
   Point3F pos(0,0,0);
   QuatF rot(0,0,0,1);
   if (argc >= 4)
   {
      if (!parsePositionAndRotation(argv[3], pos, rot))
         return false;
   }

   if ((argc > 4) && dAtob(argv[4]))
   {
      // World transform

      // Get the node's parent (if any)
      if (node->parentIndex != -1)
      {
         MatrixF mat, mat2;
         object->mShape->getNodeWorldTransform(node->parentIndex, &mat);
         rot.setMatrix(&mat2);
         mat2.setPosition(pos);

         // Pre-multiply by inverse of parent's world transform to get
         // local node transform
         mat.inverse();
         mat.mul(mat2);

         rot.set(mat);
         pos = mat.getPosition();
      }
   }

   if (object->mShape->setNodeTransform(argv[2], pos, rot))
   {
      ADD_TO_CHANGE_SET();
      return true;
   }
   return false;
}

ConsoleMethod(TSShapeConstructor, renameNode, bool, 4, 4,
   "(string old_name, string new_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::renameNode() - shape not loaded" );
      return false;
   }

   GET_NODE_INDEX_NO_ROOT(node, argv[2], false);

   if (object->mShape->renameNode(argv[2], argv[3]))
   {
      ADD_TO_CHANGE_SET();
      return true;
   }
   return false;
}

ConsoleMethod(TSShapeConstructor, addNode, bool, 4, 6,
   "(string node_name, string parent_name, [string px py pz [rx ry rz ra]], [isworld])")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::addNode() - shape not loaded" );
      return false;
   }

   const char* name = argv[2];
   const char* parentName = argv[3];

   // optional position and rotation
   Point3F pos(0,0,0);
   QuatF rot(0,0,0,1);
   if (argc > 4)
   {
      if (!parsePositionAndRotation(argv[4], pos, rot))
         return false;
   }

   if ((argc > 5) && dAtob(argv[5]))
   {
      // World transform

      // Get the node's parent (if any)
      S32 parentIndex = object->mShape->findNode(parentName);
      if (parentIndex != -1)
      {
         MatrixF mat, mat2;
         object->mShape->getNodeWorldTransform(parentIndex, &mat);
         rot.setMatrix(&mat2);
         mat2.setPosition(pos);

         // Pre-multiply by inverse of parent's world transform to get
         // local node transform
         mat.inverse();
         mat.mul(mat2);

         rot.set(mat);
         pos = mat.getPosition();
      }
   }

   if (object->mShape->addNode(name, parentName, pos, rot))
   {
      ADD_TO_CHANGE_SET();
      return true;
   }
   return false;
}

ConsoleMethod(TSShapeConstructor, removeNode, bool, 3, 3,
   "(string node_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::removeNode() - shape not loaded" );
      return false;
   }

   GET_NODE_INDEX_NO_ROOT(node, argv[2], false);

   if (object->mShape->removeNode(argv[2]))
   {
      ADD_TO_CHANGE_SET();
      return true;
   }
   return false;
}

//-----------------------------------------------------------------------------
// OBJECTS

ConsoleMethod(TSShapeConstructor, getObjectCount, S32, 2, 2,
   "()")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getObjectCount() - shape not loaded" );
      return 0;
   }

   return object->mShape->objects.size();
}

ConsoleMethod(TSShapeConstructor, getObjectName, const char*, 3, 4,
   "(int index)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getObjectName() - shape not loaded" );
      return "";
   }

   S32 index = dAtoi(argv[2]);
   if ((index < 0) || (index >= object->mShape->objects.size()))
   {
      Con::errorf("getObjectName: index out of range (0-%d)", object->mShape->objects.size()-1);
      return "";
   }
   return object->mShape->getName(object->mShape->objects[index].nameIndex);
}

ConsoleMethod(TSShapeConstructor, getObjectNode, const char*, 3, 3,
   "(string object_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getObjectNode() - shape not loaded" );
      return "";
   }

   GET_OBJECT(obj, argv[2], 0);
   if (obj->nodeIndex < 0)
      return "";
   else
      return object->mShape->getName(object->mShape->nodes[obj->nodeIndex].nameIndex);
}

ConsoleMethod(TSShapeConstructor, setObjectNode, bool, 4, 4,
   "(string object_name, string node_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::setObjectNode() - shape not loaded" );
      return false;
   }

   return object->mShape->setObjectNode(argv[2], argv[3]);
}

ConsoleMethod(TSShapeConstructor, renameObject, bool, 4, 4,
   "(string old_name, string new_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::renameObject() - shape not loaded" );
      return false;
   }

   return object->mShape->renameObject(argv[2], argv[3]);
}

ConsoleMethod(TSShapeConstructor, removeObject, bool, 3, 3,
   "(string object_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::remoeObject() - shape not loaded" );
      return false;
   }

   return object->mShape->removeObject(argv[2]);
}

//-----------------------------------------------------------------------------
// MESHES
ConsoleMethod(TSShapeConstructor, getMeshCount, S32, 3, 3,
   "(string object_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getMeshCount() - shape not loaded" );
      return 0;
   }

   GET_OBJECT(obj, argv[2], 0);

   Vector<S32> objectDetails;
   object->mShape->getObjectDetails(objIndex, objectDetails);
   return objectDetails.size();
}

ConsoleMethod(TSShapeConstructor, getMeshName, const char*, 4, 4,
   "(string object_name, int index)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getMeshName() - shape not loaded" );
      return "";
   }

   GET_OBJECT(obj, argv[2], "");

   Vector<S32> objectDetails;
   object->mShape->getObjectDetails(objIndex, objectDetails);

   S32 index = dAtoi(argv[3]);
   if ((index < 0) || (index >= objectDetails.size()))
   {
      Con::errorf("getMeshName: index out of range (0-%d)", objectDetails.size()-1);
      return "";
   }

   char* returnBuffer = Con::getReturnBuffer(256);
   dSprintf(returnBuffer, 256, "%s%d", argv[2], (S32)object->mShape->details[objectDetails[index]].size);
   return returnBuffer;
}

ConsoleMethod(TSShapeConstructor, setMeshSize, bool, 4, 4,
   "(string old_name, int new_size)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::setMeshSize() - shape not loaded" );
      return false;
   }

   return object->mShape->setMeshSize(argv[2], dAtoi(argv[3]));
}

ConsoleMethod(TSShapeConstructor, getMeshType, const char*, 3, 3,
   "(string mesh_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getMeshType() - shape not loaded" );
      return "";
   }

   GET_MESH(mesh, argv[2], "normal");

   if (mesh->getFlags(TSMesh::BillboardZAxis))
      return "billboardzaxis";
   else if (mesh->getFlags(TSMesh::Billboard))
      return "billboard";
   else
      return "normal";
}

ConsoleMethod(TSShapeConstructor, setMeshType, bool, 4, 4,
   "(string mesh_name, string mesh_type)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::setMeshType() - shape not loaded" );
      return false;
   }

   GET_MESH(mesh, argv[2], false);

   // Update the mesh flags
   mesh->clearFlags(TSMesh::Billboard | TSMesh::BillboardZAxis);
   if (dStrEqual(argv[3], "billboard"))
      mesh->setFlags(TSMesh::Billboard);
   else if (dStrEqual(argv[3], "billboardzaxis"))
      mesh->setFlags(TSMesh::BillboardZAxis);
   else if (!dStrEqual(argv[3], "normal"))
   {
      Con::printf("setMeshType: Unknown mesh type '%s'", argv[3]);
      return false;
   }

   return true;
}

ConsoleMethod(TSShapeConstructor, getMeshMaterial, const char*, 3, 3,
   "(string mesh_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getMeshMaterial() - shape not loaded" );
      return "";
   }

   GET_MESH(mesh, argv[2], "");

   // Return the name of the first material attached to this mesh
   S32 matIndex = mesh->primitives[0].matIndex & TSDrawPrimitive::MaterialMask;
   if ((matIndex >= 0) && (matIndex < object->mShape->materialList->getMaterialCount()))
      return object->mShape->materialList->getMaterialName(matIndex);
   else
      return "";
}

ConsoleMethod(TSShapeConstructor, setMeshMaterial, bool, 4, 4,
   "(string mesh_name, string material_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::setMeshMaterial() - shape not loaded" );
      return false;
   }

   GET_MESH(mesh, argv[2], false);

   // Check if this material is already in the shape
   S32 matIndex;
   for (matIndex = 0; matIndex < object->mShape->materialList->size(); matIndex++)
   {
      if (dStrEqual(argv[3], object->mShape->materialList->getMaterialName(matIndex)))
         break;
   }
   if (matIndex == object->mShape->materialList->size())
   {
      // Add a new material to the shape
      U32 flags = TSMaterialList::S_Wrap | TSMaterialList::T_Wrap;
      object->mShape->materialList->push_back(argv[3], flags);
   }

   // Set this material for all primitives in the shape
   for (S32 i = 0; i < mesh->primitives.size(); i++)
   {
      U32 matType = mesh->primitives[i].matIndex & (TSDrawPrimitive::TypeMask | TSDrawPrimitive::Indexed);
      mesh->primitives[i].matIndex = (matType | matIndex);
   }

   return true;
}

// addMesh(shape_filename, srcMesh, mesh_name);
// eg. addMesh("./MyCollisionMesh.dae", "MyMesh2", "Col-1");
//
// addMesh("cube", "sx sy sz cx cy cz|bounds", mesh_name);
// eg. addMesh("cube", "4 10 6 0 0 0", "Col-1");
// eg. addMesh("cube", "bounds", "Col-1");

ConsoleMethod(TSShapeConstructor, addMesh, bool, 5, 5,
   "((string shape_filename, string src_mesh)|(prim_type, prim_size), string mesh_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::addMesh() - shape not loaded" );
      return false;
   }

   // Check for predefined geometric primitives
   if (!dStrcmp(argv[2], "cube"))
   {
      // Get the size and center of the primitive (either "bounds" or "sx sy sz cx cy cz")
      Point3F cubeSize(1, 1, 1);
      Point3F cubeCenter(0, 0, 0);
      if (!dStrcmp(argv[3], "bounds"))
      {
         const Box3F& bounds = object->mShape->bounds;
         cubeSize.set(bounds.len_x(), bounds.len_y(), bounds.len_z());
         bounds.getCenter(&cubeCenter);
      }
      else
      {
         S32 count = dSscanf(argv[3],"%g %g %g %g %g %g",
            &cubeSize.x, &cubeSize.y, &cubeSize.z,
            &cubeCenter.x, &cubeCenter.y, &cubeCenter.z);
         if ((count != 3) && (count != 6))
         {
            Con::printf("Failed to parse cube size and center \"sx sy sz cx cy cz\" from '%s'", argv[3]);
            return false;
         }
      }

      TSMesh* cube = TSShape::createMeshCube(cubeCenter, cubeSize);
      return object->mShape->addMesh(cube, argv[4]);
   }
   else
   {
      // Load the shape source file
      char filenameBuf[1024];
      Con::expandScriptFilename(filenameBuf, sizeof(filenameBuf), argv[2]);

      // Copy arguments (since they may change if loading the source shape
      // causes another onLoad method to be called)
      String srcMeshName(argv[3]);
      String meshName(argv[4]);

      Resource<TSShape> hSrcShape = ResourceManager::get().load(filenameBuf);
      if (!bool(hSrcShape))
      {
         Con::errorf("addMesh failed: Could not load source shape: '%s'", filenameBuf);
         return false;
      }

      TSShape* srcShape = const_cast<TSShape*>((const TSShape*)hSrcShape);
      return object->mShape->addMesh(srcShape, srcMeshName, meshName);
   }
}

ConsoleMethod(TSShapeConstructor, removeMesh, bool, 3, 3,
   "(string mesh_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::removeMesh() - shape not loaded" );
      return false;
   }

   return object->mShape->removeMesh(argv[2]);
}

//-----------------------------------------------------------------------------
// AUTO BILLBOARDS
ConsoleMethod(TSShapeConstructor, addAutoBillboard, bool, 9, 9,
   "(int size, int equator_steps, int polar_steps, int dl, int dim, bool include_poles, float polar_angle)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::addAutoBillboard() - shape not loaded" );
      return false;
   }

   S32 size = dAtoi(argv[2]);
   S32 numEquatorSteps = dAtoi(argv[3]);
   S32 numPolarSteps = dAtoi(argv[4]);
   S32 dl = dAtoi(argv[5]);
   S32 dim = dAtoi(argv[6]);
   bool includePoles = dAtob(argv[7]);
   F32 polarAngle = dAtof(argv[8]);

   // Check that there is not already a detail level at this size
   Vector<S32> validDetails;
   object->mShape->getSubShapeDetails(0, validDetails);
   for (S32 i = 0; i < validDetails.size(); i++)
   {
      if (object->mShape->details[validDetails[i]].size == size)
      {
         Con::errorf("addAutoBillboard: A detail level with size %d already exists", size);
         return false;
      }
   }

   // Add the new detail level
   object->mShape->addBillboardDetail("detail", size, numEquatorSteps,
                     numPolarSteps, dl, dim, includePoles, polarAngle);

   return true;
}

ConsoleMethod(TSShapeConstructor, removeAutoBillboard, void, 3, 3,
   "(int size)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::removeAutoBillboard() - shape not loaded" );
      return;
   }

   S32 size = dAtoi(argv[2]);

   // Get the auto-billboard details
   Vector<S32> bbDetails;
   object->mShape->getSubShapeDetails(-1, bbDetails);
   for (S32 i = 0; i < bbDetails.size(); i++)
   {
      if (object->mShape->details[bbDetails[i]].size == size)
      {
         object->mShape->details.erase(bbDetails[i]);
         return;
      }
   }

   Con::errorf("removeAutoBillboard: Could not find autobillboard detail with size '%d'", size);
}

//-----------------------------------------------------------------------------
// SEQUENCES
ConsoleMethod(TSShapeConstructor, getSequenceCount, S32, 2, 2,
   "()")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getSequenceCount() - shape not loaded" );
      return 0;
   }

   return object->mShape->sequences.size();
}

ConsoleMethod(TSShapeConstructor, getSequenceIndex, S32, 3, 3,
   "(string sequence_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getSequenceIndex() - shape not loaded" );
      return -1;
   }

   return object->mShape->findSequence(argv[2]);
}

ConsoleMethod(TSShapeConstructor, getSequenceName, const char*, 3, 3,
   "(int index)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getSequenceName() - shape not loaded" );
      return "";
   }

   S32 index = dAtoi(argv[2]);
   if ((index < 0) || (index >= object->mShape->sequences.size()))
   {
      Con::errorf("getSequenceName: index out of range (0-%d)", object->mShape->sequences.size()-1);
      return "";
   }
   return object->mShape->getName(object->mShape->sequences[index].nameIndex);
}

ConsoleMethod(TSShapeConstructor, getSequenceSource, const char*, 3, 3,
   "(string sequence_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getSequenceSource() - shape not loaded" );
      return "";
   }

   GET_SEQUENCE(seq, argv[2], "");

   // Return information about the source data for this sequence
   char* returnBuffer = Con::getReturnBuffer(512);
   dSprintf(returnBuffer, 512, "%s\t%d\t%d\t%d",
      seqData->from.c_str(), seqData->start, seqData->end, seqData->total);
   return returnBuffer;
}

ConsoleMethod(TSShapeConstructor, getSequenceFrameCount, S32, 3, 3,
   "(string sequence_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getSequenceFrameCount() - shape not loaded" );
      return 0;
   }

   GET_SEQUENCE(seq, argv[2], 0);
   return seq->numKeyframes;
}

ConsoleMethod(TSShapeConstructor, getSequencePriority, F32, 3, 3,
   "(string sequence_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getSequencePriority() - shape not loaded" );
      return -1.0;
   }

   GET_SEQUENCE(seq, argv[2], 0.0f);
   return seq->priority;
}

ConsoleMethod(TSShapeConstructor, setSequencePriority, bool, 4, 4,
   "(string sequence_name, float priority)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::setSequencePriority() - shape not loaded" );
      return false;
   }

   GET_SEQUENCE(seq, argv[2], false);
   seq->priority = dAtof(argv[3]);
   ADD_TO_CHANGE_SET();
   return true;
}

ConsoleMethod(TSShapeConstructor, getSequenceGroundSpeed, const char*, 3, 3,
   "(string sequence_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getSequenceGroundSpeed() - shape not loaded" );
      return "";
   }

   // Find the sequence and return the ground speed (assumed to be constant)
   GET_SEQUENCE(seq, argv[2], "");

   Point3F trans(0,0,0), rot(0,0,0);
   if (seq->numGroundFrames > 0)
   {
      const Point3F& p1 = object->mShape->groundTranslations[seq->firstGroundFrame];
      const Point3F& p2 = object->mShape->groundTranslations[seq->firstGroundFrame + 1];
      trans = p2 - p1;

      QuatF r1 = object->mShape->groundRotations[seq->firstGroundFrame].getQuatF();
      QuatF r2 = object->mShape->groundRotations[seq->firstGroundFrame + 1].getQuatF();
      r2 -= r1;

      MatrixF mat;
      r2.setMatrix(&mat);
      rot = mat.toEuler();
   }

   char* returnBuffer = Con::getReturnBuffer(256);
   dSprintf(returnBuffer, 256, "%g %g %g %g %g %g", trans.x, trans.y, trans.z, rot.x, rot.y, rot.z);
   return returnBuffer;
}

ConsoleMethod(TSShapeConstructor, setSequenceGroundSpeed, bool, 4, 5,
   "(string sequence_name, string trans.x trans.y trans.z, [string rot.x rot.y rot.z)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::setSequenceGroundSpeed() - shape not loaded" );
      return false;
   }

   Point3F trans, rot(0, 0, 0);
   dSscanf(argv[3], "%g %g %g", &trans.x, &trans.y, &trans.z);
   if (argc >= 5)
      dSscanf(argv[4], "%g %g %g", &rot.x, &rot.y, &rot.z);

   return object->mShape->setSequenceGroundSpeed(argv[2], trans, rot);
}

ConsoleMethod(TSShapeConstructor, getSequenceCyclic, bool, 3, 3,
   "(string sequence_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getSequenceCyclic() - shape not loaded" );
      return false;
   }

   GET_SEQUENCE(seq, argv[2], false);
   return seq->isCyclic();
}

ConsoleMethod(TSShapeConstructor, setSequenceCyclic, bool, 4, 4,
   "(string sequence_name, bool cyclic)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::setSequencCyclic() - shape not loaded" );
      return false;
   }

   GET_SEQUENCE(seq, argv[2], false);

   // update cyclic flag
   bool cyclic = dAtob(argv[3]);
   if (cyclic != seq->isCyclic())
   {
      if (cyclic && !seq->isCyclic())
         seq->flags |= TSShape::Cyclic;
      else if (!cyclic && seq->isCyclic())
         seq->flags &= (~(TSShape::Cyclic));

      ADD_TO_CHANGE_SET();
   }

   return true;
}

ConsoleMethod(TSShapeConstructor, getSequenceBlend, const char*, 3, 3,
   "(string sequence_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getSequenceBlend() - shape not loaded" );
      return "";
   }

   GET_SEQUENCE(seq, argv[2], "0");

   // Return the blend information (flag reference_sequence reference_frame)
   char* returnBuffer = Con::getReturnBuffer(512);
   dSprintf(returnBuffer, 512, "%d\t%s\t%d", (int)seq->isBlend(),
      seqData->blendSeq.c_str(), seqData->blendFrame);
   return returnBuffer;
}

ConsoleMethod(TSShapeConstructor, setSequenceBlend, bool, 6, 6,
   "(string sequence_name, string blend_seq_name, int blend_ref_frame)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::setSequenceBlend() - shape not loaded" );
      return false;
   }

   GET_SEQUENCE(seq, argv[2], false);

   if (object->mShape->setSequenceBlend(argv[2], dAtob(argv[3]), argv[4], dAtoi(argv[5])))
   {
      ADD_TO_CHANGE_SET();

      // Update sequence blend information
      seqData->blendSeq = argv[4];
      seqData->blendFrame = dAtoi(argv[5]);
      return true;
   }
   return false;
}

ConsoleMethod(TSShapeConstructor, renameSequence, bool, 4, 4,
   "(string old_name, string new_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::renameSequence() - shape not loaded" );
      return false;
   }

   GET_SEQUENCE(seq, argv[2], false);

   if (object->mShape->renameSequence(argv[2], argv[3]))
   {
      ADD_TO_CHANGE_SET();
      return true;
   }
   return false;
}

ConsoleMethod(TSShapeConstructor, addSequence, bool, 4, 6,
   "(string sequence_or_shape_file, string new_name, [int start_frame, [int end_frame]])")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::addSequence() - shape not loaded" );
      return false;
   }

   char fileBuf[MAX_PATH_LENGTH + 1];
   dStrncpy(fileBuf, argv[2], MAX_PATH_LENGTH);
   fileBuf[MAX_PATH_LENGTH] = '\0'; // ensure string is terminated, even if it was truncated

   const char* seqName = 0;

   // Determine if there is a sequence name at the end of the source string, and
   // if so, split the filename from the sequence name
   char* split = dStrrchr(fileBuf, ' ');
   char* split2 = dStrrchr(fileBuf, '\t');
   if (!split || (split2 > split))
      split = split2;
   if (split)
   {
      seqName = split;
      while (*seqName && dIsspace(*seqName))
         seqName++;
      *split = '\0';
   }

   const char* destName = argv[3];

   S32 startFrame = (argc >= 5) ? dAtoi(argv[4]) : 0;
   S32 endFrame = (argc >= 6) ? dAtoi(argv[5]) : -1;

   // Check what type of file (if any) we are loading the sequence from
   S32 totalFrames;
   if (object->mShape->addSequence(fileBuf, seqName, destName, startFrame, endFrame, &totalFrames))
   {
      ADD_TO_CHANGE_SET();

      // Store information about how this sequence was created
      object->mSeqData.increment();
      object->mSeqData.last().from = String::ToString("%s\t%s", fileBuf, seqName ? seqName : "");
      object->mSeqData.last().total = totalFrames;
      object->mSeqData.last().start = ((startFrame < 0) || (startFrame >= totalFrames)) ? 0 : startFrame;
      object->mSeqData.last().end = ((endFrame < 0) || (endFrame >= totalFrames)) ? totalFrames-1 : endFrame;
      return true;
   }
   return false;
}

ConsoleMethod(TSShapeConstructor, removeSequence, bool, 3, 3,
   "(string sequence_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::removeSequence() - shape not loaded" );
      return false;
   }

   GET_SEQUENCE(seq, argv[2], false);

   if (object->mShape->removeSequence(argv[2]))
   {
      ADD_TO_CHANGE_SET();
      object->mSeqData.erase(seqIndex);
      return true;
   }
   return false;
}

//-----------------------------------------------------------------------------
// TRIGGERS
ConsoleMethod(TSShapeConstructor, getTriggerCount, S32, 3, 3,
   "(string sequence_name)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getTriggerCount() - shape not loaded" );
      return 0;
   }

   GET_SEQUENCE(seq, argv[2], 0);
   return seq->numTriggers;
}

ConsoleMethod(TSShapeConstructor, getTrigger, const char*, 4, 4,
   "(string sequence_name, int trigger_index)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::getTrigger() - shape not loaded" );
      return "";
   }
   
   // Find the sequence and return the indexed trigger (frame and state)
   GET_SEQUENCE(seq, argv[2], "");

   S32 trigIndex = dAtoi(argv[3]);
   if ((trigIndex < 0) || (trigIndex >= seq->numTriggers))
   {
      Con::errorf("getTrigger: index out of range (0-%d)", seq->numTriggers-1);
      return "";
   }

   const TSShape::Trigger& trig = object->mShape->triggers[seq->firstTrigger + trigIndex];
   S32 frame = trig.pos * seq->numKeyframes;
   S32 state = getBinLog2(trig.state & TSShape::Trigger::StateMask) + 1;
   if (!(trig.state & TSShape::Trigger::StateOn))
      state = -state;

   char* returnBuffer = Con::getReturnBuffer(32);
   dSprintf(returnBuffer, 32, "%d %d", frame, state);
   return returnBuffer;
}

ConsoleMethod(TSShapeConstructor, addTrigger, bool, 5, 5,
   "(string sequence_name, int keyframe, int state)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::addTrigger() - shape not loaded" );
      return false;
   }

   if (object->mShape->addTrigger(argv[2], dAtoi(argv[3]), dAtoi(argv[4])))
   {
      ADD_TO_CHANGE_SET();
      return true;
   }
   return false;
}

ConsoleMethod(TSShapeConstructor, removeTrigger, bool, 5, 5,
   "(string sequence_name, int keyframe, int state)")
{
   if( !object->getShape() )
   {
      Con::errorf( "TSShapeConstructor::removeTrigger() - shape not loaded" );
      return false;
   }

   if (object->mShape->removeTrigger(argv[2], dAtoi(argv[3]), dAtoi(argv[4])))
   {
      ADD_TO_CHANGE_SET();
      return true;
   }
   return false;
}

//-----------------------------------------------------------------------------
// Change-Set manipulation
TSShapeConstructor::ChangeSet::eCommandType TSShapeConstructor::ChangeSet::getCmdType(const char* name)
{
   #define RETURN_IF_MATCH(type)   if (!dStricmp(name, #type)) return Cmd##type

   RETURN_IF_MATCH(AddNode);
   else RETURN_IF_MATCH(RemoveNode);
   else RETURN_IF_MATCH(RenameNode);
   else RETURN_IF_MATCH(SetNodeTransform);
   else RETURN_IF_MATCH(SetNodeParent);

   else RETURN_IF_MATCH(AddSequence);
   else RETURN_IF_MATCH(RemoveSequence);
   else RETURN_IF_MATCH(RenameSequence);
   else RETURN_IF_MATCH(SetSequenceCyclic);
   else RETURN_IF_MATCH(SetSequenceBlend);
   else RETURN_IF_MATCH(SetSequencePriority);
   else RETURN_IF_MATCH(SetSequenceGroundSpeed);

   else RETURN_IF_MATCH(AddTrigger);
   else RETURN_IF_MATCH(RemoveTrigger);

   else return CmdInvalid;

   #undef RETURN_IF_MATCH
}

void TSShapeConstructor::ChangeSet::add(const char* cmdName, S32 argc, const char* argv[])
{
   // Lookup the command type
   eCommandType type = getCmdType(cmdName);
   if (type == CmdInvalid)
      return;

   // Ignore operations on the __proxy__ sequence (it is only used by the shape editor)
   if (!dStricmp(argv[0], "__proxy__") || ((type == CmdAddSequence) && !dStricmp(argv[1], "__proxy__")))
      return;

   // Perform some simple change-set compression. For example, successive renames
   // to the same node will result in only a single rename command.
   if ( mCommands.size() )
   {
      Command& last = mCommands.last();

      switch (type)
      {
      // Rename commands replace the immediately prior instance of the command,
      // and remove the command altogether if it has no effect (ie. rename 'a' to 'a')
      case CmdRenameNode:
      case CmdRenameSequence:
         if (!dStricmp(argv[0], argv[1]))
         {
            // Ignore redundant rename commands
            return;
         }
         else if ((last.type == type) && !dStricmp(last.argv[1], argv[0]))
         {
            last.argv[1] = argv[1];

            // Remove redundant rename commands
            if (!dStricmp(last.argv[0], last.argv[1]))
               mCommands.pop_back();
            return;
         }
         break;

      // Node and sequence property modifications replace the immediately prior
      // instance of the command
      case CmdSetNodeParent:
      case CmdSetNodeTransform:
      case CmdSetSequencePriority:
      case CmdSetSequenceGroundSpeed:
         if ((last.type == type) && !dStricmp(last.argv[0], argv[0]))
         {
            last.argc = argc;
            last.argv[1] = argv[1];
            last.argv[2] = argv[2];
            return;
         }
         break;

      // Successive toggles of the cyclic and blend flags pop the last command
      case CmdSetSequenceCyclic:
         if (!dStricmp(last.argv[0], argv[0]) &&
            (dAtob(last.argv[1]) != dAtob(argv[1])))
         {
            mCommands.pop_back();
            return;
         }
         break;

      case CmdSetSequenceBlend:
         if (!dStricmp(last.argv[0], argv[0]) &&
            (dAtob(last.argv[1]) != dAtob(argv[1])) &&
            !dStricmp(last.argv[2], argv[2]) &&
            !dStricmp(last.argv[3], argv[3]))
         {
            mCommands.pop_back();
            return;
         }
         break;

      // Removing a node, sequence or trigger immediately after adding
      // it pops the add command
      case CmdRemoveNode:
         if ((last.type == CmdAddNode) && !dStricmp(last.argv[0], argv[0]))
         {
            mCommands.pop_back();
            return;
         }
         break;

      case CmdRemoveSequence:
         if ((last.type == CmdAddSequence) && !dStricmp(last.argv[1], argv[0]))
         {
            mCommands.pop_back();
            return;
         }
         break;

      case CmdRemoveTrigger:
         if ((last.type == CmdAddTrigger) &&
            !dStricmp(last.argv[0], argv[0]) &&
            !dStricmp(last.argv[1], argv[1]) &&
            !dStricmp(last.argv[2], argv[2]))
         {
            mCommands.pop_back();
            return;
         }
         break;

      // Adding a trigger immediately after removing it pops the remove command
      case CmdAddTrigger:
         if ((last.type == CmdRemoveTrigger) &&
            !dStricmp(last.argv[0], argv[0]) &&
            !dStricmp(last.argv[1], argv[1]) &&
            !dStricmp(last.argv[2], argv[2]))
         {
            mCommands.pop_back();
            return;
         }
         break;

      case CmdAddNode:
      case CmdAddSequence:
      case CmdInvalid:
         /* No compression for these command types */
         break;
      }
   }

   // Add a new command
   mCommands.increment();
   mCommands.last().type = type;
   mCommands.last().name = StringTable->insert(cmdName);
   mCommands.last().argc = argc;
   for (S32 i = 0; i < argc; i++)
      mCommands.last().argv[i] = argv[i];
}

void TSShapeConstructor::ChangeSet::optimize(const ChangeSet& input, ChangeSet& output)
{
   for (S32 i = 0; i < input.mCommands.size(); i++)
   {
      const Command& cmd = input.mCommands[i];

      // @todo: other optimisations:
      // addSequence followed by renameSequence => change dest argument and ignore rename
      // addNode followed by renameNode => change name argument and ignore rename
      // addNode followed by setNodeTransform => change the transform in addNode
      // addSequence and removeSequence with no addSequence that references the sequence in between => remove the whole lot!
      // setNodeParent collapses all previous setNodeParents for that node
      // setNodeTransform collapses all previous setNodeTransform for that node

      output.mCommands.push_back(cmd);

      // Detect special 'rename' deletions (from the shape editor) and insert
      // a delete command instead
      if ((cmd.type == CmdRenameNode) && dStrStartsWith(cmd.argv[1], "__deleted_"))
      {
         output.mCommands.last().type = CmdRemoveNode;
         output.mCommands.last().name = StringTable->insert("removeNode");
         output.mCommands.last().argc = 1;
      }

      if ((cmd.type == CmdRenameSequence) && dStrStartsWith(cmd.argv[1], "__deleted_"))
      {
         output.mCommands.last().type = CmdRemoveSequence;
         output.mCommands.last().name = StringTable->insert("removeSequence");
         output.mCommands.last().argc = 1;
      }
   }
}

void TSShapeConstructor::ChangeSet::write(Stream& stream)
{
   for (U32 i = 0; i < mCommands.size(); i++)
   {
      const Command& cmd = mCommands[i];

      // Write the command and the first 2 arguments (always present)
      stream.writeTabs(1);
      stream.writeText("%this.");

      stream.writeText(cmd.name);
      stream.writeText("(\"");
      stream.write(cmd.argv[0].length(), cmd.argv[0].c_str());
      stream.writeText("\"");

      // Write remaining arguments and newline
      for (U32 j = 1; j < cmd.argc; j++)
      {
         stream.writeText(", \"");
         stream.write(cmd.argv[j].length(), cmd.argv[j].c_str());
         stream.writeText("\"");
      }
      stream.writeText(");\r\n");
   }
}
