//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "gui/worldEditor/worldEditor.h"

#include "gui/worldEditor/gizmo.h"
#include "gui/worldEditor/undoActions.h"
#include "gui/worldEditor/editorIconRegistry.h"
#include "core/stream/memStream.h"
#include "sceneGraph/simPath.h"
#include "gui/core/guiCanvas.h"
#include "T3D/gameConnection.h"
#include "collision/earlyOutPolyList.h"
#include "collision/concretePolyList.h"
#include "console/consoleInternal.h"
#include "T3D/shapeBase.h"
#include "T3D/cameraSpline.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxTransformSaver.h"
#include "gfx/gfxDrawUtil.h"
#include "platform/typetraits.h"



IMPLEMENT_CONOBJECT(WorldEditor);

// unnamed namespace for static data
namespace {

   static VectorF axisVector[3] = {
      VectorF(1.0f,0.0f,0.0f),
      VectorF(0.0f,1.0f,0.0f),
      VectorF(0.0f,0.0f,1.0f)
   };

   static Point3F BoxPnts[] = {
      Point3F(0.0f,0.0f,0.0f),
      Point3F(0.0f,0.0f,1.0f),
      Point3F(0.0f,1.0f,0.0f),
      Point3F(0.0f,1.0f,1.0f),
      Point3F(1.0f,0.0f,0.0f),
      Point3F(1.0f,0.0f,1.0f),
      Point3F(1.0f,1.0f,0.0f),
      Point3F(1.0f,1.0f,1.0f)
   };

   static U32 BoxVerts[][4] = {
      {0,2,3,1},     // -x
      {7,6,4,5},     // +x
      {0,1,5,4},     // -y
      {3,2,6,7},     // +y
      {0,4,6,2},     // -z
      {3,7,5,1}      // +z
   };

   //
   U32 getBoxNormalIndex(const VectorF & normal)
   {
      const F32 * pNormal = ((const F32 *)normal);

      F32 max = 0;
      S32 index = -1;

      for(U32 i = 0; i < 3; i++)
         if(mFabs(pNormal[i]) >= mFabs(max))
         {
            max = pNormal[i];
            index = i*2;
         }

      AssertFatal(index >= 0, "Failed to get best normal");
      if(max > 0.f)
         index++;

      return(index);
   }

   //
   Point3F getBoundingBoxCenter(SceneObject * obj)
   {
      Box3F box = obj->getObjBox();
      MatrixF mat = obj->getTransform();
      VectorF scale = obj->getScale();

      Point3F center(0,0,0);
      Point3F projPnts[8];

      for(U32 i = 0; i < 8; i++)
      {
         Point3F pnt;
         pnt.set(BoxPnts[i].x ? box.maxExtents.x : box.minExtents.x,
                 BoxPnts[i].y ? box.maxExtents.y : box.minExtents.y,
                 BoxPnts[i].z ? box.maxExtents.z : box.minExtents.z);

         // scale it
         pnt.convolve(scale);
         mat.mulP(pnt, &projPnts[i]);
         center += projPnts[i];
      }

      center /= 8;
      return(center);
   }

   //
   const char * parseObjectFormat(SimObject * obj, const char * format)
   {
      static char buf[1024];

      U32 curPos = 0;
      U32 len = dStrlen(format);

      for(U32 i = 0; i < len; i++)
      {
         if(format[i] == '$')
         {
            U32 j;
            for(j = i+1; j < len; j++)
               if(format[j] == '$')
                  break;

            if(j == len)
               break;

            char token[80];

            AssertFatal((j - i) < (sizeof(token) - 1), "token too long");
            dStrncpy(token, &format[i+1], (j - i - 1));
            token[j-i-1] = 0;

            U32 remaining = sizeof(buf) - curPos - 1;

            // look at the token
            if(!dStricmp(token, "id"))
               curPos += dSprintf(buf + curPos, remaining, "%d", obj->getId());
            else if(!dStricmp(token, "name|internal"))
            {
               if( obj->getName() || !obj->getInternalName() )
                  curPos += dSprintf(buf + curPos, remaining, "%s", obj->getName());
               else
                  curPos += dSprintf(buf + curPos, remaining, "[%s]", obj->getInternalName());
            }
            else if(!dStricmp(token, "name"))
               curPos += dSprintf(buf + curPos, remaining, "%s", obj->getName());
            else if(!dStricmp(token, "class"))
               curPos += dSprintf(buf + curPos, remaining, "%s", obj->getClassName());
            else if(!dStricmp(token, "namespace") && obj->getNamespace())
               curPos += dSprintf(buf + curPos, remaining, "%s", obj->getNamespace()->mName);

            //
            i = j;
         }
         else
            buf[curPos++] = format[i];
      }

      buf[curPos] = 0;
      return(buf);
   }

   //
   F32 snapFloat(F32 val, F32 snap)
   {
      if(snap == 0.f)
         return(val);

      F32 a = mFmod(val, snap);

      if(mFabs(a) > (snap / 2))
         val < 0.f ? val -= snap : val += snap;

      return(val - a);
   }

   //
   EulerF extractEuler(const MatrixF & matrix)
   {
      const F32 * mat = (const F32*)matrix;

      EulerF r;
      r.x = mAsin(mat[MatrixF::idx(2,1)]);

      if(mCos(r.x) != 0.f)
      {
         r.y = mAtan2(-mat[MatrixF::idx(2,0)], mat[MatrixF::idx(2,2)]);
         r.z = mAtan2(-mat[MatrixF::idx(0,1)], mat[MatrixF::idx(1,1)]);
      }
      else
      {
         r.y = 0.f;
         r.z = mAtan2(mat[MatrixF::idx(1,0)], mat[MatrixF::idx(0,0)]);
      }

      return(r);
   }
}

F32 WorldEditor::smProjectDistance = 20000.0f;

//------------------------------------------------------------------------------
// Class WorldEditor::Selection
//------------------------------------------------------------------------------

WorldEditor::Selection::Selection() :
   mCentroidValid(false),
   mAutoSelect(false),
   mPrevCentroid(0.0f, 0.0f, 0.0f),
   mContainsGlobalBounds(false)
{
   registerObject();
}

WorldEditor::Selection::~Selection()
{
   unregisterObject();
}

bool WorldEditor::Selection::objInSet( SimObject* obj )
{
   for(U32 i = 0; i < mObjectList.size(); i++)
      if(mObjectList[i] == (SimObject*)obj)
         return(true);
   return(false);
}

bool WorldEditor::Selection::addObject( SimObject* obj )
{
   if(objInSet(obj))
      return(false);

   mCentroidValid = false;

   mObjectList.pushBack(obj);
   deleteNotify(obj);

   if(mAutoSelect)
   {
      obj->setSelected(true);
      
      if( dynamic_cast< SceneObject* >( obj ) )
      {
         SceneObject * clientObj = WorldEditor::getClientObj( static_cast< SceneObject* >( obj ) );
         if(clientObj)
            clientObj->setSelected(true);
      }
   }

   return(true);
}

bool WorldEditor::Selection::removeObject( SimObject* obj )
{
   if(!objInSet(obj))
      return(false);

   mCentroidValid = false;

   mObjectList.remove(obj);
   clearNotify(obj);

   if(mAutoSelect)
   {
      obj->setSelected(false);

      if( dynamic_cast< SceneObject* >( obj ) )
      {
         SceneObject * clientObj = WorldEditor::getClientObj( static_cast< SceneObject* >( obj ) );
         if(clientObj)
            clientObj->setSelected(false);
      }
   }

   return(true);
}

void WorldEditor::Selection::clear()
{
   while(mObjectList.size())
      removeObject(static_cast<SceneObject*>(mObjectList[0]));
}

void WorldEditor::Selection::onDeleteNotify(SimObject * obj)
{
   removeObject(static_cast<SceneObject*>(obj));
}

bool WorldEditor::Selection::containsGlobalBounds()
{
   updateCentroid();
   return mContainsGlobalBounds;
}

void WorldEditor::Selection::updateCentroid()
{
   if(mCentroidValid)
      return;

   //mCentroidValid = true;

   //
   mCentroid.set(0,0,0);
   mBoxCentroid = mCentroid;
   mBoxBounds.minExtents.set(1e10, 1e10, 1e10);
   mBoxBounds.maxExtents.set(-1e10, -1e10, -1e10);

   mContainsGlobalBounds = false;

   if(!mObjectList.size())
      return;

   //
   for(U32 i = 0; i < mObjectList.size(); i++)
   {
      SceneObject* obj = dynamic_cast<SceneObject*>(mObjectList[i]);
      if( !obj )
         continue;

      const MatrixF & mat = obj->getTransform();
      Point3F wPos;
      mat.getColumn(3, &wPos);

      //
      mCentroid += wPos;

      //
      const Box3F& bounds = obj->getWorldBox();
      mBoxBounds.minExtents.setMin(bounds.minExtents);
      mBoxBounds.maxExtents.setMax(bounds.maxExtents);

      if(obj->isGlobalBounds())
         mContainsGlobalBounds = true;
   }

   mCentroid /= (F32)mObjectList.size();
   mBoxCentroid = mBoxBounds.getCenter();

   // Multi-selections always use mCentroid otherwise we
   // break rotation.
   if ( mObjectList.size() > 1 )
      mBoxCentroid = mCentroid;
}

const Point3F & WorldEditor::Selection::getCentroid()
{
   updateCentroid();
   return(mCentroid);
}

const Point3F & WorldEditor::Selection::getBoxCentroid()
{
   updateCentroid();
   return(mBoxCentroid);
}

const Box3F & WorldEditor::Selection::getBoxBounds()
{
   updateCentroid();
   return(mBoxBounds);
}

Point3F WorldEditor::Selection::getBoxBottomCenter()
{
   updateCentroid();
   
   Point3F bottomCenter = mBoxCentroid;
   bottomCenter.z -= mBoxBounds.len_z() * 0.5f;
   
   return bottomCenter;
}

void WorldEditor::Selection::enableCollision()
{
   for(U32 i = 0; i < mObjectList.size(); i++)
   {
      SceneObject* object = dynamic_cast<SceneObject*>(mObjectList[i]);
      if( object )
         object->enableCollision();
   }
}

void WorldEditor::Selection::disableCollision()
{
   for(U32 i = 0; i < mObjectList.size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >(mObjectList[i]);
      if( object )
         object->disableCollision();
   }
}

void WorldEditor::Selection::offset(const Point3F & offset)
{
   for(U32 i = 0; i < mObjectList.size(); i++)
   {
      SceneObject* obj = dynamic_cast<SceneObject*>(mObjectList[i]);
      if( !obj )
         continue;

      MatrixF mat = obj->getTransform();
      Point3F wPos;
      mat.getColumn(3, &wPos);

      // adjust
      wPos += offset;
      mat.setColumn(3, wPos);
      obj->setTransform(mat);
   }

   mCentroidValid = false;
}

void WorldEditor::Selection::setPosition(const Point3F & pos)
{
   for(U32 i = 0; i < mObjectList.size(); i++)
   {
      SceneObject* object = dynamic_cast<SceneObject*>(mObjectList[i]);
      if( object )
         object->setPosition(pos);
   }

   mCentroidValid = false;
}
void WorldEditor::Selection::setCentroidPosition(bool useBoxCenter, const Point3F & pos)
{
   Point3F centroid;
   if( containsGlobalBounds() )
   {
      centroid = getCentroid();
   }
   else
   {
      centroid = useBoxCenter ? getBoxCentroid() : getCentroid();
   }

   offset(pos - centroid);
}

void WorldEditor::Selection::orient(const MatrixF & rot, const Point3F & center)
{
   // Orient all the selected objects to the given rotation
   for(U32 i = 0; i < size(); i++) 
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ i ] );
      if( !object )
         continue;
         
      MatrixF mat = rot;
      mat.setPosition( object->getPosition() );
      object->setTransform(mat);
   }

   mCentroidValid = false;
}

void WorldEditor::Selection::rotate(const EulerF &rot)
{
   for(U32 i = 0; i < mObjectList.size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ i ] );
      if( !object )
         continue;

         MatrixF mat = object->getTransform();

         MatrixF transform(rot);
         mat.mul(transform);

         object->setTransform(mat);
   }
}

void WorldEditor::Selection::rotate(const EulerF & rot, const Point3F & center)
{
   // single selections will rotate around own axis, multiple about world
   if(mObjectList.size() == 1)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ 0 ] );
      if( object )
      {
         MatrixF mat = object->getTransform();

         Point3F pos;
         mat.getColumn(3, &pos);

         // get offset in obj space
         Point3F offset = pos - center;
         MatrixF wMat = object->getWorldTransform();
         wMat.mulV(offset);

         //
         MatrixF transform(EulerF(0,0,0), -offset);
         transform.mul(MatrixF(rot));
         transform.mul(MatrixF(EulerF(0,0,0), offset));
         mat.mul(transform);

         object->setTransform(mat);
      }
   }
   else
   {
      for(U32 i = 0; i < size(); i++)
      {
         SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ i ] );
         if( !object )
            continue;
            
         MatrixF mat = object->getTransform();

         Point3F pos;
         mat.getColumn(3, &pos);

         // get offset in obj space
         Point3F offset = pos - center;

         MatrixF transform(rot);
         Point3F wOffset;
         transform.mulV(offset, &wOffset);

         MatrixF wMat = object->getWorldTransform();
         wMat.mulV(offset);

         //
         transform.set(EulerF(0,0,0), -offset);

         mat.setColumn(3, Point3F(0,0,0));
         wMat.setColumn(3, Point3F(0,0,0));

         transform.mul(wMat);
         transform.mul(MatrixF(rot));
         transform.mul(mat);
         mat.mul(transform);

         mat.normalize();
         mat.setColumn(3, wOffset + center);

         object->setTransform(mat);
      }
   }

   mCentroidValid = false;
}

void WorldEditor::Selection::setRotate(const EulerF & rot)
{
   for(U32 i = 0; i < mObjectList.size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ i ] );
      if( !object )
         continue;

      MatrixF mat = object->getTransform();
      Point3F pos;
      mat.getColumn(3, &pos);

      MatrixF rmat(rot);
      rmat.setPosition(pos);

      object->setTransform(rmat);
   }
}

void WorldEditor::Selection::scale(const VectorF & scale)
{
   for(U32 i = 0; i < mObjectList.size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ i ] );
      if( !object )
         continue;
         
      VectorF current = object->getScale();
      current.convolve(scale);
      object->setScale(current);
   }

   mCentroidValid = false;
}

void WorldEditor::Selection::scale(const VectorF & scale, const Point3F & center)
{
   for(U32 i = 0; i < size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ i ] );
      if( !object )
         continue;
         
      MatrixF mat = object->getTransform();

      Point3F pos;
      mat.getColumn(3, &pos);

      Point3F offset = pos - center;
      offset *= scale;

      object->setPosition(offset + center);

      VectorF current = object->getScale();
      current.convolve(scale);
      object->setScale(current);
   }
}

void WorldEditor::Selection::setScale(const VectorF & scale)
{
   for(U32 i = 0; i < mObjectList.size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ i ] );
      if( object )
         object->setScale( scale );
   }

   mCentroidValid = false;
}

void WorldEditor::Selection::setScale(const VectorF & scale, const Point3F & center)
{
   for(U32 i = 0; i < size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ i ] );
      if( !object )
         continue;
         
      MatrixF mat = object->getTransform();

      Point3F pos;
      mat.getColumn(3, &pos);

      Point3F offset = pos - center;
      offset *= scale;

      object->setPosition(offset + center);
      object->setScale(scale);
   }
}

void WorldEditor::Selection::addSize(const VectorF & newsize)
{
   for(U32 i = 0; i < size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ i ] );
      if( !object )
         continue;

      if( object->isGlobalBounds() )
         continue;

      const Box3F& bounds = object->getObjBox();
      VectorF extent = bounds.getExtents();
      VectorF scaledextent = object->getScale() * extent;

      VectorF scale = (newsize + scaledextent) / scaledextent;
      object->setScale( object->getScale() * scale );
   }
}

void WorldEditor::Selection::setSize(const VectorF & newsize)
{
   for(U32 i = 0; i < size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mObjectList[ i ] );
      if( !object )
         continue;

      if( object->isGlobalBounds() )
         continue;

      const Box3F& bounds = object->getObjBox();
      VectorF extent = bounds.getExtents();

      VectorF scale = newsize / extent;
      object->setScale( scale );
   }
}

SceneObject* WorldEditor::getClientObj(SceneObject * obj)
{
   AssertFatal(obj->isServerObject(), "WorldEditor::getClientObj: not a server object!");

   NetConnection * toServer = NetConnection::getConnectionToServer();
   NetConnection * toClient = NetConnection::getLocalClientConnection();
   if (!toServer || !toClient)
      return NULL;

   S32 index = toClient->getGhostIndex(obj);
   if(index == -1)
      return(0);

   return(dynamic_cast<SceneObject*>(toServer->resolveGhost(index)));
}

void WorldEditor::setClientObjInfo(SceneObject * obj, const MatrixF & mat, const VectorF & scale)
{
   SceneObject * clientObj = getClientObj(obj);
   if(!clientObj)
      return;

   clientObj->setTransform(mat);
   clientObj->setScale(scale);
}

void WorldEditor::updateClientTransforms(Selection & sel)
{

   for(U32 i = 0; i < sel.size(); i++)
   {
      SceneObject* serverObj = dynamic_cast< SceneObject* >( sel[ i ] );
      if( !serverObj )
         continue;
         
      SceneObject* clientObj = getClientObj( serverObj );
      if(!clientObj)
         continue;

      clientObj->setTransform(serverObj->getTransform());
      clientObj->setScale(serverObj->getScale());
   }
}

void WorldEditor::submitUndo( Selection &sel, const UTF8* label )
{
   // Grab the world editor undo manager.
   UndoManager *undoMan = NULL;
   if ( !Sim::findObject( "EUndoManager", undoMan ) )
   {
      Con::errorf( "WorldEditor::createUndo() - EUndoManager not found!" );
      return;           
   }

   // Setup the action.
   WorldEditorUndoAction *action = new WorldEditorUndoAction( label );
   for(U32 i = 0; i < sel.size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( sel[ i ] );
      if( !object )
         continue;
         
      WorldEditorUndoAction::Entry entry;

      entry.mMatrix = object->getTransform();
      entry.mScale = object->getScale();
      entry.mObjId = object->getId();
      action->mEntries.push_back( entry );
   }

   // Submit it.
   action->mWorldEditor = this;
   undoMan->addAction( action );
   
   // Mark the world editor as dirty!
   setDirty();
}

void WorldEditor::WorldEditorUndoAction::undo()
{
   // NOTE: This function also handles WorldEditorUndoAction::redo().

   MatrixF oldMatrix;
   VectorF oldScale;
   for( U32 i = 0; i < mEntries.size(); i++ )
   {
      SceneObject *obj;
      if ( !Sim::findObject( mEntries[i].mObjId, obj ) )
         continue;

      mWorldEditor->setClientObjInfo( obj, mEntries[i].mMatrix, mEntries[i].mScale );

      // Grab the current state.
      oldMatrix = obj->getTransform();
      oldScale = obj->getScale();

      // Restore the saved state.
      obj->setTransform( mEntries[i].mMatrix );
      obj->setScale( mEntries[i].mScale );

      // Store the previous state so the next time
      // we're called we can restore it.
      mEntries[i].mMatrix = oldMatrix;
      mEntries[i].mScale = oldScale;
   }

   // Mark the world editor as dirty!
   mWorldEditor->setDirty();
   mWorldEditor->mSelected.invalidateCentroid();

   // Let the script get a chance at it.
   Con::executef( mWorldEditor, "onWorldEditorUndo" );
}

//------------------------------------------------------------------------------
// edit stuff

bool WorldEditor::cutSelection(Selection & sel)
{
   if ( !sel.size() )
      return false;

   // First copy the selection.
   copySelection( sel );

   // Grab the world editor undo manager.
   UndoManager *undoMan = NULL;
   if ( !Sim::findObject( "EUndoManager", undoMan ) )
   {
      Con::errorf( "WorldEditor::cutSelection() - EUndoManager not found!" );
      return false;           
   }

   // Setup the action.
   MEDeleteUndoAction *action = new MEDeleteUndoAction();
   while ( sel.size() )
      action->deleteObject( sel[0] );
   undoMan->addAction( action );

   // Mark the world editor as dirty!
   setDirty();

   return true;
}

bool WorldEditor::copySelection(Selection & sel)
{
   mCopyBuffer.clear();

   for( U32 i = 0; i < sel.size(); i++ )
   {
      mCopyBuffer.increment();
      mCopyBuffer.last().save( sel[i] );
   }

   return true;
}

bool WorldEditor::pasteSelection( bool dropSel )
{
   clearSelection();

   // Grab the world editor undo manager.
   UndoManager *undoMan = NULL;
   if ( !Sim::findObject( "EUndoManager", undoMan ) )
   {
      Con::errorf( "WorldEditor::pasteSelection() - EUndoManager not found!" );
      return false;           
   }

   SimGroup *missionGroup = NULL;
   Sim::findObject( "MissionGroup", missionGroup );

   // Setup the action.
   MECreateUndoAction *action = new MECreateUndoAction( "Paste" );

   for( U32 i = 0; i < mCopyBuffer.size(); i++ )
   {
      SceneObject *obj = dynamic_cast<SceneObject*>( mCopyBuffer[i].restore() );
      if ( !obj )
         continue;

      if ( missionGroup )
         missionGroup->addObject( obj );

      action->addObject( obj );

      if ( !mSelectionLocked )
      {         
         mSelected.addObject( obj );
         Con::executef( this, "onSelect", obj->getIdString() );
      }
   }

   // Its safe to submit the action before the selection
   // is dropped below because the state of the objects 
   // are not stored until they are first undone.
   undoMan->addAction( action );

   // drop it ...
   if ( dropSel )
      dropSelection( mSelected );

   if ( mSelected.size() )
   {
      if ( isMethod("onClick") )
      {
         char buf[16];
         dSprintf( buf, sizeof(buf), "%d", mSelected[0]->getId() );

         SimObject * obj = 0;
         if(mRedirectID)
            obj = Sim::findObject(mRedirectID);
         Con::executef(obj ? obj : this, "onClick", buf);
      }
   }

   // Mark the world editor as dirty!
   setDirty();

   return true;
}

//------------------------------------------------------------------------------

void WorldEditor::hideSelection(bool hide)
{
   // set server/client objects hide field
   for(U32 i = 0; i < mSelected.size(); i++)
   {
      SceneObject* serverObj = dynamic_cast< SceneObject* >( mSelected[ i ] );
      if( !serverObj )
         continue;
         
      // client
      SceneObject * clientObj = getClientObj( serverObj );
      if(!clientObj)
         continue;

      clientObj->setHidden(hide);

      // server
      serverObj->setHidden(hide);
   }
}

void WorldEditor::lockSelection(bool lock)
{
   //
   for(U32 i = 0; i < mSelected.size(); i++)
      mSelected[i]->setLocked(lock);
}

//------------------------------------------------------------------------------
// the centroid get's moved to the drop point...
void WorldEditor::dropSelection(Selection & sel)
{
   if(!sel.size())
      return;

   setDirty();

   Point3F centroid = mObjectsUseBoxCenter ? sel.getBoxCentroid() : sel.getCentroid();

   switch(mDropType)
   {
      case DropAtCentroid:
         // already there
         break;

      case DropAtOrigin:
      {
         if(mDropAtBounds && !sel.containsGlobalBounds())
         {
            const Point3F& boxCenter = sel.getBoxCentroid();
            const Box3F& bounds = sel.getBoxBounds();
            Point3F offset = -boxCenter;
            offset.z += bounds.len_z() * 0.5f;

            sel.offset(offset);
         }
         else
            sel.offset(Point3F(-centroid));

         break;
      }

      case DropAtCameraWithRot:
      {
         Point3F center = centroid;
         if(mDropAtBounds && !sel.containsGlobalBounds())
            center = sel.getBoxBottomCenter();

         sel.offset(Point3F(smCamPos - center));
         sel.orient(smCamMatrix, center);
         break;
      }

      case DropAtCamera:
      {
         Point3F center = centroid;
         if(mDropAtBounds && !sel.containsGlobalBounds())
            sel.getBoxBottomCenter();

         sel.offset(Point3F(smCamPos - center));
         break;
      }

      case DropBelowCamera:
      {
         Point3F center = centroid;
         if(mDropAtBounds && !sel.containsGlobalBounds())
            center = sel.getBoxBottomCenter();

         Point3F offset = smCamPos - center;
         offset.z -= mDropBelowCameraOffset;
         sel.offset(offset);
         break;
      }

      case DropAtScreenCenter:
      {
         // Use the center of the selection bounds
         Point3F center = sel.getBoxCentroid();

         Gui3DMouseEvent event;
         event.pos = smCamPos;

         // Calculate the center of the sceen (in global screen coordinates)
         Point2I offset = localToGlobalCoord(Point2I(0,0));
         Point3F sp(F32(offset.x + F32(getExtent().x / 2)), F32(offset.y + (getExtent().y / 2)), 1.0f);

         // Calculate the view distance to fit the selection
         // within the camera's view.
         const Box3F bounds = sel.getBoxBounds();
         F32 radius = bounds.len()*0.5f;
         F32 viewdist = calculateViewDistance(radius) * mDropAtScreenCenterScalar;

         // Be careful of infinite sized objects, or just large ones in general.
         if(viewdist > mDropAtScreenCenterMax)
            viewdist = mDropAtScreenCenterMax;

         // Position the selection
         Point3F wp;
         unproject(sp, &wp);
         event.vec = wp - smCamPos;
         event.vec.normalizeSafe();
         event.vec *= viewdist;
         sel.offset(Point3F(event.pos - center) += event.vec);

         break;
      }

      case DropToTerrain:
      {
         terrainSnapSelection(sel, 0, mGizmo->getPosition(), true);
         break;
      }

      case DropBelowSelection:
      {
         dropBelowSelection(sel, centroid, mDropAtBounds);
         break;
      }
   }

   //
   updateClientTransforms(sel);
}

void WorldEditor::dropBelowSelection(Selection & sel, const Point3F & centroid, bool useBottomBounds)
{
   if(!sel.size())
      return;

   Point3F start;
   if(useBottomBounds && !sel.containsGlobalBounds())
      start = sel.getBoxBottomCenter();
   else
      start = centroid;

   Point3F end = start;
   end.z -= 4000.f;
      
   sel.disableCollision(); // Make sure we don't hit ourselves.

   RayInfo ri;
   bool hit;
   if(mBoundingBoxCollision)
      hit = gServerContainer.collideBox(start, end, STATIC_COLLISION_MASK, &ri);
   else
      hit = gServerContainer.castRay(start, end, STATIC_COLLISION_MASK, &ri);
      
   sel.enableCollision();

   if( hit )
      sel.offset(ri.point - start);
}

//------------------------------------------------------------------------------

void WorldEditor::terrainSnapSelection(Selection& sel, U8 modifier, Point3F gizmoPos, bool forceStick)
{
   mStuckToGround = false;

   if ( !mStickToGround && !forceStick )
      return;

   if(!sel.size())
      return;

   if(sel.containsGlobalBounds())
      return;

   Point3F centroid;
   if(mDropAtBounds && !sel.containsGlobalBounds())
      centroid = sel.getBoxBottomCenter();
   else
      centroid = mObjectsUseBoxCenter ? sel.getBoxCentroid() : sel.getCentroid();

   Point3F start = centroid;
   Point3F end = start;
   start.z -= 2000;
   end.z += 2000.f;
      
   sel.disableCollision(); // Make sure we don't hit ourselves.

   RayInfo ri;
   bool hit;
   if(mBoundingBoxCollision)
      hit = gServerContainer.collideBox(start, end, TerrainObjectType, &ri);
   else
      hit = gServerContainer.castRay(start, end, TerrainObjectType, &ri);
      
   sel.enableCollision();

   if( hit )
   {
      mStuckToGround = true;

      sel.offset(ri.point - centroid);

      if(mTerrainSnapAlignment != AlignNone)
      {
         EulerF rot(0.0f, 0.0f, 0.0f); // Equivalent to AlignPosY
         switch(mTerrainSnapAlignment)
         {
            case AlignPosX:
               rot.set(0.0f, 0.0f, mDegToRad(-90.0f));
               break;

            case AlignPosZ:
               rot.set(mDegToRad(90.0f), 0.0f, mDegToRad(180.0f));
               break;

            case AlignNegX:
               rot.set(0.0f, 0.0f, mDegToRad(90.0f));
               break;

            case AlignNegY:
               rot.set(0.0f, 0.0f, mDegToRad(180.0f));
               break;

            case AlignNegZ:
               rot.set(mDegToRad(-90.0f), 0.0f, mDegToRad(180.0f));
               break;
         }

         MatrixF mat = MathUtils::createOrientFromDir(ri.normal);
         MatrixF rotMat(rot);

         sel.orient(mat.mul(rotMat), Point3F::Zero);
      }
   }
}

void WorldEditor::softSnapSelection(Selection& sel, U8 modifier, Point3F gizmoPos)
{
   mSoftSnapIsStuck = false;
   mSoftSnapActivated = false;

   // If soft snap is activated, holding CTRL will temporarily deactivate it.
   // Conversely, if soft snapping is deactivated, holding CTRL will activate it.
   if( (mSoftSnap && (modifier & SI_PRIMARY_CTRL)) || (!mSoftSnap && !(modifier & SI_PRIMARY_CTRL)) )
      return;

   if(!sel.size())
      return;

   if(sel.containsGlobalBounds())
      return;

   mSoftSnapActivated = true;

   Point3F centroid = mObjectsUseBoxCenter ? sel.getBoxCentroid() : sel.getCentroid();

   // Find objects we may stick against
   Vector<SceneObject*> foundobjs;

   SceneObject *controlObj = getControlObject();
   if ( controlObj )
      controlObj->disableCollision();

   sel.disableCollision();

   if(mSoftSnapSizeByBounds)
   {
      mSoftSnapBounds = sel.getBoxBounds();
      mSoftSnapBounds.setCenter(centroid);
   }
   else
   {
      mSoftSnapBounds.set(Point3F(mSoftSnapSize, mSoftSnapSize, mSoftSnapSize));
      mSoftSnapBounds.setCenter(centroid);
   }

   mSoftSnapPreBounds = mSoftSnapBounds;
   mSoftSnapPreBounds.setCenter(gizmoPos);

   SphereF sphere(centroid, mSoftSnapBounds.len()*0.5f);

   gServerContainer.findObjectList(mSoftSnapBounds, 0xFFFFFFFF, &foundobjs);

   sel.enableCollision();

   if ( controlObj )
      controlObj->enableCollision();

   ConcretePolyList polys;
   for(S32 i=0; i<foundobjs.size(); ++i)
   {
      SceneObject* so = foundobjs[i];
      polys.setTransform(&(so->getTransform()), so->getScale());
      polys.setObject(so);
      so->buildRenderedPolyList(&polys, mSoftSnapBounds, sphere);
   }

   // Calculate sticky point
   bool     found = false;
   F32      foundDist = 1e10;
   Point3F  foundPoint(0.0f, 0.0f, 0.0f);
   PlaneF   foundPlane;
   MathUtils::IntersectInfo info;

   if(mSoftSnapDebugRender)
   {
      mSoftSnapDebugPoint.set(0.0f, 0.0f, 0.0f);
      mSoftSnapDebugTriangles.clear();
   }

   F32 backfaceToleranceSize = mSoftSnapBackfaceTolerance*mSoftSnapSize;
   for(S32 i=0; i<polys.mPolyList.size(); ++i)
   {
      ConcretePolyList::Poly p = polys.mPolyList[i];

      if(p.vertexCount >= 3)
      {
         S32 vertind[3];
		   vertind[0] = polys.mIndexList[p.vertexStart];
		   vertind[1] = polys.mIndexList[p.vertexStart + 1];
		   vertind[2] = polys.mIndexList[p.vertexStart + 2];

         // Distance to the triangle
         F32 d = MathUtils::mTriangleDistance(polys.mVertexList[vertind[0]], polys.mVertexList[vertind[1]], polys.mVertexList[vertind[2]], centroid, &info);

         // Cull backface polys that are not within tolerance
         if(p.plane.whichSide(centroid) == PlaneF::Back && d > backfaceToleranceSize)
            continue;

         bool changed = false;
         if(d < foundDist)
         {
            changed = true;
            found = true;
            foundDist = d;
            foundPoint = info.segment.p1;
            foundPlane = p.plane;

            if(mSoftSnapRenderTriangle)
            {
               mSoftSnapTriangle.p0 = polys.mVertexList[vertind[0]];
               mSoftSnapTriangle.p1 = polys.mVertexList[vertind[1]];
               mSoftSnapTriangle.p2 = polys.mVertexList[vertind[2]];
            }
         }

         if(mSoftSnapDebugRender)
         {
            Triangle debugTri;
            debugTri.p0 = polys.mVertexList[vertind[0]];
            debugTri.p1 = polys.mVertexList[vertind[1]];
            debugTri.p2 = polys.mVertexList[vertind[2]];
            mSoftSnapDebugTriangles.push_back(debugTri);

            if(changed)
            {
               mSoftSnapDebugSnapTri = debugTri;
               mSoftSnapDebugPoint = foundPoint;
            }
         }
      }
   }

   if(found)
   {
      sel.offset(foundPoint - centroid);

      if(mSoftSnapAlignment != AlignNone)
      {
         EulerF rot(0.0f, 0.0f, 0.0f); // Equivalent to AlignPosY
         switch(mSoftSnapAlignment)
         {
            case AlignPosX:
               rot.set(0.0f, 0.0f, mDegToRad(-90.0f));
               break;

            case AlignPosZ:
               rot.set(mDegToRad(90.0f), 0.0f, mDegToRad(180.0f));
               break;

            case AlignNegX:
               rot.set(0.0f, 0.0f, mDegToRad(90.0f));
               break;

            case AlignNegY:
               rot.set(0.0f, 0.0f, mDegToRad(180.0f));
               break;

            case AlignNegZ:
               rot.set(mDegToRad(-90.0f), 0.0f, mDegToRad(180.0f));
               break;
         }

         MatrixF mat = MathUtils::createOrientFromDir(foundPlane.getNormal());
         MatrixF rotMat(rot);

         sel.orient(mat.mul(rotMat), Point3F::Zero);
      }
   }

   mSoftSnapIsStuck = found;
}

//------------------------------------------------------------------------------

SceneObject * WorldEditor::getControlObject()
{
   GameConnection * connection = GameConnection::getLocalClientConnection();
   if(connection)
      return(dynamic_cast<SceneObject*>(connection->getControlObject()));
   return(0);
}

bool WorldEditor::collide( const Gui3DMouseEvent &event, SceneObject **hitObj )
{
   if ( mBoundingBoxCollision )
   {
      // Raycast against sceneObject bounding boxes...

      SceneObject *controlObj = getControlObject();
      if ( controlObj )
         controlObj->disableCollision();

      Point3F startPnt = event.pos;
      Point3F endPnt = event.pos + event.vec * smProjectDistance;
      RayInfo ri;

      bool hit = gServerContainer.collideBox(startPnt, endPnt, 0xFFFFFFFF, &ri);

      if ( controlObj )
         controlObj->enableCollision();

      if ( hit )      
         *hitObj = ri.object;

      return hit;         
   }

   // Collide against the screen-space class icons...

   S32 collidedIconIdx = -1;
   F32 collidedIconDist = F32_MAX;

   for ( U32 i = 0; i < mIcons.size(); i++ )      
   {
      const IconObject &icon = mIcons[i];      

      if ( icon.rect.pointInRect( event.mousePoint ) &&
           icon.dist < collidedIconDist )
      {
         collidedIconIdx = i;
         collidedIconDist = icon.dist;
      }
   }

   if ( collidedIconIdx != -1 )
   {
      *hitObj = mIcons[collidedIconIdx].object;
      return true;
   }

   // No icon hit so check against the mesh
   if ( mObjectMeshCollision )
   {
      SceneObject *controlObj = getControlObject();
      if ( controlObj )
         controlObj->disableCollision();

      Point3F startPnt = event.pos;
      Point3F endPnt = event.pos + event.vec * smProjectDistance;
      RayInfo ri;

      bool hit = gServerContainer.castRayRendered(startPnt, endPnt, 0xFFFFFFFF, &ri);
      if(hit && ri.object && ri.object->getTypeMask() & (TerrainObjectType))
      {
         // We don't want to mesh select terrain
         hit = false;
      }

      if ( controlObj )
         controlObj->enableCollision();

      if ( hit )      
         *hitObj = ri.object;

      return hit;         
   }

   return false;   
}

//------------------------------------------------------------------------------
// main render functions

void WorldEditor::renderSelectionWorldBox(Selection & sel)
{

   if(!mRenderSelectionBox)
      return;

   //
   if(!sel.size())
      return;

   // build the world bounds
   Box3F selBox( TypeTraits< F32 >::MAX, TypeTraits< F32 >::MAX, TypeTraits< F32 >::MAX,
                 TypeTraits< F32 >::MIN, TypeTraits< F32 >::MIN, TypeTraits< F32 >::MIN );

   U32 i;
   for(i = 1; i < sel.size(); i++)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( sel[ i ] );
      if( !object )
         continue;
         
      const Box3F & wBox = object->getWorldBox();
      selBox.minExtents.setMin(wBox.minExtents);
      selBox.maxExtents.setMax(wBox.maxExtents);
   }

   //

   // GFX2_RENDER_MEGE
   /*
   PrimBuild::color( mSelectionBoxColor );

   GFX->setCullMode( GFXCullNone );
   GFX->setAlphaBlendEnable( false );
   GFX->setTextureStageColorOp( 0, GFXTOPDisable );
   GFX->setZEnable( true );

   // create the box points
   Point3F projPnts[8];
   for(i = 0; i < 8; i++)
   {
      Point3F pnt;
      pnt.set(BoxPnts[i].x ? selBox.maxExtents.x : selBox.minExtents.x,
              BoxPnts[i].y ? selBox.maxExtents.y : selBox.minExtents.y,
              BoxPnts[i].z ? selBox.maxExtents.z : selBox.minExtents.z);
      projPnts[i] = pnt;
   }

   // do the box
   for(U32 j = 0; j < 6; j++)
   {
      PrimBuild::begin( GFXLineStrip, 4 );
      for(U32 k = 0; k < 4; k++)
      {
         PrimBuild::vertex3fv( projPnts[BoxVerts[j][k]] );
      }
      PrimBuild::end();
   }

   GFX->setZEnable( true );
   */
}

void WorldEditor::renderObjectBox( SceneObject *obj, const ColorI &color )
{
   if ( mRenderObjectBoxSB.isNull() )
   {
      GFXStateBlockDesc desc;
      desc.setCullMode( GFXCullNone );
      desc.setZReadWrite( true, true );
      mRenderObjectBoxSB = GFX->createStateBlock( desc );
   }

   GFX->setStateBlock(mRenderObjectBoxSB);

   GFXTransformSaver saver;

   Box3F objBox = obj->getObjBox();
   Point3F objScale = obj->getScale();   
   Point3F boxScale = objBox.getExtents();
   Point3F boxCenter = obj->getWorldBox().getCenter();

   MatrixF objMat = obj->getTransform();
   objMat.scale(objScale);
   objMat.scale(boxScale);
   objMat.setPosition(boxCenter);

   //GFX->multWorld( objMat );

   PrimBuild::color( ColorI(255,255,255,255) );
   PrimBuild::begin( GFXLineList, 48 );

   //Box3F objBox = obj->getObjBox();
   //Point3F size = objBox.getExtents();
   //Point3F halfSize = size * 0.5f;

   static const Point3F cubePoints[8] = 
   {
      Point3F(-0.5, -0.5, -0.5), Point3F(-0.5, -0.5,  0.5), Point3F(-0.5,  0.5, -0.5), Point3F(-0.5,  0.5,  0.5),
      Point3F( 0.5, -0.5, -0.5), Point3F( 0.5, -0.5,  0.5), Point3F( 0.5,  0.5, -0.5), Point3F( 0.5,  0.5,  0.5)
   };

   // 8 corner points of the box   
   for ( U32 i = 0; i < 8; i++ )
   {
      //const Point3F &start = cubePoints[i];  

      // 3 lines per corner point
      for ( U32 j = 0; j < 3; j++ )
      {
         Point3F start = cubePoints[i];
         Point3F end = start;
         end[j] *= 0.8f;

         objMat.mulP(start);
         PrimBuild::vertex3fv(start);
         objMat.mulP(end);
         PrimBuild::vertex3fv(end);            
      }
   }

   PrimBuild::end();
}

void WorldEditor::renderObjectFace(SceneObject * obj, const VectorF & normal, const ColorI & col)
{
   if ( mRenderObjectFaceSB.isNull() )
   {
      GFXStateBlockDesc desc;
      desc.setCullMode( GFXCullNone );
      desc.setBlend(true, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha);
      desc.setZReadWrite( false );
      mRenderObjectFaceSB = GFX->createStateBlock( desc );
   }

   GFX->setStateBlock(mRenderObjectFaceSB);

   // get the normal index
   VectorF objNorm;
   obj->getWorldTransform().mulV(normal, &objNorm);

   U32 normI = getBoxNormalIndex(objNorm);

   //
   Box3F box = obj->getObjBox();
   MatrixF mat = obj->getTransform();
   VectorF scale = obj->getScale();

   Point3F projPnts[4];
   for(U32 i = 0; i < 4; i++)
   {
      Point3F pnt;
      pnt.set(BoxPnts[BoxVerts[normI][i]].x ? box.maxExtents.x : box.minExtents.x,
              BoxPnts[BoxVerts[normI][i]].y ? box.maxExtents.y : box.minExtents.y,
              BoxPnts[BoxVerts[normI][i]].z ? box.maxExtents.z : box.minExtents.z);

      // scale it
      pnt.convolve(scale);
      mat.mulP(pnt, &projPnts[i]);
   }

   PrimBuild::color( col );

   PrimBuild::begin( GFXTriangleFan, 4 );
      for(U32 k = 0; k < 4; k++)
      {
         PrimBuild::vertex3f(projPnts[k].x, projPnts[k].y, projPnts[k].z);
      }
   PrimBuild::end();
}

void WorldEditor::renderMousePopupInfo()
{
   if ( !mMouseDragged )
      return;

      
   if ( mGizmoProfile->mode == NoneMode )
      return;

   char buf[256];

   switch ( mGizmoProfile->mode )
   {
      case MoveMode:      
      {
         if ( !mSelected.size() )
            return;

         Point3F pos = getSelectionCentroid();
         dSprintf(buf, sizeof(buf), "x: %0.3f, y: %0.3f, z: %0.3f", pos.x, pos.y, pos.z);

         break;
      }

      case RotateMode:
      {
         if ( !bool(mHitObject) || (mSelected.size() != 1) )
            return;

         // print out the angle-axis
         AngAxisF aa(mHitObject->getTransform());

         dSprintf(buf, sizeof(buf), "x: %0.3f, y: %0.3f, z: %0.3f, a: %0.3f",
            aa.axis.x, aa.axis.y, aa.axis.z, mRadToDeg(aa.angle));

         break;
      }

      case ScaleMode:
      {
         if ( !bool(mHitObject) || (mSelected.size() != 1) )
            return;

         VectorF scale = mHitObject->getScale();

         Box3F box = mHitObject->getObjBox();
         box.minExtents.convolve(scale);
         box.maxExtents.convolve(scale);

         box.maxExtents -= box.minExtents;
         dSprintf(buf, sizeof(buf), "w: %0.3f, h: %0.3f, d: %0.3f", box.maxExtents.x, box.maxExtents.y, box.maxExtents.z);

         break;
      }

      default:
         return;
   }

   U32 width = mProfile->mFont->getStrWidth((const UTF8 *)buf);
   Point2I posi( mLastMouseEvent.mousePoint.x, mLastMouseEvent.mousePoint.y + 12 );

   if ( mRenderPopupBackground )
   {
      Point2I minPt(posi.x - width / 2 - 2, posi.y - 1);
      Point2I maxPt(posi.x + width / 2 + 2, posi.y + mProfile->mFont->getHeight() + 1);

      GFX->getDrawUtil()->drawRectFill(minPt, maxPt, mPopupBackgroundColor);
   }

	GFX->getDrawUtil()->setBitmapModulation(mPopupTextColor);
   GFX->getDrawUtil()->drawText(mProfile->mFont, Point2I(posi.x - width / 2, posi.y), buf);
}

void WorldEditor::renderPaths(SimObject *obj)
{
   if (obj == NULL)
      return;
   bool selected = false;

   // Loop through subsets
   if (SimSet *set = dynamic_cast<SimSet*>(obj))
      for(SimSetIterator itr(set); *itr; ++itr) {
         renderPaths(*itr);
         if ((*itr)->isSelected())
            selected = true;
      }

   // Render the path if it, or any of it's immediate sub-objects, is selected.
   if (SimPath::Path *path = dynamic_cast<SimPath::Path*>(obj))
      if (selected || path->isSelected())
         renderSplinePath(path);
}


void WorldEditor::renderSplinePath(SimPath::Path *path)
{
   // at the time of writing the path properties are not part of the path object
   // so we don't know to render it looping, splined, linear etc.
   // for now we render all paths splined+looping

   Vector<Point3F> positions;
   Vector<QuatF>   rotations;

   path->sortMarkers();
   CameraSpline spline;

   for(SimSetIterator itr(path); *itr; ++itr)
   {
      Marker *pathmarker = dynamic_cast<Marker*>(*itr);
      if (!pathmarker)
         continue;
      Point3F pos;
      pathmarker->getTransform().getColumn(3, &pos);

      QuatF rot;
      rot.set(pathmarker->getTransform());
      CameraSpline::Knot::Type type;
      switch (pathmarker->mKnotType)
      {
         case Marker::KnotTypePositionOnly:  type = CameraSpline::Knot::POSITION_ONLY; break;
         case Marker::KnotTypeKink:          type = CameraSpline::Knot::KINK; break;
         case Marker::KnotTypeNormal:
         default:                            type = CameraSpline::Knot::NORMAL; break;

      }

      CameraSpline::Knot::Path path;
      switch (pathmarker->mSmoothingType)
      {
         case Marker::SmoothingTypeLinear:   path = CameraSpline::Knot::LINEAR; break;
         case Marker::SmoothingTypeSpline:
         default:                            path = CameraSpline::Knot::SPLINE; break;

      }

      spline.push_back(new CameraSpline::Knot(pos, rot, 1.0f, type, path));
   }

   F32 t = 0.0f;
   S32 size = spline.size();
   if (size <= 1)
      return;

   // DEBUG
   //spline.renderTimeMap();

   if (mSplineSB.isNull())
   {
      GFXStateBlockDesc desc;

      desc.setCullMode( GFXCullNone );
      desc.setBlend( true, GFXBlendSrcAlpha, GFXBlendInvSrcAlpha);
      desc.samplersDefined = true;
      desc.samplers[0].textureColorOp = GFXTOPDisable;

      mSplineSB = GFX->createStateBlock( desc );
   }

   GFX->setStateBlock(mSplineSB);


   if (path->isLooping())
   {
      CameraSpline::Knot *front = new CameraSpline::Knot(*spline.front());
      CameraSpline::Knot *back  = new CameraSpline::Knot(*spline.back());
      spline.push_back(front);
      spline.push_front(back);
      t = 1.0f;
      size += 2;
   }

   VectorF a(-0.45f, -0.55f, 0.0f);
   VectorF b( 0.0f,  0.55f, 0.0f);
   VectorF c( 0.45f, -0.55f, 0.0f);

   U32 vCount=0;

   F32 tmpT = t;
   while (tmpT < size - 1)
   {
      tmpT = spline.advanceDist(tmpT, 4.0f);
      vCount++;
   }

   // Build vertex buffer
 
   U32 batchSize = vCount;

   if(vCount > 4000)
      batchSize = 4000;

   GFXVertexBufferHandle<GFXVertexPC> vb;
   vb.set(GFX, 3*batchSize, GFXBufferTypeVolatile);
   vb.lock();

   U32 vIdx=0;

   while (t < size - 1)
   {
      CameraSpline::Knot k;
      spline.value(t, &k);
      t = spline.advanceDist(t, 4.0f);

      k.mRotation.mulP(a, &vb[vIdx+0].point);
      k.mRotation.mulP(b, &vb[vIdx+1].point);
      k.mRotation.mulP(c, &vb[vIdx+2].point);

      vb[vIdx+0].point += k.mPosition;
      vb[vIdx+1].point += k.mPosition;
      vb[vIdx+2].point += k.mPosition;

      vb[vIdx+0].color.set(0, 255, 0, 100);
      vb[vIdx+1].color.set(0, 255, 0, 100);
      vb[vIdx+2].color.set(0, 255, 0, 100);

      // vb[vIdx+3] = vb[vIdx+1];

      vIdx+=3;

      // Do we have to knock it out?
      if(vIdx > 3 * batchSize - 10)
      {
         vb.unlock();

         // Render the buffer
         GFX->setVertexBuffer(vb);
         GFX->drawPrimitive(GFXTriangleList,0,vIdx/3);

         // Reset for next pass...
         vIdx = 0;
         vb.lock();
      }
   }

   vb.unlock();

   // Render the buffer
   GFX->setVertexBuffer(vb);
   //GFX->drawPrimitive(GFXLineStrip,0,3);

   if(vIdx)
      GFX->drawPrimitive(GFXTriangleList,0,vIdx/3);
}

void WorldEditor::renderScreenObj( SceneObject *obj, Point3F projPos )
{
   // do not render control object stuff
   if(obj == getControlObject() || obj->isHidden() )
      return;

   GFXDrawUtil *drawer = GFX->getDrawUtil();
   
   // Lookup the ClassIcon - TextureHandle
   GFXTexHandle classIcon = gEditorIcons.findIcon( obj );

   if ( classIcon.isNull() )
      classIcon = mDefaultClassEntry.mDefaultHandle;

   U32 iconWidth = classIcon->getWidth();
   U32 iconHeight = classIcon->getHeight();

   bool isHighlight = ( obj == mHitObject || mDragSelected.objInSet(obj) );

   if ( isHighlight )
   {
      iconWidth += 0;
      iconHeight += 0;
   }

   Point2I sPos( (S32)projPos.x, (S32)projPos.y );
   //if ( obj->isSelected() )
   //   sPos.y += 4;
   Point2I renderPos = sPos;
   renderPos.x -= iconWidth / 2;
   renderPos.y -= iconHeight / 2;  

   Point2I iconSize( iconWidth, iconHeight );

   RectI renderRect( renderPos, iconSize );

   //
   if ( mRenderObjHandle && !obj->isSelected() )
   {
      if ( isHighlight )      
         drawer->setBitmapModulation( ColorI(255,255,255,255) );               
      else      
         drawer->setBitmapModulation( ColorI(255,255,255,125) );               

      drawer->drawBitmapStretch( classIcon, renderRect );      
      drawer->clearBitmapModulation();

      if ( obj->isLocked() )      
         drawer->drawBitmap( mDefaultClassEntry.mLockedHandle, renderPos );      

      // Save an IconObject for performing icon-click testing later.
      {
         IconObject icon;
         icon.object = obj;
         icon.rect = renderRect;
         icon.dist = projPos.z;             
         mIcons.push_back( icon );
      }           
   }

   //
   if ( mRenderObjText && ( obj == mHitObject || obj->isSelected() ) )
   {      
      const char * str = parseObjectFormat(obj, mObjTextFormat);

      Point2I extent(mProfile->mFont->getStrWidth((const UTF8 *)str), mProfile->mFont->getHeight());

      Point2I pos(sPos);

      if(mRenderObjHandle)
      {
         pos.x += (classIcon->getWidth() / 2) - (extent.x / 2);
         pos.y += (classIcon->getHeight() / 2) + 3;
      }
	  
      
	  if(mGizmoProfile->mode == NoneMode){
		  
		 drawer->drawBitmapStretch( classIcon, renderRect );
		 drawer->setBitmapModulation( ColorI(255,255,255,255) ); 
		 drawer->drawText(mProfile->mFont, pos, str); 
		 if ( obj->isLocked() )      
			drawer->drawBitmap( mDefaultClassEntry.mLockedHandle, renderPos );      

			// Save an IconObject for performing icon-click testing later.
		{
			IconObject icon;
			icon.object = obj;
			icon.rect = renderRect;
			icon.dist = projPos.z;             
			mIcons.push_back( icon );
		}
	  }else{
		  drawer->setBitmapModulation(mObjectTextColor);
		  drawer->drawText(mProfile->mFont, pos, str);
	  };
   }
}

//------------------------------------------------------------------------------
// ClassInfo stuff

WorldEditor::ClassInfo::~ClassInfo()
{
   for(U32 i = 0; i < mEntries.size(); i++)
      delete mEntries[i];
}

bool WorldEditor::objClassIgnored(const SimObject * obj)
{
   ClassInfo::Entry * entry = getClassEntry(obj);
   if(mToggleIgnoreList)
      return(!(entry ? entry->mIgnoreCollision : false));
   else
      return(entry ? entry->mIgnoreCollision : false);
}

WorldEditor::ClassInfo::Entry * WorldEditor::getClassEntry(StringTableEntry name)
{
   AssertFatal(name, "WorldEditor::getClassEntry - invalid args");
   for(U32 i = 0; i < mClassInfo.mEntries.size(); i++)
      if(!dStricmp(name, mClassInfo.mEntries[i]->mName))
         return(mClassInfo.mEntries[i]);
   return(0);
}

WorldEditor::ClassInfo::Entry * WorldEditor::getClassEntry(const SimObject * obj)
{
   AssertFatal(obj, "WorldEditor::getClassEntry - invalid args");
   return(getClassEntry(obj->getClassName()));
}

bool WorldEditor::addClassEntry(ClassInfo::Entry * entry)
{
   AssertFatal(entry, "WorldEditor::addClassEntry - invalid args");
   if(getClassEntry(entry->mName))
      return(false);

   mClassInfo.mEntries.push_back(entry);
   return(true);
}

//------------------------------------------------------------------------------
// Mouse cursor stuff
void WorldEditor::setCursor(U32 cursor)
{
   mCurrentCursor = cursor;
}

//------------------------------------------------------------------------------
Signal<void(WorldEditor*)> WorldEditor::smRenderSceneSignal;

WorldEditor::WorldEditor() : mCurrentCursor(PlatformCursorController::curArrow)
{
   // init the field data
   mDropType = DropAtScreenCenter;   
   mBoundingBoxCollision = true;   
   mObjectMeshCollision = true;
   mRenderPopupBackground = true;
   mPopupBackgroundColor.set(100,100,100);
   mPopupTextColor.set(255,255,0);
   mSelectHandle = StringTable->insert("tools/worldEditor/images/SelectHandle");
   mDefaultHandle = StringTable->insert("tools/worldEditor/images/DefaultHandle");
   mLockedHandle = StringTable->insert("tools/worldEditor/images/LockedHandle");
   mObjectTextColor.set(255,255,255);
   mObjectsUseBoxCenter = true;
   
   mObjSelectColor.set(255,0,0);
   mObjMouseOverSelectColor.set(0,0,255);
   mObjMouseOverColor.set(0,255,0);
   mShowMousePopupInfo = true;
   mDragRectColor.set(255,255,0);
   mRenderObjText = true;
   mRenderObjHandle = true;
   mObjTextFormat = StringTable->insert("$id$: $name|internal$");
   mFaceSelectColor.set(0,0,100,100);
   mRenderSelectionBox = true;
   mSelectionBoxColor.set(255,255,0);
   mSelectionLocked = false;

   mToggleIgnoreList = false;

   mIsDirty = false;

   mRedirectID = 0;

   //
   mHitObject = NULL;

   //
   //mDefaultMode = mCurrentMode = Move;
   mMouseDown = false;
   mDragSelect = false;

   mStickToGround = false;
   mStuckToGround = false;
   mTerrainSnapAlignment = AlignNone;
   mDropAtBounds = false;
   mDropBelowCameraOffset = 15.0f;
   mDropAtScreenCenterScalar = 1.0f;
   mDropAtScreenCenterMax = 100.0f;

   //
   mSelected.autoSelect(true);
   mDragSelected.autoSelect(false);

   //
   mSoftSnap = false;
   mSoftSnapActivated = false;
   mSoftSnapIsStuck = false;
   mSoftSnapAlignment = AlignNone;
   mSoftSnapRender = true;
   mSoftSnapRenderTriangle = false;
   mSoftSnapSizeByBounds = false;
   mSoftSnapSize = 2.0f;
   mSoftSnapBackfaceTolerance = 0.5f;
   mSoftSnapDebugRender = false;
   mSoftSnapDebugPoint.set(0.0f, 0.0f, 0.0f);
}

WorldEditor::~WorldEditor()
{
}

//------------------------------------------------------------------------------

bool WorldEditor::onAdd()
{
   if(!Parent::onAdd())
      return(false);

   // create the default class entry
   mDefaultClassEntry.mName = 0;
   mDefaultClassEntry.mIgnoreCollision = false;
   mDefaultClassEntry.mDefaultHandle   = GFXTexHandle(mDefaultHandle,   &GFXDefaultStaticDiffuseProfile, avar("%s() - mDefaultClassEntry.mDefaultHandle (line %d)", __FUNCTION__, __LINE__));
   mDefaultClassEntry.mSelectHandle    = GFXTexHandle(mSelectHandle,    &GFXDefaultStaticDiffuseProfile, avar("%s() - mDefaultClassEntry.mSelectHandle (line %d)", __FUNCTION__, __LINE__));
   mDefaultClassEntry.mLockedHandle    = GFXTexHandle(mLockedHandle,    &GFXDefaultStaticDiffuseProfile, avar("%s() - mDefaultClassEntry.mLockedHandle (line %d)", __FUNCTION__, __LINE__));

   if(!(mDefaultClassEntry.mDefaultHandle && mDefaultClassEntry.mSelectHandle && mDefaultClassEntry.mLockedHandle))
      return false;

   //mGizmo = new Gizmo();
   //mGizmo->registerObject("WorldEditorGizmo");   
   mGizmo->assignName("WorldEditorGizmo");   

   return true;
}

//------------------------------------------------------------------------------

void WorldEditor::onEditorEnable()
{
   // go through and copy the hidden field to the client objects...
   for(SimSetIterator itr(Sim::getRootGroup());  *itr; ++itr)
   {
      SceneObject * obj = dynamic_cast<SceneObject *>(*itr);
      if(!obj)
         continue;

      // only work with a server obj...
      if(obj->isClientObject())
         continue;

      // grab the client object
      SceneObject * clientObj = getClientObj(obj);
      if(!clientObj)
         continue;

      //
      clientObj->setHidden(obj->isHidden());
   }
}

//------------------------------------------------------------------------------

void WorldEditor::get3DCursor(GuiCursor *&cursor, bool &visible, const Gui3DMouseEvent &event)
{
   TORQUE_UNUSED(event);
   cursor = NULL;
   visible = false;

   GuiCanvas *pRoot = getRoot();
   if( !pRoot )
      return Parent::get3DCursor(cursor,visible,event);

   if(pRoot->mCursorChanged != mCurrentCursor)
   {
      PlatformWindow *pWindow = static_cast<GuiCanvas*>(getRoot())->getPlatformWindow();
      AssertFatal(pWindow != NULL,"GuiControl without owning platform window!  This should not be possible.");
      PlatformCursorController *pController = pWindow->getCursorController();
      AssertFatal(pController != NULL,"PlatformWindow without an owned CursorController!");

      // We've already changed the cursor, 
      // so set it back before we change it again.
      if(pRoot->mCursorChanged != -1)
         pController->popCursor();

      // Now change the cursor shape
      pController->pushCursor(mCurrentCursor);
      pRoot->mCursorChanged = mCurrentCursor;
   }
}

void WorldEditor::on3DMouseMove(const Gui3DMouseEvent & event)
{   
   setCursor(PlatformCursorController::curArrow);
   mHitObject = NULL;

   //
   mUsingAxisGizmo = false;

   if ( mSelected.size() > 0 )
   {
      mGizmo->on3DMouseMove( event );

      if ( mGizmo->getSelection() != Gizmo::None )
      {
         mUsingAxisGizmo = true;
         mHitObject = dynamic_cast< SceneObject* >( mSelected[0] );
      }
   }

   if ( !mHitObject )
   {
      SceneObject *hitObj = NULL;
      if ( collide(event, &hitObj) && !objClassIgnored(hitObj) )
      {
         mHitObject = hitObj;
      }
   }
   
   mLastMouseEvent = event;
}

void WorldEditor::on3DMouseDown(const Gui3DMouseEvent & event)
{
   mMouseDown = true;
   mMouseDragged = false;
   mPerformedDragCopy = false;
   mLastMouseDownEvent = event;

   mouseLock();

   // check gizmo first
   mUsingAxisGizmo = false;
   mNoMouseDrag = false;

   if ( mSelected.size() > 0 )
   {
      mGizmo->on3DMouseDown( event );

      if ( mGizmo->getSelection() != Gizmo::None )
      {
         mUsingAxisGizmo = true;         
         mHitObject = dynamic_cast< SceneObject* >( mSelected[0] );

         return;
      }
   }   

   SceneObject *hitObj = NULL;
   if ( collide( event, &hitObj ) && !objClassIgnored( hitObj ) )
   {
      mPossibleHitObject = hitObj;
      mNoMouseDrag = true;
   }
   else if ( !mSelectionLocked )
   {
      if ( !(event.modifier & SI_SHIFT) )
         clearSelection();

      mDragSelect = true;
      mDragSelected.clear();
      mDragRect.set( Point2I(event.mousePoint), Point2I(0,0) );
      mDragStart = event.mousePoint;
   }

   mLastMouseEvent = event;
}

void WorldEditor::on3DMouseUp( const Gui3DMouseEvent &event )
{
   mMouseDown = false;
   mStuckToGround = false;
   mSoftSnapIsStuck = false;
   mSoftSnapActivated = false;
   mUsingAxisGizmo = false;
   mGizmo->on3DMouseUp(event);

   // check if selecting objects....
   if ( mDragSelect )
   {
      mDragSelect = false;
      mPossibleHitObject = NULL;

      // add all the objects from the drag selection into the normal selection
      clearSelection();

      for ( U32 i = 0; i < mDragSelected.size(); i++ )
      {
         Con::executef( this, "onSelect", avar("%d", mDragSelected[i]->getId()) );
         mSelected.addObject( mDragSelected[i] );
      }
      mDragSelected.clear();

      if ( mSelected.size() )
      {
         char buf[16];
         dSprintf( buf, sizeof(buf), "%d", mSelected[0]->getId() );

         SimObject *obj = NULL;
         if ( mRedirectID )
            obj = Sim::findObject( mRedirectID );
         Con::executef( obj ? obj : this, "onClick", buf );
      }

      mouseUnlock();
      return;
   }
   else if( mPossibleHitObject.isValid() )
   {
      if ( !mSelectionLocked )
      {
         if ( event.modifier & SI_SHIFT )
         {
            mNoMouseDrag = true;
            if ( mSelected.objInSet( mPossibleHitObject ) )
            {
               mSelected.removeObject( mPossibleHitObject );
               mSelected.storeCurrentCentroid();
               Con::executef( this, "onUnSelect", avar("%d", mPossibleHitObject->getId()) );
            }
            else
            {
               mSelected.addObject( mPossibleHitObject );
               mSelected.storeCurrentCentroid();
               Con::executef( this, "onSelect", avar("%d", mPossibleHitObject->getId()) );
            }
         }
         else
         {
            if ( !mSelected.objInSet( mPossibleHitObject ) )
            {
               mNoMouseDrag = true;
               for ( U32 i = 0; i < mSelected.size(); i++ )               
                  Con::executef( this, "onUnSelect", avar("%d", mSelected[i]->getId()) );
               
               mSelected.clear();
               mSelected.addObject( mPossibleHitObject );
               mSelected.storeCurrentCentroid();
               Con::executef( this, "onSelect", avar("%d", mPossibleHitObject->getId()) );
            }
         }
      }

      if ( event.mouseClickCount > 1 )
      {
         //
         char buf[16];
         dSprintf(buf, sizeof(buf), "%d", mPossibleHitObject->getId());

         SimObject *obj = NULL;
         if ( mRedirectID )
            obj = Sim::findObject( mRedirectID );
         Con::executef( obj ? obj : this, "onDblClick", buf );
      }
      else 
      {
         char buf[16];
         dSprintf( buf, sizeof(buf), "%d", mPossibleHitObject->getId() );

         SimObject *obj = NULL;
         if ( mRedirectID )
            obj = Sim::findObject( mRedirectID );
         Con::executef( obj ? obj : this, "onClick", buf );
      }

      mHitObject = mPossibleHitObject;
   }

   if(mSelected.hasCentroidChanged())
   {
      Con::executef( this, "onSelectionCentroidChanged");
   }

   if ( mMouseDragged && mSelected.size() )
   {
      if ( mSelected.size() )
      {
         if ( isMethod("onEndDrag") )
         {
            char buf[16];
            dSprintf( buf, sizeof(buf), "%d", mSelected[0]->getId() );

            SimObject * obj = 0;
            if ( mRedirectID )
               obj = Sim::findObject( mRedirectID );
            Con::executef( obj ? obj : this, "onEndDrag", buf );
         }
      }
   }

   //if ( mHitObject )
   //   mHitObject->inspectPostApply();
   //mHitObject = NULL;  

   //
   //mHitObject = hitObj;
   mouseUnlock();
}

void WorldEditor::on3DMouseDragged(const Gui3DMouseEvent & event)
{
   if ( !mMouseDown )
      return;

   if ( mNoMouseDrag )
   {
      // Perhaps we should start the drag after all
      if( mAbs(mLastMouseDownEvent.mousePoint.x - event.mousePoint.x) > 2 || mAbs(mLastMouseDownEvent.mousePoint.y - event.mousePoint.y) > 2 )
      {
         if ( !(event.modifier & SI_SHIFT) )
            clearSelection();

         mDragSelect = true;
         mDragSelected.clear();
         mDragRect.set( Point2I(mLastMouseDownEvent.mousePoint), Point2I(0,0) );
         mDragStart = mLastMouseDownEvent.mousePoint;

         mNoMouseDrag = false;
         mHitObject = NULL;
      }
      else
      {
         return;
      }
   }

   //
   if ( !mMouseDragged )
   {
      if ( !mUsingAxisGizmo )
      {
         // vert drag on new object.. reset hit offset
         if ( mHitObject && !mSelected.objInSet( mHitObject ) )
         {
            if ( !mSelectionLocked )
               mSelected.addObject( mHitObject );
         }
      }

      // create and add an undo state
      if ( !mDragSelect )
        submitUndo( mSelected );

      mMouseDragged = true;
   }

   // update the drag selection
   if ( mDragSelect )
   {
      // build the drag selection on the renderScene method - make sure no neg extent!
      mDragRect.point.x = (event.mousePoint.x < mDragStart.x) ? event.mousePoint.x : mDragStart.x;
      mDragRect.extent.x = (event.mousePoint.x > mDragStart.x) ? event.mousePoint.x - mDragStart.x : mDragStart.x - event.mousePoint.x;
      mDragRect.point.y = (event.mousePoint.y < mDragStart.y) ? event.mousePoint.y : mDragStart.y;
      mDragRect.extent.y = (event.mousePoint.y > mDragStart.y) ? event.mousePoint.y - mDragStart.y : mDragStart.y - event.mousePoint.y;
      return;
   }

   if ( !mUsingAxisGizmo && ( !mHitObject || !mSelected.objInSet( mHitObject ) ) )
      return;

   // anything locked?
   for ( U32 i = 0; i < mSelected.size(); i++ )
      if ( mSelected[i]->isLocked() )
         return;

   if ( mUsingAxisGizmo )
      mGizmo->on3DMouseDragged(event);

   switch ( mGizmoProfile->mode )
   {
   case MoveMode:

      // grabbed axis gizmo?
      if ( mUsingAxisGizmo )
      {
         // Check if a copy should be made
         if ( event.modifier & SI_SHIFT && !mPerformedDragCopy )
         {
            mPerformedDragCopy = true;
            copySelection(mSelected);
            pasteSelection(false);
         }

         mSelected.offset( mGizmo->getOffset() );

         // Handle various sticking
         terrainSnapSelection( mSelected, event.modifier, mGizmo->getPosition() );
         softSnapSelection( mSelected, event.modifier, mGizmo->getPosition() );

         updateClientTransforms( mSelected );
      }     
      break;

   case ScaleMode:

      // can scale only single selections
      if ( mSelected.size() > 1 )
         break;

      if ( mUsingAxisGizmo )
      {
         Point3F scale = mGizmo->getScale();

         mSelected.setScale( scale );
         updateClientTransforms(mSelected);
      }      
   
      break;

   case RotateMode:
   {
      Point3F centroid = getSelectionCentroid();
      EulerF rot = mGizmo->getDeltaRot();

      mSelected.rotate(rot, centroid);
      updateClientTransforms(mSelected);

      break;  
   }
      
   default:
      break;
   }

   mLastMouseEvent = event;
}

void WorldEditor::on3DMouseEnter(const Gui3DMouseEvent &)
{
}

void WorldEditor::on3DMouseLeave(const Gui3DMouseEvent &)
{
}

void WorldEditor::on3DRightMouseDown(const Gui3DMouseEvent & event)
{
}

void WorldEditor::on3DRightMouseUp(const Gui3DMouseEvent & event)
{
}

//------------------------------------------------------------------------------

void WorldEditor::updateGuiInfo()
{
   SimObject * obj = 0;
   if ( mRedirectID )
      obj = Sim::findObject( mRedirectID );

   char buf[] = "";
   Con::executef( obj ? obj : this, "onGuiUpdate", buf );
}

//------------------------------------------------------------------------------

static void findObjectsCallback( SceneObject *obj, void *val )
{
   Vector<SceneObject*> * list = (Vector<SceneObject*>*)val;
   list->push_back(obj);
}

struct DragMeshCallbackData {
   WorldEditor*            mWorldEditor;
   Box3F                   mBounds;
   SphereF                 mSphereBounds;
   Vector<SceneObject *>   mObjects;
   EarlyOutPolyList        mPolyList;
   MatrixF                 mStandardMat;
   Point3F                 mStandardScale;

   DragMeshCallbackData(WorldEditor* we, Box3F &bounds, SphereF &sphereBounds)
   {
      mWorldEditor = we;
      mBounds = bounds;
      mSphereBounds = sphereBounds;
      mStandardMat.identity();
      mStandardScale.set(1.0f, 1.0f, 1.0f);
   }
};

static Frustum gDragFrustum;
static void findDragMeshCallback( SceneObject *obj, void *data )
{
   DragMeshCallbackData* dragData = reinterpret_cast<DragMeshCallbackData*>(data);

   if(dragData->mWorldEditor->objClassIgnored(obj) || (obj->getTypeMask() & (TerrainObjectType | ProjectileObjectType)))
      return;

   // Reset the poly list for us
   dragData->mPolyList.clear();
   dragData->mPolyList.setTransform(&(dragData->mStandardMat), dragData->mStandardScale);

   // Do the work
   obj->buildRenderedPolyList(&(dragData->mPolyList), dragData->mBounds, dragData->mSphereBounds);
   if (!dragData->mPolyList.isEmpty())
   {
      dragData->mObjects.push_back(obj);
   }
}

void WorldEditor::renderScene( const RectI &updateRect )
{
   smRenderSceneSignal.trigger(this);
	
   // Grab this before anything here changes it.
   Frustum frustum;
   {
      F32 left, right, top, bottom, nearPlane, farPlane;
      bool isOrtho = false;   
      GFX->getFrustum( &left, &right, &bottom, &top, &nearPlane, &farPlane, &isOrtho );

      MatrixF cameraMat = GFX->getWorldMatrix();
      cameraMat.inverse();

      frustum.set( isOrtho, left, right, top, bottom, nearPlane, farPlane, cameraMat );
   }

   // Render the paths
   renderPaths(Sim::findObject("MissionGroup"));

   // walk selected
   U32 i;
   for ( i = 0; i < mSelected.size(); i++ )
   {
      if ( (const SceneObject *)mHitObject == mSelected[i] )
         continue;
      SceneObject* object = dynamic_cast< SceneObject* >( mSelected[ i ] );
      if( !object )
         continue;
      renderObjectBox(object, mObjSelectColor);
   }

   // do the drag selection
   for ( i = 0; i < mDragSelected.size(); i++ )
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mDragSelected[ i ] );
      if( object )
         renderObjectBox(object, mObjSelectColor);
   }

   // draw the mouse over obj
   if ( bool(mHitObject) )
   {
      ColorI & col = mSelected.objInSet(mHitObject) ? mObjMouseOverSelectColor : mObjMouseOverColor;
      renderObjectBox(mHitObject, col);
   }

   // stuff to do if there is a selection
   if ( mSelected.size() )
   {   
      if ( mRenderSelectionBox )
         renderSelectionWorldBox(mSelected);
         
      SceneObject* singleSelectedSceneObject = NULL;
      if ( mSelected.size() == 1 )
         singleSelectedSceneObject = dynamic_cast< SceneObject* >( mSelected[ 0 ] );

      MatrixF objMat(true);
      if( singleSelectedSceneObject )
         objMat = singleSelectedSceneObject->getTransform();

      Point3F worldPos = getSelectionCentroid();

      Point3F objScale = singleSelectedSceneObject ? singleSelectedSceneObject->getScale() : Point3F(1,1,1);
      
      mGizmo->set( objMat, worldPos, objScale );

      // Change the gizmo's centroid highlight based on soft sticking state
      if( mSoftSnapIsStuck || mStuckToGround )
         mGizmo->setCentroidHandleHighlight( true );

      mGizmo->renderGizmo(mLastCameraQuery.cameraMatrix);

      // Reset any highlighting
      if( mSoftSnapIsStuck || mStuckToGround )
         mGizmo->setCentroidHandleHighlight( false );

      // Soft snap box rendering
      if( (mSoftSnapRender || mSoftSnapRenderTriangle) && mSoftSnapActivated)
      {
         GFXDrawUtil *drawUtil = GFX->getDrawUtil();
         ColorI color;

         GFXStateBlockDesc desc;

         if(mSoftSnapRenderTriangle && mSoftSnapIsStuck)
         {
            desc.setBlend( false );
            desc.setZReadWrite( false, false );
            desc.fillMode = GFXFillWireframe;
            desc.cullMode = GFXCullNone;

            color.set( 255, 255, 128, 255 );
            drawUtil->drawTriangle( desc, mSoftSnapTriangle.p0, mSoftSnapTriangle.p1, mSoftSnapTriangle.p2, color );
         }

         if(mSoftSnapRender)
         {
            desc.setBlend( true );
            desc.blendSrc = GFXBlendOne;
            desc.blendDest = GFXBlendOne;
            desc.blendOp = GFXBlendOpAdd;
            desc.setZReadWrite( true, false );
            desc.cullMode = GFXCullCCW;

            color.set( 64, 64, 0, 255 );

            desc.fillMode = GFXFillWireframe;
            drawUtil->drawCube(desc, mSoftSnapPreBounds, color);

            desc.fillMode = GFXFillSolid;
            drawUtil->drawSphere(desc, mSoftSnapPreBounds.len()*0.05f, mSoftSnapPreBounds.getCenter(), color);
         }
      }
   }

   // Debug rendering of the soft stick
   if(mSoftSnapDebugRender)
   {
      GFXDrawUtil *drawUtil = GFX->getDrawUtil();
      ColorI color( 255, 0, 0, 255 );

      GFXStateBlockDesc desc;
      desc.setBlend( false );
      desc.setZReadWrite( false, false );

      if(mSoftSnapIsStuck)
      {
         drawUtil->drawArrow( desc, getSelectionCentroid(), mSoftSnapDebugPoint, color );

         color.set(255, 255, 255);
         desc.fillMode = GFXFillWireframe;
         for(S32 i=0; i<mSoftSnapDebugTriangles.size(); ++i)
         {
            drawUtil->drawTriangle( desc, mSoftSnapDebugTriangles[i].p0, mSoftSnapDebugTriangles[i].p1, mSoftSnapDebugTriangles[i].p2, color );
         }

         color.set(255, 255, 0);
         desc.fillMode = GFXFillSolid;
         desc.cullMode = GFXCullNone;
         drawUtil->drawTriangle( desc, mSoftSnapDebugSnapTri.p0, mSoftSnapDebugSnapTri.p1, mSoftSnapDebugSnapTri.p2, color );
      }

   }

   // Now do the 2D stuff...
   // icons and text
   GFX->setClipRect(updateRect);

   // update what is in the selection
   if ( mDragSelect )
      mDragSelected.clear();

   // Determine selected objects based on the drag box touching
   // a mesh if a drag operation has begun.
   if( mDragSelect && mDragRect.extent.x > 1 && mDragRect.extent.y > 1 )
   {
      // Build the drag frustum based on the rect
      F32 wwidth;
      F32 wheight;
      if(!mLastCameraQuery.ortho)
      {
         wwidth = mLastCameraQuery.nearPlane * mTan(mLastCameraQuery.fov / 2);
         wheight = F32(getHeight()) / F32(getWidth()) * wwidth;
      }
      else
      {
         wwidth = mLastCameraQuery.fov;
         wheight = F32(getHeight()) / F32(getWidth()) * wwidth;
      }

      F32 hscale = wwidth * 2 / F32(getWidth());
      F32 vscale = wheight * 2 / F32(getHeight());

      F32 left = (mDragRect.point.x - getPosition().x) * hscale - wwidth;
      F32 right = (mDragRect.point.x - getPosition().x + mDragRect.extent.x) * hscale - wwidth;
      F32 top = wheight - vscale * (mDragRect.point.y - getPosition().y);
      F32 bottom = wheight - vscale * (mDragRect.point.y - getPosition().y + mDragRect.extent.y);
      gDragFrustum.set(mLastCameraQuery.ortho, left, right, top, bottom, mLastCameraQuery.nearPlane, mLastCameraQuery.farPlane, mLastCameraQuery.cameraMatrix );

      // Create the search bounds and callback data
      Box3F bounds = gDragFrustum.getBounds();
      SphereF sphere;
      sphere.center = bounds.getCenter();
      sphere.radius = (bounds.maxExtents - sphere.center).len();
      DragMeshCallbackData data(this, bounds, sphere);

      // Set up the search normal and planes
      Point3F vec;
      mLastCameraQuery.cameraMatrix.getColumn(1,&vec);
      vec.neg();
      data.mPolyList.mNormal.set(vec);
      const PlaneF* planes = gDragFrustum.getPlanes();
      for(i=0; i<Frustum::PlaneCount; ++i)
      {
         data.mPolyList.mPlaneList.push_back(planes[i]);

         // Invert the planes as the poly list routines require a different
         // facing from gServerContainer.findObjects().
         data.mPolyList.mPlaneList.last().invert();
      }

      gServerContainer.findObjects( gDragFrustum, 0xFFFFFFFF, findDragMeshCallback, &data);
      for ( i = 0; i < data.mObjects.size(); i++ )
      {
         SceneObject *obj = data.mObjects[i];
         if(objClassIgnored(obj) || (obj->getTypeMask() & (TerrainObjectType | ProjectileObjectType)))
            continue;

         mDragSelected.addObject(obj);
      }
   }

   // Clear the vector of onscreen icons, will populate this below
   // Necessary for performing click testing efficiently
   mIcons.clear();

   // Cull Objects and perform icon rendering
   Vector<SceneObject *> objects;
   gServerContainer.findObjects( frustum, 0xFFFFFFFF, findObjectsCallback, &objects);
   for ( i = 0; i < objects.size(); i++ )
   {
      SceneObject *obj = objects[i];
      if(objClassIgnored(obj))
         continue;

      Point3F wPos;
      if(mObjectsUseBoxCenter)
         wPos = getBoundingBoxCenter(obj);
      else
         obj->getTransform().getColumn(3, &wPos);

      Point3F sPos;
      if ( project(wPos, &sPos) )
      {
         Point2I sPosI( (S32)sPos.x,(S32)sPos.y );
         if ( !updateRect.pointInRect(sPosI) )
            continue;

         // check if object needs to be added into the regions select

         // Probably should test the entire icon screen-rect instead of just the centerpoint
         // but would need to move some code from renderScreenObj to here.
         if ( mDragSelect )
            if ( mDragRect.pointInRect(sPosI) && !mSelected.objInSet(obj) )
               mDragSelected.addObject(obj);

         //
         renderScreenObj( obj, sPos );
      }
   }

   //// Debug render rect around icons
   //for ( U32 i = 0; i < mIcons.size(); i++ )
   //   GFX->getDrawUtil()->drawRect( mIcons[i].rect, ColorI(255,255,255,255) );

   if ( mShowMousePopupInfo && mMouseDown )
      renderMousePopupInfo();

   if ( mDragSelect )
      GFX->getDrawUtil()->drawRect( mDragRect, mDragRectColor );

   if ( mSelected.size() )
      mGizmo->renderText( mSaveViewport, mSaveModelview, mSaveProjection );
}

//------------------------------------------------------------------------------
// Console stuff

static EnumTable::Enums dropEnums[] =
{
	{ WorldEditor::DropAtOrigin,           "atOrigin"        },
   { WorldEditor::DropAtCamera,           "atCamera"        },
   { WorldEditor::DropAtCameraWithRot,    "atCameraRot"     },
   { WorldEditor::DropBelowCamera,        "belowCamera"     },
   { WorldEditor::DropAtScreenCenter,     "screenCenter"    },
   { WorldEditor::DropAtCentroid,         "atCentroid"      },
   { WorldEditor::DropToTerrain,          "toTerrain"       },
   { WorldEditor::DropBelowSelection,     "belowSelection"  }
};
static EnumTable gEditorDropTable(8, &dropEnums[0]);

static EnumTable::Enums snapAlignEnums[] =
{
	{ WorldEditor::AlignNone,  "None"   },
   { WorldEditor::AlignPosX,  "+X"     },
   { WorldEditor::AlignPosY,  "+Y"     },
   { WorldEditor::AlignPosZ,  "+Z"     },
   { WorldEditor::AlignNegX,  "-X"     },
   { WorldEditor::AlignNegY,  "-Y"     },
   { WorldEditor::AlignNegZ,  "-Z"     }
};
static EnumTable gSnapAlignTable(7, &snapAlignEnums[0]);

void WorldEditor::initPersistFields()
{
   addGroup( "Misc" );	

   addField( "isDirty",                TypeBool,   Offset(mIsDirty, WorldEditor) );
   addField( "stickToGround",          TypeBool,   Offset(mStickToGround, WorldEditor) );
   addField( "dropAtBounds",           TypeBool,   Offset(mDropAtBounds, WorldEditor) );
   addField( "dropBelowCameraOffset",  TypeF32,    Offset(mDropBelowCameraOffset, WorldEditor) );
   addField( "dropAtScreenCenterScalar",TypeF32,   Offset(mDropAtScreenCenterScalar, WorldEditor) );
   addField( "dropAtScreenCenterMax",  TypeF32,    Offset(mDropAtScreenCenterMax, WorldEditor) );
   addField( "dropType",               TypeEnum,   Offset(mDropType, WorldEditor), 1, &gEditorDropTable);   
   addField( "boundingBoxCollision",   TypeBool,   Offset(mBoundingBoxCollision, WorldEditor) );
   addField( "objectMeshCollision",    TypeBool,   Offset(mObjectMeshCollision, WorldEditor) );
   addField( "renderPopupBackground",  TypeBool,   Offset(mRenderPopupBackground, WorldEditor) );
   addField( "popupBackgroundColor",   TypeColorI, Offset(mPopupBackgroundColor, WorldEditor) );
   addField( "popupTextColor",         TypeColorI, Offset(mPopupTextColor, WorldEditor) );
   addField( "objectTextColor",        TypeColorI, Offset(mObjectTextColor, WorldEditor) );
   addProtectedField( "objectsUseBoxCenter", TypeBool, Offset(mObjectsUseBoxCenter, WorldEditor), &setObjectsUseBoxCenter, &defaultProtectedGetFn, "" );
   addField( "objSelectColor",         TypeColorI, Offset(mObjSelectColor, WorldEditor) );
   addField( "objMouseOverSelectColor",TypeColorI, Offset(mObjMouseOverSelectColor, WorldEditor) );
   addField( "objMouseOverColor",      TypeColorI, Offset(mObjMouseOverColor, WorldEditor) );
   addField( "showMousePopupInfo",     TypeBool,   Offset(mShowMousePopupInfo, WorldEditor) );
   addField( "dragRectColor",          TypeColorI, Offset(mDragRectColor, WorldEditor) );
   addField( "renderObjText",          TypeBool,   Offset(mRenderObjText, WorldEditor) );
   addField( "renderObjHandle",        TypeBool,   Offset(mRenderObjHandle, WorldEditor) );
   addField( "objTextFormat",          TypeString, Offset(mObjTextFormat, WorldEditor) );
   addField( "faceSelectColor",        TypeColorI, Offset(mFaceSelectColor, WorldEditor) );
   addField( "renderSelectionBox",     TypeBool,   Offset(mRenderSelectionBox, WorldEditor) );
   addField( "selectionBoxColor",      TypeColorI, Offset(mSelectionBoxColor, WorldEditor) );
   addField( "selectionLocked",        TypeBool,   Offset(mSelectionLocked, WorldEditor) );   
   //addField("sameScaleAllAxis", TypeBool, Offset(mSameScaleAllAxis, WorldEditor));
   addField( "toggleIgnoreList",       TypeBool,   Offset(mToggleIgnoreList, WorldEditor) );
   addField( "selectHandle",           TypeFilename, Offset(mSelectHandle, WorldEditor) );
   addField( "defaultHandle",          TypeFilename, Offset(mDefaultHandle, WorldEditor) );
   addField( "lockedHandle",           TypeFilename, Offset(mLockedHandle, WorldEditor) );

   endGroup( "Misc" );

   Parent::initPersistFields();
}

//------------------------------------------------------------------------------
// These methods are needed for the console interfaces.

void WorldEditor::ignoreObjClass( U32 argc, const char **argv )
{
   for(S32 i = 2; i < argc; i++)
   {
      ClassInfo::Entry * entry = getClassEntry(argv[i]);
      if(entry)
         entry->mIgnoreCollision = true;
      else
      {
         entry = new ClassInfo::Entry;
         entry->mName = StringTable->insert(argv[i]);
         entry->mIgnoreCollision = true;
         if(!addClassEntry(entry))
            delete entry;
      }
   }	
}

void WorldEditor::clearIgnoreList()
{
   for(U32 i = 0; i < mClassInfo.mEntries.size(); i++)
      mClassInfo.mEntries[i]->mIgnoreCollision = false;	
}

void WorldEditor::setObjectsUseBoxCenter(bool state)
{
   mObjectsUseBoxCenter = state;
   if( isMethod( "onSelectionCentroidChanged" ) )
      Con::executef( this, "onSelectionCentroidChanged" );
}

void WorldEditor::clearSelection()
{
   if(mSelectionLocked)
      return;

   // Give selected objects a chance at removing
   // any custom editors.
   for ( U32 i = 0; i < mSelected.size(); i++ )
      Con::executef(this, "onUnSelect", avar("%d", mSelected[i]->getId()));

   Con::executef(this, "onClearSelection");
   mSelected.clear();
}

void WorldEditor::selectObject( const char* obj )
{
   if ( mSelectionLocked )
      return;

   SimObject* select;

   if ( Sim::findObject(obj, select) && !objClassIgnored(select) )
   {
      Con::executef(this, "onSelect", avar("%d", select->getId()));
      mSelected.addObject(select);	
   }
}

void WorldEditor::unselectObject( const char *obj )
{
   if(mSelectionLocked)
      return;

   SimObject* select;

   if ( Sim::findObject(obj, select) && !objClassIgnored(select) )
   {

      if ( mSelected.objInSet(select) )
      {
         mSelected.removeObject(select);	
         Con::executef(this, "onUnSelect", avar("%d", select->getId()));
      }
   }
}

S32 WorldEditor::getSelectionSize()
{
	return mSelected.size();
}

S32 WorldEditor::getSelectObject(S32 index)
{
	// Return the object's id
	return mSelected[index]->getId();	
}

const Point3F& WorldEditor::getSelectionCentroid()
{
   return mObjectsUseBoxCenter ? mSelected.getBoxCentroid() : mSelected.getCentroid();
}

const char* WorldEditor::getSelectionCentroidText()
{
   const Point3F & centroid = getSelectionCentroid();
   char * ret = Con::getReturnBuffer(100);
   dSprintf(ret, 100, "%g %g %g", centroid.x, centroid.y, centroid.z);
   return ret;	
}

const Box3F& WorldEditor::getSelectionBounds()
{
   return mSelected.getBoxBounds();
}

Point3F WorldEditor::getSelectionExtent()
{
   const Box3F& box = getSelectionBounds();
   return box.getExtents();
}

F32 WorldEditor::getSelectionRadius()
{
   const Box3F box = getSelectionBounds();
   return box.len() * 0.5f;
}

void WorldEditor::dropCurrentSelection( bool skipUndo )
{
   if ( !mSelected.size() )
      return;

   if ( !skipUndo )
      submitUndo( mSelected );

	dropSelection( mSelected );	
}

void WorldEditor::redirectConsole( S32 objID )
{
	mRedirectID = objID;		
}

//------------------------------------------------------------------------------

bool WorldEditor::alignByBounds( S32 boundsAxis )
{
   if(boundsAxis < 0 || boundsAxis > 5)
      return false;

   if(mSelected.size() < 2)
      return true;

   S32 axis = boundsAxis >= 3 ? boundsAxis-3 : boundsAxis;
   bool useMax = boundsAxis >= 3 ? false : true;
   
   // Find out which selected object has its bounds the farthest out
   F32 pos;
   S32 baseObj = 0;
   if(useMax)
      pos = TypeTraits< F32 >::MIN;
   else
      pos = TypeTraits< F32 >::MAX;

   for(S32 i=1; i<mSelected.size(); ++i)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mSelected[ i ] );
      if( !object )
         continue;
         
      const Box3F& bounds = object->getWorldBox();

      if(useMax)
      {
         if(bounds.maxExtents[axis] > pos)
         {
            pos = bounds.maxExtents[axis];
            baseObj = i;
         }
      }
      else
      {
         if(bounds.minExtents[axis] < pos)
         {
            pos = bounds.minExtents[axis];
            baseObj = i;
         }
      }
   }

   submitUndo( mSelected, "Align By Bounds" );

   // Move all selected objects to align with the calculated bounds
   for(S32 i=0; i<mSelected.size(); ++i)
   {
      if(i == baseObj)
         continue;
         
      SceneObject* object = dynamic_cast< SceneObject* >( mSelected[ i ] );
      if( !object )
         continue;

      const Box3F& bounds = object->getWorldBox();
      F32 delta;
      if(useMax)
         delta = pos - bounds.maxExtents[axis];
      else
         delta = pos - bounds.minExtents[axis];

      Point3F objPos = object->getPosition();
      objPos[axis] += delta;
      object->setPosition(objPos);
   }

   return true;
}

bool WorldEditor::alignByAxis( S32 axis )
{
   if(axis < 0 || axis > 2)
      return false;

   if(mSelected.size() < 2)
      return true;
      
   SceneObject* object = dynamic_cast< SceneObject* >( mSelected[ 0 ] );
   if( !object )
      return false;

   submitUndo( mSelected, "Align By Axis" );

   // All objects will be repositioned to line up with the
   // first selected object
   Point3F pos = object->getPosition();

   for(S32 i=0; i<mSelected.size(); ++i)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mSelected[ i ] );
      if( !object )
         continue;
         
      Point3F objPos = object->getPosition();
      objPos[axis] = pos[axis];
      object->setPosition(objPos);
   }

   return true;
}

//------------------------------------------------------------------------------

void WorldEditor::transformSelection(bool position, Point3F& p, bool relativePos, bool rotate, EulerF& r, bool relativeRot, bool rotLocal, S32 scaleType, Point3F& s, bool sRelative, bool sLocal)
{
   if(mSelected.size() == 0)
      return;

   submitUndo( mSelected, "Transform Selection" );

   if( position )
   {
      if( relativePos )
      {
         mSelected.offset(p);
      }
      else
      {
         mSelected.setCentroidPosition(mObjectsUseBoxCenter, p);
      }
   }

   if( rotate )
   {
      Point3F centroid;
      if( mSelected.containsGlobalBounds() )
      {
         centroid = mSelected.getCentroid();
      }
      else
      {
         centroid = mObjectsUseBoxCenter ? mSelected.getBoxCentroid() : mSelected.getCentroid();
      }

      if( relativeRot )
      {
         if( rotLocal )
         {
            mSelected.rotate(r);
         }
         else
         {
            mSelected.rotate(r, centroid);
         }
      }
      else if( rotLocal )
      {
         // Can only do absolute rotation for multiple objects about
         // object center
         mSelected.setRotate(r);
      }
   }

   if( scaleType == 1 )
   {
      // Scale

      Point3F centroid;
      if( mSelected.containsGlobalBounds() )
      {
         centroid = mSelected.getCentroid();
      }
      else
      {
         centroid = mObjectsUseBoxCenter ? mSelected.getBoxCentroid() : mSelected.getCentroid();
      }

      if( sRelative )
      {
         if( sLocal )
         {
            mSelected.scale(s);
         }
         else
         {
            mSelected.scale(s, centroid);
         }
      }
      else
      {
         if( sLocal )
         {
            mSelected.setScale(s);
         }
         else
         {
            mSelected.setScale(s, centroid);
         }
      }
   }
   else if( scaleType == 2 )
   {
      // Size

      if( mSelected.containsGlobalBounds() )
         return;

      if( sRelative )
      {
         // Size is always local/object based
         mSelected.addSize(s);
      }
      else
      {
         // Size is always local/object based
         mSelected.setSize(s);
      }
   }

   updateClientTransforms(mSelected);

   if(mSelected.hasCentroidChanged())
   {
      Con::executef( this, "onSelectionCentroidChanged");
   }

   if ( isMethod("onEndDrag") )
   {
      char buf[16];
      dSprintf( buf, sizeof(buf), "%d", mSelected[0]->getId() );

      SimObject * obj = 0;
      if ( mRedirectID )
         obj = Sim::findObject( mRedirectID );
      Con::executef( obj ? obj : this, "onEndDrag", buf );
   }
}

//------------------------------------------------------------------------------

void WorldEditor::resetSelectedRotation()
{
   for(S32 i=0; i<mSelected.size(); ++i)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mSelected[ i ] );
      if( !object )
         continue;
         
      MatrixF mat(true);
      mat.setPosition(object->getPosition());
      object->setTransform(mat);
   }
}

void WorldEditor::resetSelectedScale()
{
   for(S32 i=0; i<mSelected.size(); ++i)
   {
      SceneObject* object = dynamic_cast< SceneObject* >( mSelected[ i ] );
      if( object )
         object->setScale(Point3F(1,1,1));
   }
}

//------------------------------------------------------------------------------

ConsoleMethod( WorldEditor, ignoreObjClass, void, 3, 0, "(string class_name, ...)")
{
	object->ignoreObjClass(argc, argv);
}

ConsoleMethod( WorldEditor, clearIgnoreList, void, 2, 2, "")
{
	object->clearIgnoreList();
}

ConsoleMethod( WorldEditor, clearSelection, void, 2, 2, "")
{
	object->clearSelection();
}

ConsoleMethod( WorldEditor, selectObject, void, 3, 3, "(SimObject obj)")
{
	object->selectObject(argv[2]);
}

ConsoleMethod( WorldEditor, unselectObject, void, 3, 3, "(SimObject obj)")
{
	object->unselectObject(argv[2]);
}

ConsoleMethod( WorldEditor, getSelectionSize, S32, 2, 2, "")
{
	return object->getSelectionSize();
}

ConsoleMethod( WorldEditor, getSelectedObject, S32, 3, 3, "(int index)")
{
   S32 index = dAtoi(argv[2]);
   if(index < 0 || index >= object->getSelectionSize())
   {
      Con::errorf(ConsoleLogEntry::General, "WorldEditor::getSelectedObject: invalid object index");
      return(-1);
   }

   return(object->getSelectObject(index));
}

ConsoleMethod( WorldEditor, getSelectionRadius, F32, 2, 2, "")
{
	return object->getSelectionRadius();
}

ConsoleMethod( WorldEditor, getSelectionCentroid, const char *, 2, 2, "")
{
	return object->getSelectionCentroidText();
}

ConsoleMethod( WorldEditor, getSelectionExtent, const char *, 2, 2, "")
{
   Point3F bounds = object->getSelectionExtent();
   char * ret = Con::getReturnBuffer(100);
   dSprintf(ret, 100, "%g %g %g", bounds.x, bounds.y, bounds.z);
   return ret;	
}

ConsoleMethod( WorldEditor, dropSelection, void, 2, 3, "( bool skipUndo = false )")
{
   bool skipUndo = false;
   if ( argc > 2 )
      skipUndo = dAtob( argv[2] );

	object->dropCurrentSelection( skipUndo );
}

void WorldEditor::cutCurrentSelection()
{
	cutSelection(mSelected);	
}

void WorldEditor::copyCurrentSelection()
{
	copySelection(mSelected);	
}

ConsoleMethod( WorldEditor, cutSelection, void, 2, 2, "")
{
   object->cutCurrentSelection();
}

ConsoleMethod( WorldEditor, copySelection, void, 2, 2, "")
{
   object->copyCurrentSelection();
}

ConsoleMethod( WorldEditor, pasteSelection, void, 2, 2, "")
{
   object->pasteSelection();
}

bool WorldEditor::canPasteSelection()
{
	return mCopyBuffer.empty() != true;
}

ConsoleMethod( WorldEditor, canPasteSelection, bool, 2, 2, "")
{
	return object->canPasteSelection();
}

ConsoleMethod( WorldEditor, hideSelection, void, 3, 3, "(bool hide)")
{
   object->hideSelection(dAtob(argv[2]));
}

ConsoleMethod( WorldEditor, lockSelection, void, 3, 3, "(bool lock)")
{
   object->lockSelection(dAtob(argv[2]));
}

ConsoleMethod( WorldEditor, alignByBounds, void, 3, 3, "(int boundsAxis)"
              "Align all selected objects against the given bounds axis.")
{
	if(!object->alignByBounds(dAtoi(argv[2])))
		Con::warnf(ConsoleLogEntry::General, avar("worldEditor.alignByBounds: invalid bounds axis '%s'", argv[2]));
}

ConsoleMethod( WorldEditor, alignByAxis, void, 3, 3, "(int axis)"
              "Align all selected objects along the given axis.")
{
	if(!object->alignByAxis(dAtoi(argv[2])))
		Con::warnf(ConsoleLogEntry::General, avar("worldEditor.alignByAxis: invalid axis '%s'", argv[2]));
}

ConsoleMethod( WorldEditor, resetSelectedRotation, void, 2, 2, "")
{
	object->resetSelectedRotation();
}

ConsoleMethod( WorldEditor, resetSelectedScale, void, 2, 2, "")
{
	object->resetSelectedScale();
}

ConsoleMethod( WorldEditor, redirectConsole, void, 3, 3, "( int objID )")
{
   object->redirectConsole(dAtoi(argv[2]));
}

ConsoleMethod( WorldEditor, addUndoState, void, 2, 2, "")
{
	object->addUndoState();
}

//-----------------------------------------------------------------------------

ConsoleMethod( WorldEditor, getSoftSnap, bool, 2, 2, "getSoftSnap()\n"
              "Is soft snapping always on?")
{
	return object->mSoftSnap;
}

ConsoleMethod( WorldEditor, setSoftSnap, void, 3, 3, "setSoftSnap(bool)\n"
              "Allow soft snapping all of the time.")
{
	object->mSoftSnap = dAtob(argv[2]);
}

ConsoleMethod( WorldEditor, getSoftSnapSize, F32, 2, 2, "getSoftSnapSize()\n"
              "Get the absolute size to trigger a soft snap.")
{
	return object->mSoftSnapSize;
}

ConsoleMethod( WorldEditor, setSoftSnapSize, void, 3, 3, "setSoftSnapSize(F32)\n"
              "Set the absolute size to trigger a soft snap.")
{
	object->mSoftSnapSize = dAtof(argv[2]);
}

ConsoleMethod( WorldEditor, getSoftSnapAlignment, const char*, 2, 2, "getSoftSnapAlignment()\n"
              "Get the soft snap alignment.")
{
   return gSnapAlignTable.table[object->mSoftSnapAlignment].label;
}

ConsoleMethod( WorldEditor, setSoftSnapAlignment, void, 3, 3, "setSoftSnapAlignment(align)\n"
              "Set the soft snap alignment.")
{
   S32 val = 0;
   for (S32 i = 0; i < gSnapAlignTable.size; i++)
   {
      if (! dStricmp(argv[2], gSnapAlignTable.table[i].label))
      {
         val = gSnapAlignTable.table[i].index;
         break;
      }
   }
   object->mSoftSnapAlignment = val;
}

ConsoleMethod( WorldEditor, softSnapSizeByBounds, void, 3, 3, "softSnapSizeByBounds(bool)\n"
              "Use selection bounds size as soft snap bounds.")
{
	object->mSoftSnapSizeByBounds = dAtob(argv[2]);
}

ConsoleMethod( WorldEditor, getSoftSnapBackfaceTolerance, F32, 2, 2, "getSoftSnapBackfaceTolerance()\n"
              "The fraction of the soft snap radius that backfaces may be included.")
{
	return object->mSoftSnapBackfaceTolerance;
}

ConsoleMethod( WorldEditor, setSoftSnapBackfaceTolerance, void, 3, 3, "setSoftSnapBackfaceTolerance(F32 with range of 0..1)\n"
              "The fraction of the soft snap radius that backfaces may be included.")
{
	object->mSoftSnapBackfaceTolerance = dAtof(argv[2]);
}

ConsoleMethod( WorldEditor, softSnapRender, void, 3, 3, "softSnapRender(bool)\n"
              "Render the soft snapping bounds.")
{
	object->mSoftSnapRender = dAtob(argv[2]);
}

ConsoleMethod( WorldEditor, softSnapRenderTriangle, void, 3, 3, "softSnapRenderTriangle(bool)\n"
              "Render the soft snapped triangle.")
{
	object->mSoftSnapRenderTriangle = dAtob(argv[2]);
}

ConsoleMethod( WorldEditor, softSnapDebugRender, void, 3, 3, "softSnapDebugRender(bool)\n"
              "Toggle soft snapping debug rendering.")
{
	object->mSoftSnapDebugRender = dAtob(argv[2]);
}

ConsoleMethod( WorldEditor, getTerrainSnapAlignment, const char*, 2, 2, "getTerrainSnapAlignment()\n"
              "Get the terrain snap alignment.")
{
   return gSnapAlignTable.table[object->mSoftSnapAlignment].label;
}

ConsoleMethod( WorldEditor, setTerrainSnapAlignment, void, 3, 3, "setTerrainSnapAlignment(align)\n"
              "Set the terrain snap alignment.")
{
   S32 val = 0;
   for (S32 i = 0; i < gSnapAlignTable.size; i++)
   {
      if (! dStricmp(argv[2], gSnapAlignTable.table[i].label))
      {
         val = gSnapAlignTable.table[i].index;
         break;
      }
   }
   object->mTerrainSnapAlignment = val;
}

ConsoleMethod( WorldEditor, transformSelection, void, 13, 13, "transformSelection(...)\n"
              "Transform selection by given parameters.")
{
   bool position = dAtob(argv[2]);
   Point3F p(0.0f, 0.0f, 0.0f);
   dSscanf(argv[3], "%g %g %g", &p.x, &p.y, &p.z);
   bool relativePos = dAtob(argv[4]);

   bool rotate = dAtob(argv[5]);
   EulerF r(0.0f, 0.0f, 0.0f);
   dSscanf(argv[6], "%g %g %g", &r.x, &r.y, &r.z);
   bool relativeRot = dAtob(argv[7]);
   bool rotLocal = dAtob(argv[8]);

   S32 scaleType = dAtoi(argv[9]);
   Point3F s(1.0f, 1.0f, 1.0f);
   dSscanf(argv[10], "%g %g %g", &s.x, &s.y, &s.z);
   bool sRelative = dAtob(argv[11]);
   bool sLocal = dAtob(argv[12]);

   object->transformSelection(position, p, relativePos, rotate, r, relativeRot, rotLocal, scaleType, s, sRelative, sLocal);
}
