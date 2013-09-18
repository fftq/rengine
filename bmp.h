/*! bmp.h
 *# Low-level routines to manipulate BMP files.\n
 *# At the moment, only 24-bit bitmap files are supported.
 *# 
 *@ References:
 *{
 ** http://en.wikipedia.org/wiki/BMP_file_format
 ** http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
 ** http://members.chello.at/~easyfilter/bresenham.html
 ** http://en.wikipedia.org/wiki/Flood_fill
 ** http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
 ** http://web.archive.org/web/20110706093850/http://free.pages.at/easyfilter/bresenham.html
 ** http://damieng.com/blog/2011/02/20/typography-in-8-bits-system-fonts
 *} 
 *=
 *[
 *# Author: Werner Stoop
 *# This is free and unencumbered software released into the public domain.
 *# http://unlicense.org/
 *]
 *=
 */
 
#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif
 
/*@ struct bitmap
 *# Structure containing a bitmap image.
 */
struct bitmap {	
	/* Dimesions of the bitmap */
	int w, h;	
	
	/* The actual pixel data */
	unsigned char *data;
		
	/* Color for the pen, of the canvas */
	int r,g,b;
	
	/* XBM font. See font.xbm */
	const unsigned char *font;
	
	/* The width of eac character in the font */
	int font_spacing;
};

/*@ struct bitmap *bm_create(int w, int h)
 *# Creates a bitmap of the specified dimensions
 */
struct bitmap *bm_create(int w, int h);

/*@ void bm_free(struct bitmap *b)
 *# Destroys a bitmap previously created with bm_create()
 */
void bm_free(struct bitmap *b);

/*@ struct bitmap *bm_load(const char *filename)
 *# Loads the file {{filename}} into a bitmap structure.\n
 *# Returns NULL if the file could not be loaded.
 */
struct bitmap *bm_load(const char *filename);

/*@ struct bitmap *bm_load_fp(FILE *f)
 *# Loads the a bitmap from a FILE* that's already open.\n
 *# Returns NULL if the file could not be loaded.
 */
struct bitmap *bm_load_fp(FILE *f);

/*@ int bm_write(struct bitmap *b, const char *fname)
 *# Saves the bitmap b to the file named fname 
 */
int bm_save(struct bitmap *b, const char *fname);

/*@ struct bitmap *bm_copy(struct bitmap *b)
 *# Creates a duplicate of the bitmap structure 
 */
/* THIS FUNCTION IS NOT PROPERLY TESTED YET */
struct bitmap *bm_copy(struct bitmap *b);

/*@ unsigned char bm_getr(struct bitmap *b, int x, int y)
 *# Sets a pixel at x,y in the bitmap b to the specified R,G,B color
 */ 
void bm_set(struct bitmap *b, int x, int y, unsigned char R, unsigned char G, unsigned char B);

/*@ unsigned char bm_getr(struct bitmap *b, int x, int y)
 *# Retrieves the R value of the pixel at x,y in bitmap b 
 */
unsigned char bm_getr(struct bitmap *b, int x, int y);

/*@ unsigned char bm_getg(struct bitmap *b, int x, int y)
 *# Retrieves the G value of the pixel at x,y in bitmap b 
 */
unsigned char bm_getg(struct bitmap *b, int x, int y);

/*@ unsigned char bm_getb(struct bitmap *b, int x, int y)
 *# Retrieves the B value of the pixel at x,y in bitmap b 
 */
unsigned char bm_getb(struct bitmap *b, int x, int y);

/*@ void bm_set_color(struct bitmap *bc, int r, int g, int b)
 *# Sets the colour of the pen to (r,g,b)
 */
void bm_set_color(struct bitmap *bm, int r, int g, int b);

/*@ void bm_set_color_s(struct bitmap *bm, const char *text)
 *# Sets the colour of the pen to a colour represented by text.
 *# The text can be in the HTML format, like #RRGGBB or one of
 *# a variety of colour names, eg.: "white", "black", "red".
 */
void bm_set_color_s(struct bitmap *bm, const char *text);

/*@ int bm_color_atoi(const char *text)
 *# Converts a text string like "#FF00FF" or "white" to
 *# an integer of the form 0xFF00FF.
 *# (It is used internally by {{bm_set_color_s()}})
 */
int bm_color_atoi(const char *text);

/*@ void bm_set_color_i(struct bitmap *bm, int col)
 *# Sets the colour of the pen to a colour represented 
 *# by an integer, like 0x00RRGGBB
 */
void bm_set_color_i(struct bitmap *bm, int col);

/*@ void bm_get_color(struct bitmap *bc, int *r, int *g, int *b)
 *# Retrieves the pen colour into &r, &g and &b.
 */
void bm_get_color(struct bitmap *bm, int *r, int *g, int *b);

/*@ void bm_picker(struct bitmap *bm, int x, int y)
 *# Sets the colour of the pen to the colour of the pixel at <x,y>
 *# on the bitmap.
 *# The pen colour can then be retrieved through {{bm_get_color()}}.
 */
void bm_picker(struct bitmap *bm, int x, int y);

/*@ int bm_color_is(struct bitmap *bm, int x, int y, int r, int g, int b) 
 *# Returns 1 if the colour at <x,y> on the bitmap is (r,g,b), 0 otherwise
 */
int bm_color_is(struct bitmap *bm, int x, int y, int r, int g, int b); 

/*@ int bm_gradient(int color1, int color2, double t)
 *# Computes the color that is a distance {{t}} along the line between 
 *# color1 and color2.\n
 *# If {{t}} is 0 it returns color1. If t is 1.0 it returns color2.
 */
int bm_gradient(int color1, int color2, double t);

/*@ struct bitmap *bm_fromXbm(int w, int h, unsigned char *data)
 *# Creates a struct bitmap object from XBM data 
 */
struct bitmap *bm_fromXbm(int w, int h, unsigned char *data);

/*@ void bm_blit(struct bitmap *dst, int dx, int dy, struct bitmap *src, int sx, int sy, int w, int h)
 *# Blits an area of w*h pixels at sx,sy on the src bitmap to 
 *# dx,dy on the dst bitmap.
 */
void bm_blit(struct bitmap *dst, int dx, int dy, struct bitmap *src, int sx, int sy, int w, int h);

/*@ void bm_maskedblit(struct bitmap *dst, int dx, int dy, struct bitmap *src, int sx, int sy, int w, int h)
 *# Blits an area of w*h pixels at sx,sy on the src bitmap to 
 *# dx,dy on the dst bitmap.
 *# Pixels on the src bitmap that matches the src bitmap colour are not blitted.
 */
void bm_maskedblit(struct bitmap *dst, int dx, int dy, struct bitmap *src, int sx, int sy, int w, int h);

/*@ void bm_smooth(struct bitmap *b)
 *# Smoothes the bitmap by essentially applying a 3x3 median filter.
 */
void bm_smooth(struct bitmap *b);

/*@ struct bitmap *bm_resample(const struct bitmap *in, int nw, int nh)
 *# Creates a new bitmap of dimensions nw*nh that is a scaled
 *# (linearly resampled) version of the input bitmap. 
 *# The input bimap remains untouched.
 */
struct bitmap *bm_resample(const struct bitmap *in, int nw, int nh);

/*@ void bm_swap_colour(struct bitmap *b, unsigned char sR, unsigned char sG, unsigned char sB, unsigned char dR, unsigned char dG, unsigned char dB)
 *# Replaces all pixels of colour [sR,sG,sB] in bitmap b with the colour [dR,dG,dB]
 */
void bm_swap_colour(struct bitmap *b, unsigned char sR, unsigned char sG, unsigned char sB, unsigned char dR, unsigned char dG, unsigned char dB);

/*@ int bm_width(struct bitmap *b)
 *# Retrieves the width of the bitmap {{b}}
 */
int bm_width(struct bitmap *b);

/*@ int bm_height(struct bitmap *b)
 *# Retrieves the height of the bitmap {{b}}
 */
int bm_height(struct bitmap *b);

/*@ void bm_clear(struct bitmap *b)
 *# Clears the bitmap to the colour of the pen.
 */
void bm_clear(struct bitmap *b);

/*@ void bm_putpixel(struct bitmap *b, int x, int y)
 *# Draws a single pixel at <x,y> using the pen colour.
 */
void bm_putpixel(struct bitmap *b, int x, int y);

/*@ void bm_line(struct bitmap *b, int x0, int y0, int x1, int y1)
 *# Draws a line from <x0,y0> to <x1,y1> using the colour of the pen.
 */
void bm_line(struct bitmap *b, int x0, int y0, int x1, int y1);

/*@ void bm_rect(struct bitmap *b, int x0, int y0, int x1, int y1)
 *# Draws a rectangle from <x0,y0> to <x1,y1> using the pen colour
 */
void bm_rect(struct bitmap *b, int x0, int y0, int x1, int y1);

/*@ void bm_fillrect(struct bitmap *b, int x0, int y0, int x1, int y1)
 *# Draws a filled rectangle from <x0,y0> to <x1,y1> using the pen colour
 */
void bm_fillrect(struct bitmap *b, int x0, int y0, int x1, int y1);

/*@ void bm_circle(struct bitmap *b, int x0, int y0, int r)
 *# Draws a circle of radius {{r}} centered at <x,y> using the pen colour
 */
void bm_circle(struct bitmap *b, int x0, int y0, int r);

/*@ void bm_fillcircle(struct bitmap *b, int x0, int y0, int r)
 *# Draws a filled circle of radius {{r}} centered at <x,y> using the pen colour
 */
void bm_fillcircle(struct bitmap *b, int x0, int y0, int r);

/*@ void bm_ellipse(struct bitmap *b, int x0, int y0, int x1, int y1)
 *# Draws an ellipse that occupies the rectangle from <x0,y0> to 
 *# <x1,y1>, using the pen colour
 */
void bm_ellipse(struct bitmap *b, int x0, int y0, int x1, int y1);

/* MISSING: bm_fillellipse */

/*@ void bm_round_rect(struct bitmap *b, int x0, int y0, int x1, int y1, int r)
 *# Draws a rect from <x0,y0> to <x1,y1> using the pen colour with rounded corners 
 *# of radius {{r}}
 */
void bm_roundrect(struct bitmap *b, int x0, int y0, int x1, int y1, int r);

/*@ void bm_fill_round_rect(struct bitmap *b, int x0, int y0, int x1, int y1, int r)
 *# Draws a filled rect from <x0,y0> to <x1,y1> using the pen colour with rounded corners 
 *# of radius {{r}}
 */
void bm_fillroundrect(struct bitmap *b, int x0, int y0, int x1, int y1, int r);

/* FIXME: The bezier works kinda-sorta for specific values of the control points, but
 * it is unreliable, so I'd replace it with something more robust at a later stage.
 */
void bm_bezier3(struct bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2);

/*@ void bm_fill(struct bitmap *b, int x, int y)
 *# Floodfills from <x,y> using the pen colour.\n
 *# The colour of the pixel at <x,y> is used as the source colour.
 *# The colour of the pen is used as the target colour.
 */
void bm_fill(struct bitmap *b, int x, int y);

/*@ bm_set_font(struct bitmap *b, const unsigned char *font, int spacing)
 *# Changes the font used to render text on the bitmap. {{font}} should be
 *# an XBM char array that should match the layout of font.xbm.
 *# {{spacing}} is the width of the characters in pixels (typically 6, 7 or 8).
 *# If you only want to change the spacing, set {{font}} to {{NULL}}.
 *# If you want to keep the current spacing, set it to {{0}}
 */
void bm_set_font(struct bitmap *b, const unsigned char *font, int spacing);

/*@ enum bm_fonts
 *# Built-in fonts that can be set with {{bm_std_font()}}
 *{
 ** {{BM_FONT_NORMAL}} - A default font.
 ** {{BM_FONT_BOLD}} - A bold font.
 ** {{BM_FONT_CIRCUIT}} - A "computer" font that looks like a circuit board.
 ** {{BM_FONT_HAND}} - A font that looks like hand writing.
 ** {{BM_FONT_SMALL}} - A small font.
 ** {{BM_FONT_SMALL_I}} - A variant of the small font with the foreground and background inverted.
 ** {{BM_FONT_THICK}} - A thicker variant of the normal font.
 *}
 */
enum bm_fonts {
	BM_FONT_NORMAL,
	BM_FONT_BOLD,
	BM_FONT_CIRCUIT,
	BM_FONT_HAND,
	BM_FONT_SMALL,
	BM_FONT_SMALL_I,
	BM_FONT_THICK
};

/*@ void bm_std_font(struct bitmap *b, enum bm_fonts font)
 *# Sets the font to one of the standard (built-in) ones.
 */
void bm_std_font(struct bitmap *b, enum bm_fonts font);

/*@ int bm_text_width(struct bitmap *b, const char *s)
 *# Returns the width in pixels of a string of text.
 */
int bm_text_width(struct bitmap *b, const char *s);

/*@ int bm_text_height(struct bitmap *b, const char *s)
 *# returns the height in pixels of a string of text.
 */
int bm_text_height(struct bitmap *b, const char *s);

/*@ void bm_putc(struct bitmap *b, int x, int y, char c)
 *# Draws a single ASCII character {{c}} at position x,y on the bitmap.
 */
void bm_putc(struct bitmap *b, int x, int y, char c);

/*@ void bm_puts(struct bitmap *b, int x, int y, const char *s)
 *# Prints the string {{s}} at position x,y on the bitmap b.
 */
void bm_puts(struct bitmap *b, int x, int y, const char *text);

/*@ void bm_printf(struct bitmap *b, int x, int y, const char *fmt, ...)
 *# Prints a printf()-formatted style string to the bitmap b,
 *# {{fmt}} is a {{printf()}}-style format string (It uses vsnprintf() internally).
 */
void bm_printf(struct bitmap *b, int x, int y, const char *fmt, ...);

/*@ void bm_putcs(struct bitmap *b, int x, int y, int s, char c)
 *# Draws a single ASCII character {{c}} at position x,y on the bitmap,
 *# scaled by 2^s
 */
void bm_putcs(struct bitmap *b, int x, int y, int s, char c);

/*@ void bm_putss(struct bitmap *b, int x, int y, int s, const char *text)
 *# Prints the string {{s}} at position x,y on the bitmap b,
 *# scaled by 2^s.
 */
void bm_putss(struct bitmap *b, int x, int y, int s, const char *text);

/*@ void bm_printfs(struct bitmap *b, int x, int y, int s, const char *fmt, ...)
 *# Prints a printf()-formatted style string to the bitmap b,,
 *# scaled by 2^s
 *# {{fmt}} is a {{printf()}}-style format string (It uses vsnprintf() internally).
 */
void bm_printfs(struct bitmap *b, int x, int y, int s, const char *fmt, ...);

#if defined(__cplusplus) || defined(c_plusplus)
} /* extern "C" */
#endif