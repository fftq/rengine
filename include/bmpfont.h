/*1 bmpfont.h
 *# Wrapper around FreeType (http://www.freetype.org/) to allow rendering of 
 *# freetype-supported fonts on {{Bitmap}}s from my bitmap module in {{bmp.h}}.\n
 *# All functions in this module is prefixed by {{bmf_}} for clarity.\n
 *# In order to use this, you must have something like the following in your
 *# Makefile:
 *[
 *# CFLAGS = `freetype-config --cflags`
 *# LDFLAGS = `freetype-config --libs` -lpng -lz
 *]
 *2 License
 *[
 *# Author: Werner Stoop
 *# This is free and unencumbered software released into the public domain.
 *# http://unlicense.org/
 *]
 *2 API 
 */

/*@ int bmf_init()
 *# Initializes the FreeType wrapper.\n
 *# Returns 1 on success, 0 on failure.
 */
int bmf_init();

/*@ void bmf_deinit()
 *# Deinitializes the FreeType wrapper and deletes all cached fonts.
 */
void bmf_deinit();

/*@ BmFont *bmf_load_font(const char *file, const char *name)
 *# Loads a font in the file specified by the {{file}} parameter.\n
 *# Returns {{NULL}} on failure.
 */
BmFont *bmf_load_font(const char *file);

#ifdef USESDL
/*@ BmFont *bmf_load_font_rw(SDL_RWops *rw, const char *name)
 *# If you're using SDL2, you can use the {{SDL_RWops}} mechanisms
 *# to load a font using this function.\n
 *# The {{name}} parameter is used for caching the font.\n
 *# Returns {{NULL}} on failure.
 */
BmFont *bmf_load_font_rw(SDL_RWops *rw, const char *name);
#endif

/*@ int bmf_set_size(BmfFont *font, int px)
 *# Sets the {{size}} of a font in pixels.\n
 *# Returns 1 on success, 0 on failure.
 */
int bmf_set_size(BmFont *font, int px);
