//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "gfx/gfxDevice.h"
#include "console/consoleTypes.h"
#include "gui/core/guiCanvas.h"
#include "gui/buttons/guiButtonBaseCtrl.h"
#include "i18n/lang.h"
#include "sfx/sfxSystem.h"


IMPLEMENT_CONOBJECT( GuiButtonBaseCtrl );

GuiButtonBaseCtrl::GuiButtonBaseCtrl()
{
   mDepressed = false;
   mMouseOver = false;
   mActive = true;
   mButtonText = StringTable->insert("Button");
   mButtonTextID = StringTable->insert("");
   mStateOn = false;
	mRadioGroup = -1;
   mButtonType = ButtonTypePush;
   mUseMouseEvents = false;
}

bool GuiButtonBaseCtrl::onWake()
{
   if(!Parent::onWake())
      return false;

   // is we have a script variable, make sure we're in sync
   if ( mConsoleVariable[0] )
   	mStateOn = Con::getBoolVariable( mConsoleVariable );
   if(mButtonTextID && *mButtonTextID != 0)
	   setTextID(mButtonTextID);

   return true;
}

ConsoleMethod( GuiButtonBaseCtrl, performClick, void, 2, 2, "() - simulates a button click from script." )
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);
   object->onAction();
}

ConsoleMethod( GuiButtonBaseCtrl, setText, void, 3, 3, "(string text) - sets the text of the button to the string." )
{
   TORQUE_UNUSED(argc);
   object->setText( argv[2] );
}

ConsoleMethod( GuiButtonBaseCtrl, setTextID, void, 3, 3, "(string id) - sets the text of the button to the localized string." )
{
	TORQUE_UNUSED(argc);
	object->setTextID(argv[2]);
}
ConsoleMethod( GuiButtonBaseCtrl, getText, const char *, 2, 2, "() - returns the text of the button." )
{
   TORQUE_UNUSED(argc); TORQUE_UNUSED(argv);
   return object->getText( );
}
ConsoleMethod( GuiButtonBaseCtrl, setStateOn, void, 3, 3, "(bool isStateOn) - sets the state on member and updates siblings of the same group." )
{
   TORQUE_UNUSED(argc);
   object->setStateOn(dAtob(argv[2]));
}

ConsoleMethod( GuiButtonBaseCtrl, resetState, void, 2, 2, "() - resets the state of the button." )
{
   TORQUE_UNUSED(argc);
   object->resetState();
}

static const EnumTable::Enums buttonTypeEnums[] = 
{
   { GuiButtonBaseCtrl::ButtonTypePush, "PushButton" },
   { GuiButtonBaseCtrl::ButtonTypeCheck, "ToggleButton" },
   { GuiButtonBaseCtrl::ButtonTypeRadio, "RadioButton" },
};
      
static const EnumTable gButtonTypeTable(3, &buttonTypeEnums[0]); 
      
      

void GuiButtonBaseCtrl::initPersistFields()
{
   addGroup("GuiButtonBaseCtrl");		
   addField("text", TypeCaseString, Offset(mButtonText, GuiButtonBaseCtrl));
   addField("textID", TypeString, Offset(mButtonTextID, GuiButtonBaseCtrl));
   addField("groupNum", TypeS32, Offset(mRadioGroup, GuiButtonBaseCtrl));
   addField("buttonType", TypeEnum, Offset(mButtonType, GuiButtonBaseCtrl), 1, &gButtonTypeTable);
   addField("useMouseEvents", TypeBool, Offset(mUseMouseEvents, GuiButtonBaseCtrl));
   endGroup("GuiButtonBaseCtrl");
   Parent::initPersistFields();
}

void GuiButtonBaseCtrl::setText(const char *text)
{
   mButtonText = StringTable->insert(text, true);
}

void GuiButtonBaseCtrl::setStateOn( bool bStateOn )
{
   if(!mActive)
      return;

   if(mButtonType == ButtonTypeCheck)
   {
      mStateOn = bStateOn;
   }
   else if(mButtonType == ButtonTypeRadio)
   {
      messageSiblings(mRadioGroup);
      mStateOn = bStateOn;
   }		
   setUpdate();
}

void GuiButtonBaseCtrl::setTextID(const char *id)
{
	S32 n = Con::getIntVariable(id, -1);
	if(n != -1)
	{
		mButtonTextID = StringTable->insert(id);
		setTextID(n);
	}
}
void GuiButtonBaseCtrl::setTextID(S32 id)
{
	const UTF8 *str = getGUIString(id);
	if(str)
		setText((const char*)str);
	//mButtonTextID = id;
}
const char *GuiButtonBaseCtrl::getText()
{
   return mButtonText;
}

//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::acceleratorKeyPress(U32)
{
   if (! mActive)
      return;

   //set the bool
   mDepressed = true;

   if (mProfile->mTabable)
      setFirstResponder();
}

//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::acceleratorKeyRelease(U32)
{
   if (! mActive)
      return;

   if (mDepressed)
   {
      //set the bool
      mDepressed = false;
      //perform the action
      onAction();
   }

   //update
   setUpdate();
}

void GuiButtonBaseCtrl::onMouseDown(const GuiEvent &event)
{
   if (! mActive)
      return;

   if (mProfile->mCanKeyFocus)
      setFirstResponder();

   if (mProfile->mSoundButtonDown)
      SFX->playOnce(mProfile->mSoundButtonDown);

   if(mUseMouseEvents)
      Con::executef( this, "onMouseDown" );

   //lock the mouse
   mouseLock();
   mDepressed = true;

   // If we have a double click then execute the alt command.
   if ( event.mouseClickCount == 2 )
   {
      if ( isMethod( "onDoubleClick" ) )
         Con::executef( this, "onDoubleClick" );

      execAltConsoleCallback();
   }

   //update
   setUpdate();
}

void GuiButtonBaseCtrl::onMouseEnter(const GuiEvent &event)
{
   setUpdate();

   if(mUseMouseEvents)
      Con::executef( this, "onMouseEnter" );

   if(isMouseLocked())
   {
      mDepressed = true;
      mMouseOver = true;
   }
   else
   {
      if ( mActive && mProfile->mSoundButtonOver )
         SFX->playOnce(mProfile->mSoundButtonOver);

      mMouseOver = true;
   }
}

void GuiButtonBaseCtrl::onMouseLeave(const GuiEvent &)
{
   setUpdate();

   if(mUseMouseEvents)
      Con::executef( this, "onMouseLeave" );
   if(isMouseLocked())
      mDepressed = false;
   mMouseOver = false;
}

void GuiButtonBaseCtrl::onMouseUp(const GuiEvent &event)
{
   if (! mActive)
      return;

   mouseUnlock();

   setUpdate();

   if(mUseMouseEvents)
      Con::executef( this, "onMouseUp" );

   //if we released the mouse within this control, perform the action
   if (mDepressed)
      onAction();

   mDepressed = false;
}

void GuiButtonBaseCtrl::onRightMouseUp(const GuiEvent &event)
{
   if(isMethod("onRightClick"))
      Con::executef( this, "onRightClick" );

   Parent::onRightMouseUp( event );
}

//--------------------------------------------------------------------------
bool GuiButtonBaseCtrl::onKeyDown(const GuiEvent &event)
{
   //if the control is a dead end, kill the event
   if (!mActive)
      return true;

   //see if the key down is a return or space or not
   if ((event.keyCode == KEY_RETURN || event.keyCode == KEY_SPACE)
       && event.modifier == 0)
   {
	   if ( mProfile->mSoundButtonDown )
         SFX->playOnce( mProfile->mSoundButtonDown);

      return true;
   }
   //otherwise, pass the event to it's parent
   return Parent::onKeyDown(event);
}

//--------------------------------------------------------------------------
bool GuiButtonBaseCtrl::onKeyUp(const GuiEvent &event)
{
   //if the control is a dead end, kill the event
   if (!mActive)
      return true;

   //see if the key down is a return or space or not
   if (mDepressed &&
      (event.keyCode == KEY_RETURN || event.keyCode == KEY_SPACE) &&
      event.modifier == 0)
   {
      onAction();
      return true;
   }

   //otherwise, pass the event to it's parent
   return Parent::onKeyUp(event);
}

//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::setScriptValue(const char *value)
{
	mStateOn = dAtob(value);

	// Update the console variable:
	if ( mConsoleVariable[0] )
		Con::setBoolVariable( mConsoleVariable, mStateOn );

   setUpdate();
}

//---------------------------------------------------------------------------
const char *GuiButtonBaseCtrl::getScriptValue()
{
	return mStateOn ? "1" : "0";
}

//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::onAction()
{
    if(!mActive)
        return;

    if(mButtonType == ButtonTypeCheck)
    {
        mStateOn = mStateOn ? false : true;

        // Update the console variable:
        if ( mConsoleVariable[0] )
            Con::setBoolVariable( mConsoleVariable, mStateOn );
        // Execute the console command (if any). Unnecessary. Parent does this already.
        //if( mConsoleCommand[0] )
        //   Con::evaluate( mConsoleCommand, false );

	}
	else if(mButtonType == ButtonTypeRadio)
    {
        mStateOn = true;
        messageSiblings(mRadioGroup);
    }		
    setUpdate();


    // Provide and onClick script callback.
    if( isMethod("onClick") )
       Con::executef( this, "onClick" );

    Parent::onAction();
}

//---------------------------------------------------------------------------
void GuiButtonBaseCtrl::onMessage( GuiControl *sender, S32 msg )
{
	Parent::onMessage(sender, msg);
	if( mRadioGroup == msg && mButtonType == ButtonTypeRadio )
	{
		setUpdate();
		mStateOn = ( sender == this );
	}
}



