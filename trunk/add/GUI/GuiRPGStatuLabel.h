#ifndef _AFX_STATUS_LABEL_H_
#define _AFX_STATUS_LABEL_H_

#include "gui/controls/guiMLTextCtrl.h"

class rpgStatusLabel : public GuiMLTextCtrl
{
private:
	typedef GuiMLTextCtrl Parent;

public:   
	/*C*/         rpgStatusLabel();

	virtual void  onMouseDown(const GuiEvent &event);

	DECLARE_CONOBJECT(rpgStatusLabel);
};

//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~//~~~~~~~~~~~~~~~~~~~~~//

#endif //_AFX_STATUS_LABEL_H_
