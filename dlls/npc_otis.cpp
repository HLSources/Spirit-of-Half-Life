/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// monster_otis for SOHL 1.8 VC++ 2010
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"talkmonster.h"
#include	"schedule.h"
#include	"defaultai.h"
#include	"scripted.h"
#include	"weapons.h"
#include	"soundent.h"

extern DLL_GLOBAL int g_iSkillLevel;

//=========================================================
// monster-specific DEFINE's
//=========================================================
#define	OTIS_CLIP_SIZE 9
#define OTIS_VOL 0.40
#define OTIS_ATTN ATTN_NORM

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
enum { SCHED_OTIS_COVER_AND_RELOAD };

#define OTIS_NUM_HEADS	2

#define	OTIS_AE_DRAW		( 2 )
#define	OTIS_AE_SHOOT		( 3 )
#define	OTIS_AE_HOLSTER		( 4 )
#define	OTIS_AE_RELOAD		( 5 )

#define OT_GUN_GROUP		1
#define OT_GUN_HOLSTERED	0
#define OT_GUN_DRAWN 		1
#define OT_GUN_NONE			2

#define	OT_HEAD_GROUP		2
#define	OT_HEAD_NORMAL		0
#define OT_HEAD_BALD		1

//=========================================================
// monster-specific conditions
//=========================================================
class COtis : public CTalkMonster
{
	public:
		void Spawn( void );
		void Precache( void );
		void SetYawSpeed( void );
		int  ISoundMask( void );
		void Eagle( void );
		void AlertSound( void );
		int  Classify( void );
		void HandleAnimEvent( MonsterEvent_t *pEvent );
		void CheckAmmo( void );
	
		void RunTask( Task_t *pTask );
		void StartTask( Task_t *pTask );

		virtual int	ObjectCaps( void ) { return CTalkMonster :: ObjectCaps() | FCAP_IMPULSE_USE; }
		int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);

		BOOL CheckRangeAttack1 ( float flDot, float flDist );
		void KeyValue( KeyValueData *pkvd );
	
		void DeclineFollowing( void );

		// Override these to set behavior
		Schedule_t *GetScheduleOfType ( int Type );
		Schedule_t *GetSchedule ( void );
		MONSTERSTATE GetIdealState ( void );

		void DeathSound( void );
		void PainSound( void );
	
		void TalkInit( void );

		void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
		void Killed( entvars_t *pevAttacker, int iGib );
	
		virtual int	Save( CSave &save );
		virtual int	Restore( CRestore &restore );
		static	TYPEDESCRIPTION m_SaveData[];

		BOOL	m_fHostile;
		int		m_iBaseBody; //LRC - for barneys with different bodies
		BOOL	m_fGunDrawn;
		float	m_painTime;
		float	m_checkAttackTime;
		BOOL	m_lastAttackCheck;
		int		m_cClipSize;
		float	m_flPlayerDamage;// how much pain has the player inflicted on me?
		int		m_iHead;

		CUSTOM_SCHEDULES;
};

//=========================================================
// monster-specific schedule types
//=========================================================

LINK_ENTITY_TO_CLASS( monster_barney, COtis );

TYPEDESCRIPTION	COtis::m_SaveData[] = 
{
	DEFINE_FIELD( COtis, m_iBaseBody, FIELD_INTEGER ), //LRC
	DEFINE_FIELD( COtis, m_fGunDrawn, FIELD_BOOLEAN ),
	DEFINE_FIELD( COtis, m_painTime, FIELD_TIME ),
	DEFINE_FIELD( COtis, m_checkAttackTime, FIELD_TIME ),
	DEFINE_FIELD( COtis, m_lastAttackCheck, FIELD_BOOLEAN ),
	DEFINE_FIELD( COtis, m_flPlayerDamage, FIELD_FLOAT ),
	DEFINE_FIELD( COtis, m_cClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( COtis, m_iHead, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( COtis, CTalkMonster );


//=========================================================
// KeyValue
//
// !!! netname entvar field is used in squadmonster for groupname!!!
//=========================================================
void COtis :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "hostile"))
	{
		m_fHostile = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "head"))
	{
		m_iHead = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseMonster::KeyValue( pkvd );
	}
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
Task_t	tlOtisFollow[] =
{
	{ TASK_MOVE_TO_TARGET_RANGE,(float)128		},	// Move within 128 of target ent (client)
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_FACE },
};

Schedule_t	slOtisFollow[] =
{
	{
		tlOtisFollow,
		ARRAYSIZE ( tlOtisFollow ),
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"Follow"
	},
};

//=========================================================
// Otis reload schedule
//=========================================================
Task_t	tlOtisHideReload[] =
{
	{ TASK_STOP_MOVING,				(float)0					},
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_RELOAD			},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0					},
	{ TASK_RUN_PATH,				(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_REMEMBER,				(float)bits_MEMORY_INCOVER	},
	{ TASK_FACE_ENEMY,				(float)0					},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_RELOAD			},
};

Schedule_t slOtisHideReload[] = 
{
	{
		tlOtisHideReload,
		ARRAYSIZE ( tlOtisHideReload ),
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND,
		bits_SOUND_DANGER,
		"OtisHideReload"
	}
};

//=========================================================
// BarneyDraw- much better looking draw schedule for when
// barney knows who he's gonna attack.
//=========================================================
Task_t	tlOtisEnemyDraw[] =
{
	{ TASK_STOP_MOVING,					0				},
	{ TASK_FACE_ENEMY,					0				},
	{ TASK_PLAY_SEQUENCE_FACE_ENEMY,	(float) ACT_ARM },
};

Schedule_t slOtisEnemyDraw[] = 
{
	{
		tlOtisEnemyDraw,
		ARRAYSIZE ( tlOtisEnemyDraw ),
		0,
		0,
		"EnemyDraw"
	}
};

Task_t	tlOtisFaceTarget[] =
{
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_FACE_TARGET,			(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_SET_SCHEDULE,		(float)SCHED_TARGET_CHASE },
};

Schedule_t	slOtisFaceTarget[] =
{
	{
		tlOtisFaceTarget,
		ARRAYSIZE ( tlOtisFaceTarget ),
		bits_COND_CLIENT_PUSH	|
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND |
		bits_COND_PROVOKED,
		bits_SOUND_DANGER,
		"FaceTarget"
	},
};


Task_t	tlIdleOtisStand[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		}, // repick IDLESTAND every two seconds.
	{ TASK_TLK_HEADRESET,		(float)0		}, // reset head position
};

Schedule_t	slIdleOtisStand[] =
{
	{ 
		tlIdleOtisStand,
		ARRAYSIZE ( tlIdleOtisStand ), 
		bits_COND_NEW_ENEMY		|
		bits_COND_LIGHT_DAMAGE	|
		bits_COND_HEAVY_DAMAGE	|
		bits_COND_HEAR_SOUND	|
		bits_COND_SMELL			|
		bits_COND_PROVOKED,

		bits_SOUND_COMBAT		|// sound flags - change these, and you'll break the talking code.
		
		bits_SOUND_DANGER		|
		bits_SOUND_MEAT			|// scents
		bits_SOUND_CARCASS		|
		bits_SOUND_GARBAGE,
		"IdleStand"
	},
};

DEFINE_CUSTOM_SCHEDULES( COtis )
{
	slOtisFollow,
	slOtisEnemyDraw,
	slOtisFaceTarget,
	slIdleOtisStand,
	slOtisHideReload,
};

IMPLEMENT_CUSTOM_SCHEDULES( COtis, CTalkMonster );

void COtis :: StartTask( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
		default: 
			CTalkMonster::StartTask( pTask );
		break;
	}
}

void COtis :: RunTask( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_RANGE_ATTACK1:
			if (m_hEnemy != NULL && (m_hEnemy->IsPlayer()))
				pev->framerate = 1.5;

			CTalkMonster::RunTask( pTask );
		break;

		default:
			CTalkMonster::RunTask( pTask );
		break;
	}
}

//=========================================================
// ISoundMask - returns a bit mask indicating which types
// of sounds this monster regards. 
//=========================================================
int COtis :: ISoundMask ( void) 
{
	return	bits_SOUND_WORLD	|
			bits_SOUND_COMBAT	|
			bits_SOUND_CARCASS	|
			bits_SOUND_MEAT		|
			bits_SOUND_GARBAGE	|
			bits_SOUND_DANGER	|
			bits_SOUND_PLAYER;
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	COtis :: Classify ( void )
{
	if ( m_fHostile )
		return	CLASS_HUMAN_MILITARY;
	else
		return m_iClass?m_iClass:CLASS_PLAYER_ALLY;
}

//=========================================================
// ALertSound - barney says "Freeze!"
//=========================================================
void COtis :: AlertSound( void )
{
	if ( m_hEnemy != NULL )
	{
		if ( FOkToSpeak() )
		{
			if (m_iszSpeakAs)
			{
				char szBuf[32];
				strcpy(szBuf,STRING(m_iszSpeakAs));
				strcat(szBuf,"_ATTACK");
				PlaySentence( szBuf, RANDOM_FLOAT(2.8, 3.2), VOL_NORM, ATTN_IDLE );
			}
			else
				PlaySentence( "OT_ATTACK", RANDOM_FLOAT(2.8, 3.2), VOL_NORM, ATTN_IDLE );
		}
	}

}
//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void COtis :: SetYawSpeed ( void )
{
	int ys = 0;
	switch ( m_Activity )
	{
		case ACT_IDLE:		
			ys = 70;
		break;
		case ACT_WALK:
			ys = 70;
		break;
		case ACT_RUN:
			ys = 90;
		break;
		default:
			ys = 70;
		break;
	}

	pev->yaw_speed = ys;
}


//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL COtis :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( flDist <= 1024 && flDot >= 0.5 )
	{
		if ( gpGlobals->time > m_checkAttackTime )
		{
			TraceResult tr;
			
			Vector shootOrigin = pev->origin + Vector( 0, 0, 55 );
			CBaseEntity *pEnemy = m_hEnemy;
			Vector shootTarget = ( (pEnemy->BodyTarget( shootOrigin ) - pEnemy->pev->origin) + m_vecEnemyLKP );
			UTIL_TraceLine( shootOrigin, shootTarget, dont_ignore_monsters, ENT(pev), &tr );
			m_checkAttackTime = gpGlobals->time + 1;
			if ( tr.flFraction == 1.0 || (tr.pHit != NULL && CBaseEntity::Instance(tr.pHit) == pEnemy) )
				m_lastAttackCheck = TRUE;
			else
				m_lastAttackCheck = FALSE;
			m_checkAttackTime = gpGlobals->time + 1.5;
		}

		return m_lastAttackCheck;
	}

	return FALSE;
}

//=========================================================
// BarneyFirePistol - shoots one round from the pistol at
// the enemy barney is facing.
//=========================================================
void COtis :: Eagle ( void )
{
	Vector vecShootOrigin;

	UTIL_MakeVectors(pev->angles);
	vecShootOrigin = pev->origin + Vector( 0, 0, 55 );
	Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

	Vector angDir = UTIL_VecToAngles( vecShootDir );
	SetBlending( 0, angDir.x );
	pev->effects = EF_MUZZLEFLASH;

	int pitchShift = RANDOM_LONG( 0, 20 );
	// Only shift about half the time
	if ( pitchShift > 10 )
		pitchShift = 0;
	else
		pitchShift -= 5;

	FireBullets(1, vecShootOrigin, vecShootDir, VECTOR_CONE_2DEGREES, 1024, BULLET_PLAYER_357);
	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/desert_eagle_fire.wav", 1, ATTN_NORM, 0, 100 + pitchShift );
	WeaponFlash( vecShootOrigin ); //Monster WeaponFlash

	CSoundEnt::InsertSound ( bits_SOUND_COMBAT, pev->origin, 384, 0.3 );
	WeaponFlash( vecShootOrigin );

	m_cAmmoLoaded--;// take away a bullet!
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void COtis :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case OTIS_AE_SHOOT:
			Eagle();
		break;

		case OTIS_AE_DRAW:
			SetBodygroup( OT_GUN_GROUP, OT_GUN_DRAWN );
			m_fGunDrawn = TRUE;
		break;

		case OTIS_AE_HOLSTER:
			SetBodygroup( OT_GUN_GROUP, OT_GUN_HOLSTERED );
			m_fGunDrawn = FALSE;
		break;

		case OTIS_AE_RELOAD:
			EMIT_SOUND( ENT(pev), CHAN_WEAPON, "weapons/desert_eagle_reload.wav", 1, ATTN_NORM );
			m_cAmmoLoaded = m_cClipSize;
			ClearConditions(bits_COND_NO_AMMO_LOADED);
		break;

		default:
			ALERT ( at_debug, "EVENT: %s", pEvent->event );
			CTalkMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// Spawn
//=========================================================
void COtis :: Spawn()
{
	Precache( );

	if (pev->model)
		SET_MODEL(ENT(pev), STRING(pev->model)); //LRC
	else
		SET_MODEL(ENT(pev), "models/otis.mdl");

	UTIL_SetSize(pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX);

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;

	if (pev->health == 0) //LRC
		pev->health			= gSkillData.otisHealth;

	pev->view_ofs		= Vector ( 0, 0, 50 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_WIDE; // NOTE: we need a wide field of view so npc will notice player and say hello
	m_MonsterState		= MONSTERSTATE_NONE;

	m_iBaseBody = pev->body; //LRC
	SetBodygroup( OT_GUN_GROUP, OT_GUN_HOLSTERED );

	if (RANDOM_LONG( 0, 99 ) < 80)
		pev->skin = 0;	// light skin
	else
		pev->skin = 1;	// dark skin

    if ( m_iBaseBody == -1 )
		m_iBaseBody = RANDOM_LONG(0, OTIS_NUM_HEADS-1);// pick a head, any head

	m_fGunDrawn			= FALSE;
	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;
	m_cClipSize			= OTIS_CLIP_SIZE;
	m_cAmmoLoaded		= m_cClipSize;

	SetUse(&COtis :: FollowerUse );
	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void COtis :: Precache()
{
	if (pev->model)
		PRECACHE_MODEL((char*)STRING(pev->model)); //LRC
	else
		PRECACHE_MODEL("models/otis.mdl");

	PRECACHE_SOUND("weapons/desert_eagle_fire.wav" );
	PRECACHE_SOUND("weapons/desert_eagle_reload.wav");

	PRECACHE_SOUND("barney/ba_pain1.wav");
	PRECACHE_SOUND("barney/ba_pain2.wav");
	PRECACHE_SOUND("barney/ba_pain3.wav");

	PRECACHE_SOUND("barney/ba_die1.wav");
	PRECACHE_SOUND("barney/ba_die2.wav");
	PRECACHE_SOUND("barney/ba_die3.wav");
	
	TalkInit();
	CTalkMonster::Precache();
}	

// Init talk data
void COtis :: TalkInit()
{
	CTalkMonster::TalkInit();

	m_szFriends[0] = "monster_barney";
	m_szFriends[1] = "monster_scientist";
	m_szFriends[2] = "monster_sitting_scientist";
	m_szFriends[3] = "monster_otis";

	if (!m_iszSpeakAs)
	{
		m_szGrp[TLK_ANSWER]		=	"OT_ANSWER";
		m_szGrp[TLK_QUESTION]	=	"OT_QUESTION";
		m_szGrp[TLK_IDLE]		=	"OT_IDLE";
		m_szGrp[TLK_STARE]		=	"OT_STARE";

		if (pev->spawnflags & SF_MONSTER_PREDISASTER) //LRC
			m_szGrp[TLK_USE]	=	"BA_PFOLLOW";
		else
			m_szGrp[TLK_USE] =	"OT_OK";
		if (pev->spawnflags & SF_MONSTER_PREDISASTER)
			m_szGrp[TLK_UNUSE] = "OT_PWAIT";
		else
			m_szGrp[TLK_UNUSE] = "OT_WAIT";
		if (pev->spawnflags & SF_MONSTER_PREDISASTER)
			m_szGrp[TLK_DECLINE] =	"OT_POK";
		else
			m_szGrp[TLK_DECLINE] =	"OT_NOTOK";

		m_szGrp[TLK_STOP] =		"OT_STOP";

		m_szGrp[TLK_NOSHOOT] =	"OT_SCARED";
		m_szGrp[TLK_HELLO] =	"OT_HELLO";

		m_szGrp[TLK_PLHURT1] =	"!OT_CUREA";
		m_szGrp[TLK_PLHURT2] =	"!OT_CUREB"; 
		m_szGrp[TLK_PLHURT3] =	"!OT_CUREC";

		m_szGrp[TLK_PHELLO] =	NULL;
		m_szGrp[TLK_PIDLE] =	NULL;
		m_szGrp[TLK_PQUESTION] = "OT_PQUEST";

		m_szGrp[TLK_SMELL] =	"OT_SMELL";
	
		m_szGrp[TLK_WOUND] =	"OT_WOUND";
		m_szGrp[TLK_MORTAL] =	"OT_MORTAL";
	}

	// get voice for head - just one barney voice for now
	m_voicePitch = 100;
}

static BOOL IsFacing( entvars_t *pevTest, const Vector &reference )
{
	Vector vecDir = (reference - pevTest->origin);
	vecDir.z = 0;
	vecDir = vecDir.Normalize();
	Vector forward, angle;
	angle = pevTest->v_angle;
	angle.x = 0;
	UTIL_MakeVectorsPrivate( angle, forward, NULL, NULL );

	// He's facing me, he meant it
	if ( DotProduct( forward, vecDir ) > 0.96 )	// +/- 15 degrees or so
		return TRUE;
	else
		return FALSE;
}

int COtis :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	// make sure friends talk about it if player hurts talkmonsters...
	int ret = CTalkMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	if ( !IsAlive() || pev->deadflag == DEAD_DYING )
		return ret;

	// LRC - if my reaction to the player has been overridden, don't do this stuff
	if (m_iPlayerReact) return ret;

	if ( m_MonsterState != MONSTERSTATE_PRONE && (pevAttacker->flags & FL_CLIENT) )
	{
		m_flPlayerDamage += flDamage;

		// This is a heurstic to determine if the player intended to harm me
		// If I have an enemy, we can't establish intent (may just be crossfire)
		if ( m_hEnemy == NULL )
		{
			// If the player was facing directly at me, or I'm already suspicious, get mad
			if ( (m_afMemory & bits_MEMORY_SUSPICIOUS) || IsFacing( pevAttacker, pev->origin ) )
			{
				// Alright, now I'm pissed!
				if (m_iszSpeakAs)
				{
					char szBuf[32];
					strcpy(szBuf,STRING(m_iszSpeakAs));
					strcat(szBuf,"_MAD");
					PlaySentence( szBuf, 4, VOL_NORM, ATTN_NORM );
				}
				else
				{
					PlaySentence( "OT_MAD", 4, VOL_NORM, ATTN_NORM );
				}

				Remember( bits_MEMORY_PROVOKED );
				StopFollowing( TRUE );
			}
			else
			{
				// Hey, be careful with that
				if (m_iszSpeakAs)
				{
					char szBuf[32];
					strcpy(szBuf,STRING(m_iszSpeakAs));
					strcat(szBuf,"_SHOT");
					PlaySentence( szBuf, 4, VOL_NORM, ATTN_NORM );
				}
				else
				{
					PlaySentence( "OT_SHOT", 4, VOL_NORM, ATTN_NORM );
				}
				Remember( bits_MEMORY_SUSPICIOUS );
			}
		}
		else if ( !(m_hEnemy->IsPlayer()) && pev->deadflag == DEAD_NO )
		{
			if (m_iszSpeakAs)
			{
				char szBuf[32];
				strcpy(szBuf,STRING(m_iszSpeakAs));
				strcat(szBuf,"_SHOT");
				PlaySentence( szBuf, 4, VOL_NORM, ATTN_NORM );
			}
			else
			{
				PlaySentence( "OT_SHOT", 4, VOL_NORM, ATTN_NORM );
			}
		}
	}

	return ret;
}

//=========================================================
// PainSound
//=========================================================
void COtis :: PainSound ( void )
{
	if (gpGlobals->time < m_painTime)
		return;
	
	m_painTime = gpGlobals->time + RANDOM_FLOAT(0.5, 0.75);

	switch (RANDOM_LONG(0,2))
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_pain3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}

//=========================================================
// DeathSound 
//=========================================================
void COtis :: DeathSound ( void )
{
	switch (RANDOM_LONG(0,2))
	{
		case 0: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die1.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
		case 1: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die2.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
		case 2: EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "barney/ba_die3.wav", 1, ATTN_NORM, 0, GetVoicePitch()); break;
	}
}

void COtis::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	switch( ptr->iHitgroup)
	{
		case HITGROUP_CHEST:
		case HITGROUP_STOMACH:
			if (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_BLAST))
				flDamage = flDamage / 2;
		break;
		case 10:
			if (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_CLUB))
			{
				flDamage -= 20;
				if (flDamage <= 0)
				{
					UTIL_Ricochet( ptr->vecEndPos, 1.0 );
					flDamage = 0.01;
				}
			}

			ptr->iHitgroup = HITGROUP_HEAD;
		break;
	}

	CTalkMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

void COtis::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( GetBodygroup( 1 ) != 2 && !(pev->spawnflags & SF_MONSTER_NO_WPN_DROP))
	{
		Vector vecGunPos;
		Vector vecGunAngles;

		SetBodygroup( OT_GUN_GROUP, OT_GUN_NONE );
		GetAttachment( 0, vecGunPos, vecGunAngles );
		CBaseEntity * pGun = DropItem( "weapon_357", vecGunPos, vecGunAngles );
	}

	SetUse( NULL );	
	CTalkMonster::Killed( pevAttacker, iGib );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

Schedule_t* COtis :: GetScheduleOfType ( int Type )
{
	Schedule_t *psched;

	switch( Type )
	{
		case SCHED_ARM_WEAPON:
			if ( m_hEnemy != NULL )
				return slOtisEnemyDraw; // face enemy, then draw.
		break;
		case SCHED_OTIS_COVER_AND_RELOAD:
			return &slOtisHideReload[ 0 ];
		break;
		case SCHED_TARGET_FACE: // Hook these to make a looping schedule
			psched = CTalkMonster::GetScheduleOfType(Type);

			if (psched == slIdleStand)
				return slOtisFaceTarget; // override this for different target face behavior
			else
				return psched;
		break;
		case SCHED_TARGET_CHASE:
			return slOtisFollow;
		break;
		case SCHED_IDLE_STAND:
			psched = CTalkMonster::GetScheduleOfType(Type);

			if (psched == slIdleStand)
				return slIdleOtisStand;
			else
				return psched;
		break;
	}

	return CTalkMonster::GetScheduleOfType( Type );
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *COtis :: GetSchedule ( void )
{
	if ( HasConditions( bits_COND_HEAR_SOUND ) )
	{
		CSound *pSound;
		pSound = PBestSound();

		ASSERT( pSound != NULL );
		if ( pSound && (pSound->m_iType & bits_SOUND_DANGER) )
			return GetScheduleOfType( SCHED_TAKE_COVER_FROM_BEST_SOUND );
	}

	if ( HasConditions( bits_COND_ENEMY_DEAD ) && FOkToSpeak() )
	{
		// Hey, be careful with that
		if (m_iszSpeakAs)
		{
			char szBuf[32];
			strcpy(szBuf,STRING(m_iszSpeakAs));
			strcat(szBuf,"_KILL");
			PlaySentence( szBuf, 4, VOL_NORM, ATTN_NORM );
		}
		else
		{
			PlaySentence( "OT_KILL", 4, VOL_NORM, ATTN_NORM );
		}
	}

	switch( m_MonsterState )
	{
		case MONSTERSTATE_COMBAT:
		{
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
				return CBaseMonster :: GetSchedule();

			// always act surprized with a new enemy
			if ( HasConditions( bits_COND_NEW_ENEMY ) && HasConditions( bits_COND_LIGHT_DAMAGE) )
				return GetScheduleOfType( SCHED_SMALL_FLINCH );
				
			// wait for one schedule to draw gun
			if (!m_fGunDrawn )
				return GetScheduleOfType( SCHED_ARM_WEAPON );

			if ( HasConditions( bits_COND_HEAVY_DAMAGE ) )
				return GetScheduleOfType( SCHED_TAKE_COVER_FROM_ENEMY );

			if ( HasConditions ( bits_COND_NO_AMMO_LOADED ) )
				return GetScheduleOfType ( SCHED_OTIS_COVER_AND_RELOAD );
		}
		break;
		case MONSTERSTATE_ALERT:
		case MONSTERSTATE_IDLE:
		if ( HasConditions(bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
			return GetScheduleOfType( SCHED_SMALL_FLINCH );

		if ( m_hEnemy == NULL && IsFollowing() )
		{
			if ( !m_hTargetEnt->IsAlive() )
			{
				// UNDONE: Comment about the recently dead player here?
				StopFollowing( FALSE );
				break;
			}
			else
			{
				if ( HasConditions( bits_COND_CLIENT_PUSH ) )
					return GetScheduleOfType( SCHED_MOVE_AWAY_FOLLOW );
				else
					return GetScheduleOfType( SCHED_TARGET_FACE );
			}
		}

		if ( HasConditions( bits_COND_CLIENT_PUSH ) )
			return GetScheduleOfType( SCHED_MOVE_AWAY );

		// try to say something about smells
		TrySmellTalk();
		break;
	}
	
	return CTalkMonster::GetSchedule();
}

MONSTERSTATE COtis :: GetIdealState ( void )
{
	return CTalkMonster::GetIdealState();
}

void COtis::DeclineFollowing( void )
{
	if ( IsAlive() )
		PlaySentence( m_szGrp[TLK_DECLINE], 2, VOL_NORM, ATTN_NORM ); //LRC
}

//=========================================================
// CheckAmmo - overridden for the grunt because he actually
// uses ammo! (base class doesn't)
//=========================================================
void COtis :: CheckAmmo ( void )
{
	if ( m_cAmmoLoaded <= 0 )
		SetConditions(bits_COND_NO_AMMO_LOADED);
}

//=========================================================
// DEAD OTIS PROP
//
// Designer selects a pose in worldcraft, 0 through num_poses-1
// this value is added to what is selected as the 'first dead pose'
// among the monster's normal animations. All dead poses must
// appear sequentially in the model file. Be sure and set
// the m_iFirstPose properly!
//
//=========================================================
class CDeadOtis : public CBaseMonster
{
	public:
		void Spawn( void );
		int	Classify ( void ) { return	CLASS_PLAYER_ALLY; }

		void KeyValue( KeyValueData *pkvd );

		int	m_iPose;// which sequence to display	-- temporary, don't need to save
		static char *m_szPoses[3];
};

char *CDeadOtis::m_szPoses[] = { "lying_on_back", "lying_on_side", "lying_on_stomach"};

void CDeadOtis::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else 
		CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_otis_dead, CDeadOtis );

//=========================================================
// ********** DeadBarney SPAWN **********
//=========================================================
void CDeadOtis :: Spawn( )
{
	PRECACHE_MODEL("models/otis.mdl");
	SET_MODEL(ENT(pev), "models/otis.mdl");

	pev->effects		= 0;
	pev->yaw_speed		= 8;
	pev->sequence		= 0;
	m_bloodColor		= BLOOD_COLOR_RED;

	pev->sequence = LookupSequence( m_szPoses[m_iPose] );

	if (pev->sequence == -1)
		ALERT ( at_debug, "Dead Otis with bad pose\n" );

	// Corpses have less health
	pev->health			= 20;

	MonsterInitDead();
}