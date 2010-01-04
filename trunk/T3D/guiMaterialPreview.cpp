//-----------------------------------------------------------------------------
// GuiMaterialPreview Control for Material Editor Written by Travis Vroman of Gaslight Studios
// Updated 2-14-09
// Portions based off Constructor viewport code.
//-----------------------------------------------------------------------------

#include "T3D/guiMaterialPreview.h"
#include "renderInstance/renderPassManager.h"
#include "lighting/lightManager.h"
#include "lighting/lightInfo.h"
#include "core/resourceManager.h"
#include "sceneGraph/sceneGraph.h"
#include "sceneGraph/sceneState.h"

// GuiMaterialPreview
GuiMaterialPreview::GuiMaterialPreview()
:  mMaxOrbitDist(5.0f),
   mMinOrbitDist(0.0f),
   mOrbitDist(5.0f),
   mMouseState(None),
   mModel(NULL),
   mLastMousePoint(0, 0),
   lastRenderTime(0),
   runThread(0),
   mFakeSun(NULL)
{
   mActive = true;
   mCameraMatrix.identity();
   mCameraRot.set( mDegToRad(30.0f), 0, mDegToRad(-30.0f) );
   mCameraPos.set(0.0f, 1.75f, 1.25f);
   mCameraMatrix.setColumn(3, mCameraPos);
   mOrbitPos.set(0.0f, 0.0f, 0.0f);
   mTransStep = 0.01f;
   mTranMult = 4.0;
   mLightTransStep = 0.01f;
   mLightTranMult = 4.0;
   mOrbitRelPos = Point3F(0,0,0);

   // By default don't do dynamic reflection
   // updates for this viewport.
   mReflectPriority = 0.0f;
}

GuiMaterialPreview::~GuiMaterialPreview()
{
   SAFE_DELETE(mModel);
   SAFE_DELETE(mFakeSun);
}

bool GuiMaterialPreview::onWake()
{
   if( !Parent::onWake() )
      return false;

   if (!mFakeSun)
      mFakeSun = LightManager::createLightInfo();

   mFakeSun->setColor( ColorF( 1.0f, 1.0f, 1.0f ) );
   mFakeSun->setAmbient( ColorF( 0.5f, 0.5f, 0.5f ) );
   mFakeSun->setDirection( VectorF( 0.0f, 0.707f, -0.707f ) );
	mFakeSun->setPosition( mFakeSun->getDirection() * -10000.0f );
   mFakeSun->setRange( 2000000.0f );

   return true;
}

// This function allows the viewport's ambient color to be changed. This is exposed to script below.
void GuiMaterialPreview::setAmbientLightColor( F32 r, F32 g, F32 b )
{
   ColorF temp(r, g, b);
   temp.clamp();
	GuiMaterialPreview::mFakeSun->setAmbient( temp );
}

// This function allows the light's color to be changed. This is exposed to script below.
void GuiMaterialPreview::setLightColor( F32 r, F32 g, F32 b )
{
   ColorF temp(r, g, b);
   temp.clamp();
	GuiMaterialPreview::mFakeSun->setColor( temp );
}

// This function is for moving the light in the scene. This needs to be adjusted to keep the light
// from getting all out of whack. For now, we'll just rely on the reset function if we need it 
// fixed.
void GuiMaterialPreview::setLightTranslate(S32 modifier, F32 xstep, F32 ystep)
{
	F32 _lighttransstep = (modifier & SI_SHIFT ? mLightTransStep : (mLightTransStep*mLightTranMult));

	Point3F relativeLightDirection = GuiMaterialPreview::mFakeSun->getDirection();
	// May be able to get rid of this. For now, it helps to fix the position of the light if i gets messed up.
	if (modifier & SI_PRIMARY_CTRL)
	{
		relativeLightDirection.x += ( xstep * _lighttransstep * -1 );//need to invert this for some reason. Otherwise, it's backwards.
		relativeLightDirection.y += ( ystep * _lighttransstep );
		GuiMaterialPreview::mFakeSun->setDirection(relativeLightDirection);
	}
	// Default action taken by mouse wheel clicking.
	else
	{
		relativeLightDirection.x += ( xstep * _lighttransstep * -1 ); //need to invert this for some reason. Otherwise, it's backwards.
		relativeLightDirection.z += ( ystep * _lighttransstep );
		GuiMaterialPreview::mFakeSun->setDirection(relativeLightDirection);
	}
}

// This is for panning the viewport camera.
void GuiMaterialPreview::setTranslate(S32 modifier, F32 xstep, F32 ystep)
{
	F32 transstep = (modifier & SI_SHIFT ? mTransStep : (mTransStep*mTranMult));

	F32 nominalDistance = 20.0;
	Point3F vec = mCameraPos;
	vec -= mOrbitPos;
	transstep *= vec.len() / nominalDistance;

	if (modifier & SI_PRIMARY_CTRL)
	{
		mOrbitRelPos.x += ( xstep * transstep );
		mOrbitRelPos.y += ( ystep * transstep );
	}
	else
	{
		mOrbitRelPos.x += ( xstep * transstep );
		mOrbitRelPos.z += ( ystep * transstep );
	}
}

// Left Click
void GuiMaterialPreview::onMouseDown(const GuiEvent &event)
{
   mMouseState = MovingLight;
   mLastMousePoint = event.mousePoint;
   mouseLock();
}

// Left Click Release
void GuiMaterialPreview::onMouseUp(const GuiEvent &event)
{
   mouseUnlock();
   mMouseState = None;
}

// Left Click Drag
void GuiMaterialPreview::onMouseDragged(const GuiEvent &event)
{
   if(mMouseState != MovingLight)
   {
	   return;
   }
   // If we are MovingLight...
   else
   {
   Point2I delta = event.mousePoint - mLastMousePoint;
   mLastMousePoint = event.mousePoint;
   setLightTranslate(event.modifier, delta.x, delta.y);
   }
}

// Right Click
void GuiMaterialPreview::onRightMouseDown(const GuiEvent &event)
{
   mMouseState = Rotating;
   mLastMousePoint = event.mousePoint;
   mouseLock();
}

// Right Click Release
void GuiMaterialPreview::onRightMouseUp(const GuiEvent &event)
{
   mouseUnlock();
   mMouseState = None;
}

// Right Click Drag
void GuiMaterialPreview::onRightMouseDragged(const GuiEvent &event)
{
   if (mMouseState != Rotating)
   {
	   return;
   }
   Point2I delta = event.mousePoint - mLastMousePoint;
   mLastMousePoint = event.mousePoint;
   mCameraRot.x += (delta.y * 0.01f);
   mCameraRot.z += (delta.x * 0.01f);
}

// Mouse Wheel Scroll Up
bool GuiMaterialPreview::onMouseWheelUp(const GuiEvent &event)
{
   mOrbitDist = (mOrbitDist - 0.10f);
   return true;
}

// Mouse Wheel Scroll Down
bool GuiMaterialPreview::onMouseWheelDown(const GuiEvent &event)
{
	mOrbitDist = (mOrbitDist + 0.10f);
	return true;
}

// Mouse Wheel Click
void GuiMaterialPreview::onMiddleMouseDown(const GuiEvent &event)
{
   if (!mActive || !mVisible || !mAwake)
   {
      return;
   }
   mMouseState = Panning;
   mLastMousePoint = event.mousePoint;
   mouseLock();
}

// Mouse Wheel Click Release
void GuiMaterialPreview::onMiddleMouseUp(const GuiEvent &event)
{
   mouseUnlock();
   mMouseState = None;
}

// Mouse Wheel Click Drag
void GuiMaterialPreview::onMiddleMouseDragged(const GuiEvent &event)
{
   if (mMouseState != Panning)
   {
      return;
   }
   Point2I delta = event.mousePoint - mLastMousePoint;
   mLastMousePoint = event.mousePoint;
   setTranslate(event.modifier, delta.x, delta.y);
}

// This is used to set the model we want to view in the control object.
void GuiMaterialPreview::setObjectModel(const char* modelName)
{
   deleteModel();

   Resource<TSShape> model = ResourceManager::get().load(modelName);
   if (! bool(model))
   {
      Con::warnf(avar("GuiMaterialPreview: Failed to load model %s. Please check your model name and load a valid model.", modelName));
      return;
   }

   mModel = new TSShapeInstance(model, true);
   AssertFatal(mModel, avar("GuiMaterialPreview: Failed to load model %s. Please check your model name and load a valid model.", modelName));

   // Initialize camera values:
   mOrbitPos = mModel->getShape()->center;
   mMinOrbitDist = mModel->getShape()->radius;

   lastRenderTime = Platform::getVirtualMilliseconds();
}

void GuiMaterialPreview::deleteModel()
{
   SAFE_DELETE(mModel);
   runThread = 0;
}

// This is called whenever there is a change in the camera.
bool GuiMaterialPreview::processCameraQuery(CameraQuery* query)
{
	MatrixF xRot, zRot;
	Point3F vecf, vecu, vecr;;
	xRot.set(EulerF(mCameraRot.x, 0.0f, 0.0f));
    zRot.set(EulerF(0.0f, 0.0f, mCameraRot.z));

	if(mMouseState != Panning)
	{
      // Adjust the camera so that we are still facing the model:
      Point3F vec;

      mCameraMatrix.mul(zRot, xRot);
      mCameraMatrix.getColumn(1, &vec);
      vec *= mOrbitDist;
      mCameraPos = mOrbitPos - vec;

      query->farPlane = 2100.0f;
      query->nearPlane = query->farPlane / 5000.0f;
      query->fov = 45.0f;
      mCameraMatrix.setColumn(3, mCameraPos);
      query->cameraMatrix = mCameraMatrix;
	}
	else
	{
	  mCameraMatrix.mul( zRot, xRot );
	  mCameraMatrix.getColumn( 1, &vecf ); // Forward vector
	  mCameraMatrix.getColumn( 2, &vecu ); // Up vector
	  mCameraMatrix.getColumn( 0, &vecr ); // Right vector

      Point3F flatVecf(vecf.x, vecf.y, 0.0f);

      Point3F modvecf = flatVecf * mOrbitRelPos.y;
      Point3F modvecu = vecu * mOrbitRelPos.z;
      Point3F modvecr = vecr * mOrbitRelPos.x;

	  // Change the orbit position
	  mOrbitPos += modvecu - modvecr + modvecf;

	  F32 vecfmul = mOrbitDist;
	  Point3F virtualVecF = vecf * mOrbitDist;
	  vecf *= vecfmul;

	  mCameraPos = mOrbitPos - virtualVecF;

	  // Update the camera's position
	  mCameraMatrix.setColumn( 3, (mOrbitPos - vecf) );

	  query->farPlane = 2100.0f;
      query->nearPlane = query->farPlane / 5000.0f;
      query->fov = 45.0f;
	  query->cameraMatrix = mCameraMatrix;

	  // Reset the relative position
	  mOrbitRelPos = Point3F(0,0,0);
	}
   return true;
}

void GuiMaterialPreview::onMouseEnter(const GuiEvent & event)
{
   Con::executef(this, "onMouseEnter");
}

void GuiMaterialPreview::onMouseLeave(const GuiEvent & event)
{
   Con::executef(this, "onMouseLeave");
}

void GuiMaterialPreview::renderWorld(const RectI &updateRect)
{
   // nothing to render, punt
   if ( !mModel && !mMountedModel )
      return;

   S32 time = Platform::getVirtualMilliseconds();
   //S32 dt = time - lastRenderTime;
   lastRenderTime = time;

   

   F32 left, right, top, bottom, nearPlane, farPlane;
   bool isOrtho;
   GFX->getFrustum( &left, &right, &bottom, &top, &nearPlane, &farPlane, &isOrtho);
   Frustum frust( isOrtho, left, right, bottom, top, nearPlane, farPlane, MatrixF::Identity );

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
   rdata.setSceneState( &state );

   // Set up pass transforms
   RenderPassManager *renderPass = state.getRenderPass();
   renderPass->assignSharedXform(RenderPassManager::View, MatrixF::Identity);
   renderPass->assignSharedXform(RenderPassManager::Projection, GFX->getProjectionMatrix());
	
	LightManager* lm = gClientSceneGraph->getLightManager();
   lm->unregisterAllLights();
   lm->setSpecialLight( LightManager::slSunLightType, mFakeSun );
   lm->setupLights(NULL, SphereF( Point3F::Zero, 1.0f ) );

   if ( mModel )
   {
      mModel->render( rdata );
   }

   if ( mMountedModel )
   {
      // render a weapon
	   /*
      MatrixF mat;

      GFX->pushWorldMatrix();
      GFX->multWorld( mat );

      GFX->popWorldMatrix();
	  */
   }

   gClientSceneGraph->getRenderPass()->renderPass( &state );
}

// Make sure the orbit distance is within the acceptable range.
void GuiMaterialPreview::setOrbitDistance(F32 distance)
{
   mOrbitDist = mClampF(distance, mMinOrbitDist, mMaxOrbitDist);
}

// This function is meant to be used with a button to put everything back to default settings.
void GuiMaterialPreview::resetViewport()
{
   // Reset the camera's orientation.
   mCameraRot.set( mDegToRad(30.0f), 0, mDegToRad(-30.0f) );
   mCameraPos.set(0.0f, 1.75f, 1.25f);
   mOrbitDist = 5.0f;
   mOrbitPos = mModel->getShape()->center;

   // Reset the viewport's lighting.
   GuiMaterialPreview::mFakeSun->setColor( ColorF( 1.0f, 1.0f, 1.0f ) );
   GuiMaterialPreview::mFakeSun->setAmbient( ColorF( 0.5f, 0.5f, 0.5f ) );
   GuiMaterialPreview::mFakeSun->setDirection( VectorF( 0.0f, 0.707f, -0.707f ) );
}

// Expose the class and functions to the console.
IMPLEMENT_CONOBJECT(GuiMaterialPreview);

// Set the model.
ConsoleMethod(GuiMaterialPreview, setModel, void, 3, 3,
              "(string shapeName)\n"
              "Sets the model to be displayed in this control\n\n"
              "\\param shapeName Name of the model to display.\n")
{
   TORQUE_UNUSED(argc);
   object->setObjectModel(argv[2]);
}

ConsoleMethod(GuiMaterialPreview, deleteModel, void, 2, 2,
              "()\n"
              "Deletes the preview model.\n")
{
   TORQUE_UNUSED(argc);
   object->deleteModel();
}


// Set orbit distance around the model.
ConsoleMethod(GuiMaterialPreview, setOrbitDistance, void, 3, 3,
              "(float distance)\n"
              "Sets the distance at which the camera orbits the object. Clamped to the acceptable range defined in the class by min and max orbit distances.\n\n"
              "\\param distance The distance to set the orbit to (will be clamped).")
{
   TORQUE_UNUSED(argc);
   object->setOrbitDistance(dAtof(argv[2]));
}

// Reset control to default values. Meant to be used with a button.
ConsoleMethod(GuiMaterialPreview, reset, void, 2, 2, "Resets the viewport to default zoom, pan, rotate and lighting.")
{
	TORQUE_UNUSED(argc);
	object->resetViewport();
}

// This function allows the user to change the light's color.
ConsoleMethod(GuiMaterialPreview, setLightColor, void, 5, 5, "Usage: %obj.setLightColor(r,g,b) Sets the color of the light in the scene. \n")
{
   ColorF color(  dAtof( argv[2] ) / 255.0f, 
                  dAtof( argv[3] ) / 255.0f,
                  dAtof( argv[4] ) / 255.0f );
   color.clamp();

   TORQUE_UNUSED( argc );
	object->setLightColor( color.red, color.green, color.blue );
}

// This function allows the user to change the viewports's ambient color.
ConsoleMethod(GuiMaterialPreview, setAmbientLightColor, void, 5, 5, "Usage: %obj.setAmbientLightColor(r,g,b) Sets the color of the ambient light in the scene. \n")
{
   ColorF color(  dAtof( argv[2] ) / 255.0f, 
                  dAtof( argv[3] ) / 255.0f,
                  dAtof( argv[4] ) / 255.0f );
   color.clamp();

   TORQUE_UNUSED( argc );
	object->setAmbientLightColor( color.red, color.green, color.blue );
}