#include "GuiCellArray.h"
#include "../RPGPack/RPGBook.h"
#include "console/consoleTypes.h"
#include "gfx/gfxDrawUtil.h"
#include "gui/core/guiCanvas.h"
#include "../RPGPack/RPGBaseData.h"

IMPLEMENT_CONOBJECT(guiCellArray);

bool guiCellArray::isValidPosition( U8 page,U8 slot )
{
	return page >= 0 && page < m_nPages && slot >= 0 && slot < m_nColumnsPerPage * m_nRowsPerPage;
}

U8 guiCellArray::getSlotByLocalPosition( Point2I pos )
{
	U8 row = pos.y / (m_nCellSizeY + m_nCellPadingY);
	U8 column = pos.x / (m_nCellSizeX + m_nCellPadingX);
	return row * m_nColumnsPerPage + column;
}

F32 guiCellArray::getRatioOfCoolDown(const U8 & idx)
{
	return m_pBook ? m_pBook->getRatioOfCDTime(idx) : 0;
}

void guiCellArray::initPersistFields()
{
	Parent::initPersistFields();
	addGroup("guiCellArray");		
	addField("RowsPerPage",   TypeS8,     Offset(m_nRowsPerPage,       guiCellArray));
	addField("ColumnsPerPage",   TypeS8,     Offset(m_nColumnsPerPage,       guiCellArray));
	addField("Pages",   TypeS8,     Offset(m_nPages,       guiCellArray));
	addField("xPad",   TypeS8,     Offset(m_nCellPadingX,       guiCellArray));
	addField("yPad",   TypeS8,     Offset(m_nCellPadingY,       guiCellArray));
	addField("cellSizeX",   TypeS8,     Offset(m_nCellSizeX,       guiCellArray));
	addField("cellSizeY",   TypeS8,     Offset(m_nCellSizeY,       guiCellArray));
	addField("locked",   TypeBool ,     Offset(m_bLocked,       guiCellArray));
	addField("clientOnly",   TypeBool ,     Offset(m_bClientOnly,       guiCellArray));
	endGroup("guiCellArray");	
}

U8 guiCellArray::getPickedIndex()
{
	return _nPickedIndex;
}

U8 guiCellArray::getCurrentPage()
{
	return m_nCurrentPage;
}

void guiCellArray::setPickedIndex( U8 idx )
{
	_nPickedIndex = idx;
}

void guiCellArray::clearPickedIndex()
{
	_nPickedIndex = -1;
}

void guiCellArray::setRolloverIndex( U8 idx )
{
	if (_nRolloveIndex >= 0)
	{
		m_nCellItems[_nRolloveIndex]._statu = CellItem::Item_Normal;
	}
	_nRolloveIndex = idx;
	if (_nRolloveIndex >= 0)
	{
		m_nCellItems[_nRolloveIndex]._statu = CellItem::Item_Rollover;
	}
}

void guiCellArray::clearRolloverIndex()
{
	setRolloverIndex(-1);
}

RPGBook * guiCellArray::getPlayerBook()
{
	return m_pBook;
}

U8 guiCellArray::getIndex( U8 page,U8 slot )
{
	if (isValidPosition(page,slot))
	{
		return page * m_nRowsPerPage * m_nColumnsPerPage + slot;
	}
	return -1;
}

U8 guiCellArray::getSlotByGlobalPosition( Point2I pos )
{
	return getSlotByLocalPosition(globalToLocalCoord(pos));
}

void guiCellArray::sendDragEvent( GuiControl* target, const char* event,Point2I mousePoint )
{
	if(!target || !target->isMethod(event))
		return;

	/*
	function guiCellArray::onControlDropped(%this,%srcarray,%srcPickedIndex,%globalMouseX,%globalMouseY)
	*/
	Con::executef(target, event, Con::getIntArg(this->getId()),\
		Con::getIntArg(this->getPickedIndex()), \
		Con::getIntArg(mousePoint.x), \
		Con::getIntArg(mousePoint.y));
}

GuiControl* guiCellArray::findDragTarget( Point2I mousePoint, const char* method )
{
	// If there are any children and we have a parent.
	GuiControl* parent = (GuiControl*)getRoot();
	//if (size() && parent)
	if(parent)
	{
		//mVisible = false;
		GuiControl* dropControl = parent->findHitControl(mousePoint);
		//mVisible = true;
		while( dropControl )
		{      
			if (dropControl->isMethod(method))
				return dropControl;
			else
				dropControl = dropControl->getParent();
		}
	}
	return NULL;
}

void guiCellArray::onRender( Point2I offset, const RectI &updateRect )
{
	//draw lines
	Point2I ptUperLeft = updateRect.point;
	Point2I ptLowerRight = ptUperLeft + updateRect.extent;

	Point2I ptDrawStart = ptUperLeft;
	Point2I ptDrawEnd = ptDrawStart;
	ColorI color(255,0,0);

	ptDrawEnd.x += m_nCellSizeX * m_nColumnsPerPage;
	ptDrawEnd.x += m_nCellPadingX * (m_nColumnsPerPage - 1);
	for (int n = 0 ; n <= m_nRowsPerPage ; n++)
	{
		//GFX->getDrawUtil()->drawLine(ptDrawStart,ptDrawEnd,color);
		ptDrawStart.y += m_nCellSizeY;
		ptDrawEnd.y += m_nCellSizeY;
		//GFX->getDrawUtil()->drawLine(ptDrawStart,ptDrawEnd,color);
		ptDrawStart.y += m_nCellPadingY;
		ptDrawEnd.y += m_nCellPadingY;
	}

	ptDrawStart = ptDrawEnd = ptUperLeft;
	ptDrawEnd.y += m_nCellSizeY * m_nRowsPerPage;
	ptDrawEnd.y += m_nCellPadingY * (m_nRowsPerPage - 1);
	for (int n = 0 ; n <= m_nColumnsPerPage ; n++)
	{
		//GFX->getDrawUtil()->drawLine(ptDrawStart,ptDrawEnd,color);
		ptDrawStart.x += m_nCellSizeX;
		ptDrawEnd.x += m_nCellSizeX;
		//GFX->getDrawUtil()->drawLine(ptDrawStart,ptDrawEnd,color);
		ptDrawStart.x += m_nCellPadingX;
		ptDrawEnd.x += m_nCellPadingX;
	}

	//draw items from book
	if (m_pBook)
	{
		F32 ratio = 0;//getRatioOfCoolDown();
		U8 idx = 0;
		//Point2I pt = ptUperLeft;
		RectI rect,rect2;
		rect.point = ptUperLeft;
		rect.extent = Point2I(m_nCellSizeX,m_nCellSizeY);
		RPGBaseData * pRPGData = NULL;

		Point2I center;
		Point2I diff(4,4);
		ColorI color(0,0,0,128);
		GFX->getDrawUtil()->clearBitmapModulation();
		for (int n = 0 ; n < m_nRowsPerPage ; n++)
		{
			for (int m = 0 ; m < m_nColumnsPerPage ; m++)
			{
				idx = n * m_nColumnsPerPage + m;
				rect.point.x = ptUperLeft.x + m * (m_nCellSizeX + m_nCellPadingX);
				rect.point.y = ptUperLeft.y + n * (m_nCellSizeY + m_nCellPadingY);

				rect2 = rect;
				rect2.point += diff;
				rect2.extent -= diff * 2;

				if (mTextureObject)
				{
					GFX->getDrawUtil()->drawBitmapStretch(mTextureObject,rect);
				}

				pRPGData = m_pBook->getRpgBaseData(idx);
				ratio = getRatioOfCoolDown(idx);
				if (pRPGData)
				{
					if(m_nCellItems[idx]._statu == CellItem::Item_Normal ||
						m_nCellItems[idx]._statu == CellItem::Item_Rollover)
					{
						GFX->getDrawUtil()->drawBitmapStretch(m_nCellItems[idx]._textureHandle,rect2);
						if (mFabs(ratio) > 0.01 && mFabs(ratio) < 0.99)
						{
							center.x = rect2.point.x + rect2.extent.x / 2;
							center.y = rect2.point.y + rect2.extent.y / 2;
							GFX->getDrawUtil()->drawCDRectFill(center,rect2.extent,M_2PI * ratio,color);
						}
					}
				}
			}
		}
	}

	renderChildControls(offset, updateRect);
}

void guiCellArray::onMouseDown( const GuiEvent &event )
{
	if (m_bLocked)
		return;
	GuiCanvas* canvas = getRoot();
	AssertFatal(canvas, "guiCellArray wasn't added to the gui before the drag started.");
	if (canvas->getMouseLockedControl())
	{
		GuiEvent event;
		canvas->getMouseLockedControl()->onMouseLeave(event);
		canvas->mouseUnlock(canvas->getMouseLockedControl());
	}
	canvas->mouseLock(this);
	canvas->setFirstResponder(this);

	U8 slot = getSlotByGlobalPosition(event.mousePoint);
	if (isValidPosition(m_nCurrentPage,slot))
	{
		U8 n = getIndex(m_nCurrentPage,slot);
		m_nCellItems[n]._statu = CellItem::Item_PickedUp;
		setPickedIndex(n);
		if (isItemEmpty(n))
		{
			return;
		}
		GuiCanvas* Canvas = getRoot();
		GuiCursor * pCursor = Canvas->getCurrentCursor();
		if (pCursor)
		{
			pCursor->setPickedBmp(m_nCellItems[n]._bmpPath);
		}
	}
}

void guiCellArray::onMouseUp( const GuiEvent &event )
{
	if (m_bLocked)
		return;
	mouseUnlock();
	GuiCanvas* Canvas = getRoot();
	GuiCursor * pCursor = Canvas->getCurrentCursor();
	if (pCursor)
	{
		pCursor->clearPickedBmp();
	}

	GuiControl* target = findDragTarget(event.mousePoint, "onControlDropped");
	if (target)
	{
		sendDragEvent(target, "onControlDropped",event.mousePoint);
	}
	else
	{
		cancelMove();
	}
}

void guiCellArray::onRightMouseDown( const GuiEvent &event )
{
	if (m_bLocked)
		return;

	U8 slot = getSlotByGlobalPosition(event.mousePoint);
	if (isValidPosition(m_nCurrentPage,slot))
	{
		U8 n = getIndex(m_nCurrentPage,slot);
		F32 ratio = getRatioOfCoolDown(n);
		if (ratio < 1.f && ratio > 0)
		{
			return;
		}
		Con::executef(this,"onRightMouseDown",Con::getIntArg(n));
	}
}

void guiCellArray::onMouseMove( const GuiEvent &event )
{
	if (NULL == m_pBook)
	{
		return;
	}
	Parent::onMouseMove(event);
	U8 slot = getSlotByGlobalPosition(event.mousePoint);
	if (isValidPosition(m_nCurrentPage,slot))
	{
		U8 n = getIndex(m_nCurrentPage,slot);
		if (_nRolloveIndex == n)
		{
			return;
		}
		RPGBaseData * pRPGDataOld = m_pBook->getRpgBaseData(_nRolloveIndex);
		if (pRPGDataOld)
		{
			Con::executef(this,"onLeaveSlot",Con::getIntArg(pRPGDataOld->getId()));
		}
		setRolloverIndex(n);
		if (isItemEmpty(n))
		{
			return;
		}
		RPGBaseData * pRPGData = m_pBook->getRpgBaseData(n);
		if (pRPGData)
		{
			Con::executef(this,"onEnterSlot",Con::getIntArg(pRPGData->getId()));
		}
	}
}

void guiCellArray::onMouseLeave( const GuiEvent &event )
{
	clearRolloverIndex();
	Con::executef(this,"onLeaveSlot");
}

void guiCellArray::cancelMove()
{
	if (_nPickedIndex != -1)
	{
		m_nCellItems[_nPickedIndex]._statu = CellItem::Item_Normal;
	}
	clearPickedIndex();
}

void guiCellArray::setBook( RPGBook * pBook )
{
	m_pBook = pBook;
	refreshItems();
}

void guiCellArray::pageUp()
{
	if (m_nCurrentPage > 0 && m_nCurrentPage <= m_nPages - 1)
	{
		--m_nCurrentPage;
	}
}

void guiCellArray::pageDown()
{
	if (m_nCurrentPage >= 0 && m_nCurrentPage < m_nPages - 1)
	{
		++m_nCurrentPage;
	}
}

void guiCellArray::refreshItems()
{
	RPGBaseData * pRPGData = NULL;
	int idx = 0;
	if (NULL == m_pBook)
	{
		return;
	}
	for (int n = 0 ; n < m_nRowsPerPage ; n++)
	{
		for (int m = 0 ; m < m_nColumnsPerPage ; m++)
		{
			idx = n * m_nColumnsPerPage + m;
			pRPGData = m_pBook->getRpgBaseData(idx);
			if (pRPGData)
			{
				if (dStrcmp(pRPGData->getIconName(),m_nCellItems[idx]._bmpPath) != 0)
				{
					dStrcpy(m_nCellItems[idx]._bmpPath,pRPGData->getIconName());
					if(m_nCellItems[idx]._bmpPath[0] == 0)
						m_nCellItems[idx]._textureHandle = NULL;
					else
						m_nCellItems[idx]._textureHandle.set(m_nCellItems[idx]._bmpPath,&GFXDefaultGUIProfile,"cell item bmp");
				}
			}
			m_nCellItems[idx]._statu = CellItem::Item_Normal;
		}
	}
}

bool guiCellArray::isItemEmpty( U8 index )
{
	RPGBook * pBook =  getPlayerBook();
	if (pBook)
	{
		return pBook->isItemEmpty(index);
	}
	return true;
}

guiCellArray::guiCellArray():m_pBook(NULL),m_nPages(0),m_nCurrentPage(0),\
m_nRowsPerPage(0),m_nColumnsPerPage(0),m_nCellSizeX(42),m_nCellSizeY(42),\
_nPickedIndex(-1),m_nCellPadingX(0),m_nCellPadingY(0),_nRolloveIndex(-1),\
m_bLocked(false),m_bClientOnly(false)
{

}

guiCellArray * guiCellArray::getBookGUI( const U8 & bookType )
{
	guiCellArray * pGUI = NULL;
	char guiName[128];
	switch(bookType)
	{
	case BOOK_PACK:
		dStrcpy(guiName,"backpack");
		break;
	case BOOK_SHOTCUTBAR1:
		dStrcpy(guiName,"shortcut1_gui");
		break;
	default:
		dStrcpy(guiName,"nothing");
		break;
	}
	pGUI = dynamic_cast<guiCellArray*>(Sim::findObject(guiName));
	return pGUI;
}

ConsoleMethod(guiCellArray,getCurrentPage,S32,2,2,"return current page")
{
	return object->getCurrentPage();
}

ConsoleMethod(guiCellArray,getIndex,S32,4,4,"%page,%slot")
{
	return object->getIndex(dAtoi(argv[2]),dAtoi(argv[3]));
}

ConsoleMethod(guiCellArray,getSlotByGlobalPosition,S32,4,4,"%x,%y")
{
	return object->getSlotByGlobalPosition(Point2I(dAtoi(argv[2]),dAtoi(argv[3])));
}

ConsoleMethod(guiCellArray,getPlayerBook,S32,2,2,"return the rpgbook")
{
	RPGBook * pBook = object->getPlayerBook();
	return pBook ? pBook->getId() : -1;
}

ConsoleMethod(guiCellArray,isItemEmpty,bool,3,3,"%idx")
{
	return ((guiCellArray*)object)->isItemEmpty(dAtoi(argv[2]));
}