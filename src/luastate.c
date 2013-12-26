/*
*# [Lua](http://www.lua.org/home.html) is a popular scripting language for games. 
*# Through Rengine's Lua State you get access to a Lua interpreter, with most of 
*# Rengine's functionality exposed.
*#
*# Links:
*{
** The [Lua Homepage](http://www.lua.org/home.html)
** A great introduction to Lua is the book [Programming In Lua](http://www.lua.org/pil/contents.html)
*}
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#ifdef WIN32
#include <SDL.h>
#else
#include <SDL/SDL.h>
#endif

#include "bmp.h"
#include "states.h"
#include "tileset.h"
#include "map.h"
#include "game.h"
#include "ini.h"
#include "resources.h"
#include "utils.h"
#include "particles.h"
#include "log.h"

#define MAX_TIMEOUTS 20

/*
These are the Lua scripts in the ../scripts/ directory.
The script files are converted to C code strings using the Bace utility.
These strings are then executed through the Lua interpreter to provide
a variety of built in Lua code.
*/
extern const char base_lua[];
extern size_t base_lua_len;

/* Don't tamper with this variable from your Lua scripts. */
#define STATE_DATA_VAR	"___state_data"

struct _update_function {
	int ref;
	struct _update_function *next;
};

struct lustate_data {
	
	struct game_state *state;
	
	struct bitmap *bmp;
	struct map *map;	

	struct _timeout_element {
		int fun;
		int time;
		Uint32 start;
	} timeout[MAX_TIMEOUTS];
	int n_timeout;
	
	struct _update_function *update_fcn;
	
	int change_state;
	char *next_state;
};

/* LUA FUNCTIONS */

static struct lustate_data *get_state_data(lua_State *L) {
	struct lustate_data *sd;
	lua_getglobal(L, STATE_DATA_VAR);
	if(!lua_islightuserdata(L, -1)) {
		luaL_error(L, "Variable %s got tampered with.", STATE_DATA_VAR);
		return 0; /* satisfy the compiler */
	} else {
		sd = lua_touserdata(L, -1);
	}
	lua_pop(L, 1);
	return sd;
}

/*1 Global functions 
 *# These functions have global scope in the Lua script.
 */

/*@ log(message)
 *# Writes a message to Rengine's log file.
 */
static int l_log(lua_State *L) {
	const char * s = lua_tolstring(L, 1, NULL);
	if(s) {
		sublog("Lua", "%s", s);
	}
	return 0;
}

/*@ setTimeout(func, millis)
 *# Waits for {{millis}} milliseconds, then calls {{func}}
 */
static int l_set_timeout(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);

	/* This link was useful: 
	http://stackoverflow.com/questions/2688040/how-to-callback-a-lua-function-from-a-c-function 
	*/
	if(lua_gettop(L) == 2 && lua_isfunction(L, -2) && lua_isnumber(L, -1)) {
		
		if(sd->n_timeout == MAX_TIMEOUTS) {
			luaL_error(L, "Maximum number of timeouts [%d] reached", MAX_TIMEOUTS);
		}
		
		/* Push the callback function on to the top of the stack */
		lua_pushvalue(L, -2);
		
		/* And create a reference to it in the special LUA_REGISTRYINDEX */
		sd->timeout[sd->n_timeout].fun = luaL_ref(L, LUA_REGISTRYINDEX);
		
		sd->timeout[sd->n_timeout].time = luaL_checkinteger(L, 2);		
		sd->timeout[sd->n_timeout].start = SDL_GetTicks();
		
		sd->n_timeout++;
		
	} else {
		luaL_error(L, "setTimeout() requires a function and a time as parameters");
	}
	
	return 0;
}

static void process_timeouts(lua_State *L) {
	struct lustate_data *sd;
	int i = 0;
	
	lua_getglobal(L, STATE_DATA_VAR);
	if(!lua_islightuserdata(L, -1)) {
		/* Can't call LuaL_error here. */
		rerror("Variable %s got tampered with.", STATE_DATA_VAR);
		return;
	} else {
		sd = lua_touserdata(L, -1);
	}
	lua_pop(L, 1);
	
	while(i < sd->n_timeout) {
		Uint32 elapsed = SDL_GetTicks() - sd->timeout[i].start;
		if(elapsed > sd->timeout[i].time) {			
			/* Retrieve the callback */
			lua_rawgeti(L, LUA_REGISTRYINDEX, sd->timeout[i].fun);
			
			/* Call it */
			if(lua_pcall(L, 0, 0, 0)) {
				rerror("Unable to execute setTimeout() callback: %s", lua_tostring(L, -1));				
			}
			/* Release the reference so that it can be collected */
			luaL_unref(L, LUA_REGISTRYINDEX, sd->timeout[i].fun);
			
			/* Now delete this timeout by replacing it with the last one */
			sd->timeout[i] = sd->timeout[--sd->n_timeout];
		} else {
			i++;
		}
	}
}

/*@ onUpdate(func)
 *# Registers the function {{func}} to be called every frame
 */
static int l_onUpdate(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);

	if(lua_gettop(L) == 1 && lua_isfunction(L, -1)) {
		struct _update_function *fn;
				
		fn = malloc(sizeof *fn);
		if(!fn)
			luaL_error(L, "Out of memory");
		
		/* And create a reference to it in the special LUA_REGISTRYINDEX */
		fn->ref = luaL_ref(L, LUA_REGISTRYINDEX);		
		rlog("Registering onUpdate() callback %d", fn->ref);
		
		fn->next = sd->update_fcn;
		sd->update_fcn = fn;
		
		lua_pushinteger(L, fn->ref);
		
	} else {
		luaL_error(L, "onUpdate() requires a function as a parameter");
	}
	
	return 1;
}

/*1 Game object
 *# Functions in the {{Game}} scope
 */

/*@ Game.changeState(newstate)
 *# Changes the game's [[state|State Machine]] to the state identified by {{newstate}}
 */
static int l_changeState(lua_State *L) {
	const char *next_state = luaL_checkstring(L, -1);
	struct lustate_data *sd = get_state_data(L);
	
	sd->next_state = strdup(next_state);
	sd->change_state = 1;
	
	return 0;
}

/*@ Game.createParticle(x,y, dx,dy, life, color)
 *# Creates a particle at position (x,y), with color {{color}} moving in direction (dx,dy) every frame.
 *# The particle {{life}} lasts for frames (So if the game's FPS is 33, then a {{life}}
 *# value of 66 will mean the the particle will last for 2 seconds)
 */
static int l_createParticle(lua_State *L) {
	float x = luaL_checknumber(L, -6);
	float y = luaL_checknumber(L, -5);
	float dx = luaL_checknumber(L, -4);
	float dy = luaL_checknumber(L, -3);
	int life = luaL_checkinteger(L, -2); 
	int color = bm_color_atoi(luaL_checkstring(L, -1));	
	add_particle(x, y, dx, dy, life, color);
	return 0;
}

/*@ Game.getStyle(style)
 *# Retrieves a specific [[Style]] from the [[game.ini]] file.
 */
static int l_getstyle(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	const char * s = luaL_checkstring(L,1);
	lua_pushstring(L, get_style(sd->state, s));
	return 1;
}

static const luaL_Reg game_funcs[] = {
  {"changeState",     l_changeState},
  {"getStyle",        l_getstyle},
  {"createParticle",  l_createParticle},
  {0, 0}
};

/*1 BmpObj
 *# The Bitmap object encapsulates a bitmap in the engine.
 *# Instances of BmpObj are drawn to the screen with the [[G.blit()|Lua-state#gblitbmp-dx-dy-sx-sy-w-h]] function
 */

/*@ Bmp(filename)
 *# Loads the bitmap file specified by {{filename}} from the
 *# [[Resources|Resource Management]] and returns it
 *# encapsulated within a `BmpObj` instance.
 */
static int new_bmp_obj(lua_State *L) {
	const char *filename = luaL_checkstring(L,1);
	
	struct bitmap **bp = lua_newuserdata(L, sizeof *bp);	
	luaL_setmetatable(L, "BmpObj");
	
	*bp = re_get_bmp(filename);
	if(!*bp) {
		luaL_error(L, "Unable to load bitmap '%s'", filename);
	}
	return 1;
}

/*@ BmpObj:__tostring()
 *# Returns a string representation of the `BmpObj` instance.
 */
static int bmp_tostring(lua_State *L) {	
	struct bitmap **bp = luaL_checkudata(L,1, "BmpObj");
	struct bitmap *b = *bp;
	lua_pushfstring(L, "BmpObj[%dx%d]", b->w, b->h);
	return 1;
}

/*@ BmpObj:__gc()
 *# Garbage collects the `BmpObj` instance.
 */
static int gc_bmp_obj(lua_State *L) {
	/* No need to free the bitmap: It's in the resource cache. */
	return 0;
}

/*@ BmpObj:setMask(color)
 *# Sets the color used as a mask when the bitmap is drawn to the screen.
 */
static int bmp_set_mask(lua_State *L) {	
	struct bitmap **bp = luaL_checkudata(L,1, "BmpObj");
	const char *mask = luaL_checkstring(L, 2);
	bm_set_color_s(*bp, mask);
	return 0;
}

static void bmp_obj_meta(lua_State *L) {
	/* Create the metatable for MyObj */
	luaL_newmetatable(L, "BmpObj");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index"); /* BmpObj.__index = BmpObj */
	
	/* Add methods */
	lua_pushcfunction(L, bmp_set_mask);
	lua_setfield(L, -2, "setMask");
	
	lua_pushcfunction(L, bmp_tostring);
	lua_setfield(L, -2, "__tostring");	
	lua_pushcfunction(L, gc_bmp_obj);
	lua_setfield(L, -2, "__gc");	
	
	/* The global method Bmp() */
	lua_pushcfunction(L, new_bmp_obj);
	lua_setglobal(L, "Bmp");
}

/*1 CellObj
 *# The Cell object.
 *# The CellObj encapsulates a collection of cells on the 
 *# map. You then use the methods of the CellObj to manipulate
 *# the cells.
 *#
 *# You use the [[C(selector)|Lua-state#cselector]] function
 *# to create a CellObj instance that collects all the cells on 
 *# the maps that match the specific selector.
 */

typedef struct _cell_obj {
	struct map_cell *cell;
	struct _cell_obj *next;
} cell_obj;

static int class_selector(struct map_cell *c, const char *data) {
	if(!c->clas) return 0;
	return !my_stricmp(c->clas, data + 1);
}

static int id_selector(struct map_cell *c, const char *data) {
	if(!c->id) return 0;
	return !my_stricmp(c->id, data);
}

/*@ C(selector)
 *# Returns a `CellObj` instance that encapsulates all the
 *# cells on a map that matches the particular selector.
 */
static int new_cell_obj(lua_State *L) {
	const char *selector;
	int i;
	int (*sel_fun)(struct map_cell *c, const char *data);
	struct lustate_data *sd;
	cell_obj **o;

	o = lua_newuserdata(L, sizeof *o);	
	luaL_setmetatable(L, "CellObj");
	*o = NULL;
	
	lua_getglobal(L, STATE_DATA_VAR);	
	if(!lua_islightuserdata(L, -1)) {
		/* Don't ever do anything to ___the_map in your Lua scripts */
		luaL_error(L, "Variable %s got tampered with.", STATE_DATA_VAR);
	}
	sd = lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	if(!sd->map) {
		/* This Lua state does not have a map, so return an empty list */
		return 1;		
	}
	
	selector = luaL_checkstring(L,1);
	if(selector[0] == '.') {
		sel_fun = class_selector;
	} else {		
		sel_fun = id_selector;
	}
	
	/* Is there a non-O(n) way to find cells matching a selector? 
	 * I ought to put the ids in a hashtable at least
	 */
	for(i = 0; i < sd->map->nr * sd->map->nc; i++) {
		struct map_cell *c = &sd->map->cells[i];
		if(sel_fun(c, selector)) {
			cell_obj *co = malloc(sizeof *co);
			co->cell = c;
			co->next = *o;				
			*o = co;
			if(sel_fun == id_selector)
				break;
		}
	}
	
	return 1;
}

/*@ CellObj:__tostring()
 *# Returns a string representation of the CellObj
 */
static int cell_tostring(lua_State *L) {	
	cell_obj **os = luaL_checkudata(L,1, "CellObj");
	cell_obj *o = *os;
	int count = 0;
	while(o) {
		count++;
		o=o->next;
	}
	lua_pushfstring(L, "CellObj[%d]", count);
	return 1;
}

/*@ CellObj:__gc()
 *# Frees the CellObj when it is garbage collected.
 */
static int gc_cell_obj(lua_State *L) {
	cell_obj **o = luaL_checkudata(L,1, "CellObj");
	while(*o) {
		cell_obj *t = *o;
		*o = (*o)->next;		
		free(t);
	}
	return 0;
}

/*@ CellObj:set(layer, si, ti)
 *# Sets the specific layer of the cells encapsulated in the CellObj to 
 *# the {{si,ti}} value, where
 *{
 ** {{si}} is the Set Index
 ** {{ti}} is the Tile Index
 *}
 */
static int cell_set(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	cell_obj **os = luaL_checkudata(L,1, "CellObj");
	cell_obj *o;
	int l = luaL_checkint(L,2);
	int si = luaL_checkint(L,3);
	int ti = luaL_checkint(L,4);
	
	if(l < 0 || l > 2) {
		lua_pushfstring(L, "Invalid level passed to CellObj.set()");
		lua_error(L);
	}
	
	assert(sd->map); /* cant create a CellObj if this is false. */
	if(si < 0 || si >= ts_get_num(&sd->map->tiles)) {
		lua_pushfstring(L, "Invalid si passed to CellObj.set()");
		lua_error(L);
	}
	/* FIXME: error checking on ti? */
	
	o = *os;
	while(o) {
		struct map_cell *c = o->cell;
		/* TODO: Maybe you ought to store
		this change in some sort of list
		so that the savedgames can handle 
		changes to the map like this. */
		c->tiles[l].ti = ti;
		c->tiles[l].si = si;
		o = o->next;
	}
	
	/* Push the CellObj back onto the stack so that other methods can be called on it */
	lua_pushvalue(L, -4);
	
	return 1;
}

/*
	I may have been doing this wrong. See http://lua-users.org/wiki/UserDataWithPointerExample
*/
static void cell_obj_meta(lua_State *L) {
	/* Create the metatable for MyObj */
	luaL_newmetatable(L, "CellObj");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index"); /* CellObj.__index = CellObj */
	
	/* FIXME: Add other methods. */
	lua_pushcfunction(L, cell_set);
	lua_setfield(L, -2, "set");
	
	lua_pushcfunction(L, cell_tostring);
	lua_setfield(L, -2, "__tostring");	
	lua_pushcfunction(L, gc_cell_obj);
	lua_setfield(L, -2, "__gc");	
	
	/* The global method C() */
	lua_pushcfunction(L, new_cell_obj);
	lua_setglobal(L, "C");
}

/*1 G
 *# {{G}} is the Graphics object that allows you to draw primitives on the screen. \n
 *# You can only call these functions when the screen is being drawn. That's to say, you can only call then inside 
 *# functions registered through {{onUpdate()}}
 *# If you call them elsewhere, you will get the error {/"Call to graphics function outside of a screen update"/}
 *# 
 *# These fields are also available:
 *{
 ** {{G.FPS}} - The configured frames per second of the game (see [[game.ini]]).
 ** {{G.SCREEN_WIDTH}} - The configured width of the screen.
 ** {{G.SCREEN_HEIGHT}} - The configured height of the screen.
 *}
 */

/* 
The "Call to graphics function outside of a screen update" error happens because
sd->bmp is not yet set when the script is first executed in the state's initialisation
function (lus_init()).
*/

/*@ G.setColor(color)
 *# Sets the color used to draw the graphics primitives.
 */
static int gr_setcolor(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");	
	if(lua_gettop(L) > 0) {
		const char *c = luaL_checkstring(L,1);
		bm_set_color_s(sd->bmp, c);
	} else {
		bm_set_color_s(sd->bmp, get_style(sd->state, "forEGrouND"));
	}
	return 0;
}

/*@ G.pixel(x,y)
 *# Plots a pixel at {{x,y}} on the screen.
 */
static int gr_putpixel(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	int x = luaL_checkint(L,1);
	int y = luaL_checkint(L,2);
	bm_putpixel(sd->bmp, x, y);
	return 0;
}

/*@ G.line(x0, y0, x1, y1)
 *# Draws a line from {{x0,y0}} to {{x1,y1}}
 */
static int gr_line(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	
	int x0 = luaL_checkint(L,1);
	int y0 = luaL_checkint(L,2);
	int x1 = luaL_checkint(L,3);
	int y1 = luaL_checkint(L,4);
	bm_line(sd->bmp, x0, y0, x1, y1);
	return 0;
}

/*@ G.rect(x0, y0, x1, y1)
 *# Draws a rectangle from {{x0,y0}} to {{x1,y1}}
 */
static int gr_rect(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	int x0 = luaL_checkint(L,1);
	int y0 = luaL_checkint(L,2);
	int x1 = luaL_checkint(L,3);
	int y1 = luaL_checkint(L,4);
	bm_rect(sd->bmp, x0, y0, x1, y1);
	return 0;
}

/*@ G.fillRect(x0, y0, x1, y1)
 *# Draws a filled rectangle from {{x0,y0}} to {{x1,y1}}
 */
static int gr_fillrect(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	int x0 = luaL_checkint(L,1);
	int y0 = luaL_checkint(L,2);
	int x1 = luaL_checkint(L,3);
	int y1 = luaL_checkint(L,4);
	bm_fillrect(sd->bmp, x0, y0, x1, y1);
	return 0;
}

/*@ G.circle(x, y, r)
 *# Draws a circle centered at {{x,y}} with radius {{r}}
 */
static int gr_circle(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	int x = luaL_checkint(L,1);
	int y = luaL_checkint(L,2);
	int r = luaL_checkint(L,3);
	bm_circle(sd->bmp, x, y, r);
	return 0;
}

/*@ G.fillCircle(x, y, r)
 *# Draws a filled circle centered at {{x,y}} with radius {{r}}
 */
static int gr_fillcircle(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	int x = luaL_checkint(L,1);
	int y = luaL_checkint(L,2);
	int r = luaL_checkint(L,3);
	bm_fillcircle(sd->bmp, x, y, r);
	return 0;
}

/*@ G.ellipse(x0, y0, x1, y1)
 *# Draws an ellipse from {{x0,y0}} to {{x1,y1}}
 */
static int gr_ellipse(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	int x0 = luaL_checkint(L,1);
	int y0 = luaL_checkint(L,2);
	int x1 = luaL_checkint(L,3);
	int y1 = luaL_checkint(L,4);
	bm_ellipse(sd->bmp, x0, y0, x1, y1);
	return 0;
}

/*@ G.roundRect(x0, y0, x1, y1, r)
 *# Draws a rectangle from {{x0,y0}} to {{x1,y1}}
 *# with rounded corners of radius {{r}}
 */
static int gr_roundrect(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	int x0 = luaL_checkint(L,1);
	int y0 = luaL_checkint(L,2);
	int x1 = luaL_checkint(L,3);
	int y1 = luaL_checkint(L,4);
	int r = luaL_checkint(L,5);
	bm_roundrect(sd->bmp, x0, y0, x1, y1, r);
	return 0;
}

/*@ G.fillRoundRect(x0, y0, x1, y1, r)
 *# Draws a rectangle from {{x0,y0}} to {{x1,y1}}
 *# with rounded corners of radius {{r}}
 */
static int gr_fillroundrect(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	int x0 = luaL_checkint(L,1);
	int y0 = luaL_checkint(L,2);
	int x1 = luaL_checkint(L,3);
	int y1 = luaL_checkint(L,4);
	int r = luaL_checkint(L,5);
	bm_fillroundrect(sd->bmp, x0, y0, x1, y1, r);
	return 0;
}

/*@ G.curve(x0, y0, x1, y1, x2, y2)
 *# Draws a Bezier curve from {{x0,y0}} to {{x2,y2}}
 *# with {{x1,y1}} as the control point.
 *# Note that it doesn't pass through {{x1,y1}}
 */
static int gr_bezier3(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	int x0 = luaL_checkint(L,1);
	int y0 = luaL_checkint(L,2);
	int x1 = luaL_checkint(L,3);
	int y1 = luaL_checkint(L,4);
	int x2 = luaL_checkint(L,5);
	int y2 = luaL_checkint(L,6);
	bm_bezier3(sd->bmp, x0, y0, x1, y1, x2, y2);
	return 0;
}

/*@ G.lerp(col1, col2, frac)
 *# Returns a [[color|Colors]] that is some fraction {{frac}} along 
 *# the line from {{col1}} to {{col2}}.
 *# For example, `G.lerp("red", "blue", 0.33)` will return a color
 *# that is 1/3rd of the way from red to blue.
 */
static int gr_lerp(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	const char *c1 = luaL_checkstring(L,1);
	const char *c2 = luaL_checkstring(L,2);
	lua_Number v = luaL_checknumber(L,3);	
	int col = bm_lerp(bm_color_atoi(c1), bm_color_atoi(c2), v);
	bm_set_color_i(sd->bmp, col);
	
	lua_pushinteger(L, col);
	return 1;
}

/*@ G.setFont(font)
 *# Sets the [[font|Fonts]] used for the {{G.print()}} function. 
 */
static int gr_setfont(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	enum bm_fonts font;	
	if(lua_gettop(L) > 0) {
		const char *name = luaL_checkstring(L,1);
		font = bm_font_index(name);
	} else {
		font = bm_font_index(get_style(sd->state, "font"));
	}
	bm_std_font(sd->bmp, font);	
	return 0;
}

/*@ G.print(x,y,text)
 *# Prints the {{text}} to the screen, with its top left position at {{x,y}}.
 */
static int gr_print(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);
	const char *s = luaL_checkstring(L, 3);
	
	bm_puts(sd->bmp, x, y, s);
	
	return 0;
}

/*@ G.textDims(text)
 *# Returns the width,height in pixels that the {{text}}
 *# will occupy on the screen.
 *X local w,h = G.textDims(message);
 */
static int gr_textdims(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	const char *s = luaL_checkstring(L, 1);
	
	lua_pushinteger(L, bm_text_width(sd->bmp, s));
	lua_pushinteger(L, bm_text_height(sd->bmp, s));
	
	return 2;
}

/*@ G.blit(bmp, dx, dy, [sx], [sy], [w], [h])
 *# Draws an instance {{bmp}} of {{BmpObj}} to the screen at {{dx, dy}}.
 *# {{sx,sy}} specify the source x,y position and {{w,h}} specifies the
 *# width and height of the source to draw.
 *# {{sx,sy}} defaults to {{0,0}} and {{w,h}} defaults to the entire 
 *# source bitmap.
 */
static int gr_blit(lua_State *L) {
	struct lustate_data *sd = get_state_data(L);
	if(!sd->bmp)
		luaL_error(L, "Call to graphics function outside of a screen update");
	struct bitmap **bp = luaL_checkudata(L, 1, "BmpObj");
	
	int dx = luaL_checkinteger(L, 2);
	int dy = luaL_checkinteger(L, 3);
	
	int sx = 0, sy = 0, w = (*bp)->w, h = (*bp)->h;
	
	if(lua_gettop(L) >= 4)
		sx = luaL_checkinteger(L, 4);
	if(lua_gettop(L) >= 5)
		sy = luaL_checkinteger(L, 5);
	if(lua_gettop(L) >= 6)
		w = luaL_checkinteger(L, 6);
	if(lua_gettop(L) >= 7)
		h = luaL_checkinteger(L, 7);
	
	bm_maskedblit(sd->bmp, dx, dy, *bp, sx, sy, w, h);
	
	return 0;
}

static const luaL_Reg graphics_funcs[] = {
  {"setColor",      gr_setcolor},
  {"pixel",         gr_putpixel},
  {"line",          gr_line},
  {"rect",          gr_rect},
  {"fillRect",      gr_fillrect},
  {"circle",        gr_circle},
  {"fillCircle",    gr_fillcircle},
  {"ellipse",    	gr_ellipse},
  {"roundRect",    	gr_roundrect},
  {"fillRoundRect", gr_fillroundrect},
  {"curve",         gr_bezier3},
  {"lerp",          gr_lerp},
  {"print",         gr_print},
  {"setFont",       gr_setfont},
  {"textDims",      gr_textdims},
  {"blit",          gr_blit},
  {0, 0}
};

/*1 Keyboard
 *# Keyboard Input Functions */

/*@ Keyboard.down([key])
 *# Checks whether a key is down on the keyboard.
 *# The parameter {{key}} is the name of specific key.
 *# See http://wiki.libsdl.org/SDL_Scancode for the names of all the possible keys.
 *# If {{key}} is omitted, the function returns true if _any_ key is down.
 */
static int in_kbhit(lua_State *L) {	
	if(lua_gettop(L) > 0) {
		const char *name = luaL_checkstring(L, 1);
		/* Names of the keys are listed on
			http://wiki.libsdl.org/SDL_Scancode
		*/
		lua_pushboolean(L, keys[SDL_GetScancodeFromName(name)]);
	} else {
		lua_pushboolean(L, kb_hit());
	}	
	return 1;
}

/*@ Keyboard.reset()
 *# Resets the keyboard input.
 */
static int in_reset_keys(lua_State *L) {	
	reset_keys();
	return 0;
}

static const luaL_Reg keyboard_funcs[] = {
  {"down",   in_kbhit},
  {"reset",  in_reset_keys},
  {0, 0}
};

/*1 Mouse
 *# Mouse input functions.
 *#
 *# These constants are used with {{Mouse.down()}} and {{Mouse.click()}}
 *# to identify specific mouse buttons:
 *{
 ** {{Mouse.LEFT}}
 ** {{Mouse.MIDDLE}}
 ** {{Mouse.RIGHT}}
 *}
 */

/*@ Mouse.position()
 *# Returns the {{x,y}} position of the mouse.
 *X local x,y = Mouse.position();
 */
static int in_mousepos(lua_State *L) {	
	lua_pushinteger(L, mouse_x);
	lua_pushinteger(L, mouse_y);
	return 2;
}

/*@ Mouse.down(btn)
 *# Returns true if the button {{btn}} is down.
 */
static int in_mousedown(lua_State *L) {	
	int btn = luaL_checkinteger(L, 1);
	lua_pushboolean(L, mouse_btns & SDL_BUTTON(btn));
	return 1;
}

/*@ Mouse.click(btn)
 *# Returns true if the button {{btn}} was clicked.
 *# A button is considered clicked if it was down the
 *# previous frame and is not anymore this frame.
 */
static int in_mouseclick(lua_State *L) {	
	int btn = luaL_checkinteger(L, 1);
	lua_pushboolean(L, mouse_clck & SDL_BUTTON(btn));
	return 1;
}

static const luaL_Reg mouse_funcs[] = {
  {"position",  in_mousepos},
  {"down",  in_mousedown},
  {"click",  in_mouseclick},
  {0, 0}
};

/* STATE FUNCTIONS */

#define GLOBAL_FUNCTION(name, fun)	lua_pushcfunction(L, fun);	lua_setglobal(L, name);
#define SET_TABLE_INT_VAL(k, v) lua_pushstring(L, k); lua_pushinteger(L, v); lua_rawset(L, -3);

static int lus_init(struct game_state *s) {
	
	const char *map_file, *script_file;
	char *map_text, *script;
	lua_State *L = NULL;
	struct lustate_data *sd;
		
	rlog("Initializing Lua state '%s'", s->name);
	
	/* Load the Lua script */
	script_file = ini_get(game_ini, s->name, "script", NULL);	
	if(!script_file) {
		rerror("Lua state '%s' doesn't specify a script file.", s->name);
		return 0;
	}
	script = re_get_script(script_file);
	if(!script) {
		rerror("Script %s was not found (state %s).", script_file, s->name);
		return 0;
	}
	
	/* Create the Lua interpreter */
	L = luaL_newstate();
	if(!L) { 
		rerror("Couldn't create Lua state.");
		return 0;
	}
	s->data = L;
	luaL_openlibs(L);
	
	/* Create and init the State Data that the interpreter carries with it. */
	sd = malloc(sizeof *sd);
	if(!sd)
		return 0;
	
	sd->state = s;	
	sd->update_fcn = NULL;	
	sd->n_timeout = 0;
	sd->map = NULL;
	sd->bmp = NULL;
		
	sd->change_state = 0;
	sd->next_state = NULL;
	
	/* Store the State Data in the interpreter */
	lua_pushlightuserdata(L, sd);		
	lua_setglobal(L, STATE_DATA_VAR);
	
	/* Load the map, if one is specified. */
	map_file = ini_get(game_ini, s->name, "map", NULL);
	if(map_file) {
		map_text = re_get_script(map_file);
		if(!map_text) {
			rerror("Unable to retrieve map resource '%s' (state %s).", map_file, s->name);
			return 0;		
		}
		
		sd->map = map_parse(map_text);
		if(!sd->map) {
			rerror("Unable to parse map '%s' (state %s).", map_file, s->name);
			return 0;		
		}
		free(map_text);
	} else {
		rlog("Lua state %s does not specify a map file.", s->name);
	}
	
	GLOBAL_FUNCTION("log", l_log);
	GLOBAL_FUNCTION("setTimeout", l_set_timeout);
	GLOBAL_FUNCTION("onUpdate", l_onUpdate);
	
	luaL_newlib(L, game_funcs);
	/* Register some Lua variables. */	
	SET_TABLE_INT_VAL("BACKGROUND", 0);
	SET_TABLE_INT_VAL("CENTER", 1);
	SET_TABLE_INT_VAL("FOREGROUND", 2);
	lua_setglobal(L, "Game");
	
	/* The graphics object G gives you access to all the 2D drawing functions */
	luaL_newlib(L, graphics_funcs);
	SET_TABLE_INT_VAL("FPS", fps);
	SET_TABLE_INT_VAL("SCREEN_WIDTH", 0);
	SET_TABLE_INT_VAL("SCREEN_HEIGHT", 0);
	lua_setglobal(L, "G");
	
	/* The input object Input gives you access to the keyboard and mouse. */
	luaL_newlib(L, keyboard_funcs);
	lua_setglobal(L, "Keyboard");
	
	luaL_newlib(L, mouse_funcs);
	SET_TABLE_INT_VAL("LEFT", 1);
	SET_TABLE_INT_VAL("MIDDLE", 2);
	SET_TABLE_INT_VAL("RIGHT", 3);
	lua_setglobal(L, "Mouse");
	
	/* The Bitmap object is constructed through the Bmp() function that loads 
		a bitmap through the resources module that can be drawn with G.blit() */
	bmp_obj_meta(L);
	
	/* There is a function C('selector') that returns a CellObj object that
		gives you access to the cells on the map. */
	cell_obj_meta(L);
	
	if(luaL_dostring(L, base_lua)) {
		rerror("Unable load base library.");
		sublog("lua", "%s", lua_tostring(L, -1));
		free(script);				
		return 0;
	}
	
	/* Load the Lua script itself, and execute it. */
	if(luaL_loadstring(L, script)) {		
		rerror("Unable to load script %s (state %s).", script_file, s->name);
		sublog("lua", "%s", lua_tostring(L, -1));
		free(script);				
		return 0;
	}
	free(script);
	
	rlog("Running script %s", script_file);	
	if(lua_pcall(L, 0, 0, 0)) {
		rerror("Unable to execute script %s (state %s).", script_file, s->name);
		sublog("lua", "%s", lua_tostring(L, -1));
		return 0;
	}
		
	return 1;
}

static int lus_update(struct game_state *s, struct bitmap *bmp) {
	int i;
	struct lustate_data *sd = NULL;
	lua_State *L = s->data;
	struct _update_function *fn;
	
	assert(L);
	
	lua_getglobal(L, STATE_DATA_VAR);
	if(!lua_isnil(L,-1)) {
		if(!lua_islightuserdata(L, -1)) {
			rerror("Variable %s got tampered with (lus_update)", STATE_DATA_VAR);
			return 0;
		}
		sd = lua_touserdata(L, -1);
	} else {
		rerror("Variable %s got tampered with (lus_update)", STATE_DATA_VAR);
		return 0;
	}
	lua_pop(L, 1);
	
	sd->bmp = bmp;
	
	lua_getglobal (L, "G");
	SET_TABLE_INT_VAL("SCREEN_WIDTH", bmp->w);
	SET_TABLE_INT_VAL("SCREEN_HEIGHT", bmp->h);
	
	/* TODO: Maybe background colour metadata in the map file? */
	bm_set_color_s(bmp, "black");
	bm_clear(bmp);
	
	process_timeouts(L);
	
	fn = sd->update_fcn;
	while(fn) {				
		/* Retrieve the callback */
		lua_rawgeti(L, LUA_REGISTRYINDEX, fn->ref);
		
		/* Call it */
		if(lua_pcall(L, 0, 0, 0)) {
			rerror("Unable to execute onUpdate() callback (%d)", fn->ref);
			sublog("lua", "%s", lua_tostring(L, -1));
			/* Should we remove it maybe? */
		}
		
		fn = fn->next;
	}
	
	if(sd->map) {
		for(i = 0; i < 3; i++)
			map_render(sd->map, bmp, i, 0, 0);
	}
	
	if(sd->change_state) {
		if(!sd->next_state) {
			rwarn("Lua script didn't specify a next state; terminating...");
			change_state(NULL);
		} else {	
			rlog("Lua script changing state to %s", sd->next_state);
			set_state(sd->next_state);
		}
	}
	
	return 1;
}

static int lus_deinit(struct game_state *s) {
	lua_State *L = s->data;
	struct lustate_data *sd;
	
	if(!L)
		return 0;
	
	lua_getglobal(L, STATE_DATA_VAR);
	if(!lua_isnil(L,-1)) {
		if(!lua_islightuserdata(L, -1)) {
			rerror("Variable %s got tampered with (lus_deinit)", STATE_DATA_VAR);
		} else {
			struct _update_function *fn;
			
			sd = lua_touserdata(L, -1);
			map_free(sd->map);
			
			while(sd->update_fcn) {
				fn = sd->update_fcn;
				sd->update_fcn = sd->update_fcn->next;
				free(fn);
			}
			
			if(sd->next_state);
				free(sd->next_state);
			
			free(sd);
		}
	}
	lua_pop(L, 1);
	
	lua_close(L);
	L = NULL;
	
	return 1;
}

struct game_state *get_lua_state(const char *name) {
	struct game_state *state = malloc(sizeof *state);
	if(!state)
		return NULL;
	
	state->data = NULL;
	
	state->init = lus_init;
	state->update = lus_update;
	state->deinit = lus_deinit;
	
	return state;
}