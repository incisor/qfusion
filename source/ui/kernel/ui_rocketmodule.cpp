#include "ui_precompiled.h"
#include "kernel/ui_common.h"
#include "kernel/ui_main.h"
#include "kernel/ui_keyconverter.h"
#include "kernel/ui_rocketmodule.h"
#include "kernel/ui_eventlistener.h"

#include "widgets/ui_widgets.h"
#include "as/asui.h"

#include "decorators/ui_decorators.h"

#include "../gameshared/q_keycodes.h"

#include "parsers/ui_parsersound.h"

#include <Rocket/Core/FontEffectInstancer.h>
#include <Rocket/Controls.h>

namespace WSWUI
{

//==================================================

class MyEventInstancer : public Rocket::Core::EventInstancer
{
	typedef Rocket::Core::Event Event;
	typedef Rocket::Core::Element Element;
	typedef Rocket::Core::Dictionary Dictionary;

public:
	MyEventInstancer() : Rocket::Core::EventInstancer() {}
	~MyEventInstancer() {}

	// rocket overrides
	virtual Event *InstanceEvent(Element *target, const String &name, const Dictionary &parameters, bool interruptible)
	{
		// Com_Printf("MyEventInstancer: instancing %s %s\n", name.CString(), target->GetTagName().CString() );
		return __new__( Event )( target, name, parameters, interruptible );
	}

	virtual void ReleaseEvent(Event *event)
	{
		// Com_Printf("MyEventInstancer: releasing %s %s\n", event->GetType().CString(), event->GetTargetElement()->GetTagName().CString() );
		__delete__( event );
	}

	virtual void Release()
	{
		__delete__( this );
	}
};

//==================================================

RocketModule::RocketModule( int vidWidth, int vidHeight, float pixelRatio )
	: rocketInitialized( false ), hideCursorBits( 0 ), touchID( -1 ),
	// pointers
	systemInterface(0), fsInterface(0), renderInterface(0), context(0)
{

	renderInterface = __new__( UI_RenderInterface )( vidWidth, vidHeight, pixelRatio );
	Rocket::Core::SetRenderInterface( renderInterface );
	systemInterface = __new__( UI_SystemInterface )();
	Rocket::Core::SetSystemInterface( systemInterface );
	fsInterface = __new__( UI_FileInterface )();
	Rocket::Core::SetFileInterface( fsInterface );
	fontProviderInterface = __new__( UI_FontProviderInterface )( renderInterface );
	Rocket::Core::SetFontProviderInterface( fontProviderInterface );

	// TODO: figure out why renderinterface has +1 refcount
	renderInterface->AddReference();
	fontProviderInterface->AddReference();

	rocketInitialized = Rocket::Core::Initialise();
	if( !rocketInitialized )
		throw std::runtime_error( "UI: Rocket::Core::Initialise failed" );

	// initialize the controls plugin
	Rocket::Controls::Initialise();

	// Create our context
	context = Rocket::Core::CreateContext( trap::Cvar_String( "gamename" ), Vector2i( vidWidth, vidHeight ) );
}

// here for hax0rz, TODO: move to "common" area
template<typename T>
void unref_object( T *obj ) {
	obj->RemoveReference();
}

RocketModule::~RocketModule()
{
	if( fontProviderInterface )
		fontProviderInterface->RemoveReference();
	if( context )
		context->RemoveReference();
	context = 0;

	if( rocketInitialized )
		Rocket::Core::Shutdown();
	rocketInitialized = false;

	__SAFE_DELETE_NULLIFY( fontProviderInterface );
	__SAFE_DELETE_NULLIFY( fsInterface );
	__SAFE_DELETE_NULLIFY( systemInterface );
	__SAFE_DELETE_NULLIFY( renderInterface );
}

//==================================================

void RocketModule::mouseMove( int mousex, int mousey )
{
	context->ProcessMouseMove( mousex, mousey, KeyConverter::getModifiers() );
}

void RocketModule::textInput( wchar_t c )
{
	if( c >= ' ' )
		context->ProcessTextInput( c );
}

void RocketModule::keyEvent( int key, bool pressed )
{
	// DEBUG
#if 0
	if( key >= 32 && key <= 126 )
		Com_Printf("**KEYEVENT CHAR %c\n", key & 0xff );
	else
		Com_Printf("**KEYEVENT KEY %d\n", key );
#endif

	if( key == K_MOUSE1DBLCLK )
		return; // Rocket handles double click internally

	Element *element = context->GetFocusElement();

	int mod = KeyConverter::getModifiers();

	// send the blur event, to the current focused element, when ESC key is pressed
	if( ( key == K_ESCAPE ) && element )
		element->Blur();

	if( element && ( element->GetTagName() == "keyselect" ) )
	{
		if( pressed )
		{
			Rocket::Core::Dictionary parameters;
			parameters.Set( "key", key );
			element->DispatchEvent( "keyselect", parameters );
		}
	}
	else if( key >= K_MOUSE1 && key <= K_MOUSE8 ) // warsow sends mousebuttons as keys
	{
		if( pressed )
			context->ProcessMouseButtonDown( key-K_MOUSE1, mod );
		else
			context->ProcessMouseButtonUp( key-K_MOUSE1, mod );
	}
	else if( key == K_MWHEELDOWN ) // and ditto for wheel
	{
		context->ProcessMouseWheel( 1, mod );
	}
	else if( key == K_MWHEELUP )
	{
		context->ProcessMouseWheel( -1, mod );
	}
	else
	{
		if( ( key == K_A_BUTTON ) || ( key == K_DPAD_CENTER ) )
		{
			if( pressed )
				context->ProcessMouseButtonDown( 0, mod );
			else
				context->ProcessMouseButtonUp( 0, mod );
		}
		else
		{
			int rkey = KeyConverter::toRocketKey( key );

			if( key == K_B_BUTTON )
			{
				rkey = Rocket::Core::Input::KI_ESCAPE;
				if( element )
					element->Blur();
			}

			if( rkey != 0 )
			{
				if( pressed )
					context->ProcessKeyDown( Rocket::Core::Input::KeyIdentifier( rkey ), mod );
				else
					context->ProcessKeyUp( Rocket::Core::Input::KeyIdentifier( rkey ), mod );
			}
		}
	}
}

void RocketModule::touchEvent( int id, touchevent_t type, int x, int y )
{
	if( ( type == TOUCH_DOWN ) && ( touchID < 0 ) ) {
		touchID = id;
		touchOrigin.x = x;
		touchOrigin.y = y;
		touchY = y;
		touchScroll = false;
	}

	if( id != touchID ) {
		return;
	}

	UI_Main::Get()->mouseMove( x, y, true, false );

	if( type == TOUCH_DOWN ) {
		context->ProcessMouseButtonDown( 0, KeyConverter::getModifiers() );
	} else {
		int delta = touchY - y;
		if( delta ) {
			if( !touchScroll ) {
				int threshold = 32 * ( renderInterface->GetPixelsPerInch() / renderInterface->GetBasePixelsPerInch() );
				if( abs( delta ) > threshold ) {
					touchScroll = true;
					touchY += ( ( delta < 0 ) ? threshold : -threshold );
					delta = touchY - y;
				}
			}

			if( touchScroll ) {
				Element *element;
				for( element = context->GetElementAtPoint( touchOrigin ); element; element = element->GetParentNode() ) {
					if( element->GetTagName() == "scrollbarvertical" ) {
						break;
					}

					int overflow = element->GetProperty< int >( "overflow-y" );
					if( ( overflow != Rocket::Core::OVERFLOW_AUTO ) && ( overflow != Rocket::Core::OVERFLOW_SCROLL ) ) {
						continue;
					}

					int scrollTop = element->GetScrollTop();
					if( ( ( delta < 0 ) && ( scrollTop > 0 ) ) ||
						( ( delta > 0 ) && ( element->GetScrollHeight() > scrollTop + element->GetClientHeight() ) ) ) {
						element->SetScrollTop( element->GetScrollTop() + delta );
						break;
					}
				}
				touchY = y;
			}
		}

		if( type == TOUCH_UP ) {
			cancelTouches();
		}
	}
}

void RocketModule::cancelTouches( void )
{
	if( touchID < 0 ) {
		return;
	}

	touchID = -1;
	context->ProcessMouseButtonUp( 0, KeyConverter::getModifiers() );
	UI_Main::Get()->mouseMove( 0, 0, true, false );
}

//==================================================

Rocket::Core::ElementDocument *RocketModule::loadDocument( const char *filename, bool show, void *user_data )
{
	ASUI::UI_ScriptDocument *document = dynamic_cast<ASUI::UI_ScriptDocument *>(context->LoadDocument( filename ));
	if( !document ) {
		return NULL;
	}

	if( show )
	{
		// load documents with autofocus disabled
		document->Show( Rocket::Core::ElementDocument::NONE );
		document->Focus();

		// reference counting may bog on us if we cache documents!
		document->RemoveReference();

		// optional element specific eventlisteners here

		// only for UI documents! FIXME: we are already doing this in NavigationStack
		Rocket::Core::EventListener *listener = UI_GetMainListener();
		document->AddEventListener( "keydown", listener );
		document->AddEventListener( "change", listener );
	}

	return document;
}

void RocketModule::closeDocument( Rocket::Core::ElementDocument *doc )
{
	doc->Close();
}

//==================================================

void RocketModule::registerElementDefaults( Rocket::Core::Element *element )
{
	// add these as they pile up in BaseEventListener
	element->AddEventListener( "mouseover", GetBaseEventListener() );
	element->AddEventListener( "click", GetBaseEventListener() );
}

void RocketModule::registerElement( const char *tag, Rocket::Core::ElementInstancer *instancer )
{
	Rocket::Core::Factory::RegisterElementInstancer( tag, instancer );
	instancer->RemoveReference();
	elementInstancers.push_back( instancer );
}

void RocketModule::registerFontEffect( const char *name, Rocket::Core::FontEffectInstancer *instancer )
{
	Rocket::Core::Factory::RegisterFontEffectInstancer( name, instancer );
	instancer->RemoveReference();
}

void RocketModule::registerDecorator( const char *name, Rocket::Core::DecoratorInstancer *instancer )
{
	Rocket::Core::Factory::RegisterDecoratorInstancer( name, instancer );
	instancer->RemoveReference();
}

void RocketModule::registerEventInstancer( Rocket::Core::EventInstancer *instancer )
{
	Rocket::Core::Factory::RegisterEventInstancer( instancer );
	instancer->RemoveReference();
}

void RocketModule::registerEventListener( Rocket::Core::EventListenerInstancer *instancer )
{
	Rocket::Core::Factory::RegisterEventListenerInstancer( instancer );
	instancer->RemoveReference();
}

// Load the mouse cursor and release the caller's reference:
// NOTE: be sure to use it before initRocket( .. ) function
void RocketModule::loadCursor( const String& rmlCursor )
{
	Rocket::Core::ElementDocument* cursor = context->LoadMouseCursor( rmlCursor );

	if( cursor )
		cursor->RemoveReference();
}

void RocketModule::hideCursor( unsigned int addBits, unsigned int clearBits )
{
	hideCursorBits = ( hideCursorBits & ~clearBits ) | addBits;
	context->ShowMouseCursor( hideCursorBits == 0 );
}

void RocketModule::update( void )
{
	ASUI::GarbageCollectEventListenersFunctions( scriptEventListenerInstancer );

	context->Update();
}

void RocketModule::render( void )
{
	context->Render();
}

void RocketModule::registerCustoms()
{
	//
	// ELEMENTS
	registerElement( "*", __new__( GenericElementInstancer<Element> )() );

	// Main document that implements <script> tags
	registerElement( "body", ASUI::GetScriptDocumentInstancer() );
	// Soft keyboard listener
	registerElement( "input",
		__new__( GenericElementInstancerSoftKeyboard<Rocket::Controls::ElementFormControlInput> )() );
	registerElement( "textarea",
		__new__( GenericElementInstancerSoftKeyboard<Rocket::Controls::ElementFormControlTextArea> )() );
	// other widgets
	registerElement( "keyselect", GetKeySelectInstancer() );
	registerElement( "a", GetAnchorWidgetInstancer() );
	registerElement( "optionsform", GetOptionsFormInstancer() );
	registerElement( "levelshot", GetLevelShotInstancer() );
	registerElement( "datagrid", GetSelectableDataGridInstancer() );
	registerElement( "dataspinner", GetDataSpinnerInstancer() );
	registerElement( "modelview", GetModelviewInstancer() );
	registerElement( "worldview", GetWorldviewInstancer() );
	registerElement( "colorselector", GetColorSelectorInstancer() );
	registerElement( "color", GetColorBlockInstancer() );
	registerElement( "idiv", GetInlineDivInstancer() );
	registerElement( "img", GetImageWidgetInstancer() );
	registerElement( "field", GetElementFieldInstancer() );
	registerElement( "video", GetVideoInstancer() );
	registerElement( "irclog", GetIrcLogWidgetInstancer() );
	registerElement( "iframe", GetIFrameWidgetInstancer() );
	registerElement( "l10n", GetElementL10nInstancer() );

	//
	// EVENTS
	registerEventInstancer( __new__( MyEventInstancer )() );

	//
	// EVENT LISTENERS

	// inline script events
	scriptEventListenerInstancer = ASUI::GetScriptEventListenerInstancer();
	scriptEventListenerInstancer->AddReference();

	registerEventListener( scriptEventListenerInstancer );

	//
	// FONT EFFECTS

	//
	// DECORATORS
	registerDecorator( "gradient", GetGradientDecoratorInstancer() );
	registerDecorator( "ninepatch", GetNinePatchDecoratorInstancer() );

	//
	// GLOBAL CUSTOM PROPERTIES

	Rocket::Core::StyleSheetSpecification::RegisterProperty("background-music", "", false).AddParser("string");

	Rocket::Core::StyleSheetSpecification::RegisterParser("sound", new PropertyParserSound());

	Rocket::Core::StyleSheetSpecification::RegisterProperty("sound-hover", "", false)
		.AddParser("sound");
	Rocket::Core::StyleSheetSpecification::RegisterProperty("sound-click", "", false)
		.AddParser("sound");
}

void RocketModule::unregisterCustoms()
{
	if( scriptEventListenerInstancer ) {
		ASUI::ReleaseScriptEventListenersFunctions( scriptEventListenerInstancer );
		scriptEventListenerInstancer->RemoveReference();
		scriptEventListenerInstancer = NULL;
	}
}

//==================================================

void RocketModule::clearShaderCache( void )
{
	renderInterface->ClearShaderCache();
}

void RocketModule::touchAllCachedShaders( void )
{
	renderInterface->TouchAllCachedShaders();
}

}
