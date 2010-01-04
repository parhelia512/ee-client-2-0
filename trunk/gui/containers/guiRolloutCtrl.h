//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------
#ifndef _GUI_ROLLOUTCTRL_H_
#define _GUI_ROLLOUTCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif
#ifndef _GUISTACKCTRL_H_
#include "gui/containers/guiStackCtrl.h"
#endif
#ifndef _H_GUIDEFAULTCONTROLRENDER_
#include "gui/core/guiDefaultControlRender.h"
#endif
#ifndef _GUITICKCTRL_H_
#include "gui/shiny/guiTickCtrl.h"
#endif


class GuiRolloutCtrl : public GuiTickCtrl
{
private:
   typedef GuiControl Parent;
public:
   // Members
   StringTableEntry       mCaption;
   RectI                  mHeader;
   RectI                  mExpanded;
   RectI                  mChildRect;
   RectI                  mMargin;
   bool                   mIsExpanded;
   bool                   mIsAnimating;
   bool                   mCollapsing;
   S32                    mAnimateDestHeight;
   S32                    mAnimateStep;
   S32                    mDefaultHeight;
   bool                   mCanCollapse;
   bool                   mHideHeader;

   GuiCursor*  mDefaultCursor;
   GuiCursor*  mVertSizingCursor;


   // Theme Support
   enum
   {
      CollapsedLeft = 0,
      CollapsedCenter,
      CollapsedRight,
      TopLeftHeader,
      TopMidHeader,      
      TopRightHeader,  
      MidPageLeft,
      MidPageCenter,
      MidPageRight,
      BottomLeftHeader, 
      BottomMidHeader,   
      BottomRightHeader,   
      NumBitmaps           ///< Number of bitmaps in this array
   };
   bool  mHasTexture;   ///< Indicates whether we have a texture to render the tabs with
   RectI *mBitmapBounds;///< Array of rectangles identifying textures for tab book

   // Constructor/Destructor/Conobject Declaration
   GuiRolloutCtrl();
   ~GuiRolloutCtrl();
   DECLARE_CONOBJECT(GuiRolloutCtrl);
   DECLARE_CATEGORY( "Gui Containers" );

   // Persistence
   static void initPersistFields();

   // Control Events
   bool onWake();
   void addObject(SimObject *obj);
   void removeObject(SimObject *obj);
   virtual void childResized(GuiControl *child);

   // Mouse Events
   virtual void onMouseDown( const GuiEvent &event );
   virtual void onMouseUp( const GuiEvent &event );

   // Sizing Helpers
   virtual void calculateHeights();
   virtual bool resize( const Point2I &newPosition, const Point2I &newExtent );
   virtual void sizeToContents();
   inline bool isExpanded() const { return mIsExpanded; }

   // Sizing Animation Functions
   void animateTo( S32 height );
   virtual void processTick();


   void collapse() { animateTo( mHeader.extent.y ); }
   void expand() { animateTo( mExpanded.extent.y ); }
   void instantCollapse();
   void instantExpand();
   void toggleExpanded( bool instant );

   // Property - "Expanded"
   static bool setExpanded(void* obj, const char* data)  
   { 
      bool expand = dAtob( data );
      if( expand )
         static_cast<GuiRolloutCtrl*>(obj)->instantExpand();         
      else
         static_cast<GuiRolloutCtrl*>(obj)->instantCollapse();         
      return false; 
   };


   // Control Rendering
   virtual void onRender(Point2I offset, const RectI &updateRect);
   bool onAdd();

};

#endif // _GUI_ROLLOUTCTRL_H_