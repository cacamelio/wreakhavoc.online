#include "engine_prediction.h"
#include "logger.h"

int GetTickBase(CUserCmd* pCmd, CBasePlayer* pLocal)
{
	static int iTick = 0;

	if (pCmd != nullptr)
	{
		static CUserCmd* pLastCmd = nullptr;

		// if command was not predicted - increment tickbase
		if (pLastCmd == nullptr || pLastCmd->bHasBeenPredicted)
			iTick = pLocal->m_nTickBase();
		else
			iTick++;

		pLastCmd = pCmd;
	}

	return iTick;
}

void CEnginePrediction::RunCommand( CUserCmd& cmd )
{
	if ( !ctx.m_pWeapon )
		return;

	m_flCurtime = Interfaces::Globals->flCurTime;
	m_flFrametime = Interfaces::Globals->flFrameTime;
	m_TickCount = Interfaces::Globals->iTickCount;

	Interfaces::Globals->flCurTime = ctx.m_flFixedCurtime;
	Interfaces::Globals->flFrameTime = Interfaces::Prediction->bEnginePaused ? 0.f : Interfaces::Globals->flIntervalPerTick;
	Interfaces::Globals->iTickCount = GetTickBase(&cmd, ctx.m_pLocal);

	const auto BackupInPrediction{ Interfaces::Prediction->bInPrediction };
	const auto BackupFirstTimePrediction{ Interfaces::Prediction->Split->bIsFirstTimePredicted };

	if (ctx.m_pLocal->CurrentCommand())
		ctx.m_pLocal->CurrentCommand() = &cmd;

	*(*reinterpret_cast<unsigned int**>(Displacement::Sigs.uPredictionRandomSeed)) = ((int(__thiscall*)(int))Displacement::Sigs.MD5PseudoRandom)(cmd.iCommandNumber) & 0x7fffffff;
	*(*reinterpret_cast<CBasePlayer***>(Displacement::Sigs.pPredictionPlayer)) = ctx.m_pLocal;

	Interfaces::Prediction->bInPrediction = true;
	Interfaces::Prediction->Split->bIsFirstTimePredicted = false;

	ctx.m_bProhibitSounds = true;
	{
		Interfaces::GameMovement->StartTrackPredictionErrors(ctx.m_pLocal);

		Interfaces::Prediction->CheckMovingGround(ctx.m_pLocal, Interfaces::Globals->flFrameTime);
		Interfaces::Prediction->SetLocalViewAngles(cmd.viewAngles);

		Interfaces::MoveHelper->SetHost(ctx.m_pLocal);

		Interfaces::Prediction->SetupMove(ctx.m_pLocal, &cmd, Interfaces::MoveHelper, &MoveData);
		Interfaces::GameMovement->ProcessMovement(ctx.m_pLocal, &MoveData);

		Interfaces::Prediction->FinishMove(ctx.m_pLocal, &cmd, &MoveData);
		Interfaces::MoveHelper->ProcessImpacts();
	}
	ctx.m_bProhibitSounds = false;

	if (const auto ViewModel{ static_cast<CBaseViewModel*>(Interfaces::ClientEntityList->GetClientEntityFromHandle(ctx.m_pLocal->m_hViewModel())) }; ViewModel)
	{
		m_ViewmodelData.m_flCycle = ViewModel->m_flCycle();
		m_ViewmodelData.m_flAnimationTime = ViewModel->m_flAnimTime();
	}

	ctx.m_pWeapon->UpdateAccuracyPenalty( );

	Spread = ctx.m_pWeapon->GetSpread( );
	Inaccuracy = ctx.m_pWeapon->GetInaccuracy( );

	auto& localData{ ctx.m_cLocalData.at(cmd.iCommandNumber % 150) };
	localData.SavePredVars(ctx.m_pLocal, cmd);

	Interfaces::Prediction->bInPrediction = BackupInPrediction;
	Interfaces::Prediction->Split->bIsFirstTimePredicted = BackupFirstTimePrediction;
}

void CEnginePrediction::Finish( ) 
{
	if ( !ctx.m_pWeapon )
		return;

	Interfaces::GameMovement->FinishTrackPredictionErrors(ctx.m_pLocal);
	Interfaces::MoveHelper->SetHost(nullptr);

	Interfaces::Globals->flCurTime = m_flCurtime;
	Interfaces::Globals->flFrameTime = m_flFrametime;

	if (ctx.m_pLocal->CurrentCommand())
		ctx.m_pLocal->CurrentCommand() = nullptr;

	*(*reinterpret_cast<unsigned int**>(Displacement::Sigs.uPredictionRandomSeed)) = -1;
	*(*reinterpret_cast<CBasePlayer***>(Displacement::Sigs.pPredictionPlayer)) = nullptr;

	Interfaces::GameMovement->Reset();
}

void CEnginePrediction::RestoreNetvars( int slot )
{
	const auto& local{ ctx.m_pLocal };
	if ( !local || local->IsDead( ) )
		return;

	const auto& data{ m_cCompressionVars.at( slot % 150 ) };
	if ( data.m_iCommandNumber != slot )
		return;

	const auto aimPunchAngleVelDiff{ local->m_aimPunchAngleVel( ) - data.m_aimPunchAngleVel };
	const auto aimPunchAngleDiff{ local->m_aimPunchAngle( ) - data.m_aimPunchAngle };

	if ( std::abs( aimPunchAngleDiff.x ) <= 0.03125f 
		&& std::abs( aimPunchAngleDiff.y ) <= 0.03125f 
		&& std::abs( aimPunchAngleDiff.z ) <= 0.03125f )
		local->m_aimPunchAngle( ) = data.m_aimPunchAngle;

	if ( std::abs( aimPunchAngleVelDiff.x ) <= 0.03125f 
		&& std::abs( aimPunchAngleVelDiff.y ) <= 0.03125f 
		&& std::abs( aimPunchAngleVelDiff.z ) <= 0.03125f )
		local->m_aimPunchAngleVel( ) = data.m_aimPunchAngleVel;
}

void CEnginePrediction::StoreNetvars( int slot )
{
	const auto& local{ ctx.m_pLocal };
	if ( !local || local->IsDead( ) )
		return;

	auto& data = m_cCompressionVars.at( slot % 150 );

	data.m_aimPunchAngleVel = local->m_aimPunchAngleVel( );
	data.m_aimPunchAngle = local->m_aimPunchAngle( );
	data.m_vecViewOffsetZ = local->m_vecViewOffset( ).z;
}