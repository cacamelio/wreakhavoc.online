#include "../core/hooks.h"
#include "../core/config.h"
#include "../context.h"
#include "../features/visuals/visuals.h"
#include "../features/misc/engine_prediction.h"
#include "../features/misc/shots.h"
#include "../features/animations/animation.h"
#include "../features/rage/exploits.h"

struct ClientHitVerify_t
{
	Vector m_vecPos;
	float  m_flTime;
	float  m_flExpires;
};

void FASTCALL Hooks::hkFrameStageNotify( IBaseClientDll* thisptr, int edx, EClientFrameStage stage )
{
	static auto oFrameStageNotify = DTR::FrameStageNotify.GetOriginal<decltype( &hkFrameStageNotify )>( );
	
	ctx.GetLocal( );
	static int backup_smoke_count{ };

	if ( Interfaces::ClientState->pNetChannel )
	{
		if ( !DTR::SendNetMsg.IsHooked( ) )
			DTR::SendNetMsg.Create( MEM::GetVFunc( Interfaces::ClientState->pNetChannel, VTABLE::SENDNETMSG ), &Hooks::hkSendNetMsg );

		if ( !DTR::SendDatagram.IsHooked( ) )
			DTR::SendDatagram.Create( MEM::GetVFunc( Interfaces::ClientState->pNetChannel, VTABLE::SENDDATAGRAM ), &Hooks::hkSendDatagram );
	}

	if ( stage == FRAME_RENDER_START )
	{
		Features::Visuals.Weather.Main( );

		if ( ctx.m_pLocal )
		{
			if ( Config::Get<bool>( Vars.VisClientBulletImpacts ) )
			{
				static int lastcount{ };
				auto& clientImpactsList{ *( CUtlVector < ClientHitVerify_t >* )( ( uintptr_t )ctx.m_pLocal + 0x11C50u ) };

				Color col{ Config::Get<Color>( Vars.VisClientBulletImpactsCol ) };
				for ( auto i = clientImpactsList.Count( ); i > lastcount; --i )
					Interfaces::DebugOverlay->AddBoxOverlay( clientImpactsList[ i - 1 ].m_vecPos, Vector( -2, -2, -2 ), Vector( 2, 2, 2 ), QAngle( 0, 0, 0 ),
						col.Get<COLOR_R>( ), col.Get<COLOR_G>( ), col.Get<COLOR_B>( ), col.Get<COLOR_A>( ), Displacement::Cvars.sv_showimpacts_time->GetFloat( ) );

				if ( clientImpactsList.Count( ) != lastcount )
					lastcount = clientImpactsList.Count( );
			}

			for (auto i = 1; i < 64; ++i) 
			{
				const auto player{ static_cast< CBasePlayer* >( Interfaces::ClientEntityList->GetClientEntity( i ) ) };
				if ( !player || player->IsDead( ) || !player->IsPlayer( ) || player == ctx.m_pLocal )
					continue;

				auto& varMap{ player->m_pVarMapping( ) };
				for ( int i{ }; i < varMap.m_nInterpolatedEntries; ++i )
				{
					if ( reinterpret_cast< int >( varMap.m_Entries[ i ].data ) - reinterpret_cast< int >( player ) == 172 )
						varMap.m_Entries[ i ].watcher->m_InterpolationAmount = Interfaces::Globals->flIntervalPerTick * Config::Get<int>( Vars.VisPlayerInterpolation );
					else
						varMap.m_Entries[ i ].m_bNeedsToInterpolate = false;
				}
			}

			if ( Config::Get<bool>( Vars.RemovalFlash ) )
				ctx.m_pLocal->m_flFlashDuration( ) = 0.f;
		}

		{

			if ( Displacement::Cvars.r_modelAmbientMin->GetFloat( ) != ( Config::Get<bool>( Vars.VisWorldBloom ) ? Config::Get<int>( Vars.VisWorldBloomAmbience ) / 10.0f : 0.f ) )
				Displacement::Cvars.r_modelAmbientMin->SetValue( Config::Get<bool>( Vars.VisWorldBloom ) ? Config::Get<int>( Vars.VisWorldBloomAmbience ) / 10.0f : 0.f );

			static bool reset = false;

			if ( Config::Get<bool>( Vars.WorldAmbientLighting ) )
			{
				reset = false;
				if ( Displacement::Cvars.mat_ambient_light_r->GetFloat( ) != Config::Get<Color>( Vars.WorldAmbientLightingCol ).Get<COLOR_R>( ) / 255.f )
					Displacement::Cvars.mat_ambient_light_r->SetValue( Config::Get<Color>( Vars.WorldAmbientLightingCol ).Get<COLOR_R>( ) / 255.f );

				if ( Displacement::Cvars.mat_ambient_light_g->GetFloat( ) != Config::Get<Color>( Vars.WorldAmbientLightingCol ).Get<COLOR_G>( ) / 255.f )
					Displacement::Cvars.mat_ambient_light_g->SetValue( Config::Get<Color>( Vars.WorldAmbientLightingCol ).Get<COLOR_G>( ) / 255.f );

				if ( Displacement::Cvars.mat_ambient_light_b->GetFloat( ) != Config::Get<Color>( Vars.WorldAmbientLightingCol ).Get<COLOR_B>( ) / 255.f )
					Displacement::Cvars.mat_ambient_light_b->SetValue( Config::Get<Color>( Vars.WorldAmbientLightingCol ).Get<COLOR_B>( ) / 255.f );
			}
			else 
			{
				if ( !reset ) 
				{
					Displacement::Cvars.mat_ambient_light_r->SetValue( 0.f );
					Displacement::Cvars.mat_ambient_light_g->SetValue( 0.f );
					Displacement::Cvars.mat_ambient_light_b->SetValue( 0.f );
					reset = true;
				}
			}

			auto& smokeCount = **reinterpret_cast< int** >( Displacement::Sigs.SmokeCount + 0x1 );
			backup_smoke_count = smokeCount;

			if ( Config::Get<bool>( Vars.RemovalSmoke ) )
				smokeCount = 0;

			**reinterpret_cast< bool** >( Displacement::Sigs.PostProcess + 0x2 ) = ( ctx.m_pLocal && !Config::Get<bool>( Vars.VisWorldBloom ) && Config::Get<bool>( Vars.RemovalPostProcess ) );

			if ( !Config::Get<bool>( Vars.VisWorldFog ) )
				Displacement::Cvars.fog_override->SetValue( FALSE );
			else 
			{
				Displacement::Cvars.fog_override->SetValue( TRUE );

				if ( Displacement::Cvars.fog_start->GetInt( ) )
					Displacement::Cvars.fog_start->SetValue( 0 );

				if ( Displacement::Cvars.fog_end->GetInt( ) != Config::Get<int>( Vars.VisWorldFogDistance ) )
					Displacement::Cvars.fog_end->SetValue( Config::Get<int>( Vars.VisWorldFogDistance ) );

				if ( Displacement::Cvars.fog_maxdensity->GetFloat( ) != ( float )Config::Get<int>( Vars.VisWorldFogDensity ) * 0.01f )
					Displacement::Cvars.fog_maxdensity->SetValue( ( float )Config::Get<int>( Vars.VisWorldFogDensity ) * 0.01f );

				if ( Displacement::Cvars.fog_hdrcolorscale->GetFloat( ) != Config::Get<int>( Vars.VisWorldFogHDR ) * 0.01f )
					Displacement::Cvars.fog_hdrcolorscale->SetValue( Config::Get<int>( Vars.VisWorldFogHDR ) * 0.01f );

				const auto& col{ Config::Get<Color>( Vars.VisWorldFogCol ) };

				char bufferColor[ 12 ]{ };
				sprintf_s( bufferColor, 12, "%i %i %i", col.Get<COLOR_R>( ), col.Get<COLOR_G>( ), col.Get<COLOR_B>( ) );

				if ( strcmp( Displacement::Cvars.fog_color->GetString( ), bufferColor ) )
					Displacement::Cvars.fog_color->SetValue( bufferColor );
			}
		}
	}
	else if ( stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START ) 
	{
		if ( ctx.m_pLocal ) 
		{
			if ( !ctx.m_pLocal->IsDead( ) && !ctx.m_pLocal->m_pAnimState( )->bFirstUpdate ) 
			{
				std::memcpy( &ctx.m_pLocal->m_AnimationLayers( )[ 4 ], &ctx.m_pAnimationLayers[ 4 ], sizeof CAnimationLayer );
				std::memcpy( &ctx.m_pLocal->m_AnimationLayers( )[ 5 ], &ctx.m_pAnimationLayers[ 5 ], sizeof CAnimationLayer );

				ctx.m_pLocal->m_AnimationLayers( )[ 6 ].flWeight = ctx.m_pAnimationLayers[ 6 ].flWeight;
				ctx.m_pLocal->m_AnimationLayers( )[ 6 ].flCycle = ctx.m_pAnimationLayers[ 6 ].flCycle;
				ctx.m_pLocal->m_AnimationLayers( )[ 12 ].flWeight = ctx.m_pAnimationLayers[ 12 ].flWeight;
			}

			Features::Visuals.SkyboxChanger( );
			Features::Visuals.ModelChanger( );
		}
	}
	else if ( stage == FRAME_NET_UPDATE_POSTDATAUPDATE_END )
	{
		if ( Config::Get<bool>( Vars.RemovalSmoke ) ) 
		{
			static const std::array< IMaterial*, 4u > smokeMaterials{
				Interfaces::MaterialSystem->FindMaterial( _( "particle/vistasmokev1/vistasmokev1_fire" ), nullptr ),
				Interfaces::MaterialSystem->FindMaterial( _( "particle/vistasmokev1/vistasmokev1_smokegrenade" ), nullptr ),
				Interfaces::MaterialSystem->FindMaterial( _( "particle/vistasmokev1/vistasmokev1_emods" ), nullptr ),
				Interfaces::MaterialSystem->FindMaterial( _( "particle/vistasmokev1/vistasmokev1_emods_impactdust" ), nullptr )
			};

			for ( auto& material : smokeMaterials )
			{
				if (material)
					material->SetMaterialVarFlag(MATERIAL_VAR_NO_DRAW, true);
			}
		}
	}

	if (stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START)
	{
		if (ctx.m_pLocal && ctx.m_pLocal->m_hViewModel())
		{
			if (const auto ViewModel{ static_cast<CBaseViewModel*>(Interfaces::ClientEntityList->GetClientEntityFromHandle(ctx.m_pLocal->m_hViewModel())) }; ViewModel)
			{
				ViewModel->m_flCycle() = Features::EnginePrediction.m_ViewmodelData.m_flCycle;
				ViewModel->m_flAnimTime() = Features::EnginePrediction.m_ViewmodelData.m_flAnimationTime;
			}
		}
	}

	oFrameStageNotify( thisptr, edx, stage );

	if (stage == FRAME_RENDER_END)
		**reinterpret_cast<int**>(Displacement::Sigs.SmokeCount + 0x1) = backup_smoke_count;
	else if (stage == FRAME_NET_UPDATE_END)
	{
		if (ctx.m_pLocal)
			Features::AnimSys.RunAnimationSystem();

		Features::Shots.ProcessShots();
	}

	if ( ctx.m_pLocal )
	{
		if ( !Interfaces::GameRules )
			Interfaces::GameRules = ( ( **reinterpret_cast< CCSGameRules*** >( MEM::FindPattern( CLIENT_DLL, _( "A1 ? ? ? ? 85 C0 0F 84 ? ? ? ? 80 B8 ? ? ? ? ? 74 7A" ) ) + 0x1 ) ) );

		if ( !Interfaces::GameResources )
		{
			for ( auto* pClass{ Interfaces::Client->GetAllClasses( ) }; pClass; pClass = pClass->pNext )
			{
				if ( !strcmp( pClass->szNetworkName, _( "CPlayerResource" ) ) ) {
					RecvTable_t* pClassTable = pClass->pRecvTable;

					for ( int nIndex = 0; nIndex < pClassTable->nProps; nIndex++ )
					{
						RecvProp_t* pProp = &pClassTable->pProps[ nIndex ];

						if ( !pProp || strcmp( pProp->szVarName, _( "m_iTeam" ) ) != 0 )
							continue;

						Interfaces::GameResources = **reinterpret_cast< IGameResources*** >( DWORD( pProp->pDataTable->pProps->oProxyFn ) + 0x10 );
						break;
					}
					break;
				}
			}
		}
	}
	else {
		Interfaces::GameRules = nullptr;
		Interfaces::GameResources = nullptr;
	}
}

bool FASTCALL Hooks::hkWriteUserCmdDeltaToBuffer( void* ecx, void* edx, int slot, bf_write* buf, int from, int to, bool is_new_command ) {
	static auto oWriteUserCmdDeltaToBuffer = DTR::WriteUserCmdDeltaToBuffer.GetOriginal<decltype( &hkWriteUserCmdDeltaToBuffer )>( );

	if ( from == -1 ) 
	{
		const auto moveMsg{ reinterpret_cast< MoveMsg_t* >( *reinterpret_cast< std::uintptr_t* >(reinterpret_cast< std::uintptr_t >( _AddressOfReturnAddress( ) ) - sizeof( std::uintptr_t ) ) - 0x58u ) };

		moveMsg->m_iBackupCmds = 7;
		moveMsg->m_iNewCmds = std::min( 16, Interfaces::ClientState->nChokedCommands + 1 );

		if ( ctx.m_iTicksAllowed > 0 ) 
		{
			const auto newCmds{ std::min( Interfaces::ClientState->nChokedCommands + 1 + ctx.m_iTicksAllowed, 16 ) };
			const auto dontShift{ ctx.m_bSafeFromDefensive && Features::Exploits.m_bRealCmds && (( Features::Misc.AutoPeeking && Config::Get<bool>( Vars.ExploitsDoubletapExtended ) && Config::Get<bool>( Vars.ExploitsDoubletapDefensive ) ) ) };

			if ( dontShift )
				ctx.m_iLastStopTime = Interfaces::Globals->flRealTime;

			if ( Features::Exploits.m_iShiftAmount && !dontShift )
				Features::Exploits.Shift( buf, moveMsg );
			else if ( Features::Exploits.m_bWasDefensiveTick || dontShift )
					Features::Exploits.BreakLC( buf, ctx.m_iTicksAllowed, moveMsg );
			else if ( Features::Exploits.m_iRechargeCmd != Interfaces::ClientState->iLastOutgoingCommand ) 
			{
				const auto newCmds{ std::min( moveMsg->m_iNewCmds + ctx.m_iTicksAllowed, 16 ) };
				const auto maxShiftedCmds{ newCmds - moveMsg->m_iNewCmds };
				ctx.m_iTicksAllowed = std::max( maxShiftedCmds, 0 );
			}
		}

		auto tickbase{ Features::Exploits.AdjustTickbase( moveMsg->m_iNewCmds ) };

		for ( to = Interfaces::ClientState->iLastOutgoingCommand + 1; to <= Interfaces::ClientState->iLastOutgoingCommand + moveMsg->m_iNewCmds; ++to ) 
		{
			auto& localData{ ctx.m_cLocalData.at( to % 150 ) };
			if ( auto command{ Interfaces::Input->GetUserCmd( to ) }; command && command->iTickCount != INT_MAX ) 
			{		
				localData.m_bOverrideTickbase = true;
				localData.m_iAdjustedTickbase = tickbase;
			}
		}

		if ( tickbase > ctx.m_iHighestTickbase )
		{
			ctx.m_bSafeFromDefensive = false;
			ctx.m_iHighestTickbase = tickbase;
		}
		else
			ctx.m_bSafeFromDefensive = true;

		auto nextCmdNumber{ Interfaces::ClientState->iLastOutgoingCommand + Interfaces::ClientState->nChokedCommands + 1 };

		if ( ctx.m_pLocal && !ctx.m_pLocal->IsDead( ) ) 
		{
			if ( auto command{ Interfaces::Input->GetUserCmd( nextCmdNumber ) }; command )
				Features::Antiaim.RunLocalModifications( *command, tickbase );
		}

		for ( to = Interfaces::ClientState->iLastOutgoingCommand - 7 + 1; to <= Interfaces::ClientState->iLastOutgoingCommand + moveMsg->m_iNewCmds; ++to ) 
		{
			if ( !oWriteUserCmdDeltaToBuffer( ecx, edx, slot, buf, from, to, to >= Interfaces::ClientState->iLastOutgoingCommand + 1 ) )
				return false;

			from = to;
		}

		ctx.m_cLocalData.at( nextCmdNumber % 150 ).m_iTickCount = Interfaces::Globals->iTickCount;
		ctx.m_iSentCmds.push_back( nextCmdNumber );
	}
	
	return true;
}