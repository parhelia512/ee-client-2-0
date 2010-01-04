//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TSIGNAL_H_
#define _TSIGNAL_H_

#include "core/util/delegate.h"

/// Signals (Multi-cast Delegates)
/// 
/// Signals are used throughout this engine to allow subscribers to listen
/// for generated events for various things.  
/// 
/// Signals are called according to their order parameter (lower
/// numbers first).
///
/// Signal functions can return bool or void.  If bool then returning false
/// from a signal function will cause entries in the ordered signal list after 
/// that one to not be called.
///
/// This allows handlers of a given event to say to the signal generator, "I handled this message, and 
/// it is no longer appropriate for other listeners to handle it"

class SignalBase
{
public:

   SignalBase()
   {
      mList.next = mList.prev = &mList;
      mList.order = 0.5f;
   }

   ~SignalBase();

   /// Returns true if the delegate list is empty.
   bool isEmpty() const
   {
      return mList.next == &mList;
   }

protected:

   struct DelegateLink
   {
      DelegateLink *next,*prev;
      F32 order;

      void insert(DelegateLink* node, F32 order);
      void unlink();
   };

   DelegateLink mList;
};

template<typename Signature> class SignalBaseT : public SignalBase
{
public:

   /// The delegate signature for this signal.
   typedef Delegate<Signature> DelegateSig;

   SignalBaseT() {}

   SignalBaseT( const SignalBaseT &base )
   {
      mList.next = mList.prev = &mList;

      for ( DelegateLink *ptr = base.mList.next; ptr != &base.mList; ptr = ptr->next )
      {
         DelegateLinkImpl *del = static_cast<DelegateLinkImpl*>( ptr );
         notify( del->mDelegate, del->order );
      }
   }

   void notify( const DelegateSig &dlg, F32 order = 0.5f)
   {
      mList.insert(new DelegateLinkImpl(dlg), order);
   }

   void remove(DelegateSig dlg)
   {
      for(DelegateLink *ptr = mList.next;ptr != &mList;ptr = ptr->next)
      {
         if(DelegateLinkImpl *del = static_cast<DelegateLinkImpl *>(ptr))
         {
            if(del->mDelegate == dlg)
            {
               del->unlink();
               delete del;
               return;
            }
         }
      }
   } 

   template <class T,class U>
   void notify(T obj,U func, F32 order = 0.5f)
   {
      DelegateSig dlg(obj, func);
      notify(dlg, order);
   }

   template <class T>
   void notify(T func, F32 order = 0.5f)
   {
      DelegateSig dlg(func);
      notify(dlg, order);
   }

   template <class T,class U>
   void remove(T obj,U func)
   {
      DelegateSig compDelegate(obj, func);
      remove(compDelegate);
   }

   template <class T>
   void remove(T func)
   {
      DelegateSig compDelegate(func);
      remove(compDelegate);
   } 

   /// Returns true if the signal already contains this delegate.
   bool contains( const DelegateSig &dlg ) const
   {
      for ( DelegateLink *ptr = mList.next; ptr != &mList; ptr = ptr->next )
      {
         DelegateLinkImpl *del = static_cast<DelegateLinkImpl*>( ptr );
         if ( del->mDelegate == dlg )
            return true;
      }

      return false;
   } 

protected:

   struct DelegateLinkImpl : public SignalBase::DelegateLink
   {
      DelegateSig mDelegate;
      DelegateLinkImpl(DelegateSig dlg) : mDelegate(dlg) {}
   };

   DelegateSig & getDelegate(SignalBase::DelegateLink * link)
   {
      return ((DelegateLinkImpl*)link)->mDelegate;
   }
};

//-----------------------------------------------------------------------------

template<typename Signature> class Signal;

// Short-circuit signal implementations

template<> 
class Signal<bool()> : public SignalBaseT<bool()>
{
public:
   bool trigger()
   {
      for(SignalBase::DelegateLink *ptr = mList.next;ptr != &mList;ptr = ptr->next)
         if (!getDelegate(ptr)())
            return false;
      return true;
   }
};

template<class A> 
class Signal<bool(A)> : public SignalBaseT<bool(A)>
{   
public:
   bool trigger(A a)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         if (!this->getDelegate(ptr)(a))
            return false;
      return true;
   }
};

template<class A, class B>
class Signal<bool(A,B)> : public SignalBaseT<bool(A,B)>
{
public:
   bool trigger(A a, B b)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         if (!this->getDelegate(ptr)(a,b))
            return false;
      return true;
   }
};

template<class A, class B, class C> 
class Signal<bool(A,B,C)> : public SignalBaseT<bool(A,B,C)>
{
public:
   bool trigger(A a, B b, C c)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         if (!this->getDelegate(ptr)(a,b,c))
            return false;
      return true;
   }
};

template<class A, class B, class C, class D> 
class Signal<bool(A,B,C,D)> : public SignalBaseT<bool(A,B,C,D)>
{
public:
   bool trigger(A a, B b, C c, D d)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         if (!this->getDelegate(ptr)(a,b,c,d))
            return false;
      return true;
   }
};

template<class A, class B, class C, class D, class E> 
class Signal<bool(A,B,C,D,E)> : public SignalBaseT<bool(A,B,C,D,E)>
{
public:
   bool trigger(A a, B b, C c, D d, E e)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         if (!this->getDelegate(ptr)(a,b,c,d,e))
            return false;
      return true;
   }
};

template<class A, class B, class C, class D, class E, class F> 
class Signal<bool(A,B,C,D,E,F)> : public SignalBaseT<bool(A,B,C,D,E,F)>
{
public:
   bool trigger(A a, B b, C c, D d, E e, F f)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         if (!this->getDelegate(ptr)(a,b,c,d,e,f))
            return false;
      return true;
   }
};

template<class A, class B, class C, class D, class E, class F, class G> 
class Signal<bool(A,B,C,D,E,F,G)> : public SignalBaseT<bool(A,B,C,D,E,F,G)>
{
public:
   bool trigger(A a, B b, C c, D d, E e, F f, G g)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         if (!this->getDelegate(ptr)(a,b,c,d,e,f,g))
            return false;
      return true;
   }
};

template<class A, class B, class C, class D, class E, class F, class G, class H> 
class Signal<bool(A,B,C,D,E,F,G,H)> : public SignalBaseT<bool(A,B,C,D,E,F,G,H)>
{
public:
   bool trigger(A a, B b, C c, D d, E e, F f, G g, H h)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         if (!this->getDelegate(ptr)(a,b,c,d,e,f,g,h))
            return false;
      return true;
   }
};

template<class A, class B, class C, class D, class E, class F, class G, class H, class I> 
class Signal<bool(A,B,C,D,E,F,G,H,I)> : public SignalBaseT<bool(A,B,C,D,E,F,G,H,I)>
{
public:
   bool trigger(A a, B b, C c, D d, E e, F f, G g, H h, I i)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         if (!this->getDelegate(ptr)(a,b,c,d,e,f,g,h,i))
            return false;
      return true;
   }
};

template<class A, class B, class C, class D, class E, class F, class G, class H, class I, class J> 
class Signal<bool(A,B,C,D,E,F,G,H,I,J)> : public SignalBaseT<bool(A,B,C,D,E,F,G,H,I,J)>
{
public:
   bool trigger(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         if (!this->getDelegate(ptr)(a,b,c,d,e,f,g,h,i,j))
            return false;
      return true;
   }
};

// Non short-circuit signal implementations

template<> 
class Signal<void()> : public SignalBaseT<void()>
{
public:
   void trigger()
   {
      for(SignalBase::DelegateLink *ptr = mList.next;ptr != &mList;ptr = ptr->next)
         getDelegate(ptr)();
   }
};

template<class A>
class Signal<void(A)> : public SignalBaseT<void(A)>
{
public:
   void trigger(A a)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         this->getDelegate(ptr)(a);
   }
};

template<class A, class B> 
class Signal<void(A,B)> : public SignalBaseT<void(A,B)>
{
public:
  void trigger(A a, B b)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         this->getDelegate(ptr)(a,b);
   }
};

template<class A, class B, class C> 
class Signal<void(A,B,C)> : public SignalBaseT<void(A,B,C)>
{
public:
   void trigger(A a, B b, C c)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         this->getDelegate(ptr)(a,b,c);
   }
};

template<class A, class B, class C, class D>
class Signal<void(A,B,C,D)> : public SignalBaseT<void(A,B,C,D)>
{
public:
  void trigger(A a, B b, C c, D d)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         this->getDelegate(ptr)(a,b,c,d);
   }
};

template<class A, class B, class C, class D, class E> 
class Signal<void(A,B,C,D,E)> : public SignalBaseT<void(A,B,C,D,E)>
{
public:
   void trigger(A a, B b, C c, D d, E e)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         this->getDelegate(ptr)(a,b,c,d,e);
   }
};

template<class A, class B, class C, class D, class E, class F> 
class Signal<void(A,B,C,D,E,F)> : public SignalBaseT<void(A,B,C,D,E,F)>
{
public:
   void trigger(A a, B b, C c, D d, E e, F f)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         this->getDelegate(ptr)(a,b,c,d,e,f);
   }
};

template<class A, class B, class C, class D, class E, class F, class G> 
class Signal<void(A,B,C,D,E,F,G)> : public SignalBaseT<void(A,B,C,D,E,F,G)>
{
public:
   void trigger(A a, B b, C c, D d, E e, F f, G g)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         this->getDelegate(ptr)(a,b,c,d,e,f,g);
   }
};

template<class A, class B, class C, class D, class E, class F, class G, class H> 
class Signal<void(A,B,C,D,E,F,G,H)> : public SignalBaseT<void(A,B,C,D,E,F,G,H)>
{
public:
   void trigger(A a, B b, C c, D d, E e, F f, G g, H h)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         this->getDelegate(ptr)(a,b,c,d,e,f,g,h);
   }
};

template<class A, class B, class C, class D, class E, class F, class G, class H, class I> 
class Signal<void(A,B,C,D,E,F,G,H,I)> : public SignalBaseT<void(A,B,C,D,E,F,G,H,I)>
{
public:
   void trigger(A a, B b, C c, D d, E e, F f, G g, H h, I i)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         this->getDelegate(ptr)(a,b,c,d,e,f,g,h,i);
   }
};

template<class A, class B, class C, class D, class E, class F, class G, class H, class I, class J> 
class Signal<void(A,B,C,D,E,F,G,H,I,J)> : public SignalBaseT<void(A,B,C,D,E,F,G,H,I,J)>
{
public:
   void trigger(A a, B b, C c, D d, E e, F f, G g, H h, I i, J j)
   {
      for(SignalBase::DelegateLink *ptr = this->mList.next;ptr != &this->mList;ptr = ptr->next)
         this->getDelegate(ptr)(a,b,c,d,e,f,g,h,i,j);
   }
};

#endif // _TSIGNAL_H_