#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"soundent.h"
#include	"decals.h"
#include    "weapons.h"

#define SF_PUSHABLE 2

#define VEC_WOOD_BOX_MIN	Vector( -20, -20, 0 )
#define VEC_WOOD_BOX_MAX	Vector( 20, 20, 35 )

class CBreakModel : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int  Classify ( void );
	void Killed( entvars_t *pevAttacker, int iGib );

	void PainSound(void); // WHEN HIT THE BOX
	
	static	TYPEDESCRIPTION m_SaveData[];

	float	m_painTime;
	float	m_flPlayerDamage;

	void KeyValue( KeyValueData* pkvd); // WHAT SPAWN WHEN BREAK
	int			m_iszSpawnObject;
	static const char *pSpawnObjects[];

// PUSH TEST
	float	m_maxSpeed; 
	inline float MaxSpeed( void ) { return m_maxSpeed; } 
	void	Touch ( CBaseEntity *pOther );
	void	Move( CBaseEntity *pMover, int push );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
// END

// PUSH SOUND
	void	EXPORT StopSound( void );
	int		m_lastSound;	
	float	m_soundTime;
// END

};

LINK_ENTITY_TO_CLASS( breakable_woodbox, CBreakModel );

TYPEDESCRIPTION	CBreakModel::m_SaveData[] = 
{
	DEFINE_FIELD( CBreakModel, m_painTime, FIELD_TIME ),
	DEFINE_FIELD( CBreakModel, m_flPlayerDamage, FIELD_FLOAT ),
	DEFINE_FIELD( CBreakModel, m_iszSpawnObject, FIELD_STRING ), // WHAT SPAWN WHEN BREAK
	DEFINE_FIELD( CBreakModel, m_maxSpeed, FIELD_FLOAT ), // PUSH TEST
	DEFINE_FIELD( CBreakModel, m_soundTime, FIELD_TIME ), // PUSH SOUND
};

// TEST WHAT SPAWN WHEN BOX BREAK
const char *CBreakModel::pSpawnObjects[] =
{
	NULL,				
	"item_battery",		
	"item_healthkit",	
	"weapon_pistol",    
	"ammo_9mmclip",		
	"weapon_smg",		
	"ammo_9mmAR",		
	"ammo_ARgrenades",	
	"weapon_shotgun",	
	"ammo_buckshot",	
	"weapon_crossbow",	
	"ammo_crossbow",	
	"weapon_357",		
	"ammo_357",			
	"weapon_rpg",		
	"ammo_rpgclip",		
	"weapon_handgrenade",
	"weapon_irifle",
	"ammo_irifle",
	"ammo_irifleball",
};

void CBreakModel::KeyValue( KeyValueData* pkvd )
{
    if (FStrEq(pkvd->szKeyName, "spawnobject") )
	{
		int object = atoi( pkvd->szValue );
		if ( object > 0 && object < ARRAYSIZE(pSpawnObjects) )
			m_iszSpawnObject = MAKE_STRING( pSpawnObjects[object] );
		pkvd->fHandled = TRUE;
	}
	else 
		CBaseDelay::KeyValue( pkvd );
}
// END

int	CBreakModel :: Classify ( void )
{
	return	CLASS_NONE;
}

void CBreakModel :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case 0: 
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

void CBreakModel :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
	default:
		ys = 0;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// Spawn
//=========================================================
void CBreakModel :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/props/wood_crate01.mdl");

	if ( FStrEq( STRING(pev), "models/props/wood_crate01.mdl" ) )
	UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
    else
    UTIL_SetSize(pev, VEC_WOOD_BOX_MIN, VEC_WOOD_BOX_MAX);

	pev->movetype	= MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_SLIDEBOX;
   	pev->takedamage	= DAMAGE_YES;
   	pev->health		= 30;
  	m_bloodColor	= DONT_BLEED;

	pev->origin.z += 1;
	DROP_TO_FLOOR ( ENT(pev) );

	pev->flags |= FL_WORLDBRUSH;

// SPAWN FLAG TEST MOVE PUSH
if ( pev->spawnflags & SF_PUSHABLE )
	{

	pev->movetype	= MOVETYPE_PUSHSTEP;
	pev->solid		= SOLID_BBOX;

	if ( pev->friction > 399 )
		pev->friction = 399;

	m_maxSpeed = 400 - pev->friction;
	SetBits( pev->flags, FL_FLOAT );
	pev->friction = 0;
	
	if ( FStrEq( STRING(pev), "models/props/wood_crate01.mdl" ) )
	UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
    else
    UTIL_SetSize(pev, VEC_WOOD_BOX_MIN, VEC_WOOD_BOX_MAX);

	DROP_TO_FLOOR ( ENT(pev) );
	pev->flags &= ~FL_ONGROUND;
	m_bloodColor	= DONT_BLEED;

	}
// END

}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CBreakModel :: Precache()
{
	PRECACHE_MODEL("models/props/wood_crate01.mdl");
	PRECACHE_MODEL("models/gibs/WoodCrate_Gibs.mdl");

// WHAT SPAWN WHEN BREAK
	if ( m_iszSpawnObject )
	UTIL_PrecacheOther( (char *)STRING( m_iszSpawnObject ) );
// END
}	

void CBreakModel::Killed( entvars_t *pevAttacker, int iGib )
{

  switch ( RANDOM_LONG( 0, 2 ) )
  {
  case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "impact/wood/wood_plank_break1.wav", 1.0, ATTN_NORM);	break;
  case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "impact/wood/wood_plank_break2.wav", 1.0, ATTN_NORM);	break;
  case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "impact/wood/wood_plank_break3.wav", 1.0, ATTN_NORM);	break;
  }

	CGib::SpawnWoodBoxGibs( pev, 8, 1 ); // GIB TEST
	pev->effects |= EF_NODRAW;
	UTIL_Remove( this ); // Kill Wood Box When Gibed Hack

// WHAT SPAWN WHEN BREAK
	SetThink( SUB_Remove );
	pev->nextthink = pev->ltime + 0.1;

	if ( m_iszSpawnObject )
	CBaseEntity::Create( (char *)STRING(m_iszSpawnObject), VecBModelOrigin(pev), pev->angles, edict() );
// END
}

// WHEN HIT THE WOOD BOX
void CBreakModel :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;
	
	m_painTime = gpGlobals->time + RANDOM_FLOAT(0.5, 0.5);

	switch (RANDOM_LONG(0,2))
	{
	case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "impact/wood/wood_solid_impact_bullet1.wav", 1.0, ATTN_NORM);	break;
	case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "impact/wood/wood_solid_impact_bullet2.wav", 1.0, ATTN_NORM);	break;
	case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "impact/wood/wood_solid_impact_bullet3.wav", 1.0, ATTN_NORM);	break;
	case 3:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "impact/wood/wood_solid_impact_bullet4.wav", 1.0, ATTN_NORM);	break;
	case 4:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "impact/wood/wood_solid_impact_bullet5.wav", 1.0, ATTN_NORM);	break;
	}
}
// END

// WHEN SELECT FLAG PUSHABLE
void CBreakModel :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
	{
		if ( pev->spawnflags & SF_PUSHABLE )
			this->CBreakModel::Use( pActivator, pCaller, useType, value );
		return;
	}

	if ( pActivator->pev->velocity != g_vecZero )
		Move( pActivator, 0 );
}


void CBreakModel :: Touch( CBaseEntity *pOther )
{
	if ( FClassnameIs( pOther->pev, "worldspawn" ) )
		return;

	Move( pOther, 1 );
}


void CBreakModel :: Move( CBaseEntity *pOther, int push )
{
	entvars_t*	pevToucher = pOther->pev;
	int playerTouch = 0;

	
	if ( FBitSet(pevToucher->flags,FL_ONGROUND) && pevToucher->groundentity && VARS(pevToucher->groundentity) == pev )
	{
		
		if ( pev->waterlevel > 0 )
			pev->velocity.z += pevToucher->velocity.z * 0.1;

		return;
	}


	if ( pOther->IsPlayer() )
	{
		if ( push && !(pevToucher->button & (IN_FORWARD|IN_USE)) )	
			return;
		playerTouch = 1;
	}

	float factor;

	if ( playerTouch )
	{
		if ( !(pevToucher->flags & FL_ONGROUND) )	
		{
			if ( pev->waterlevel < 1 )
				return;
			else 
				factor = 0.1;
		}
		else
			factor = 1;
	}
	else 
		factor = 0.25;

	pev->velocity.x += pevToucher->velocity.x * factor;
	pev->velocity.y += pevToucher->velocity.y * factor;

	float length = sqrt( pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y );
	if ( push && (length > MaxSpeed()) )
	{
		pev->velocity.x = (pev->velocity.x * MaxSpeed() / length );
		pev->velocity.y = (pev->velocity.y * MaxSpeed() / length );
	}

	if ( playerTouch )
	{
		pevToucher->velocity.x = pev->velocity.x;
		pevToucher->velocity.y = pev->velocity.y;

// PLAY PUSH SOUND
		if ( (gpGlobals->time - m_soundTime) > 0.7 )
		{
			m_soundTime = gpGlobals->time;
			if ( length > 0 && FBitSet(pev->flags,FL_ONGROUND) )
			{
				m_lastSound = RANDOM_LONG(0,2);
		
    if ( pev->spawnflags & SF_PUSHABLE )

	switch (RANDOM_LONG(0,2))
	{
	case 0:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/pushbox1.wav", 1.0, ATTN_NORM);	break;
	case 1:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/pushbox2.wav", 1.0, ATTN_NORM);	break;
	case 2:	EMIT_SOUND(ENT(pev), CHAN_VOICE, "debris/pushbox3.wav", 1.0, ATTN_NORM);	break;
	}
			}
		}
	}
}
// END

// WHEN PLAYER NOT PUSH THE BOX KILL PUSH SOUND
void CBreakModel::StopSound( void )
{
	Vector dist = pev->oldorigin - pev->origin;
}
// END