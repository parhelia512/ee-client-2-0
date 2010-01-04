//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "gui/controls/guiMLTextEditCtrl.h"
#include "gui/containers/guiScrollCtrl.h"
#include "gui/core/guiCanvas.h"
#include "console/consoleTypes.h"
#include "platform/event.h"
#include "core/frameAllocator.h"
#include "core/stringBuffer.h"
#include "gfx/gfxDrawUtil.h"

IMPLEMENT_CONOBJECT(GuiMLTextEditCtrl);

//--------------------------------------------------------------------------
GuiMLTextEditCtrl::GuiMLTextEditCtrl()
{
   mEscapeCommand = StringTable->insert( "" );

	mIsEditCtrl = true;

   mActive = true;

   mVertMoveAnchorValid = false;
}


//--------------------------------------------------------------------------
GuiMLTextEditCtrl::~GuiMLTextEditCtrl()
{

}


//--------------------------------------------------------------------------
bool GuiMLTextEditCtrl::resize(const Point2I &newPosition, const Point2I &newExtent)
{
   // We don't want to get any smaller than our parent:
   Point2I newExt = newExtent;
   GuiControl* parent = getParent();
   if ( parent )
      newExt.y = getMax( parent->getHeight(), newExt.y );

   return Parent::resize( newPosition, newExt );
}


//--------------------------------------------------------------------------
void GuiMLTextEditCtrl::initPersistFields()
{
   addField( "escapeCommand", TypeString, Offset( mEscapeCommand, GuiMLTextEditCtrl ) );
   Parent::initPersistFields();
}

//--------------------------------------------------------------------------
void GuiMLTextEditCtrl::setFirstResponder()
{
   Parent::setFirstResponder();

   GuiCanvas *root = getRoot();
   if (root != NULL)
   {
		root->enableKeyboardTranslation();

	   // If the native OS accelerator keys are not disabled
		// then some key events like Delete, ctrl+V, etc may
		// not make it down to us.
		root->setNativeAcceleratorsEnabled( false );
   }
}

void GuiMLTextEditCtrl::onLoseFirstResponder()
{
   GuiCanvas *root = getRoot();
   if (root != NULL)
   {
      root->setNativeAcceleratorsEnabled( true );
      root->disableKeyboardTranslation();
   }

   // Redraw the control:
   setUpdate();
}

//--------------------------------------------------------------------------
bool GuiMLTextEditCtrl::onWake()
{
   if( !Parent::onWake() )
      return false;

   getRoot()->enableKeyboardTranslation();
   return true;
}

//--------------------------------------------------------------------------
// Key events...
bool GuiMLTextEditCtrl::onKeyDown(const GuiEvent& event)
{
   if ( !isActive() )
      return false;

	setUpdate();
	//handle modifiers first...
   if (event.modifier & SI_PRIMARY_CTRL)
   {
      switch(event.keyCode)
      {
			//copy/cut
         case KEY_C:
         case KEY_X:
			{
				//make sure we actually have something selected
				if (mSelectionActive)
				{
		         copyToClipboard(mSelectionStart, mSelectionEnd);

					//if we're cutting, also delete the selection
					if (event.keyCode == KEY_X)
					{
			         mSelectionActive = false;
			         deleteChars(mSelectionStart, mSelectionEnd);
			         mCursorPosition = mSelectionStart;
					}
					else
			         mCursorPosition = mSelectionEnd + 1;
				}
				return true;
			}

			//paste
         case KEY_V:
			{
				const char *clipBuf = Platform::getClipboard();
				if (dStrlen(clipBuf) > 0)
				{
			      // Normal ascii keypress.  Go ahead and add the chars...
			      if (mSelectionActive == true)
			      {
			         mSelectionActive = false;
			         deleteChars(mSelectionStart, mSelectionEnd);
			         mCursorPosition = mSelectionStart;
			      }

			      insertChars(clipBuf, dStrlen(clipBuf), mCursorPosition);
				}
				return true;
			}
         
         default:
            break;
		}
   }
   else if ( event.modifier & SI_SHIFT )
   {
      switch ( event.keyCode )
      {
         case KEY_TAB:
            return( Parent::onKeyDown( event ) );
         default:
            break;
      }
   }
   else if ( event.modifier == 0 )
   {
      switch (event.keyCode)
      {
         // Escape:
         case KEY_ESCAPE:
            if ( mEscapeCommand[0] )
            {
               Con::evaluate( mEscapeCommand );
               return( true );
            }
            return( Parent::onKeyDown( event ) );

         // Deletion
         case KEY_BACKSPACE:
         case KEY_DELETE:
            handleDeleteKeys(event);
            return true;

         // Cursor movement
         case KEY_LEFT:
         case KEY_RIGHT:
         case KEY_UP:
         case KEY_DOWN:
         case KEY_HOME:
         case KEY_END:
            handleMoveKeys(event);
            return true;

         // Special chars...
         case KEY_TAB:
            // insert 3 spaces
            if (mSelectionActive == true)
            {
               mSelectionActive = false;
               deleteChars(mSelectionStart, mSelectionEnd);
               mCursorPosition = mSelectionStart;
            }
            insertChars( "\t", 1, mCursorPosition );
            return true;

         case KEY_RETURN:
            // insert carriage return
            if (mSelectionActive == true)
            {
               mSelectionActive = false;
               deleteChars(mSelectionStart, mSelectionEnd);
               mCursorPosition = mSelectionStart;
            }
            insertChars( "\n", 1, mCursorPosition );
            return true;
            
         default:
            break;
      }
   }

   // If it's a type-able key event, eat it.
   //
   // This is pretty lame since we aren't checking if numlock is pressed
   // or for any modifier keys which CAN affect if its REALLY an type-able
   // character.
   //
   // Probably this information should be sent from above, like "is char input 
   // event".  In the mean time, changed this to check STATE_LOWER instead of 
   // STATE_UPPER because numpad keys have no STATE_UPPER value.
   if ( event.keyCode && Input::getAscii( event.keyCode, STATE_LOWER ) )
      return true;

   if ( (mFont && mFont->isValidChar(event.ascii)) || (!mFont && event.ascii != 0) )
   {
      // Normal ascii keypress.  Go ahead and add the chars...
      if (mSelectionActive == true)
      {
         mSelectionActive = false;
         deleteChars(mSelectionStart, mSelectionEnd);
         mCursorPosition = mSelectionStart;
      }

      UTF8 *outString = NULL;
      U32 outStringLen = 0;

#ifdef TORQUE_UNICODE

      UTF16 inData[2] = { event.ascii, 0 };
      StringBuffer inBuff(inData);

      FrameTemp<UTF8> outBuff(4);
      inBuff.getCopy8(outBuff, 4);

      outString = outBuff;
      outStringLen = dStrlen(outBuff);
#else
      char ascii = char(event.ascii);
      outString = &ascii;
      outStringLen = 1;
#endif

      insertChars(outString, outStringLen, mCursorPosition);
      mVertMoveAnchorValid = false;
      return true;
   }

   // Otherwise, let the parent have the event...
   return Parent::onKeyDown(event);
}


//--------------------------------------
void GuiMLTextEditCtrl::handleDeleteKeys(const GuiEvent& event)
{
   if ( isSelectionActive() )
   {
      mSelectionActive = false;
      deleteChars(mSelectionStart, mSelectionEnd+1);
      mCursorPosition = mSelectionStart;
   }
   else
   {
      switch ( event.keyCode )
      {
         case KEY_BACKSPACE:
            if (mCursorPosition != 0)
            {
               // delete one character left
               deleteChars(mCursorPosition-1, mCursorPosition);
               setUpdate();
            }
            break;

         case KEY_DELETE:
            if (mCursorPosition != mTextBuffer.length())
            {
               // delete one character right
               deleteChars(mCursorPosition, mCursorPosition+1);
               setUpdate();
            }
            break;

        default:
            AssertFatal(false, "Unknown key code received!");
      }
   }
}


//--------------------------------------
void GuiMLTextEditCtrl::handleMoveKeys(const GuiEvent& event)
{
   if ( event.modifier & SI_SHIFT )
      return;

   mSelectionActive = false;

   switch ( event.keyCode )
   {
      case KEY_LEFT:
         mVertMoveAnchorValid = false;
         // move one left
         if ( mCursorPosition != 0 )
         {
            mCursorPosition--;
            setUpdate();
         }
         break;

      case KEY_RIGHT:
         mVertMoveAnchorValid = false;
         // move one right
         if ( mCursorPosition != mTextBuffer.length() )
         {
            mCursorPosition++;
            setUpdate();
         }
         break;

      case KEY_UP:
      case KEY_DOWN:
      {
         Line* walk;
         for ( walk = mLineList; walk->next; walk = walk->next )
         {
            if ( mCursorPosition <= ( walk->textStart + walk->len ) )
               break;
         }

         if ( !walk )
            return;

         if ( event.keyCode == KEY_UP )
         {
            if ( walk == mLineList )
               return;
         }
         else if ( walk->next == NULL )
            return;

         Point2I newPos;
         newPos.set( 0, walk->y );

         // Find the x-position:
         if ( !mVertMoveAnchorValid )
         {
            Point2I cursorTopP, cursorBottomP;
            ColorI color;
            getCursorPositionAndColor(cursorTopP, cursorBottomP, color);
            mVertMoveAnchor = cursorTopP.x;
            mVertMoveAnchorValid = true;
         }

         newPos.x = mVertMoveAnchor;

         // Set the new y-position:
         if (event.keyCode == KEY_UP)
            newPos.y--;
         else
            newPos.y += (walk->height + 1);

         if (setCursorPosition(getTextPosition(newPos)))
            mVertMoveAnchorValid = false;
         break;
      }

      case KEY_HOME:
      case KEY_END:
      {
         mVertMoveAnchorValid = false;
         Line* walk;
         for (walk = mLineList; walk->next; walk = walk->next)
         {
            if (mCursorPosition <= (walk->textStart + walk->len))
               break;
         }

         if (walk)
         {
            if (event.keyCode == KEY_HOME)
            {
               //place the cursor at the beginning of the first atom if there is one
               if (walk->atomList)
                  mCursorPosition = walk->atomList->textStart;
               else
                  mCursorPosition = walk->textStart;
            }
            else
            {
               mCursorPosition = walk->textStart;
               mCursorPosition += walk->len;
            }
            setUpdate();
         }
         break;
      }

      default:
         AssertFatal(false, "Unknown move key code was received!");
   }

   ensureCursorOnScreen();
}

//--------------------------------------------------------------------------
void GuiMLTextEditCtrl::onRender(Point2I offset, const RectI& updateRect)
{
   Parent::onRender(offset, updateRect);

   // We are the first responder, draw our cursor in the appropriate position...
   if (isFirstResponder()) 
   {
      Point2I top, bottom;
      ColorI color;
      getCursorPositionAndColor(top, bottom, color);
      GFX->getDrawUtil()->drawLine(top + offset, bottom + offset, mProfile->mCursorColor);
   }
}

