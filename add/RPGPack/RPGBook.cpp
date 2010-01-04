#include "RPGBook.h"
#include "RPGBookData.h"
#include "RPGBaseData.h"
#include "core/stream/bitStream.h"
#include "../gui/GuiCellArray.h"
#include "console/consoleTypes.h"

IMPLEMENT_CO_NETOBJECT_V1(RPGBook);

RPGBook::RPGBook() : mDataBlock(NULL)
{
	//并不是广播给每个人
	mNetFlags.clear(Ghostable);
	_nRPGType = RPGTYPE_ALL;
	_nBookType = BOOK_PACK;
	clearBook();
}

RPGBook::~RPGBook()
{

}

bool RPGBook::onNewDataBlock( GameBaseData* dptr )
{
	mDataBlock = dynamic_cast<RPGBookData*>(dptr);
	if (!mDataBlock || !Parent::onNewDataBlock(dptr))
		return false;

	return true;
}

void RPGBook::processTick( const Move* m)
{
	Parent::processTick(m);
	for ( S32 i = 0 ; i < BOOK_MAX ; ++i)
	{
		if (_mBookDataFreezeTime[i] > 0)
		{
			_mBookDataFreezeTime[i] = _mBookDataFreezeTime[i] > TickMs ? _mBookDataFreezeTime[i] - TickMs : 0;
		}
		if (_mBookDataIdx[i] >= 0)
		{
			_mBookDataStatu[i] = _mBookDataFreezeTime[i] == 0 ? STATU_NORMAL : _mBookDataStatu[i];
		}
	}
}

void RPGBook::advanceTime( F32 dt )
{
	Parent::advanceTime(dt);
}

bool RPGBook::onAdd()
{
	if (!Parent::onAdd()) 
		return(false);

	//if there is a gui control which math this book
	//then attach to it
	if (isClientObject())
	{
		guiCellArray * pBookGui = guiCellArray::getBookGUI(_nBookType);
		if ( pBookGui )
			pBookGui->setBook(this);
	}

	return(true);
}

void RPGBook::onRemove()
{
	Parent::onRemove();
}

U32 RPGBook::packUpdate( NetConnection* con, U32 mask, BitStream* stream)
{	
	U32 retMask = Parent::packUpdate(con, mask, stream);

	if (stream->writeFlag(mask & InitialUpdateMask))
	{
		stream->write(_nRPGType);
		stream->write(_nBookType);
		for (S32 i = 0 ; i < BOOK_MAX ; ++i)
		{
			stream->write(_mBookDataIdx[i]);
		}
	}

	if (stream->writeFlag( (mask & BookChangedMask) && !(mask & InitialUpdateMask)))
	{
		for (U8 n = 0 ; n < _vecChangedIdxs.size() ; ++n)
		{
			stream->writeFlag(true);
			stream->write(_vecChangedIdxs[n]);
			stream->write(_mBookDataIdx[_vecChangedIdxs[n]]);
		}
		stream->writeFlag(false);
	}

	if (stream->writeFlag( (mask & BookCoolingMask) && !(mask & InitialUpdateMask)))
	{
		for (U8 i = 0 ; i < BOOK_MAX ; ++i)
		{
			if ( _mBookDataFreezeTime[i] !=0 )
			{
				stream->writeFlag(true);
				stream->write(i);
				stream->write(_mBookDataFreezeTime[i]);
			}
		}
		stream->writeFlag(false);
	}

	if (mask & BookChangedMask)
		_vecChangedIdxs.clear();

	return(retMask);
}

void RPGBook::unpackUpdate( NetConnection* con, BitStream* stream )
{
	Parent::unpackUpdate(con, stream);

	if (stream->readFlag())
	{
		stream->read(&_nRPGType);
		stream->read(&_nBookType);
		S32 item;
		for (S32 i = 0 ; i < BOOK_MAX ; ++i)
		{
			stream->read(&item);
			insertItem(i,item);
		}
	}

	if (stream->readFlag())
	{
		U8 idx;
		S32 item;
		while(stream->readFlag())
		{
			stream->read(&idx);
			stream->read(&item);
			insertItem(idx,item);
		}
		guiCellArray * pBookGui = guiCellArray::getBookGUI(_nBookType);
		if ( pBookGui )
			pBookGui->refreshItems();
	}

	if (stream->readFlag())
	{
		U8 idx = 0;
		while(stream->readFlag())
		{
			stream->read(&idx);
			stream->read(&_mBookDataFreezeTime[idx]);
			_mClientFreezeTimeTotal[idx] = _mBookDataFreezeTime[idx];
		}
	}
}

#define myOffset(field) Offset(field, RPGBook)
void RPGBook::initPersistFields()
{
	Parent::initPersistFields();
	addField("bookType",  TypeS8,          myOffset(_nBookType));
	addField("rpgType",  TypeS8,          myOffset(_nRPGType));
	addField("slots",TypeS32,myOffset(_mBookDataIdx),BOOK_MAX);
}

void RPGBook::clearBook()
{
	for ( S32 i = 0 ; i < BOOK_MAX ; ++i)
		clearItem(i);
}

void RPGBook::clearItem(const U8 & idx )
{
	if (_mBookDataIdx[idx] == -1)
		return;
	_mBookDataIdx[idx] = -1;
	_mBookDataFreezeTime[idx] = 0;
	_mClientFreezeTimeTotal[idx] = 0;
	_mBookDataStatu[idx] = STATU_INVALID;
	if (isServerObject())
	{
		_vecChangedIdxs.push_back(idx);
		setMaskBits(BookChangedMask);
	}
}

void RPGBook::insertItem(const U8 & idx , const S32 & item )
{
	_mBookDataIdx[idx] = item;
	_mBookDataFreezeTime[idx] = 0;
	_mClientFreezeTimeTotal[idx] = 0;
	_mBookDataStatu[idx] = STATU_NORMAL;
	if (isServerObject())
	{
		_vecChangedIdxs.push_back(idx);
		setMaskBits(BookChangedMask);
	}
}

bool RPGBook::swapItem( const U8 & idx , RPGBook * pBook2 ,const U8 & idx2 )
{
	RPGBaseData * RpgBaseData1 = getRpgBaseData(idx);
	RPGBaseData * RpgBaseData2 = pBook2->getRpgBaseData(idx2);
	const U8 & bookType1 = getBookRpgDataType();
	const U8 & bookType2 = pBook2->getBookRpgDataType();
	const U8 & itemType1 = RpgBaseData1 ? RpgBaseData1->getRPGDataType() : RPGTYPE_ALL;
	const U8 & itemType2 = RpgBaseData2 ? RpgBaseData2->getRPGDataType() : RPGTYPE_ALL;
	if ( (bookType1 & itemType2) && (bookType2 & itemType1) )
	{
		S32 item1 = getItem(idx);
		S32 item2 = pBook2->getItem(idx2);
		clearItem(idx);
		pBook2->clearItem(idx2);
		insertItem(idx,item2);
		pBook2->insertItem(idx2,item1);
		if (RpgBaseData1)
		{
			RpgBaseData1->onItemMoved(this,idx,pBook2,idx2);
		}
		if (RpgBaseData2)
		{
			RpgBaseData2->onItemMoved(pBook2,idx2,this,idx);
		}
		return true;
	}
	return false;
}

const S32	& RPGBook::getItem( const U8 & idx )
{
	return _mBookDataIdx[idx];
}

RPGBaseData * RPGBook::getRpgBaseData( const U8 & idx )
{
	return mDataBlock->getRpgBaseData(_mBookDataIdx[idx]);
}

const U8 & RPGBook::getBookRpgDataType()
{
	return _nBookType;
}

bool RPGBook::useBook( const U8 & idx , ERRORS & error, const U32 & casterSimID, const U32 &  targetSimID)
{
	RPGBaseData * pRPGBaseData =	 getRpgBaseData(idx);
	bool ret = false;
	error = ERR_UNKOWN;
	if (pRPGBaseData)
	{
		switch(_mBookDataStatu[idx])
		{
		case STATU_NORMAL:
			{
				U32 coolDownTime = pRPGBaseData->onActivate(error ,casterSimID,targetSimID);
				if (error == ERR_NONE)
				{
					_mBookDataFreezeTime[idx] = coolDownTime;
					_mBookDataStatu[idx] = coolDownTime == 0 ? STATU_NORMAL : STATU_COOLDOWN ;
					ret = true;
				}
				else
					ret = false;
				break;
			}
		case STATU_COOLDOWN:
			{
				if (_mBookDataFreezeTime[idx] == -1)
				{
					if (pRPGBaseData->onDeActivate(error,casterSimID,targetSimID))
					{
						_mBookDataFreezeTime[idx] = 0;
						_mBookDataStatu[idx] = STATU_NORMAL;
						ret = true;
					}
					else
						ret = false;
				}
				break;
			}
		}
	}

	if (ret)
	{
		setMaskBits(BookCoolingMask);
	}

	return ret;
}

bool RPGBook::isItemEmpty( const U8 & idx )
{
	return getItem(idx) == -1;
}

F32 RPGBook::getRatioOfCDTime( const U8 & idx )
{
	if (_mClientFreezeTimeTotal[idx] > 0)
	{
		return 1.f - ((F32)_mBookDataFreezeTime[idx] / (F32)_mClientFreezeTimeTotal[idx]);
	}
	else if (_mClientFreezeTimeTotal[idx] == -1)
	{
		return -1.f;
	}
	else
		return 0;
}

ConsoleMethod(RPGBook,getRpgBaseData,S32,3,3,"%idx")
{
	RPGBaseData * pRPGBaseData = object->getRpgBaseData(dAtoi(argv[2]));
	return pRPGBaseData ? pRPGBaseData->getId() : 0;
}

ConsoleMethod(RPGBook,swapItem,void,5,5,"%idx,%destBook,%destIdx")
{
	RPGBook * pSelf = (RPGBook *)object;
	RPGBook * pOther = (RPGBook *)Sim::findObject(argv[3]);
	if (pSelf && pOther)
	{
		pSelf->swapItem(dAtoi(argv[2]),pOther,dAtoi(argv[4]));
	}
}

ConsoleMethod(RPGBook,useBook,S32,5,5,"%idx,%casterSimID,%targetSimID")
{
	RPGBook * pBook = (RPGBook *)object;
	RPGDefs::ERRORS error;
	pBook->useBook(dAtoi(argv[2]),error,dAtoi(argv[3]),dAtoi(argv[4]));
	return error;
}