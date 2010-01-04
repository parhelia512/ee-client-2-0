//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#include "guiGameListMenuCtrl.h"

#include "console/consoleTypes.h"
#include "gfx/gfxDrawUtil.h"

//-----------------------------------------------------------------------------
// GuiGameListMenuCtrl
//-----------------------------------------------------------------------------

GuiGameListMenuCtrl::GuiGameListMenuCtrl()
 : mSelected(NO_ROW),
   mHighlighted(NO_ROW),
   mDebugRender(false)
{
   VECTOR_SET_ASSOCIATION(mRows);

   // initialize the control callbacks
   mCallbackOnA = StringTable->insert("");
   mCallbackOnB = mCallbackOnA;
   mCallbackOnX = mCallbackOnA;
   mCallbackOnY = mCallbackOnA;
}

GuiGameListMenuCtrl::~GuiGameListMenuCtrl()
{
   for (S32 i = 0; i < mRows.size(); ++i)
   {
      delete mRows[i];
   }
}

void GuiGameListMenuCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   GuiGameListMenuProfile * profile = (GuiGameListMenuProfile *) mProfile;

   F32 xScale = (float) getWidth() / profile->getRowWidth();

   bool profileHasIcons = profile->hasArrows();

   S32 rowHeight = profile->getRowHeight();

   Point2I currentOffset = offset;
   Point2I extent = getExtent();
   Point2I rowExtent(extent.x, rowHeight);
   Point2I textOffset(profile->mTextOffset.x * xScale, profile->mTextOffset.y);
   Point2I textExtent(extent.x - textOffset.x, rowHeight);
   Point2I iconExtent, iconOffset(0.0f, 0.0f);
   if (profileHasIcons)
   {
      iconExtent = profile->getIconExtent();

      // icon is centered vertically plus any specified offset
      S32 iconOffsetY = (rowHeight - iconExtent.y) >> 1;
      iconOffsetY += profile->mIconOffset.y;
      iconOffset = Point2I(profile->mIconOffset.x * xScale, iconOffsetY);
   }
   for (Vector<Row *>::iterator row = mRows.begin(); row < mRows.end(); ++row)
   {
      if (row != mRows.begin())
      {
         // rows other than the first can have padding above them
         currentOffset.y += (*row)->mHeightPad;
         currentOffset.y += rowHeight;
      }

      // select appropriate colors and textures
      ColorI fontColor;
      U32 buttonTextureIndex;
      S32 iconIndex = (*row)->mIconIndex;
      bool useHighlightIcon = (*row)->mUseHighlightIcon;
      if (! (*row)->mEnabled)
      {
         buttonTextureIndex = Profile::TEX_DISABLED;
         fontColor = profile->mFontColorNA;
      }
      else if (row == &mRows[mSelected])
      {
         if (iconIndex != NO_ICON)
         {
            iconIndex++;
         }
         buttonTextureIndex = Profile::TEX_SELECTED;
         fontColor = profile->mFontColorSEL;
      }
      else if ((mHighlighted != NO_ROW) && (row == &mRows[mHighlighted]))
      {
         if (iconIndex != NO_ICON && useHighlightIcon)
         {
            iconIndex++;
         }
         buttonTextureIndex = Profile::TEX_HIGHLIGHT;
         fontColor = profile->mFontColorHL;
      }
      else
      {
         buttonTextureIndex = Profile::TEX_NORMAL;
         fontColor = profile->mFontColor;
      }

      // render the row bitmap
      GFX->getDrawUtil()->clearBitmapModulation();
      GFX->getDrawUtil()->drawBitmapStretchSR(profile->mTextureObject, RectI(currentOffset, rowExtent), profile->getBitmapArrayRect(buttonTextureIndex));

      // render the row icon if it has one
      if ((iconIndex != NO_ICON) && profileHasIcons && (! profile->getBitmapArrayRect((U32)iconIndex).extent.isZero()))
      {
         iconIndex += Profile::TEX_FIRST_ICON;
         GFX->getDrawUtil()->clearBitmapModulation();
         GFX->getDrawUtil()->drawBitmapStretchSR(profile->mTextureObject, RectI(currentOffset + iconOffset, iconExtent), profile->getBitmapArrayRect(iconIndex));
      }

      // render the row text
      GFX->getDrawUtil()->setBitmapModulation(fontColor);
      renderJustifiedText(currentOffset + textOffset, textExtent, (*row)->mLabel);
   }

   if (mDebugRender)
   {
      onDebugRender(offset);
   }

   renderChildControls(offset, updateRect);
}

void GuiGameListMenuCtrl::onDebugRender(Point2I offset)
{
   GuiGameListMenuProfile * profile = (GuiGameListMenuProfile *) mProfile;

   F32 xScale = (float) getWidth() / profile->getRowWidth();

   ColorI controlBorderColor(200, 200, 200); // gray
   ColorI rowBorderColor(255, 127, 255); // magenta
   ColorI hitBorderColor(255, 0, 0); // red
   Point2I shrinker(-1, -1);
   Point2I extent = getExtent();

   // render a border around the entire control
   RectI borderRect(offset, extent + shrinker);
   GFX->getDrawUtil()->drawRect(borderRect, controlBorderColor);

   S32 rowHeight = profile->getRowHeight();
   Point2I currentOffset(offset);
   Point2I rowExtent(extent.x, rowHeight);
   rowExtent += shrinker;
   Point2I hitAreaExtent(profile->getHitAreaExtent());
   hitAreaExtent.x *= xScale;
   hitAreaExtent += shrinker;
   Point2I hitAreaOffset = profile->mHitAreaUpperLeft;
   hitAreaOffset.x *= xScale;
   Point2I upperLeft;
   for (Vector<Row *>::iterator row = mRows.begin(); row < mRows.end(); ++row)
   {
      // set the top of the current row
      if (row != mRows.begin())
      {
         // rows other than the first can have padding above them
         currentOffset.y += (*row)->mHeightPad;
         currentOffset.y += rowHeight;
      }

      // draw the box around the whole row's extent
      upperLeft = currentOffset;
      borderRect.point = upperLeft;
      borderRect.extent = rowExtent;
      GFX->getDrawUtil()->drawRect(borderRect, rowBorderColor);

      // draw the box around the hit area of the row
      upperLeft = currentOffset + hitAreaOffset;
      borderRect.point = upperLeft;
      borderRect.extent = hitAreaExtent;
      GFX->getDrawUtil()->drawRect(borderRect, hitBorderColor);
   }
}

void GuiGameListMenuCtrl::addRow(const char* label, const char* callback, S32 icon, S32 yPad, bool useHighlightIcon, bool enabled)
{
   Row * row = new Row();
   addRow(row, label, callback, icon, yPad, useHighlightIcon, enabled);
}

void GuiGameListMenuCtrl::addRow(Row * row, const char* label, const char* callback, S32 icon, S32 yPad, bool useHighlightIcon, bool enabled)
{
   row->mLabel = StringTable->insert(label, true);
   row->mScriptCallback = (dStrlen(callback) > 0) ? StringTable->insert(callback, true) : NULL;
   row->mIconIndex = (icon < 0) ? NO_ICON : icon;
   row->mHeightPad = yPad;
   row->mUseHighlightIcon = useHighlightIcon;
   row->mEnabled = enabled;

   mRows.push_back(row);

   updateHeight();

   if (mSelected == NO_ROW)
   {
      selectFirstEnabledRow();
   }
}

Point2I  GuiGameListMenuCtrl::getMinExtent() const
{
   Point2I parentMin = Parent::getMinExtent();

   GuiGameListMenuProfile * profile = (GuiGameListMenuProfile *) mProfile;

   S32 minHeight = 0;
   S32 rowHeight = profile->getRowHeight();

   for (Vector<Row *>::const_iterator row = mRows.begin(); row < mRows.end(); ++row)
   {
      minHeight += rowHeight;
      if (row != mRows.begin())
      {
         minHeight += (*row)->mHeightPad;
      }
   }

   if (minHeight > parentMin.y)
      parentMin.y = minHeight;

   return parentMin;
}

bool GuiGameListMenuCtrl::onAdd()
{
   if ((! Parent::onAdd()) || (! hasValidProfile()))
   {
      return false;
   }

   return true;
}

bool GuiGameListMenuCtrl::onWake()
{
   if ((! Parent::onWake()) || (mRows.size() == 0) || (! hasValidProfile()))
   {
      AssertFatal(mRows.size() > 0, avar("GuiGameListMenuCtrl: %s can't be woken up without any rows. Please use \"addRow\" to add at least one row to the control before pushing it to the canvas.", getName()));
      return false;
   }

   enforceConstraints();

   selectFirstEnabledRow();

   setFirstResponder();

   mHighlighted = NO_ROW;

   return true;
}

bool GuiGameListMenuCtrl::hasValidProfile() const
{
   GuiGameListMenuProfile * profile = dynamic_cast<GuiGameListMenuProfile *>(mProfile);
   AssertFatal(profile, avar("GuiGameListMenuCtrl: %s can't be created with a profile of type %s. Please create it with a profile of type GuiGameListMenuProfile.", getName(), mProfile->getClassName()));
   return profile;
}

void GuiGameListMenuCtrl::enforceConstraints()
{
   if (hasValidProfile())
   {
      ((GuiGameListMenuProfile *)mProfile)->enforceConstraints();
   }
   updateHeight();
}

void GuiGameListMenuCtrl::updateHeight()
{
   S32 minHeight = getMinExtent().y;
   if (getHeight() < minHeight)
   {
      setHeight(minHeight);
   }
}

void GuiGameListMenuCtrl::onMouseDown(const GuiEvent &event)
{
   S32 hitRow = getRow(event.mousePoint);
   if (hitRow != NO_ROW)
   {
      S32 delta = (mSelected != NO_ROW) ? (hitRow - mSelected) : (mSelected + 1);
      changeRow(delta);
   }
}

void GuiGameListMenuCtrl::onMouseLeave(const GuiEvent &event)
{
   mHighlighted = NO_ROW;
}

void GuiGameListMenuCtrl::onMouseMove(const GuiEvent &event)
{
   S32 hitRow = getRow(event.mousePoint);
   // allow mHighligetd to be set to NO_ROW so rows can be unhighlighted
   mHighlighted = hitRow;
}

void GuiGameListMenuCtrl::onMouseUp(const GuiEvent &event)
{
   S32 hitRow = getRow(event.mousePoint);
   if ((hitRow != NO_ROW) && isRowEnabled(hitRow) && (hitRow == getSelected()))
   {
      activateRow();
   }
}

void GuiGameListMenuCtrl::activateRow()
{
   S32 row = getSelected();
   if ((row != NO_ROW) && isRowEnabled(row) && (mRows[row]->mScriptCallback != NULL))
   {
      setThisControl();
      if (Con::isFunction(mRows[row]->mScriptCallback))
      {
         Con::executef(mRows[row]->mScriptCallback);
      }
   }
}

S32 GuiGameListMenuCtrl::getRow(Point2I globalPoint)
{
   Point2I localPoint = globalToLocalCoord(globalPoint);
   GuiGameListMenuProfile * profile = (GuiGameListMenuProfile *) mProfile;

   F32 xScale = (float) getWidth() / profile->getRowWidth();

   S32 rowHeight = profile->getRowHeight();
   Point2I currentOffset(0, 0);
   Point2I hitAreaUpperLeft = profile->mHitAreaUpperLeft;
   hitAreaUpperLeft.x *= xScale;
   Point2I hitAreaLowerRight = profile->mHitAreaLowerRight;
   hitAreaLowerRight.x *= xScale;

   Point2I upperLeft, lowerRight;
   for (Vector<Row *>::iterator row = mRows.begin(); row < mRows.end(); ++row)
   {
      if (row != mRows.begin())
      {
         // rows other than the first can have padding above them
         currentOffset.y += (*row)->mHeightPad;
      }

      upperLeft = currentOffset + hitAreaUpperLeft;
      lowerRight = currentOffset + hitAreaLowerRight;

      if ((upperLeft.x <= localPoint.x) && (localPoint.x < lowerRight.x) &&
         (upperLeft.y <= localPoint.y) && (localPoint.y < lowerRight.y))
      {
         return row - mRows.begin();
      }

      currentOffset.y += rowHeight;
   }

   return NO_ROW;
}

void GuiGameListMenuCtrl::setSelected(S32 index)
{
   if (index == NO_ROW)
   {
      // deselection
      mSelected = NO_ROW;
      return;
   }

   if (! isValidRowIndex(index))
   {
      return;
   }

   if (! isRowEnabled(index))
   {
      // row is disabled, it can't be selected
      return;
   }

   mSelected = mClamp(index, 0, mRows.size() - 1);
}

bool GuiGameListMenuCtrl::isRowEnabled(S32 index) const
{
   if (! isValidRowIndex(index))
   {
      return false;
   }

   return mRows[index]->mEnabled;
}

void GuiGameListMenuCtrl::setRowEnabled(S32 index, bool enabled)
{
   if (! isValidRowIndex(index))
   {
      return;
   }

   mRows[index]->mEnabled = enabled;

   if (getSelected() == index)
   {
      selectFirstEnabledRow();
   }
}

bool GuiGameListMenuCtrl::isValidRowIndex(S32 index) const
{
   return ((0 <= index) && (index < mRows.size()));
}

void GuiGameListMenuCtrl::selectFirstEnabledRow()
{
   setSelected(NO_ROW);
   for (Vector<Row *>::iterator row = mRows.begin(); row < mRows.end(); ++row)
   {
      if ((*row)->mEnabled)
      {
         setSelected(row - mRows.begin());
         return;
      }
   }
}

bool GuiGameListMenuCtrl::onKeyDown(const GuiEvent &event)
{
   switch (event.keyCode)
   {
      case KEY_UP:
         changeRow(-1);
         return true;

      case KEY_DOWN:
         changeRow(1);
         return true;

      case KEY_A:
      case KEY_RETURN:
      case KEY_NUMPADENTER:
      case KEY_SPACE:
      case XI_A:
      case XI_START:
         doScriptCommand(mCallbackOnA);
         return true;

      case KEY_B:
      case KEY_ESCAPE:
      case KEY_BACKSPACE:
      case KEY_DELETE:
      case XI_B:
      case XI_BACK:
         doScriptCommand(mCallbackOnB);
         return true;

      case KEY_X:
      case XI_X:
         doScriptCommand(mCallbackOnX);
         return true;

      case KEY_Y:
      case XI_Y:
         doScriptCommand(mCallbackOnY);
         return true;
      default:
         break;
   }

   return Parent::onKeyDown(event);
}

bool GuiGameListMenuCtrl::onGamepadAxisUp(const GuiEvent &event)
{
   changeRow(-1);
   return true;
}

bool GuiGameListMenuCtrl::onGamepadAxisDown(const GuiEvent &event)
{
   changeRow(1);
   return true;
}

void GuiGameListMenuCtrl::doScriptCommand(StringTableEntry command)
{
   if (command && command[0])
   {
      setThisControl();
      Con::evaluate(command, false, __FILE__);
   }
}

void GuiGameListMenuCtrl::changeRow(S32 delta)
{
   S32 oldRowIndex = getSelected();
   S32 newRowIndex = oldRowIndex;
   do
   {
      newRowIndex += delta;
      if (newRowIndex >= mRows.size())
      {
         newRowIndex = 0;
      }
      else if (newRowIndex < 0)
      {
         newRowIndex = mRows.size() - 1;
      }
   }
   while ((! mRows[newRowIndex]->mEnabled) && (newRowIndex != oldRowIndex));

   setSelected(newRowIndex);

   // do the callback if it is defined
   StringTableEntry onChange = StringTable->insert("onChange", false);
   if (isMethod(onChange))
   {
      Con::executef(this, onChange);
   }
}

void GuiGameListMenuCtrl::setThisControl()
{
   Con::setVariable("$ThisControl", avar("%d", getId()));
}

StringTableEntry GuiGameListMenuCtrl::getRowLabel(S32 rowIndex) const
{
   AssertFatal(isValidRowIndex(rowIndex), avar("GuiGameListMenuCtrl: You can't get the label from row %d of %s because it is not a valid row index. Please specify a valid row index in the range [0, %d).", rowIndex, getName(), getRowCount()));
   if (! isValidRowIndex(rowIndex))
   {
      // not a valid row index, don't do anything
      return StringTable->insert("");
   }
   return mRows[rowIndex]->mLabel;
}

void GuiGameListMenuCtrl::setRowLabel(S32 rowIndex, const char * label)
{
   AssertFatal(isValidRowIndex(rowIndex), avar("GuiGameListMenuCtrl: You can't set the label on row %d of %s because it is not a valid row index. Please specify a valid row index in the range [0, %d).", rowIndex, getName(), getRowCount()));
   if (! isValidRowIndex(rowIndex))
   {
      // not a valid row index, don't do anything
      return;
   }

   mRows[rowIndex]->mLabel = StringTable->insert(label, true);
}

//-----------------------------------------------------------------------------
// Console stuff (GuiGameListMenuCtrl)
//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiGameListMenuCtrl);

void GuiGameListMenuCtrl::initPersistFields()
{
   addField("DebugRender",       TypeBool,      Offset(mDebugRender, GuiGameListMenuCtrl));
   addField("CallbackOnA",       TypeString,    Offset(mCallbackOnA, GuiGameListMenuCtrl));
   addField("CallbackOnB",       TypeString,    Offset(mCallbackOnB, GuiGameListMenuCtrl));
   addField("CallbackOnX",       TypeString,    Offset(mCallbackOnX, GuiGameListMenuCtrl));
   addField("CallbackOnY",       TypeString,    Offset(mCallbackOnY, GuiGameListMenuCtrl));

   Parent::initPersistFields();
}

ConsoleMethod(GuiGameListMenuCtrl, addRow, void, 4, 8,
              "(string label, string callback, int icon, int yPad, bool enabled)\n"
              "Add a row to the list control.\n\n"
              "\\param label The text to display on the row as a label.\n"
              "\\param callback Name of a script function to use as a callback when this row is activated.\n"
              "\\param icon [optional] Index of the icon to use as a marker.\n"
              "\\param yPad [optional] An extra amount of height padding before the row. Does nothing on the first row.\n"
              "\\param useHighlightIcon [optional] Does this row use the highlight icon?.\n"
              "\\param enabled [optional] If this row is initially enabled.") 
{
   object->addRow(argv[2], argv[3], argc > 4 ? dAtoi(argv[4]) : -1, argc > 5 ? dAtoi(argv[5]) : 0, argc > 6 ? dAtob(argv[6]) : true, argc > 7 ? dAtob(argv[7]) : true);
}

ConsoleMethod(GuiGameListMenuCtrl, isRowEnabled, bool, 3, 3,
              "(int row)\n"
              "Determines if the specified row is enabled or disabled.\n\n"
              "\\param row The row to set the enabled status of.\n"
              "\\return (bool) True if the specified row is enabled. False if the row is not enabled or the given index was not valid.")
{
   return object->isRowEnabled(dAtoi(argv[2]));
}

ConsoleMethod(GuiGameListMenuCtrl, setRowEnabled, void, 4, 4,
              "(int row, bool enabled)\n"
              "Sets a row's enabled status according to the given parameters.\n\n"
              "\\param row The index to check for validity.\n"
              "\\param enabled Indicate true to enable the row or false to disable it.")
{
   object->setRowEnabled(dAtoi(argv[2]), dAtob(argv[3]));
}

ConsoleMethod(GuiGameListMenuCtrl, activateRow, void, 2, 2,
              "()\n"
              "Activates the current row. The script callback of  the current row will be called (if it has one).")
{
   object->activateRow();
}

ConsoleMethod(GuiGameListMenuCtrl, getRowCount, S32, 2, 2,
              "()\n"
              "Gets the number of rows on the control.\n\n"
              "\\return (int) The number of rows on the control.")
{
   return object->getRowCount();
}

ConsoleMethod(GuiGameListMenuCtrl, getRowLabel, const char *, 3, 3,
              "(int rowIndex)\n"
              "Gets the label displayed on the specified row.\n\n"
              "\\param rowIndex Index of the row to get the label of.\n"
              "\\return (string) The label for the row.")
{
   return object->getRowLabel(dAtoi(argv[2]));
}

ConsoleMethod(GuiGameListMenuCtrl, setRowLabel, void, 4, 4,
              "(int rowIndex, string label)\n"
              "Sets the label on the given row.\n\n"
              "\\param rowIndex Index of the row to set the label on.\n"
              "\\param label Text to set as the label of the row.\n")
{
   object->setRowLabel(dAtoi(argv[2]), argv[3]);
}

ConsoleMethod(GuiGameListMenuCtrl, setSelected, void, 3, 3,
              "(int rowIndex)\n"
              "Sets the selected row. Only rows that are enabled can be selected.\n\n"
              "\\param index The index to set as selected.")
{
   object->setSelected(dAtoi(argv[2]));
}

ConsoleMethod(GuiGameListMenuCtrl, getSelectedRow, S32, 2, 2,
              "()\n"
              "Gets the index of the currently selected row.\n\n"
              "\\return (int) Index of the selected row.")
{
   return object->getSelected();
}

//-----------------------------------------------------------------------------
// GuiGameListMenuProfile
//-----------------------------------------------------------------------------

GuiGameListMenuProfile::GuiGameListMenuProfile()
: mHitAreaUpperLeft(0, 0),
  mHitAreaLowerRight(0, 0),
  mIconOffset(0, 0),
  mRowSize(0, 0),
  mRowScale(1.0f, 1.0f)
{
}

bool GuiGameListMenuProfile::onAdd()
{
   if (! Parent::onAdd())
   {
      return false;
   }

   // We can't call enforceConstraints() here because incRefCount initializes
   // some of the things to enforce. Do a basic sanity check here instead.
   AssertFatal(dStrlen(mBitmapName) > 0, avar("GuiGameListMenuProfile: %s can't be created without a bitmap. Please add a 'Bitmap' property to the object definition.", getName()));
   AssertFatal(mRowSize.x >= 0, avar("GuiGameListMenuProfile: %s can't have a negative row width. Please change the row width to be non-negative.", getName()));
   AssertFatal(mRowSize.y >= 0, avar("GuiGameListMenuProfile: %s can't have a negative row height. Please change the row height to be non-negative.", getName()));

   return true;
}

void GuiGameListMenuProfile::enforceConstraints()
{
   AssertFatal((! getBitmapArrayRect(0).extent.isZero()), avar("GuiGameListMenuCtrl: %s can't be created without a bitmap. Please add a bitmap to the profile's definition.", getName()));

   AssertFatal(mRowSize.x >= 0, avar("GuiGameListMenuProfile: %s can't have a negative row width. Please change the row width to be non-negative.", getName()));
   mRowSize.x = getMax(mRowSize.x, 0);

   AssertFatal(mRowSize.y >= 0, avar("GuiGameListMenuProfile: %s can't have a negative row height. Please change the row height to be non-negative.", getName()));
   mRowSize.y = getMax(mRowSize.y, 0);

   Point2I rowTexExtent = getBitmapArrayRect(TEX_NORMAL).extent;
   mRowScale.x = (float) getRowWidth() / rowTexExtent.x;
   mRowScale.y = (float) getRowHeight() / rowTexExtent.y;
}

Point2I GuiGameListMenuProfile::getIconExtent()
{
   Point2I iconExtent = getBitmapArrayRect(TEX_FIRST_ICON).extent;

   // scale both by y to keep the aspect ratio
   iconExtent.x *= mRowScale.y;
   iconExtent.y *= mRowScale.y;

   return iconExtent;
}

Point2I GuiGameListMenuProfile::getArrowExtent()
{
   Point2I arrowExtent = getBitmapArrayRect(TEX_FIRST_ARROW).extent;

   // scale both by y to keep the aspect ratio
   arrowExtent.x *= mRowScale.y;
   arrowExtent.y *= mRowScale.y;

   return arrowExtent;
}

Point2I GuiGameListMenuProfile::getHitAreaExtent()
{
   if (mHitAreaLowerRight == mHitAreaUpperLeft)
   {
      return mRowSize;
   }
   else
   {
      return mHitAreaLowerRight - mHitAreaUpperLeft;
   }
}

//-----------------------------------------------------------------------------
// Console stuff (GuiGameListMenuProfile)
//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(GuiGameListMenuProfile);

void GuiGameListMenuProfile::initPersistFields()
{
   addField("HitAreaUpperLeft",  TypePoint2I,   Offset(mHitAreaUpperLeft, GuiGameListMenuProfile));
   addField("HitAreaLowerRight", TypePoint2I,   Offset(mHitAreaLowerRight, GuiGameListMenuProfile));
   addField("IconOffset",        TypePoint2I,   Offset(mIconOffset, GuiGameListMenuProfile));
   addField("RowSize",           TypePoint2I,   Offset(mRowSize, GuiGameListMenuProfile));

   Parent::initPersistFields();

   removeField("tab");
   removeField("mouseOverSelected");

   removeField("modal");
   removeField("opaque");
   removeField("fillColor");
   removeField("fillColorHL");
   removeField("fillColorNA");
   removeField("border");
   removeField("borderThickness");
   removeField("borderColor");
   removeField("borderColorHL");
   removeField("borderColorNA");

   removeField("bevelColorHL");
   removeField("bevelColorLL");

   removeField("fontColorLink");
   removeField("fontColorLinkHL");

   removeField("justify");
   removeField("returnTab");
   removeField("numbersOnly");
   removeField("cursorColor");

   removeField("profileForChildren");
}
