//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// UIKit/UIView.hpp
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#include "UIKitPrivate.hpp"
#include <Foundation/NSObject.hpp>
#include <CoreGraphics/CGGeometry.h>

namespace UI
{
    _NS_OPTIONS(uint32_t, ViewAutoresizing) {
        ViewAutoresizingNone                 = 0,
        ViewAutoresizingFlexibleLeftMargin   = 1 << 0,
        ViewAutoresizingFlexibleWidth        = 1 << 1,
        ViewAutoresizingFlexibleRightMargin  = 1 << 2,
        ViewAutoresizingFlexibleTopMargin    = 1 << 3,
        ViewAutoresizingFlexibleHeight       = 1 << 4,
        ViewAutoresizingFlexibleBottomMargin = 1 << 5
    };

    class View : public NS::Referencing< View >
	{
		public:
			View* init( CGRect frame );

            void addSubview( View *view );

            void setAutoresizingMask( ViewAutoresizing resizingMask );
	};
}


_NS_INLINE UI::View* UI::View::init( CGRect frame )
{
	return Object::sendMessage< View* >( _UI_PRIVATE_CLS( UIView ), _UI_PRIVATE_SEL( initWithFrame_ ), frame );
}

_NS_INLINE void UI::View::addSubview( View *view )
{
    Object::sendMessage< void >( this, _UI_PRIVATE_SEL ( addSubview_ ), view );
}

_NS_INLINE void UI::View::setAutoresizingMask( ViewAutoresizing resizingMask )
{
    Object::sendMessage< void >(this, _UI_PRIVATE_SEL( setAutoresizingMask_ ), resizingMask );
}
