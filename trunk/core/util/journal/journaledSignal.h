//-----------------------------------------------------------------------------
// Torque 3D
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _JOURNALEDSIGNAL_H_
#define _JOURNALEDSIGNAL_H_

#include "core/util/journal/journal.h"
#include "core/util/tSignal.h"

template<typename Signature> class JournaledSignal;

template<> class JournaledSignal<void()> : public Signal<void()>
{
   typedef Signal<void()> Parent;

public:
   JournaledSignal()
   {
      Journal::DeclareFunction((Parent*)this, &Parent::trigger);
   }

   ~JournaledSignal()
   {
      Journal::RemoveFunction((Parent*)this, &Parent::trigger);
   }

   void trigger()
   {
      Journal::Call((Parent*)this, &Parent::trigger);
   }
};

template<class A> class JournaledSignal<void(A)> : public Signal<void(A)>
{
   typedef Signal<void(A)> Parent;

public:
   JournaledSignal()
   {
      Journal::DeclareFunction((Parent*)this, &Parent::trigger);
   }

   ~JournaledSignal()
   {
      Journal::RemoveFunction((Parent*)this, &Parent::trigger);
   }

   void trigger(A a)
   {
      Journal::Call((Parent*)this, &Parent::trigger, a);
   }
};

template<class A, class B> class JournaledSignal<void(A,B)> : public Signal<void(A,B)>
{
   typedef Signal<void(A,B)> Parent;

public:
   JournaledSignal()
   {
      Journal::DeclareFunction((Parent*)this, &Parent::trigger);
   }

   ~JournaledSignal()
   {
      Journal::RemoveFunction((Parent*)this, &Parent::trigger);
   }

   void trigger(A a, B b)
   {
      Journal::Call((Parent*)this, &Parent::trigger, a, b);
   }
};

template<class A, class B, class C> class JournaledSignal<void(A,B,C)> : public Signal<void(A,B,C)>
{
   typedef Signal<void(A,B,C)> Parent;

public:
   JournaledSignal()
   {
      Journal::DeclareFunction((Parent*)this, &Parent::trigger);
   }

   ~JournaledSignal()
   {
      Journal::RemoveFunction((Parent*)this, &Parent::trigger);
   }

   void trigger(A a, B b, C c)
   {
      Journal::Call((Parent*)this, &Parent::trigger, a, b, c);
   }
};

template<class A, class B, class C, class D> class JournaledSignal<void(A,B,C,D)> : public Signal<void(A,B,C,D)>
{
   typedef Signal<void(A,B,C,D)> Parent;

public:
   JournaledSignal()
   {
      Journal::DeclareFunction((Parent*)this, &Parent::trigger);
   }

   ~JournaledSignal()
   {
      Journal::RemoveFunction((Parent*)this, &Parent::trigger);
   }

   void trigger(A a, B b, C c, D d)
   {
      Journal::Call((Parent*)this, &Parent::trigger, a, b, c, d);
   }
};

template<class A, class B, class C, class D, class E> class JournaledSignal<void(A,B,C,D,E)> : public Signal<void(A,B,C,D,E)>
{
   typedef Signal<void(A,B,C,D,E)> Parent;

public:
   JournaledSignal()
   {
      Journal::DeclareFunction((Parent*)this, &Parent::trigger);
   }

   ~JournaledSignal()
   {
      Journal::RemoveFunction((Parent*)this, &Parent::trigger);
   }

   void trigger(A a, B b, C c, D d, E e)
   {
      Journal::Call((Parent*)this, &Parent::trigger, a, b, c, d, e);
   }
};

template<class A, class B, class C, class D, class E, class F> class JournaledSignal<void(A,B,C,D,E,F)> : public Signal<void(A,B,C,D,E,F)>
{
   typedef Signal<void(A,B,C,D,E,F)> Parent;

public:
   JournaledSignal()
   {
      Journal::DeclareFunction((Parent*)this, &Parent::trigger);
   }

   ~JournaledSignal()
   {
      Journal::RemoveFunction((Parent*)this, &Parent::trigger);
   }

   void trigger(A a, B b, C c, D d, E e, F f)
   {
      Journal::Call((Parent*)this, &Parent::trigger, a, b, c, d, e, f);
   }
};

template<class A, class B, class C, class D, class E, class F, class G> class JournaledSignal<void(A,B,C,D,E,F,G)> : public Signal<void(A,B,C,D,E,F,G)>
{
   typedef Signal<void(A,B,C,D,E,F,G)> Parent;

public:
   JournaledSignal()
   {
      Journal::DeclareFunction((Parent*)this, &Parent::trigger);
   }

   ~JournaledSignal()
   {
      Journal::RemoveFunction((Parent*)this, &Parent::trigger);
   }

   void trigger(A a, B b, C c, D d, E e, F f, G g)
   {
      Journal::Call((Parent*)this, &Parent::trigger, a, b, c, d, e, f, g);
   }
};

template<class A, class B, class C, class D, class E, class F, class G, class H> class JournaledSignal<void(A,B,C,D,E,F,G,H)> : public Signal<void(A,B,C,D,E,F,G,H)>
{
   typedef Signal<void(A,B,C,D,E,F,G,H)> Parent;

public:
   JournaledSignal()
   {
      Journal::DeclareFunction((Parent*)this, &Parent::trigger);
   }

   ~JournaledSignal()
   {
      Journal::RemoveFunction((Parent*)this, &Parent::trigger);
   }

   void trigger(A a, B b, C c, D d, E e, F f, G g, H h)
   {
      Journal::Call((Parent*)this, &Parent::trigger, a, b, c, d, e, f, g, h);
   }
};

//-----------------------------------------------------------------------------
// Common event callbacks definitions
enum InputModifier {
   IM_LALT     = (1 << 1),
   IM_RALT     = (1 << 2),
   IM_LSHIFT   = (1 << 3),
   IM_RSHIFT   = (1 << 4),
   IM_LCTRL    = (1 << 5),
   IM_RCTRL    = (1 << 6),
   IM_LOPT     = (1 << 7),
   IM_ROPT     = (1 << 8),
   IM_ALT      = IM_LALT | IM_RALT,
   IM_SHIFT    = IM_LSHIFT | IM_RSHIFT,
   IM_CTRL     = IM_LCTRL | IM_RCTRL,
   IM_OPT      = IM_LOPT | IM_ROPT,
};

enum InputAction {
   IA_MAKE     = (1 << 0),
   IA_BREAK    = (1 << 1),
   IA_REPEAT   = (1 << 2),
   IA_MOVE     = (1 << 3),
   IA_DELTA    = (1 << 4),
   IA_BUTTON   = (1 << 5),
};

enum ApplicationMessage {
   Quit,
   WindowOpen,          ///< Window opened
   WindowClose,         ///< Window closed.
   WindowShown,         ///< Window has been shown on screen
   WindowHidden,        ///< Window has become hidden 
   WindowDestroy,       ///< Window was destroyed.
   GainCapture,         ///< Window will capture all input
   LoseCapture,         ///< Window will no longer capture all input
   GainFocus,           ///< Application gains focus
   LoseFocus,           ///< Application loses focus
   DisplayChange,       ///< Desktop Display mode has changed
   GainScreen,          ///< Window will acquire lock on the full screen
   LoseScreen,          ///< Window has released lock on the full screen
   Timer,
};

typedef U32 WindowId;

/// void event()
typedef JournaledSignal<void()> IdleEvent;

/// void event(WindowId,U32 modifier,S32 x,S32 y, bool isRelative)
typedef JournaledSignal<void(WindowId,U32,S32,S32,bool)> MouseEvent;

/// void event(WindowId,U32 modifier,S32 wheelDeltaX, S32 wheelDeltaY)
typedef JournaledSignal<void(WindowId,U32,S32,S32)> MouseWheelEvent;

/// void event(WindowId,U32 modifier,U32 action,U16 key)
typedef JournaledSignal<void(WindowId,U32,U32,U16)> KeyEvent;

/// void event(WindowId,U32 modifier,U16 key)
typedef JournaledSignal<void(WindowId,U32,U16)> CharEvent;

/// void event(WindowId,U32 modifier,U32 action,U16 button)
typedef JournaledSignal<void(WindowId,U32,U32,U16)> ButtonEvent;

/// void event(WindowId,U32 modifier,U32 action,U32 axis,F32 value)
typedef JournaledSignal<void(WindowId,U32,U32,U32,F32)> LinearEvent;

/// void event(WindowId,U32 modifier,F32 value)
typedef JournaledSignal<void(WindowId,U32,F32)> PovEvent;

/// void event(WindowId,InputAppMessage)
typedef JournaledSignal<void(WindowId,S32)> AppEvent;

/// void event(WindowId)
typedef JournaledSignal<void(WindowId)> DisplayEvent;

/// void event(WindowId, S32 width, S32 height)
typedef JournaledSignal<void(WindowId, S32, S32)> ResizeEvent;

/// void event(S32 timeDelta)
typedef JournaledSignal<void(S32)> TimeManagerEvent;

// void event(U32 deviceInst,F32 fValue, U16 deviceType, U16 objType, U16 ascii, U16 objInst, U8 action, U8 modifier)
typedef JournaledSignal<void(U32,F32,U16,U16,U16,U16,U8,U8)> InputEvent;

#endif
