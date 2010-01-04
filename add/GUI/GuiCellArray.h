#ifndef __GUI_CELLARRAY_H__
#define __GUI_CELLARRAY_H__

#include "gui/controls/guiBitmapCtrl.h"
#include "../RPGPack/RPGDefs.h"
class RPGBook;

class guiCellArray : public GuiBitmapCtrl, public RPGDefs
{
private:
	typedef GuiBitmapCtrl Parent;
protected:
	RPGBook	 * m_pBook;
	U8				m_nPages;
	U8				m_nCurrentPage;
	U8				m_nRowsPerPage;
	U8				m_nColumnsPerPage;
	U8				m_nCellSizeX;
	U8				m_nCellSizeY;
	U8				m_nCellPadingX;
	U8				m_nCellPadingY;
	bool				m_bLocked;//不能由用户操作，默认false
	bool				m_bClientOnly;//不合服务器交互，默认false

	struct CellItem 
	{
		enum ItemStatu
		{
			Item_Normal,
			Item_PickedUp,
			Item_Rollover,
		};
		ItemStatu			_statu;
		char				_bmpPath[256];
		GFXTexHandle	_textureHandle;
		CellItem():_textureHandle(NULL),_statu(Item_Normal){_bmpPath[0] = 0;}
		~CellItem(){_textureHandle = NULL;}
	};
	CellItem		m_nCellItems[BOOK_MAX];
	U8						_nPickedIndex;
	U8						_nRolloveIndex;

	bool				isValidPosition(U8 page,U8 slot);
	U8				getSlotByLocalPosition(Point2I pos);
	F32				getRatioOfCoolDown(const U8 & idx);
public:
	DECLARE_CONOBJECT(guiCellArray);
	guiCellArray();
	static void initPersistFields();

	U8		getPickedIndex();
	U8		getCurrentPage();
	void		setPickedIndex(U8 idx);
	void		clearPickedIndex();
	void		setRolloverIndex(U8 idx);
	void		clearRolloverIndex();

	RPGBook * getPlayerBook();
	U8				getIndex(U8 page,U8 slot);
	U8				getSlotByGlobalPosition(Point2I pos);

	//get a guicontrol by the book's type id
	static guiCellArray * getBookGUI(const U8 & bookType);

	void sendDragEvent(GuiControl* target, const char* event,Point2I mousePoint);
	GuiControl* findDragTarget(Point2I mousePoint, const char* method);
	void onRender(Point2I offset, const RectI &updateRect);
	void onMouseDown(const GuiEvent &event);
	void onMouseUp(const GuiEvent &event);
	void onRightMouseDown(const GuiEvent &event);
	void onMouseMove(const GuiEvent &event);
	void onMouseLeave(const GuiEvent &event);
	void cancelMove();
	void setBook(RPGBook * pBook);
	void pageUp();
	void pageDown();
	void refreshItems();
	bool isItemEmpty(U8 index);
};

#endif