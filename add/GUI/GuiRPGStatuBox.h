
#ifndef _AFX_STATUS_BOX_H_
#define _AFX_STATUS_BOX_H_

#include "gui/controls/guiBitmapCtrl.h"

class rpgStatusBox : public GuiBitmapCtrl
{
private:
	typedef GuiBitmapCtrl Parent;

public:   
	/*C*/         rpgStatusBox();

	virtual void  onMouseDown(const GuiEvent &event);
	virtual void  onSleep();

	DECLARE_CONOBJECT(rpgStatusBox);
};

//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//

#endif //_AFX_STATUS_BOX_H_
