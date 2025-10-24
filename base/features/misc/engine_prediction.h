#pragma once
#include "../../context.h"

struct CompressionVars_t
{
	QAngle	m_aimPunchAngle{ };
	Vector	m_aimPunchAngleVel{ };
	float	m_vecViewOffsetZ{ };
	int		m_iCommandNumber{ };
};

struct ViewmodelData_t
{
	float m_flCycle{ };
	float m_flAnimationTime{ };
};

class CEnginePrediction
{
public:
	void RunCommand( CUserCmd& cmd );
	void Finish( );

	void RestoreNetvars( int slot );
	void StoreNetvars( int slot );

	float Spread{ };
	float Inaccuracy{ };

	ViewmodelData_t						m_ViewmodelData{ };
	std::array< CompressionVars_t, 150> m_cCompressionVars{ };
private:
	float		m_flCurtime{ };
	float		m_flFrametime{ };
	int			m_TickCount{ };

	float		m_flVelocityModifier{ };

	bool		m_bFirstTimePrediction{ };
	bool		m_bInPrediction{ };

	CMoveData	MoveData{ };
};

namespace Features { inline CEnginePrediction EnginePrediction; };