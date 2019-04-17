/*TODO: change editor->verbose to global variable */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define _ASSET_PATH "asset/"
#define SCREEN_W 1280
#define SCREEN_H 768
#define WIN_TITLE "EDITOR"
#define MAX_LAYERS 6
#define TILE_COUNT 4
#define SPRITESHEET_COUNT 25
#define SPRITE_DB "sprite.db"

extern int errno;
int verbose;

enum TILE_TYPE {
    SPRITE_STATIC = 0,
    SPRITE_RANGE = 1,
    EVENT = 2,
    COLLISION = 3,
};

/* Spritesheet struct contains entire spritesheet loaded from png files,
 *	including, created texture, png width & height
*/
struct Spritesheet {
    char *path;
    int width;
    int height;
    int sprite_width;
    int sprite_height;
    SDL_Texture *texture;
    SDL_Rect *rect;
};

/* Sprite struct is created from a parsed spritesheet 
 * a sprite is created with a sdl rect, which contains the x & y coordinates
 * to the sprite in the spritesheet struct
 * it also contains the sprite id and a reference to its spritesheet,
 * for example, sprite rect has x = 16, and y = 16, 
 * these coordinates points to where in referenced spritesheet the sprite is loaded from
*/
struct Sprite {
    SDL_Rect *rect;
    struct Spritesheet spritesheet;
    int id;
};

/* Tile struct contains information about tiles, sprite, id, events and actions */
/* TODO: should tile have the same structure as layers in map->layers */
/* so each id in layers corresponds to a tile which contains id of 
 * sprite and destination x / y on which to render sprite */
/* so a (3d) array of tiles same as layers */
/* type specifies what type of tile this is, see enum TILE_TYPE */
/* range is only used by type SPRITE_RANGE to set an animated sprite range */
struct Tile {
    SDL_Rect *rect;
    struct Sprite *sprite;
    int id;
    int type;
    int range_start;
    int range_end;
};
 
/* Map struct contains information about current loaded map,
 * number of layers, width, height etc
 * layers is an array to arrays to arrays
 * this is to not limit the number of possible layers in the future.
 * The graph below depicts 3 layers, each with 5 rows each, each row contains 6 cells
 * 
                +-------------+
	        0|[0,1,2,3,4,5]|
	        1|[0,1,2,3,4,5]|
	+---[0]-2|[0,1,2,3,4,5]|
	|       3|[0,1,2,3,4,5]|
	|	4|[0,1,2,3,4,5]|
	|	5|[0,1,2,3,4,5]|
	|	+-------------+
        |        +-------------+
	|	0|[0,1,2,3,4,5]|
	|       1|[0,1,2,3,4,5]|
    L---+---[1]-2|[0,1,2,3,4,5]|
	|	3|[0,1,2,3,4,5]|
	|	4|[0,1,2,3,4,5]|
	|	5|[0,1,2,3,4,5]|
	|	+-------------+
        |       +-------------+
	|	0|[0,1,2,3,4,5]|
	|       1|[0,1,2,3,4,5]|
	+---[3]-2|[0,1,2,3,4,5]|
		3|[0,1,2,3,4,5]|
		4|[0,1,2,3,4,5]|
		5|[0,1,2,3,4,5]|
		+-------------+
*/
struct Map {
    int cols;
    int rows;
    int sprite_width;
    int sprite_height;
    int tile_width; 
    int tile_height;
    int layer_count;
    int map_width;
    int map_height;
    int tile_count;
    char *path;
    char *layer0;
    char *name;
    char *md;
    int ***layers;
    struct Tile ***tiles;
};

/* SDL window settings */
struct Screen {
    char *title;
    int w;
    int h;
    SDL_Window *window;
    SDL_Renderer *renderer;   
    SDL_Surface *surface;
    SDL_Texture *texture;
};

/* General editor settings, verbose, is the editor running.
 *  what is under the mouse pointer, is a tile selected etc
*/
struct Editor {
    SDL_bool running;
    struct Screen screen;
    int selected_layer;
    int selecter_row;
    int selected_column;
    int sprite_count;
    int verbose;
    int mouse_pos_x;
    int mouse_pos_y;
};

/* print what ever is in errno */
void error_msg()
{
    fprintf(stderr, "%s\n", strerror(errno));
    exit(errno);
}

/* if editor is verbose print supplied string */
/* TODO: add va_args to accepts %s and such */
void verbose_print(const char *vstring)
{
    if(verbose == 1) {
        printf("%s", vstring);
    }
}

/* load a spritesheet from png to spritesheet struct and set width, and height of image */
/* and store all individual sprites(coordinates and size) in rect array in spritesheet struct */
int load_single_spritesheet(struct Editor *ed, struct Spritesheet *sp, const char *path, int sw, int sh, int ns)
{
	
    verbose_print("load_single_sprite... ");

    sp->rect = calloc(ns, sizeof(SDL_Rect));

    //SDL_Texture *new_texture = NULL;

    SDL_Surface *loaded_surface = IMG_Load(path);

    if(loaded_surface == NULL) {
        error_msg();
    }


    sp->sprite_width = sw;
    sp->sprite_height = sh;

    SDL_SetColorKey(loaded_surface, SDL_TRUE, SDL_MapRGB(loaded_surface->format, 0, 0xFF, 0xFF));
    sp->texture = SDL_CreateTextureFromSurface(ed->screen.renderer, loaded_surface);

    sp->path = calloc(strlen(path) + 1, sizeof(char));
    
    if(sp->path == NULL) {
        error_msg();
    }

    sp->width = loaded_surface->w;
    sp->height = loaded_surface->h;
    if(sp->texture == NULL) {
        error_msg();
    }

    SDL_FreeSurface(loaded_surface);

    /* get num rows */
    int cols = sp->width / sw;
    int rows = sp->height / sh;

    int tx = 0;
    int ty = 0;

    for(int i = 0; i < (cols * rows); i++) {
        if(tx >sp->width - sw) {
            tx = 0;
            ty += sw;
        }
        sp->rect[i].x = tx;
        sp->rect[i].y = ty;
        sp->rect[i].w = sw;
        sp->rect[i].h = sh;
        tx += sw;
    }
    
	verbose_print("OK");
    return 0;

}

/* TODO: each sprite must have a reference to the correct spritesheet */
/* create spritesheet from all files in sprite.db */
/* create individual sprite structs from each created spritesheet and give them an id */
/* id is index */
/* each spritesheet contains 25 sprites */
struct Sprite **load_sprite_database(const char *fname, struct Editor *ed)
{
    FILE *fp = fopen(fname, "r");
    
    char c = '\0';
    char line[255] = {0};
    char result[64];
    int row_count = 0;
    int c_count = 0;
    struct Sprite **sprites = NULL;
    struct Spritesheet sp;
    int sprite_count = 0;

    verbose_print("load_sprite_database... ");

    if(fp == NULL) {
	verbose_print(strerror(errno));
        error_msg();
    }

    /* get nr of lines in sprite database */
    /* this is the nr of spritesheets */
    /* and the nr of tiles are spritesheets * 25 */
    while((c = fgetc(fp)) != EOF) {
        if(ferror(fp) != 0) {
            error_msg();
        }
        if(c == '\n') {
            row_count++;
        }
    }

    rewind(fp);

    sprites = calloc(row_count * 25, sizeof(struct Sprite));
    
    if(sprites == NULL) {
        error_msg();
    }

    for(int i = 0; i < row_count * 25; i++) {
        sprites[i] = calloc(1, sizeof(struct Sprite));

        if(sprites[i] == NULL) {
            error_msg();
        }
    }

    while((c = fgetc(fp)) != EOF) {
        if(ferror(fp) != 0) {
            error_msg();
        }
        if(c != '\n') {
            line[c_count] = c;
            c_count++;
        }
        
        /* for each line, load spritesheet into struct spritesheet */
        /* then set spritesheets rect to sprite array */
        if(c == '\n') {
	    verbose_print(line);
	    verbose_print("\n");
            load_single_spritesheet(ed, &sp, line, 16,16, SPRITESHEET_COUNT);
            for(int i = 0; i < SPRITESHEET_COUNT; i++) {
                sprites[sprite_count]->rect = &sp.rect[i];
                sprites[sprite_count]->id = sprite_count;
                sprites[sprite_count]->spritesheet = sp;
                //printf("[%d] x = %d y = %d\n", sprite_count, sprites[i]->rect->x, sprites[i]->rect->y);
                sprite_count++;
            }
            /* reset line */
	    verbose_print("resetting line\n");
            memset(line, '\0', 255);

            c_count = 0;
        }
    }
    fclose(fp);
    ed->sprite_count = sprite_count-1;
	sprintf(result, "%d tiles loaded\n", ed->sprite_count);
	verbose_print(result);
    return sprites;

}

/* render single sprite  to screen x y*/
void render_sprite(int x, int y, struct Sprite *sp, struct Editor *ed) 
{
    SDL_Rect render_quad = {x, y,sp->rect->w,sp->rect->h};

    render_quad.w = sp->rect->w;
    render_quad.h = sp->rect->h;

    SDL_RenderCopy(ed->screen.renderer, sp->spritesheet.texture,sp->rect, &render_quad);
   
}
/* Not in use */
void render(int x, int y, SDL_Rect *clip, struct Editor *ed, struct Spritesheet *sp) 
{
    SDL_Rect render_quad = {x, y,sp->width,sp->height};

    render_quad.w = clip->w;
    render_quad.h = clip->h;

    SDL_RenderCopy(ed->screen.renderer, sp->texture, clip, &render_quad);
}

/* create texture from png */
SDL_Texture *load_texture(struct Editor *ed, const char *path)
{
    SDL_Texture *new_texture = NULL;

    SDL_Surface *loaded_surface = IMG_Load(path);

    if(loaded_surface == NULL) {
        error_msg();
    }


    SDL_SetColorKey(loaded_surface, SDL_TRUE, SDL_MapRGB(loaded_surface->format, 0, 0xFF, 0xFF));
    new_texture = SDL_CreateTextureFromSurface(ed->screen.renderer, loaded_surface);

    if(new_texture == NULL) {
        error_msg();
    }

    SDL_FreeSurface(loaded_surface);

    return new_texture;
}

/* init sdl and set editor settings */
int init_editor(struct Editor *ed)
{
    if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        fprintf(stderr, "%s\n", SDL_GetError());
        exit(-1);
    }

    ed->screen.w = SCREEN_W;
    ed->screen.h = SCREEN_H;
    ed->screen.title = calloc(strlen(WIN_TITLE) + 1, sizeof(char));
    
    if(ed->screen.title == NULL) {
        error_msg();
    }

    strcpy(ed->screen.title, WIN_TITLE);

    /* create SDL window */
    ed->screen.window = SDL_CreateWindow(
            WIN_TITLE, 
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            ed->screen.w,
            ed->screen.h,
            0
        );

    /* create SDL renderer */
    ed->screen.renderer = SDL_CreateRenderer(ed->screen.window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_SetRenderDrawColor(ed->screen.renderer, 0xFF,0xFF,0xFF,0xFF);

    /* init SDL Image */
    int img_flags = IMG_INIT_PNG;
    if(!(IMG_Init(img_flags) & img_flags)) {
        error_msg();
    }

    ed->screen.surface = SDL_GetWindowSurface(ed->screen.window);

    ed->running = SDL_TRUE;

    return 0;
}
/* set editor running to false and quit sdl */
int quit_editor(struct Editor *ed)
{
    SDL_DestroyRenderer(ed->screen.renderer);
    SDL_DestroyWindow(ed->screen.window);
    SDL_Quit();
    ed->running = SDL_FALSE;

    return 0;
}

/* init struct map */
int init_map(struct Map *map)
{
    map->cols = 0;
    map->rows = 0;
    map->sprite_width = 0;
    map->sprite_height = 0;
    map->layer_count = 0;
    map->path = NULL;
    map->layer0 = NULL;
    map->name = NULL;
    map->md = NULL;
    map->layers = NULL;
    map->tiles = NULL;
    return 0;
}

/* the layout for tiles in mp struct is exactly the same as layers, but with struct tile instead of int */
int alloc_tiles(struct Map *mp) 
{
    verbose_print("allocating tiles... ");
    mp->tiles = calloc(mp->layer_count, sizeof(struct Tile *));

    if(mp->tiles == NULL) {
        error_msg();
    }
    for(int i = 0; i < mp->layer_count; i++) {
        mp->tiles[i] = calloc(mp->rows, sizeof(struct Tile *));

        for(int row = 0; row < mp->rows; row++) {
            mp->tiles[i][row] = calloc(mp->cols, sizeof(struct Tile ));
            if(mp->tiles[i][row] == NULL) {
                error_msg();
            }
        }
    }

    verbose_print("OK\n");
    return 0;
}

/* allocate memory for layers in map and set to 0 */
int alloc_layers(struct Map *mp) 
{
    verbose_print("allocating layers... ");
    mp->layers = calloc(mp->layer_count, sizeof(int*));
    

    if(mp->layers == NULL) {
        error_msg();
    }

    for(int i = 0; i < mp->layer_count; i++) {

        mp->layers[i] = calloc(mp->rows,sizeof(int *));
        for(int row = 0; row < mp->rows; row++) {
            mp->layers[i][row] = calloc(mp->cols, sizeof(int ));
            if(mp->layers[i][row] == NULL) {
                fprintf(stderr,"%s\n", strerror(errno));
                exit(errno);
            }
        }
    }

    verbose_print("OK\n");
    return 0;
}
/* calculate map width and height */
/* width = tile_width * columns */
int set_map_dimensions(struct Map *mp)
{
    mp->map_width = (mp->tile_width * mp->cols);
    mp->map_height = (mp->tile_height * mp->rows);

    return 0;
}

/* calculate total nr of tiles in map */
void set_tile_count(struct Map *mp)
{
    mp->tile_count = (mp->layer_count * mp->rows * mp->cols);
}

/* init layers, set width and position of all tiles in map */
int init_tiles(struct Map *mp)
{
    int tx = 0;
    int ty = 0;

    /* for each layer set x and y position of all tiles */
    for(int l = 0; l < mp->layer_count; l++) {
        tx = 0;
        ty = 0;
        for(int i = 0; i < (mp->cols * mp->rows); i++) {
            int row = (ty / mp->tile_height);
            int col = (tx / mp->tile_width); 
            printf("[%d][%d][%d]\n", l, row, col);
            if(tx > (mp->map_width - mp->tile_width)) {
                tx = 0;
                ty += mp->tile_height;;
            }
            /* TODO: segfault */
            mp->tiles[0][0][0].rect->x = tx;
            mp->tiles[0][0][0].rect->y = ty;
            mp->tiles[0][0][0].rect->w = mp->tile_width;
            mp->tiles[0][0][0].rect->h = mp->tile_height;
            tx += mp->tile_width;
        }
    }

    return 0;
}

/* TODO: read.md is supposed to be in map folder in asset 
 * ex asset/map/map_01/map_01.md 
 * this file should contain all map metadata, such as tile width, height
 * map width and height, name, layers 
 * so one should be able to send an empty map struct and path to map to load_asset function
 * and the map struct will be populated  and loaded*/
int load_map(struct Map *mp, const char *path)
{
    FILE *fp = NULL;
    char c = '\0';
    char *tmp_width;
    char *tmp_height;
    char *tmp_line = NULL;
    int length = 0;
    int x_pos = 0;
    int n = 0;

    fp = fopen(path, "r");

    if(fp == NULL) {
        error_msg();
    }

    tmp_width = calloc(4, sizeof(char));
    tmp_height = calloc(4, sizeof(char));

    while((c = fgetc(fp)) != '\n') {
        if(c == 'x') {
            
            x_pos = length;
            printf("%d\n", x_pos);
        }

        length++;
    }

    tmp_line = calloc(length, sizeof(char));

    rewind(fp);

    while((c = fgetc(fp)) != '\n') {
        tmp_line[n] = c;
        n++;
    }

    fclose(fp);

    for(int i = 0; i < x_pos; i++) {
        tmp_width[i] = tmp_line[i];
    }

    for(int i = x_pos+1, j = 0; i < length; i++, j++) {
        tmp_height[j] = tmp_line[i];
    }
        
    free(tmp_line);
    free(tmp_height);
    free(tmp_width);
    return 0;
}

/* create metadata for map struct */
/* NOTE: tile width and height defaults to same as sprite */
int create_map(struct Map *mp, int w, int h, int layers, int sw, int sh, const char *name)
{
    verbose_print("creating map... \n");

    mp->cols = w;
    mp->rows = h;
    mp->layer_count = layers;
    mp->sprite_width = sw;
    mp->sprite_height = sh;
    mp->tile_height = sh;
    mp->tile_width = sw;

    verbose_print("\tsetting map dimensions...\n");
    set_map_dimensions(mp);
    if(verbose == 1) {
        printf("\tmap.w = %d map.h = %d\n", mp->map_height, mp->map_width);
        printf("\tOK\n");
    }

    verbose_print("\tsetting total number of tiles...\n");
    set_tile_count(mp);
    if(verbose == 1) {
        printf("\tmap.tile_count = %d\n", mp->tile_count);
        printf("\tOK\n");
    }


   
    mp->name = calloc(strlen(name)+1, sizeof(char));

    if(mp->name == NULL) {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(errno);
    }

    strcpy(mp->name, name);

    return 0;
}
int free_map(struct Map *mp)
{
    free(mp->path);
    free(mp->name);
    free(mp->md);

    for(int i = 0 ; i < mp->layer_count; i++) {
        for(int row = 0; row < mp->rows; row++) {
            free(mp->layers[i][row]);
        }

        free(mp->layers[i]);
    }   

    free(mp->layers);
    return 0;
}
/* append string and comma unless cflag is not 1 */
int append_metadata(char **str, int data, int cflag)
{
    char tmp[128];

    sprintf(tmp, "%d", data);

    strcat(*str, tmp);

    if(cflag == 1) {
        strcat(*str, ",");
    }

    return 0;
}

/* create empty layers for map */
int create_layers(struct Map *mp)
{
    /* create n layers with default tiles in asset folder */
    
    FILE *fp = NULL;
    char *fname = NULL;

    alloc_layers(mp);
    alloc_tiles(mp);

    for(int i = 0; i < mp->layer_count; i++) {
        /* create file name for layer file */
        /* /path/name_<layer>.lr */
        /* +2 for integer suffix and null terminator */
        fname = calloc(strlen(mp->path) + strlen(mp->name) + strlen("_.lr") + 2, sizeof(char));

        if(fname == NULL) {
            fprintf(stderr, "%s\n", strerror(errno));
            exit(errno);
        }
        char tmp[3]; /* tmp buffer for i to char conversion */

        sprintf(tmp, "%d", i);

        strcpy(fname,mp->path);
        strcat(fname,mp->name);
        strcat(fname,"_");
        strcat(fname,tmp);
        strcat(fname, ".lr");

        printf("%s\n", fname);
        fp = fopen(fname, "a+");

        /* set each array entry to 0 */
        /* append to fname */
            for(int row = 0; row < mp->rows; row++) {
                for(int col = 0; col < mp->cols; col++) {
                    mp->layers[i][row][col] = 0;

                    fprintf(fp,"%d,",mp->layers[i][row][col]);
                }

                fprintf(fp,"%c",'\n');
            }

        fclose(fp);
        free(fname);
    }

    return 0;
}

/* save metadata file to map folder in asset */
int save_metadata(struct Map *mp)
{
    FILE *fp = NULL;
    char *md_line;

    md_line = calloc(255, sizeof(char));
    mp->path = calloc((strlen(_ASSET_PATH) + strlen(mp->name) + 2), sizeof(char));

    if(mp->path == NULL) {
        fprintf(stderr, "%s\n", strerror(errno));
    }


    strcpy(mp->path, _ASSET_PATH);
    strcat(mp->path, mp->name);
    strcat(mp->path, "/");

    mp->md = calloc((strlen(mp->path) + strlen(".md") + strlen(mp->name) + 1), sizeof(char));

    if(mp->md == NULL) {
        fprintf(stderr, "%s\n", strerror(errno));
    }

    strcpy(mp->md, mp->path);
    strcat(mp->md, mp->name);
    strcat(mp->md, ".md");


    mkdir(mp->path, 0700);

    /* create metadata line to write to file */
    append_metadata(&md_line, mp->cols, 1);
    append_metadata(&md_line, mp->rows, 1);
    append_metadata(&md_line, mp->layer_count, 1);
    append_metadata(&md_line, mp->sprite_width, 1);
    append_metadata(&md_line, mp->sprite_height, 1);
    strcat(md_line, mp->path);

    /* write md_line to asset folder */
    fp = fopen(mp->md, "w");

    fprintf(fp, "%s", md_line);
    fclose(fp);

    free(md_line);

    return 0;
}

void print_metadata(struct Map *mp)
{
    printf("width: %d\n", mp->cols);
    printf("height: %d\n", mp->rows);
    printf("layers: %d\n", mp->layer_count);
    printf("sprite_width: %d\n", mp->sprite_width);
    printf("sprite_height: %d\n", mp->sprite_height);
    printf("name: %s\n", mp->name);
}

/* render complete spritesheet to screen */
int render_spritesheet(struct Spritesheet *sp, struct Editor *ed) 
{
    int tx = 0;
    int ty = 0;

    for(int i = 0; i < 25; i++) {
        if(tx > (sp->width - sp->sprite_width)) {
            tx = 0;
            ty += sp->sprite_height;
        }
        render(tx, ty, &sp->rect[i], ed, sp);
        tx += sp->sprite_width;
    }

    return 0;
}

void free_sprite_database(struct Sprite **sprites, int count)
{
    for(int i = 0; i < count; i++) {
    SDL_DestroyTexture(sprites[i]->spritesheet.texture);
        free(sprites[i]);
    }

    free(sprites);
}

int render_layers(struct Editor *ed, struct Map *mp, struct Sprite **db)
{
   for(int i = 0; i < mp->layer_count; i++) {
        for(int row = 0; row < mp->rows; row++) {
            for(int col = 0; col < mp->cols; col++) {
            }
        }
    }


    return 0;
}

/* set current mouse coordinates */
void get_current_mouse_pos(struct Editor *ed)
{
    SDL_GetMouseState(&ed->mouse_pos_x, &ed->mouse_pos_y);    
}

int main(void)
{
    struct Map mp;
    struct Editor ed;
    SDL_Event event;

    init_map(&mp);
    init_editor(&ed);
    
    //load_map(&mp);

    verbose = 1;

    /* TODO: add stat for dir test before creation */
    create_map(&mp, 16, 16, 3, 16,16, "test");
    print_metadata(&mp);
    save_metadata(&mp);
    create_layers(&mp);

    verbose_print("\tinitializing tile position and dimensions...\n");
    init_tiles(&mp);
     if(verbose == 1) {
        printf("\tOK\n");
    }
    free_map(&mp);
    struct Sprite **sprite_db = load_sprite_database(SPRITE_DB, &ed);
    while(ed.running == SDL_TRUE) {
        if(SDL_GetMouseState(NULL,NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
            get_current_mouse_pos(&ed);
            printf("%d:%d\n", ed.mouse_pos_x, ed.mouse_pos_y);       
        }

        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    ed.running = SDL_FALSE;
                    break;
                default:
                    break;
            }
        }

        SDL_RenderClear(ed.screen.renderer);
        render_sprite(0, 0, sprite_db[49], &ed); 
        SDL_RenderPresent(ed.screen.renderer);
    }

    free_sprite_database(sprite_db, ed.sprite_count); 
    quit_editor(&ed);

    return 0;
}

