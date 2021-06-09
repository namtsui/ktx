//========================================================================
//
//  Copyright (C)  2020 - 2021	Samuel Piper
//
//  This code is free software; you can redistribute it and/or modify
//  it under the terms of the GNU GPL (General Public License); either
//  version 2 of the License, or (at your option) any later version.
//
//========================================================================
#include "g_local.h"
#include "fb_globals.h"

int ANTILAG_MEMPOOL_WORLDSEEK;
antilag_t ANTILAG_MEMPOOL[ANTILAG_MAXEDICTS];
antilag_t *antilag_list_players;
antilag_t *antilag_list_world;
float antilag_nextthink_world;

void antilag_log(gedict_t *e, antilag_t *antilag)
{
	// stop extremely fast logging
	if (g_globalvars.time - antilag->rewind_time[antilag->rewind_seek] < 0.01)
		return;

	antilag->rewind_seek++;

	if (antilag->rewind_seek >= ANTILAG_MAXSTATES)
		antilag->rewind_seek = 0;

	VectorCopy(e->s.v.origin, antilag->rewind_origin[antilag->rewind_seek]);
	VectorCopy(e->s.v.velocity, antilag->rewind_velocity[antilag->rewind_seek]);
	antilag->rewind_time[antilag->rewind_seek] = g_globalvars.time;

	if ((int) e->s.v.flags & FL_ONGROUND)
		antilag->rewind_platform_edict[antilag->rewind_seek] = e->s.v.groundentity;
	else
		antilag->rewind_platform_edict[antilag->rewind_seek] = 0;
}

antilag_t *antilag_create_player(gedict_t *e)
{
	antilag_t *new_datastruct = &ANTILAG_MEMPOOL[NUM_FOR_EDICT(e)];
	memset(new_datastruct, 0, sizeof(antilag_t));
	new_datastruct->prev = NULL;
	new_datastruct->next = NULL;
	new_datastruct->owner = e;

	if (antilag_list_players != NULL)
	{
		new_datastruct->next = antilag_list_players;
		antilag_list_players->prev = new_datastruct;
	}

	antilag_list_players = new_datastruct;

	return new_datastruct;
}

void antilag_delete_player(gedict_t *e)
{
	antilag_t *data = e->antilag_data;
	if (data->prev != NULL)
	{
		data->prev->next = data->next;
	}
	else if (antilag_list_players == data)
	{
		antilag_list_players = data->next;
	}

	if (data->next != NULL)
		data->next->prev = data->prev;
}

antilag_t *antilag_create_world(gedict_t *e)
{
	antilag_t *new_datastruct = &ANTILAG_MEMPOOL[64 + ANTILAG_MEMPOOL_WORLDSEEK];
	memset(new_datastruct, 0, sizeof(antilag_t));
	ANTILAG_MEMPOOL_WORLDSEEK++;

	new_datastruct->prev = NULL;
	new_datastruct->next = NULL;
	new_datastruct->owner = e;

	if (antilag_list_world != NULL)
	{
		new_datastruct->next = antilag_list_world;
		antilag_list_world->prev = new_datastruct;
	}

	antilag_list_world = new_datastruct;

	return new_datastruct;
}

void antilag_delete_world(gedict_t *e)
{
	antilag_t *data = e->antilag_data;
	if (data->prev != NULL)
	{
		data->prev->next = data->next;
	}
	else if (antilag_list_world == data)
	{
		antilag_list_world = data->next;
	}

	if (data->next != NULL)
		data->next->prev = data->prev;
}

void antilag_updateworld()
{
	if (g_globalvars.time < antilag_nextthink_world)
		return;

	antilag_nextthink_world = g_globalvars.time + 0.05;
	
	antilag_t *list;
	for (list = antilag_list_world; list != NULL; list = list->next)
	{
		antilag_log(list->owner, list);
	}
}

void antilag_lagmove(antilag_t *data, float ms)
{
	float goal_time = g_globalvars.time - ms;

	int old_seek = data->rewind_seek;
	int seek = data->rewind_seek - 1;
	if (seek < 0)
		seek = ANTILAG_MAXSTATES - 1;

	float seek_time = data->rewind_time[seek];
	while (seek != data->rewind_seek && seek_time > goal_time)
	{
		old_seek = seek;
		seek--;
		if (seek < 0)
			seek = ANTILAG_MAXSTATES - 1;
		seek_time = data->rewind_time[seek];
	}

	float under_time = data->rewind_time[old_seek];
	float over_time = data->rewind_time[seek];
	float frac = (goal_time - over_time) / (under_time - over_time);

	gedict_t *owner = data->owner;

	vec3_t lerp_origin;
	if (frac < 1)
	{
		vec3_t diff;
		VectorSubtract(data->rewind_origin[old_seek], data->rewind_origin[seek], diff);
		VectorScale(diff, frac, diff);
		VectorAdd(data->rewind_origin[seek], diff, lerp_origin);
	}
	else
	{
		VectorCopy(owner->s.v.origin, lerp_origin);
	}

	trap_setorigin(NUM_FOR_EDICT(owner), PASSVEC3(lerp_origin));
}

vec3_t antilag_origin;

void antilag_getorigin(antilag_t *data, float ms)
{
	float goal_time = g_globalvars.time - ms;

	int old_seek = data->rewind_seek;
	int seek = data->rewind_seek - 1;
	if (seek < 0)
		seek = ANTILAG_MAXSTATES - 1;

	float seek_time = data->rewind_time[seek];
	while (seek != data->rewind_seek && seek_time > goal_time)
	{
		old_seek = seek;
		seek--;
		if (seek < 0)
			seek = ANTILAG_MAXSTATES - 1;
		seek_time = data->rewind_time[seek];
	}

	float under_time = data->rewind_time[old_seek];
	float over_time = data->rewind_time[seek];
	float frac = (goal_time - over_time) / (under_time - over_time);

	gedict_t *owner = data->owner;

	vec3_t lerp_origin;
	if (frac < 1)
	{
		vec3_t diff;
		VectorSubtract(data->rewind_origin[old_seek], data->rewind_origin[seek], diff);
		VectorScale(diff, frac, diff);
		VectorAdd(data->rewind_origin[seek], diff, lerp_origin);
	}
	else
	{
		VectorCopy(owner->s.v.origin, lerp_origin);
	}

	VectorCopy(lerp_origin, antilag_origin);
}

int antilag_getseek(antilag_t *data, float ms)
{
	float goal_time = g_globalvars.time - ms;

	int seek = data->rewind_seek - 1;
	if (seek < 0)
		seek = ANTILAG_MAXSTATES - 1;

	float seek_time = data->rewind_time[seek];
	while (seek != data->rewind_seek && seek_time > goal_time)
	{
		seek--;
		if (seek < 0)
			seek = ANTILAG_MAXSTATES - 1;
		seek_time = data->rewind_time[seek];
	}

	return seek;
}

void antilag_lagmove_all(gedict_t *e, float ms)
{
	antilag_t *list;
	for (list = antilag_list_players; list != NULL; list = list->next)
	{
		if (list->owner->s.v.health <= 0)
			continue;

		VectorCopy(list->owner->s.v.origin, list->held_origin);
		VectorCopy(list->owner->s.v.velocity, list->held_velocity);

		if (list->owner == e)
		{
			int lag_platform = list->rewind_platform_edict[antilag_getseek(list, ms)];
			if (lag_platform)
			{
				gedict_t *plat = PROG_TO_EDICT(lag_platform);
				if (plat->antilag_data != NULL)
				{
					vec3_t diff;
					VectorClear(diff);
					antilag_getorigin(plat->antilag_data, ms);
					VectorSubtract(antilag_origin, plat->s.v.origin, diff);

					vec3_t org;
					VectorAdd(e->s.v.origin, diff, org);

					trap_setorigin(NUM_FOR_EDICT(e), PASSVEC3(org));
				}
			}
			continue;
		}

		antilag_lagmove(list, ms);
	}

	for (list = antilag_list_world; list != NULL; list = list->next)
	{
		VectorCopy(list->owner->s.v.origin, list->held_origin);
		VectorCopy(list->owner->s.v.velocity, list->held_velocity);
		antilag_lagmove(list, ms);
	}
}

void antilag_lagmove_all_nohold(gedict_t *e, float ms, int plat_rewind)
{
	antilag_t *list;
	for (list = antilag_list_players; list != NULL; list = list->next)
	{
		if (list->owner->s.v.health <= 0)
			continue;

		if (list->owner == e)
		{
			if (plat_rewind)
			{
				int lag_platform = list->rewind_platform_edict[antilag_getseek(list, ms)];
				if (lag_platform)
				{
					gedict_t *plat = PROG_TO_EDICT(lag_platform);
					if (plat->antilag_data != NULL)
					{
						vec3_t diff;
						VectorClear(diff);
						antilag_getorigin(plat->antilag_data, ms);
						VectorSubtract(antilag_origin, plat->s.v.origin, diff);

						vec3_t org;
						VectorAdd(e->s.v.origin, diff, org);

						trap_setorigin(NUM_FOR_EDICT(e), PASSVEC3(org));
					}
				}
			}
			continue;
		}

		antilag_lagmove(list, ms);
	}

	for (list = antilag_list_world; list != NULL; list = list->next)
	{
		antilag_lagmove(list, ms);
	}
}

void antilag_unmove_all()
{
	if (cvar("sv_antilag") != 1)
		return;

	antilag_t *list;
	for (list = antilag_list_players; list != NULL; list = list->next)
	{
		if (list->owner->s.v.health <= 0)
			continue;

		trap_setorigin(NUM_FOR_EDICT(list->owner), PASSVEC3(list->held_origin));
	}

	for (list = antilag_list_world; list != NULL; list = list->next)
	{
		trap_setorigin(NUM_FOR_EDICT(list->owner), PASSVEC3(list->held_origin));
	}
}

void antilag_lagmove_all_hitscan(gedict_t *e)
{
	if (cvar("sv_antilag") != 1)
		return;

	float ms = (atof(ezinfokey(e, "ping")) / 1000) - 0.013;

	if (ms > ANTILAG_REWIND_MAXHITSCAN)
		ms = ANTILAG_REWIND_MAXHITSCAN;
	else if (ms < 0)
		ms = 0;

	antilag_lagmove_all(e, ms);
}

void antilag_lagmove_all_proj(gedict_t *owner, gedict_t *e, void touch_func())
{
	if (cvar("sv_antilag") != 1)
		return;

	float ms = (atof(ezinfokey(owner, "ping")) / 1000) - 0.013;

	if (ms > ANTILAG_REWIND_MAXPROJECTILE)
		ms = ANTILAG_REWIND_MAXPROJECTILE;
	else if (ms < 0)
		ms = 0;

	// log hold stats, because we use nohold antilag moving
	antilag_t *list;
	for (list = antilag_list_players; list != NULL; list = list->next)
	{
		if (list->owner->s.v.health <= 0)
			continue;

		VectorCopy(list->owner->s.v.origin, list->held_origin);
		VectorCopy(list->owner->s.v.velocity, list->held_velocity);
	}

	for (list = antilag_list_world; list != NULL; list = list->next)
	{
		VectorCopy(list->owner->s.v.origin, list->held_origin);
		VectorCopy(list->owner->s.v.velocity, list->held_velocity);
	}

	vec3_t old_org;
	VectorCopy(owner->s.v.origin, old_org);
	antilag_lagmove_all_nohold(owner, ms, true);
	VectorSubtract(owner->s.v.origin, old_org, old_org);
	VectorAdd(e->s.v.origin, old_org, old_org);
	trap_setorigin(NUM_FOR_EDICT(e), PASSVEC3(old_org));
	VectorCopy(e->s.v.origin, e->oldangles); // store for later maybe
	e->s.v.health = ms;

	gedict_t *oself = self;

	float step_time = min(0.05, ms);
	float current_time = g_globalvars.time - ms;
	while (current_time < g_globalvars.time)
	{
		step_time = bound(0.01, min(step_time, (g_globalvars.time - current_time) - 0.01), 0.05);
		traceline(PASSVEC3(e->s.v.origin), e->s.v.origin[0] + e->s.v.velocity[0] * step_time,
			e->s.v.origin[1] + e->s.v.velocity[1] * step_time, e->s.v.origin[2] + e->s.v.velocity[2] * step_time,
			false, e);

		antilag_lagmove_all_nohold(owner, (g_globalvars.time - current_time), false);
		trap_setorigin(NUM_FOR_EDICT(e), PASSVEC3(g_globalvars.trace_endpos));

		if (g_globalvars.trace_fraction < 1)
		{
			//if (g_globalvars.trace_ent)
			//{
			other = PROG_TO_EDICT(g_globalvars.trace_ent);
			self = e;
			self->s.v.flags = ((int)self->s.v.flags) | FL_GODMODE;
			touch_func();
			break;
			//}
		}

		current_time += step_time;
	}

	self = oself;

	// restore origins to held values
	antilag_unmove_all();
}












