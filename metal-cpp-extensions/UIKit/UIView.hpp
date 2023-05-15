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
	class View : public NS::Referencing< View >
	{
		public:
			View* init( CGRect frame );

            void addSubview( View *view );
	};
}


_NS_INLINE UI::View* UI::View::init( CGRect frame )
{
	return Object::sendMessage< View* >( _UI_PRIVATE_CLS( UIView ), _UI_PRIVATE_SEL( initWithFrame_ ), frame );
}

_NS_INLINE void UI::View::addSubview( View *view )
{
    return Object::sendMessage< void >( this, _UI_PRIVATE_SEL ( addSubview_ ), view );
}
