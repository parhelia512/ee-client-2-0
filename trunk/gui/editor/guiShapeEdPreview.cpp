//------------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//------------------------------------------------------------------------------

#include "console/consoleTypes.h"
#include "console/console.h"
#include "gui/core/guiCanvas.h"
#include "gui/editor/guiShapeEdPreview.h"
#include "renderInstance/renderPassManager.h"
#include "lighting/lightManager.h"
#include "lighting/lightInfo.h"
#include "core/resourceManager.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"
#include "gfx/primBuilder.h"
#include "gfx/gfxDrawUtil.h"

static const F32 sMoveScaler = 50.0f;
static const F32 sZoomScaler = 200.0f;
static const int sNodeRectSize = 16;

//-----------------------------------------------------------------------------
// GuiShapeEdPreview
//-----------------------------------------------------------------------------

GuiShapeEdPreview::GuiShapeEdPreview()
:  mOrbitDist( 5.0f ),
   mMoveSpeed (1.0f ),
   mZoomSpeed (1.0f ),
   mModel( NULL ),
   mRenderGhost( false ),
   mRenderNodes( false ),
   mRenderBounds( false ),
   mSelectedNode(-1 ),
   mHoverNode(-1 ),
   mUsingAxisGizmo( false ),
   mSliderCtrl( NULL ),
   mIsPlaying( false ),
   mSeqIn( 0 ),
   mSeqOut( 0 ),
   mLastRenderTime( 0 ),
   mAnimThread( 0 ),
   mTimeScale( 1.0f ),
   mAnimationSeq( 0 ),
   mFakeSun( NULL ),
   mCameraRot( 0, 0, 3.9f ),
   mOrbitPos( 0, 0, 0 )
{
   mActive = true;

   // By default don't do dynamic reflection
   // updates for this viewport.
   mReflectPriority = 0.0f;
}

GuiShapeEdPreview::~GuiShapeEdPreview()
{
   if( mModel )
      SAFE_DELETE( mModel );
   if( mFakeSun )
      SAFE_DELETE( mFakeSun );
}

void GuiShapeEdPreview::initPersistFields()
{
   addField("renderGrid",     TypeBool,         Offset( mRenderGridPlane, EditTSCtrl ));
   addField("renderNodes",    TypeBool,         Offset( mRenderNodes, GuiShapeEdPreview ));
   addField("renderGhost",    TypeBool,         Offset( mRenderGhost, GuiShapeEdPreview ));
   addField("renderBounds",   TypeBool,         Offset( mRenderBounds, GuiShapeEdPreview ));
   addField("selectedNode",   TypeS32,          Offset( mSelectedNode, GuiShapeEdPreview ));
   addField("isPlaying",      TypeBool,         Offset( mIsPlaying, GuiShapeEdPreview ));
   addField("seqIn",          TypeF32,          Offset( mSeqIn, GuiShapeEdPreview ));
   addField("seqOut",         TypeF32,          Offset( mSeqOut, GuiShapeEdPreview ));

   Parent::initPersistFields();
}

bool GuiShapeEdPreview::onWake()
{
   if (!Parent::onWake())
      return false;

   if (!mFakeSun )
      mFakeSun = gClientSceneGraph->getLightManager()->createLightInfo();

   mFakeSun->setColor( ColorF( 1.0f, 1.0f, 1.0f ) );
   mFakeSun->setAmbient( ColorF( 0.5f, 0.5f, 0.5f ) );
   mFakeSun->setDirection( VectorF( 0.f, 0.707f, -0.707f ) );
   mFakeSun->setPosition( mFakeSun->getDirection() * -10000.0f );
   mFakeSun->setRange( 2000000.0f );

   mGizmoProfile->mode = MoveMode;

   return( true );
}

void GuiShapeEdPreview::setOrbitDistance(F32 distance)
{
   mOrbitDist = distance;
}

void GuiShapeEdPreview::setTimeScale(F32 scale)
{
   mTimeScale = scale;
   if ( mModel && mAnimThread )
      mModel->setTimeScale( mAnimThread, mTimeScale );
}

void GuiShapeEdPreview::updateNodeTransforms()
{
   if ( mModel )
      mModel->mDirtyFlags[0] |= TSShapeInstance::TransformDirty;
}

void GuiShapeEdPreview::setSliderCtrl(GuiSliderCtrl* ctrl)
{
   mSliderCtrl = ctrl;
}

void GuiShapeEdPreview::get3DCursor( GuiCursor *&cursor, 
                                       bool &visible, 
                                       const Gui3DMouseEvent &event_ )
{
   cursor = NULL;
   visible = false;

   GuiCanvas *root = getRoot();
   if ( !root )
      return;

   S32 currCursor = PlatformCursorController::curArrow;

   if ( root->mCursorChanged == currCursor )
      return;

   PlatformWindow *window = root->getPlatformWindow();
   PlatformCursorController *controller = window->getCursorController();

   // We've already changed the cursor, 
   // so set it back before we change it again.
   if ( root->mCursorChanged != -1 )
      controller->popCursor();

   // Now change the cursor shape
   controller->pushCursor( currCursor );
   root->mCursorChanged = currCursor;
}

void GuiShapeEdPreview::fitToShape()
{
   if ( !mModel )
      return;

   // Determine the shape bounding box given the current camera rotation
   MatrixF camRotMatrix( smCamMatrix );
   camRotMatrix.setPosition( Point3F::Zero );
   camRotMatrix.inverse();

   Box3F bounds = mModel->getShape()->bounds;
   camRotMatrix.mul( bounds );

   // Estimate the camera distance to fill the view by comparing the radii
   // of the box and the viewport
   F32 len_x = bounds.len_x();
   F32 len_z = bounds.len_z();
   F32 shapeRadius = mSqrt( len_x*len_x + len_z*len_z ) / 2;
   F32 viewRadius = 0.45f * getMin( getExtent().x, getExtent().y );

   mOrbitPos = mModel->getShape()->bounds.getCenter();
   mOrbitDist = ( shapeRadius / viewRadius ) * mSaveWorldToScreenScale.y;
}

//-----------------------------------------------------------------------------
// Camera control and Node editing
// - moving the mouse over a node will highlight (but not select) it
// - left clicking on a node will select it, the gizmo will appear
// - left clicking on no node will unselect the current node
// - left dragging the gizmo will translate/rotate the node
// - middle drag translates the view
// - right drag rotates the view
// - mouse wheel zooms the view
// - holding shift while changing the view speeds them up

bool GuiShapeEdPreview::onKeyDown(const GuiEvent& event)
{
   switch ( event.keyCode )
   {
   case KEY_CONTROL:
      // Holding CTRL puts the gizmo in rotate mode
      mGizmoProfile->mode = RotateMode;
      return true;
   default:
      break;
   }
   return false;
}

bool GuiShapeEdPreview::onKeyUp(const GuiEvent& event)
{
   switch ( event.keyCode )
   {
   case KEY_CONTROL:
      // Releasing CTRL puts the gizmo in move mode
      mGizmoProfile->mode = MoveMode;
      return true;
   default:
      break;
   }
   return false;
}

S32 GuiShapeEdPreview::collideNode(const Gui3DMouseEvent& event) const
{
   // Check if the given position is inside the screen rectangle of
   // any shape node
   S32 nodeIndex = -1;
   F32 minZ = 0;
   for ( S32 i = 0; i < mProjectedNodes.size(); i++)
   {
      const Point3F& pt = mProjectedNodes[i];
      if ( pt.z > 1.0f )
         continue;

      RectI rect( pt.x - sNodeRectSize/2, pt.y - sNodeRectSize/2, sNodeRectSize, sNodeRectSize );
      if ( rect.pointInRect( event.mousePoint ) )
      {
         if ( ( nodeIndex == -1 ) || ( pt.z < minZ ) )
         {
            nodeIndex = i;
            minZ = pt.z;
         }
      }
   }

   return nodeIndex;
}

void GuiShapeEdPreview::handleMouseDown(const GuiEvent& event, Mode mode)
{
   if (!mActive || !mVisible || !mAwake )
      return;

   mouseLock();
   mLastMousePos = event.mousePoint;

   if ( mode == NoneMode )
   {
      make3DMouseEvent( mLastEvent, event );

      // Check gizmo first
      mUsingAxisGizmo = false;
      if ( mSelectedNode != -1 )
      {
         mGizmo->on3DMouseDown( mLastEvent );
         if ( mGizmo->getSelection() != Gizmo::None )
         {
            mUsingAxisGizmo = true;
            return;
         }
      }

      // Check if we have clicked on a node
      S32 selected = collideNode( mLastEvent );
      if ( selected != mSelectedNode )
      {
         mSelectedNode = selected;
         Con::executef( this, "onNodeSelected", Con::getIntArg( mSelectedNode ));
      }
   }
}

void GuiShapeEdPreview::handleMouseUp(const GuiEvent& event, Mode mode)
{
   mouseUnlock();
   mUsingAxisGizmo = false;

   if ( mode == NoneMode )
   {
      make3DMouseEvent( mLastEvent, event );
      mGizmo->on3DMouseUp( mLastEvent );
   }
}

void GuiShapeEdPreview::handleMouseMove(const GuiEvent& event, Mode mode)
{
   if ( mode == NoneMode )
   {
      make3DMouseEvent( mLastEvent, event );
      if ( mSelectedNode != -1 )
      {
         // Check if the mouse is hovering over an axis
         mGizmo->on3DMouseMove( mLastEvent );
         if ( mGizmo->getSelection() != Gizmo::None )
            return;
      }

      // Check if we are over another node
      mHoverNode = collideNode( mLastEvent );
   }
}

void GuiShapeEdPreview::handleMouseDragged(const GuiEvent& event, Mode mode)
{
   if ( mode == NoneMode )
   {
      make3DMouseEvent( mLastEvent, event );

      if ( mUsingAxisGizmo )
      {
         // Use gizmo to modify the transform of the selected node
         mGizmo->on3DMouseDragged( mLastEvent );
         switch ( mGizmoProfile->mode )
         {
         case MoveMode:
            // Update node transform
            if ( mSelectedNode != -1 )
            {
               Point3F pos = mModel->mNodeTransforms[mSelectedNode].getPosition() + mGizmo->getOffset();
               mModel->mNodeTransforms[mSelectedNode].setPosition( pos );
            }
            break;

         case RotateMode:
            // Update node transform
            if ( mSelectedNode != -1 )
            {
               EulerF rot = mGizmo->getDeltaRot();
               mModel->mNodeTransforms[mSelectedNode].mul( MatrixF( rot ) );
            }
            break;
         default:
            break;
         }

         // Notify the change in node transform
         const char* name = mModel->getShape()->getNodeName(mSelectedNode).c_str();
         const Point3F pos = mModel->mNodeTransforms[mSelectedNode].getPosition();
         AngAxisF aa(mModel->mNodeTransforms[mSelectedNode]);
         char buffer[256];
         dSprintf(buffer, sizeof(buffer), "%g %g %g %g %g %g %g",
            pos.x, pos.y, pos.z, aa.axis.x, aa.axis.y, aa.axis.z, aa.angle);

         Con::executef(this, "onEditNodeTransform", name, buffer);
      }
   }
   else
   {
      Point2F delta( event.mousePoint.x - mLastMousePos.x, event.mousePoint.y - mLastMousePos.y );
      mLastMousePos = event.mousePoint;

      // Use shift to increase speed
      delta.x *= ( event.modifier & SI_SHIFT ) ? 0.05f : 0.01f;
      delta.y *= ( event.modifier & SI_SHIFT ) ? 0.05f : 0.01f;

      switch ( mode )
      {
      case MoveMode:
         {
            VectorF offset(-delta.x, 0, delta.y );
            smCamMatrix.mulV( offset );
            mOrbitPos += offset * mMoveSpeed;
         }
         break;

      case RotateMode:
         mCameraRot.x += delta.y;
         mCameraRot.z += delta.x;
         break;
      default:
         break;
      }
   }
}

bool GuiShapeEdPreview::onMouseWheelUp(const GuiEvent& event)
{
   // Use shift to increase speed
   setOrbitDistance( mOrbitDist - mFabs(event.fval) * mZoomSpeed * (( event.modifier & SI_SHIFT ) ? 1.0f : 0.25f) );
   return true;
}

bool GuiShapeEdPreview::onMouseWheelDown(const GuiEvent& event)
{
   // Use shift to increase speed
   setOrbitDistance( mOrbitDist + mFabs(event.fval) * mZoomSpeed * (( event.modifier & SI_SHIFT ) ? 1.0f : 0.25f) );
   return true;
}

//-----------------------------------------------------------------------------
void GuiShapeEdPreview::setObjectModel(const char* modelName)
{
   if ( mModel )
   {
      mModel->destroyThread( mAnimThread );
      mAnimThread = 0;
      SAFE_DELETE( mModel );
   }

   if (modelName && modelName[0])
   {
      Resource<TSShape> model = ResourceManager::get().load( modelName );
      if (! bool( model ))
      {
         Con::warnf( avar("GuiShapeEdPreview: Failed to load model %s. Please check your model name and load a valid model.", modelName ));
         return;
      }

      mModel = new TSShapeInstance( model, true );
      AssertFatal( mModel, avar("GuiShapeEdPreview: Failed to load model %s. Please check your model name and load a valid model.", modelName ));

      // Initialize camera values:
      mOrbitPos = mModel->getShape()->center;

      // Set camera move and zoom speed according to model size
      mMoveSpeed = mModel->getShape()->radius / sMoveScaler;
      mZoomSpeed = mModel->getShape()->radius / sZoomScaler;

      // Reset node selection
      mHoverNode = -1;
      mSelectedNode = -1;

      // the first time recording
      mLastRenderTime = Platform::getVirtualMilliseconds();
   }
}

void GuiShapeEdPreview::setObjectAnimation(const char* seqName)
{
   if ( mModel )
   {
      mAnimationSeq = mModel->getShape()->findSequence( seqName );
      if ( mAnimationSeq == -1 )
      {
         mModel->destroyThread( mAnimThread );
         mAnimThread = 0;
      }
      else
      {
         if (!mAnimThread )
         {
            mAnimThread = mModel->addThread();
            mModel->setTimeScale( mAnimThread, mTimeScale );
         }

         mModel->setSequence( mAnimThread, mAnimationSeq, 0.0f );
      }
   }
   else
   {
      mAnimationSeq = -1;
   }
}

//-----------------------------------------------------------------------------
bool GuiShapeEdPreview::processCameraQuery(CameraQuery* query)
{
   // Adjust the camera so that we are still facing the model:
   Point3F vec;
   MatrixF xRot, zRot;
   xRot.set( EulerF( mCameraRot.x, 0.0f, 0.0f ));
   zRot.set( EulerF( 0.0f, 0.0f, mCameraRot.z ));

   smCamMatrix.mul( zRot, xRot );
   smCamMatrix.getColumn( 1, &vec );
   vec *= mOrbitDist;
   smCamPos = mOrbitPos - vec;
   smCamMatrix.setColumn( 3, smCamPos );

   query->farPlane = 1000000.0f;
   query->nearPlane = 0.01f;
   query->fov = 45.0f;
   query->cameraMatrix = smCamMatrix;

   smCamOrtho = query->ortho;
   smCamNearPlane = query->nearPlane;

   return true;
}

void GuiShapeEdPreview::updateProjectedNodePoints()
{
   // @todo: When a node is added, we need to make sure to resize the nodeTransforms array as well
   mModel->mNodeTransforms.setSize( mModel->getShape()->nodes.size() );
   mProjectedNodes.setSize( mModel->getShape()->nodes.size() );

   for ( S32 i = 0; i < mModel->mNodeTransforms.size(); i++)
      project( mModel->mNodeTransforms[i].getPosition(), &mProjectedNodes[i] );
}

void GuiShapeEdPreview::renderWorld(const RectI &updateRect)
{
   if ( !mModel )
      return;

   F32 left, right, top, bottom, nearPlane, farPlane;
   bool isOrtho;
   GFX->getFrustum(&left, &right, &bottom, &top, &nearPlane, &farPlane, &isOrtho );
   Frustum frust( false, left, right, top, bottom, nearPlane, farPlane, MatrixF::Identity );

   SceneState state(
      NULL,
      gClientSceneGraph,
      SPT_Diffuse,
      1,
      frust,
      GFX->getViewport(), 
      false,
      false );

   // Set up our TS render state here.
   TSRenderState rdata;
   rdata.setSceneState(&state);

   // Set up pass transforms
   RenderPassManager *renderPass = state.getRenderPass();
   renderPass->assignSharedXform(RenderPassManager::View, MatrixF::Identity);
   renderPass->assignSharedXform(RenderPassManager::Projection, GFX->getProjectionMatrix());

   LightManager* lm = gClientSceneGraph->getLightManager();
   lm->unregisterAllLights();
   lm->setSpecialLight( LightManager::slSunLightType, mFakeSun );
   lm->setupLights(NULL, SphereF( Point3F::Zero, 1.0f ) );

   // Determine time elapsed since last render (for animation playback)
   S32 time = Platform::getVirtualMilliseconds();
   S32 dt = time - mLastRenderTime;
   mLastRenderTime = time;

   if ( mModel )
   {
      // Render the grid (auto-sized to the model)
      if ( mRenderGridPlane )
      {
         Box3F bounds = mModel->getShape()->bounds;
         F32 dim = getMax( bounds.len_x(), bounds.len_y() ) * 2.0f;
         Point2F size( dim, dim );
         Point2F majorStep( size / 6 );
         Point2F minorStep( majorStep / 10 );
         ColorF gray( 0.5f, 0.5f, 0.5f );

         GFXStateBlockDesc desc;
         desc.setBlend( true );
         desc.setZReadWrite( true, true );

         GFX->getDrawUtil()->drawPlaneGrid( desc, Point3F::Zero, size, minorStep, gray );
         GFX->getDrawUtil()->drawPlaneGrid( desc, Point3F::Zero, size, majorStep, ColorF::BLACK );
      }

      // Update projected node points (for mouse picking)
      updateProjectedNodePoints();

      GFX->setStateBlock( mDefaultGuiSB );

      // animate and render
      if ( mAnimThread )
      {
         F32 sliderRange = mSliderCtrl ? ( mSliderCtrl->getRange().y - mSliderCtrl->getRange().x ) : 0;

         // allow slider to change the thread position (even during playback)
         F32 threadPos = mModel->getPos( mAnimThread );
         if ( sliderRange )
         {
            F32 sliderPos = mSliderCtrl->getValue() / sliderRange;
            if ( sliderPos != threadPos )
            {
               threadPos = sliderPos;
               mModel->setPos( mAnimThread, threadPos );
            }
         }

         if ( mIsPlaying )
         {
            if ( threadPos < mSeqIn )
               mModel->setPos( mAnimThread, mSeqIn );
            mModel->advanceTime(( F32 )dt/1000.f, mAnimThread );

            // Ensure that position is within in/out values
            threadPos = mModel->getPos( mAnimThread );
            if ( threadPos < mSeqIn )
            {
               if ( mAnimThread->getSequence()->isCyclic())
                  threadPos = mSeqIn + mFmod( threadPos, mSeqOut - mSeqIn );
               else
                  threadPos = mSeqIn;
               mModel->setPos( mAnimThread, threadPos );
            }
            else if ( threadPos > mSeqOut )
            {
               if ( mAnimThread->getSequence()->isCyclic())
                  threadPos = mSeqIn + mFmod( threadPos - mSeqOut, mSeqOut - mSeqIn );
               else
                  threadPos = mSeqOut;
               mModel->setPos( mAnimThread, threadPos );
            }

            // update the slider value to match
            if ( sliderRange )
               mSliderCtrl->setValue( threadPos * sliderRange );
         }
      }

      mModel->animate();
      if ( mRenderGhost )
         rdata.setFadeOverride( 0.5f );
      mModel->render( rdata );

      // Optionally render the shape bounding box
      if ( mRenderBounds )
      {
         Point3F boxSize = mModel->getShape()->bounds.maxExtents - mModel->getShape()->bounds.minExtents;

         GFXStateBlockDesc desc;
         desc.fillMode = GFXFillWireframe;
         GFX->getDrawUtil()->drawCube( desc, boxSize, mModel->getShape()->center, ColorF::WHITE );
      }

      gClientSceneGraph->getRenderPass()->render( &state );
      gClientSceneGraph->getRenderPass()->clear();

      // render the nodes in the model
      renderNodes( updateRect );
   }
}

void GuiShapeEdPreview::renderNodes(const RectI& updateRect) const
{
   if ( mRenderNodes )
   {
      // Render links between nodes
      GFXStateBlockDesc desc;
      desc.setZReadWrite( false, true );
      desc.setCullMode( GFXCullNone );
      GFX->setStateBlockByDesc( desc );

      PrimBuild::color( ColorI::WHITE );
      PrimBuild::begin( GFXLineList, mModel->getShape()->nodes.size() * 2 );
      for ( S32 i = 0; i < mModel->getShape()->nodes.size(); i++)
      {
         const TSShape::Node& node = mModel->getShape()->nodes[i];
         const String& nodeName = mModel->getShape()->getName( node.nameIndex );
         if ( nodeName.compare( "__deleted_", 10 ) == 0 )
            continue;

         if (node.parentIndex >= 0)
         {
            Point3F start(mModel->mNodeTransforms[i].getPosition());
            Point3F end(mModel->mNodeTransforms[node.parentIndex].getPosition());

            PrimBuild::vertex3f( start.x, start.y, start.z );
            PrimBuild::vertex3f( end.x, end.y, end.z );
         }
      }
      PrimBuild::end();

      // Render the node axes
      for ( S32 i = 0; i < mModel->getShape()->nodes.size(); i++)
      {
         // Render the selected and hover nodes last (so they are on top)
         if ( ( i == mSelectedNode ) || ( i == mHoverNode ) )
            continue;   

         renderNodeAxes( i, ColorF::WHITE );
      }

      // Render the hovered node
      if ( mHoverNode != -1 )
         renderNodeAxes( mHoverNode, ColorF::GREEN );
   }

   // Render the selected node (even if mRenderNodes is false)
   if ( mSelectedNode != -1 )
   {
      renderNodeAxes( mSelectedNode, ColorF::GREEN );

      const MatrixF& nodeMat = mModel->mNodeTransforms[mSelectedNode];
      mGizmo->set( nodeMat, nodeMat.getPosition(), Point3F( 1, 1, 1 ));
      mGizmo->renderGizmo( smCamMatrix );
   }

   // Render the names of the hovered and selected nodes
   GFX->setClipRect( updateRect );
   if ( mRenderNodes && mHoverNode != -1 )
      renderNodeName( mHoverNode, ColorF::WHITE );
   if ( mSelectedNode != -1 )
      renderNodeName( mSelectedNode, ColorF::WHITE );
}

void GuiShapeEdPreview::renderNodeAxes(S32 index, const ColorF& nodeColor) const
{
   // Ignore nodes marked for deletion
   const TSShape::Node& node = mModel->getShape()->nodes[index];
   const String& nodeName = mModel->getShape()->getName( node.nameIndex );
   if ( nodeName.compare( "__deleted_", 10 ) == 0 )
      return;

   const Point3F xAxis( 1.0f,  0.15f, 0.15f );
   const Point3F yAxis( 0.15f, 1.0f,  0.15f );
   const Point3F zAxis( 0.15f, 0.15f, 1.0f  );

   GFXStateBlockDesc desc;
   desc.setZReadWrite( false, true );
   desc.setCullMode( GFXCullNone );

   // Render nodes the same size regardless of zoom
   F32 scale = mOrbitDist / 60;

   GFX->pushWorldMatrix();
   GFX->multWorld( mModel->mNodeTransforms[index] );

   GFX->getDrawUtil()->drawCube( desc, xAxis * scale, Point3F::Zero, nodeColor );
   GFX->getDrawUtil()->drawCube( desc, yAxis * scale, Point3F::Zero, nodeColor );
   GFX->getDrawUtil()->drawCube( desc, zAxis * scale, Point3F::Zero, nodeColor );

   GFX->popWorldMatrix();
}

void GuiShapeEdPreview::renderNodeName(S32 index, const ColorF& textColor) const
{
   // Ignore nodes marked for deletion
   const TSShape::Node& node = mModel->getShape()->nodes[index];
   const String& nodeName = mModel->getShape()->getName( node.nameIndex );
   if ( nodeName.compare( "__deleted_", 10 ) == 0 )
      return;

   Point2I pos( mProjectedNodes[index].x, mProjectedNodes[index].y + sNodeRectSize + 6 );

   GFX->getDrawUtil()->setBitmapModulation( textColor );
   GFX->getDrawUtil()->drawText( mProfile->mFont, pos, nodeName.c_str() );
}

//-----------------------------------------------------------------------------
// Console methods (GuiShapeEdPreview)
//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT( GuiShapeEdPreview );

ConsoleMethod( GuiShapeEdPreview, setSliderCtrl, void, 3, 3,
              "( string ctrl_name )")
{
   TORQUE_UNUSED( argc );
   GuiSliderCtrl* ctrl = static_cast<GuiSliderCtrl*>( Sim::findObject( argv[2] ));
   if (!ctrl )
      Con::warnf("Could not find GuiSliderCtrl: %s", argv[2] );
   object->setSliderCtrl( ctrl );
}

ConsoleMethod( GuiShapeEdPreview, setModel, void, 3, 3,
              "( string shapeName )\n"
              "Sets the model to be displayed in this control\n\n"
              "\\param shapeName Name of the model to display.\n")
{
   TORQUE_UNUSED( argc );
   object->setObjectModel( argv[2] );
}

ConsoleMethod( GuiShapeEdPreview, setSequence, void, 3, 3,
              "( string sequence )\n"
              "Sets the animation to play for the viewed object.\n\n"
              "\\param sequence The name of the animation to play.")
{
   TORQUE_UNUSED( argc );
   object->setObjectAnimation( argv[2] );
}

ConsoleMethod( GuiShapeEdPreview, setOrbitDistance, void, 3, 3,
              "( float distance )\n"
              "Sets the distance at which the camera orbits the object. Clamped to the acceptable range defined in the class by min and max orbit distances.\n\n"
              "\\param distance The distance to set the orbit to ( will be clamped ).")
{
   TORQUE_UNUSED( argc );
   object->setOrbitDistance( dAtof( argv[2] ));
}

ConsoleMethod( GuiShapeEdPreview, setTimeScale, void, 3, 3,
              "( float scale )")
{
   TORQUE_UNUSED( argc );
   object->setTimeScale( dAtof( argv[2] ));
}

ConsoleMethod( GuiShapeEdPreview, fitToShape, void, 2, 2,
              "()")
{
   TORQUE_UNUSED( argc );
   object->fitToShape();
}

ConsoleMethod( GuiShapeEdPreview, updateNodeTransforms, void, 2, 2,
              "()")
{
   object->updateNodeTransforms();
}
