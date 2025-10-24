#include <thread>
#include "havoc.h"
#include "context.h"
#include "utils/math.h"
#include "utils/render.h"
#include "utils/crash_dumper.h"
#include "core/prop_manager.h"
#include "core/config.h"
#include "core/hooks.h"
#include "core/menu rework/menu.h"
#include "core/displacement.h"
#include "core/event_listener.h"
#include "features/misc/logger.h"
#include "features/misc/skinchanger/kitparser.h"
#include <tchar.h>

FILE* pStream;

void Entry( void* netvars )
{
	while ( !MEM::GetModuleBaseHandle( SERVERBROWSER_DLL ) )
		std::this_thread::sleep_for( 200ms );

	if (!Interfaces::Setup())
		return;

	Menu::Register( );
	Config::Setup( );

	Interfaces::GameConsole->Clear( );

	if ( !PropManager::Get( ).Create( ) )
		return;

	Displacement::Init( netvars );

	if (!Math::Setup())
		return;
	
	Render::CreateFonts( );
	EventListener.Setup( { _( "bullet_impact" ), _( "round_start" ), _( "player_hurt" ),_( "weapon_fire" ), _( "player_death" ) } );

	if (!Hooks::Setup())
		return;

	Features::Logger.Log( _( "Deployed Havoc." ), true );

	while ( !ctx.m_bUnload )
		std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );

	Hooks::Restore( );
	EventListener.Destroy( );

#ifndef _RELEASE
	delete Displacement::Netvars;
#endif

	if ( netvars )
		delete netvars;
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD dwReason, LPVOID lpReserved )
{
	if ( dwReason == DLL_PROCESS_ATTACH ) {
		DisableThreadLibraryCalls( hModule );

		// welcome to artiehack/timhack/ETHEREAL/havoc AKA BEST HVH CHEAT
		if ( const HANDLE hThread = ::CreateThread( nullptr, 0U, LPTHREAD_START_ROUTINE( Entry ), lpReserved, 0UL, nullptr ); hThread != nullptr )
			::CloseHandle( hThread );

		return TRUE;
	}

	return FALSE;
}