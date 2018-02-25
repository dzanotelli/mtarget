/* (C) 2014--2015 Daniele Zanotelli
 * dazano@gmail.com
 *
 * Re-implementing my old game `target.bas' in C (previously written in
 * QuickBasic) for learning purposes (C, struct, pointers and ncurses lib).
 *
 * References:
 * - ncurses: http://invisible-island.net/ncurses/ncurses-intro.html
 * - ncurses: http://tldp.org/HOWTO/NCURSES-Programming-HOWTO/
 *
 *
 * This software is licensed under GPL v3.
 *
 * This very program is dedicated to my cousin Federico, who was the only
 * one to use and appreciate the old target game.
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ncurses.h> /* may also autoinclude tremios.h or tremio.h or sftty.h */

#define mtLINES 24   /* workspace defined as 24 lines x 80 cols*/
#define mtCOLS 80
#define LEFT 1
#define CENTER 2
#define RIGHT 3

#define MAX_PN_LEN 13 /* max name length for player */

#define TIME_VALUE 30

#define EXIT_GAME 0
#define NEW_GAME 1

#define GAME_RUNNING 0
#define GAME_WIN 1
#define GAME_LOSE 2

#define NO_COLOR 0
#define RED_ON_BLACK 1
#define GREEN_ON_BLACK 2
#define YELLOW_ON_BLACK 3
#define BLUE_ON_BLACK 4
#define MAGENTA_ON_BLACK 5
#define CYAN_ON_BLACK 6
#define WHITE_ON_BLACK 7
#define BLACK_ON_RED 8
#define BLACK_ON_GREEN 9
#define BLACK_ON_YELLOW 10
#define BLACK_ON_BLUE 11
#define BLACK_ON_MAGENTA 12
#define BLACK_ON_CYAN 13
#define BLACK_ON_WHITE 14

#define AMMO_AVAILABLE(level) 30 - (level-1)*10

#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3

/* ---------------------------------------------------------------------------
 * data structures definition
 */
typedef struct
{
        WINDOW* win;
        int y, x;
        int height, width;
        int border;
} mtWIN;

typedef struct
{
        int x;
        int y;
} point;

/* FIXME:  `#define __attribute__((__packed__)) '  for windows mb? */
/*http://stackoverflow.com/questions/232785/use-of-pragma-in-c */

struct shot_item//__attribute__((__packed__))
{
        struct shot_item* prev;
        struct shot_item* next;
        point coords;
        unsigned int distance;
};


typedef struct
{
        char player_name[MAX_PN_LEN];
        int level;
        bool timer;
        point* target;
        int ammo_tot;
        int ammo_left;
} game_conf;

/* ---------------------------------------------------------------------------
 * global vars
 */
bool term_colors;
mtWIN* field;
mtWIN* panel;
mtWIN* lamp;
mtWIN* msg;

/* ----------------------------------------------------------------------------
 * functions' prototypes
 */
void ask_options(game_conf* configuration);
bool config_colors(void);
void clear_ammo_info();
void clear_msg();
mtWIN* create_win(int height, int width, int starty, int startx, int border);
void destroy_win(mtWIN* window);
void display_shots(struct shot_item* last_shot);
unsigned int distance(point a, point b);
void draw_ascii_circle(mtWIN* win, int tly, int tlx, int color, char* text);
void draw_border(mtWIN* window, int color_pair, bool refresh_flag);
void draw_gunsight(mtWIN* window, point gunsight, int color);
void draw_shot(mtWIN* window, point shot, int color);
void draw_target(mtWIN* window, point target);
void enter_ncurses(void);
void exit_ncurses(void);
void free_shot_list(struct shot_item* last_shot_item);
point* get_new_target(mtWIN* window);
point* get_random_point(int max_y, int max_x);
void greet(void);
void init_panel(game_conf* configuration);
void init_target_area();
void init_traffic_lamp();
void light_the_lamp(unsigned int distance);
int main_cycle(game_conf* configuration);
void move_gunsight(mtWIN* window, point* gunsight, int direction);
void mv_info_gunsight(point* gunsight, int dir);
void mv_mtw_addstr_center(mtWIN* window, int y, char* string);
struct shot_item* push_shot(struct shot_item* last, point new_shot,
                            unsigned int new_distance);
void set_msg(char* message, int color);
void show_win(mtWIN* window);
void toggle_lamp_lights(int red, int yellow, int green);
void upd_ammo_info(game_conf conf);
void upd_time_info(int time_value);
/* -------------------------------------------------------------------------- */

int main()
{
        /* standard declarations */
        int todo;
        game_conf* conf = malloc(sizeof(game_conf));
        conf->target = malloc(sizeof(point));

        /* start ncurses env -- before this also ncurses structures
           as `WINDOW' (random sample...!) will not be ready
         */
        enter_ncurses();
        refresh();

        field = create_win(15, 65, 0, 0, CYAN_ON_BLACK);
        panel = create_win(6, 80, 16, 0, MAGENTA_ON_BLACK);
        lamp = create_win(16, 15, 0, 65, WHITE_ON_BLACK);
        msg = create_win(1, 65, 15, 0, NO_COLOR);

        /* first, the introducing window */
        greet();

        /* init the available commands in stdscr, bottom line */
        if (term_colors) attron(A_BOLD);
        mvaddstr(22, 10, "[P]");
        mvaddstr(22, 20, "[N]");
        mvaddstr(22, 38, "[U]");
        mvaddstr(22, 64, "[S]");
        if (term_colors) attroff(A_BOLD);
        mvaddstr(22, 13, "ausa");
        mvaddstr(22, 23, "uova partita");
        mvaddstr(22, 41, "scita");
        mvaddstr(22, 67, "para!");

        /* go! */
        while (TRUE) {
                /* ask to the user the game configuration parameters */
                ask_options(conf);

                /* init */
                init_panel(conf);
                init_traffic_lamp();
                init_target_area();

                /* print available commands in the bottom line */
                refresh();

                todo = main_cycle(conf);
                if (todo == EXIT_GAME) {
                        break;
                }
                else if (todo == NEW_GAME) {
                        continue;
                }
        }

        /* free memory */
        free(conf->target);
        free(conf);
        destroy_win(field);
        destroy_win(panel);
        destroy_win(lamp);
        destroy_win(msg);

        /* exit */
        exit_ncurses();
        return 0;
}

void enter_ncurses()
{
        initscr();
        raw();
        nonl();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);        /* available values: 0, 1, 2. 0 is no cursor */
        term_colors = config_colors();
}

bool config_colors()
{
        if (has_colors()) {
                start_color();
                /* color on black */
                init_pair(RED_ON_BLACK, COLOR_RED, COLOR_BLACK);
                init_pair(GREEN_ON_BLACK, COLOR_GREEN, COLOR_BLACK);
                init_pair(YELLOW_ON_BLACK, COLOR_YELLOW, COLOR_BLACK);
                init_pair(BLUE_ON_BLACK, COLOR_BLUE, COLOR_BLACK);
                init_pair(MAGENTA_ON_BLACK, COLOR_MAGENTA, COLOR_BLACK);
                init_pair(CYAN_ON_BLACK, COLOR_CYAN, COLOR_BLACK);
                init_pair(WHITE_ON_BLACK, COLOR_WHITE, COLOR_BLACK);

                /* color on white ~ enligh effect */
                init_pair(BLACK_ON_RED, COLOR_BLACK, COLOR_RED);
                init_pair(BLACK_ON_GREEN, COLOR_BLACK, COLOR_GREEN);
                init_pair(BLACK_ON_YELLOW, COLOR_BLACK, COLOR_YELLOW);
                init_pair(BLACK_ON_BLUE, COLOR_BLACK, COLOR_BLUE);
                init_pair(BLACK_ON_MAGENTA, COLOR_BLACK, COLOR_MAGENTA);
                init_pair(BLACK_ON_CYAN, COLOR_BLACK, COLOR_CYAN);
                init_pair(BLACK_ON_WHITE, COLOR_BLACK, COLOR_WHITE);

                return TRUE;
        }
        else {
                return FALSE;
        }
}

void exit_ncurses()
{
        endwin();
}

mtWIN* create_win(int height, int width, int starty, int startx, int border)
{
        /* init a mtWIN data structure, with a ncurses WINDOW and draw a border
         * if *border* flag is set to a NCURSES COLOR PAIR value
         */
        mtWIN* magic_target_window = malloc(sizeof(mtWIN));

        magic_target_window->win = newwin(height, width, starty, startx);
        magic_target_window->border = border;
        magic_target_window->height = height;
        magic_target_window->width = width;
        magic_target_window->y = starty;
        magic_target_window->x = startx;

        draw_border(magic_target_window, border, FALSE);

        /* activate keypad */
        keypad(magic_target_window->win, TRUE);

        return magic_target_window;
}

void draw_border(mtWIN* win, int color_pair, bool refresh)
{
        if (color_pair) {
                if (term_colors) {
                        wattron(win->win, COLOR_PAIR(color_pair));
                }
                box(win->win, 0, 0);
                if (term_colors) {
                        wattroff(win->win, COLOR_PAIR(color_pair));
                }
        }
        else {   /* hide border */
                COLOR_PAIR(WHITE_ON_BLACK);
                wborder(win->win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
        }

        win->border = color_pair;

        if (refresh)
                wrefresh(win->win);
}
void show_win(mtWIN* win)
{
        /* Refresh and show the window even if there were no modifications
         */

        touchwin(win->win); /* let ncurses think that every char is new  */
        wrefresh(win->win); /* in order to refresh them on the screen    */
}

void destroy_win(mtWIN* win)
{
        wclear(win->win);
        wrefresh(win->win);
        delwin(win->win);
        free(win);
}

void mv_mtw_addstr_center(mtWIN* win, int y, char* string)
{
        int x_pos = floor(win->width / 2) - ceil(strlen(string) / 2);

        if (x_pos < 0) {
                if (win->border) {
                        string[win->width-2] = '\0';
                        x_pos = 1;
                }
                else {
                        string[win->width] = '\0';
                        x_pos = 0;
                }
        }
        mvwaddstr(win->win, y, x_pos, string);
}

void greet()
{
        /* Build a window with the program title, and some notes to introduce
         * Magic Target to the user.
         */

        mtWIN* greet_win = create_win(mtLINES, mtCOLS, 0, 0, RED_ON_BLACK);
        char* title[] = {
                "C Magic Target - v2.0",
                "(C) 2014 Daniele Zanotelli - dazano@gmail.com",
        };

        char* descr[] = {
                "Questo e` un remake di QuickBasic Magic Target, scritto",
                "in adolescenza. Questa versione, reimplementata in C,",
                "e` dedicata a mio cugino Federico, unico utente della prima",
                "versione il quale ha apprezzato cosi` tanto la scritta",
                "``!! BINATO !!'' che appariva quando il bersagio veniva",
                "centrato.",
        };
        char* sentence;
        int i = 0;
        int selected_color;
        size_t title_len = sizeof(title)/sizeof(title[0]);
        size_t descr_len = sizeof(descr)/sizeof(descr[0]);

        for (i=0; i<title_len; i++) {
                if (term_colors) {
                        switch (i) {
                        case 0:
                                selected_color = BLUE_ON_BLACK;
                                break;
                        case 1:
                                selected_color = CYAN_ON_BLACK;
                                break;
                        default:
                                selected_color = WHITE_ON_BLACK;
                        }
                        wattron(greet_win->win, COLOR_PAIR(selected_color));
                }

                mv_mtw_addstr_center(greet_win, 5+i+i%2, title[i]);

                if (term_colors)
                        wattroff(greet_win->win, COLOR_PAIR(selected_color));
        }

        for (i=0; i<descr_len; i++)
                mv_mtw_addstr_center(greet_win, 12+i, descr[i]);

        wrefresh(greet_win->win);
        wgetch(greet_win->win);
        destroy_win(greet_win);
        return;
}

void ask_options(game_conf* conf)
{
        /* ask the user name, the difficulty level and if she/he wants the timer
         * switch on or off
         */
        int i, ch, len;
        char title[] = "Opzioni di gioco:";
        mtWIN* win = create_win(mtLINES-6, mtCOLS-10, 3, 5, MAGENTA_ON_BLACK);

        char* label[] = {
                "Nome Giocatore    :",
                "Difficolta` (1-3) :",
                "Tempo (si/no)     :"
        };
        int pos_y[] = {5, 7, 9};
        int pos_x[] = {24, 24, 24};
        char pn_default[MAX_PN_LEN];
        int lvl_default = 1;
        bool timer_default = FALSE;

        char* field_default[3];
        field_default[0] = malloc((MAX_PN_LEN+1) * sizeof(char));
        field_default[1] = malloc(2 * sizeof(char));
        field_default[2] = malloc(3 * sizeof(char));

        char* temp = malloc((MAX_PN_LEN+1) * sizeof(char));

        /* adjust defaults with old data */
        if (strlen(conf->player_name)) strcpy(pn_default, conf->player_name);
        else strcpy(pn_default, "Daporlaor");
        if (conf->level) lvl_default = conf->level;
        else lvl_default = 1;
        if (conf->timer) timer_default = conf->timer;

        sprintf(field_default[0], "%s", pn_default);
        sprintf(field_default[1], "%d", lvl_default);
        if (timer_default) sprintf(field_default[2], "si");
        else sprintf(field_default[2], "no");

        /* print title in the main window */
        mv_mtw_addstr_center(win, 2, title);

        /* print fields and defaults */
        for (i=0; i<3; i++) {
                if (term_colors) wattron(win->win, A_BOLD);
                mvwaddstr(win->win, pos_y[i], pos_x[i]-20, label[i]);
                if (term_colors) wattroff(win->win, A_BOLD);
                mvwaddstr(win->win, pos_y[i], pos_x[i], field_default[i]);
        }


        /* 1: ask player name */
        curs_set(1);
        wmove(win->win, 5, 24);
        wrefresh(win->win);

        len = 0; //strlen(pn_default);
        while ((ch = wgetch(win->win)) != EOF && ch != 13) {
                // delete default value from screen
                if (len == 0) {
                        for (i=0; i<MAX_PN_LEN-1; i++) {
                                mvwaddch(win->win, pos_y[0], pos_x[0]+i, ' ');
                        }
                        wmove(win->win, pos_y[0], pos_x[0]);
                }

                switch (ch) {
                case KEY_BACKSPACE:
                        if (len > 0) {
                                mvwaddch(win->win, pos_y[0], --pos_x[0], ' ');
                                wmove(win->win, pos_y[0], pos_x[0]);
                                len--;
                        }
                        break;
                default:
                        if (len < MAX_PN_LEN-1 && (isalnum(ch) || ch == ' ')) {
                                mvwaddch(win->win, pos_y[0], pos_x[0]++, ch);
                                temp[len++] = ch;
                        }
                        break;
                }
                wrefresh(win->win);
        }
        if (len == 0) {
                mvwaddstr(win->win, pos_y[0], pos_x[0], field_default[0]);
                strcpy(conf->player_name, field_default[0]);
        }
        else {
                temp[len] = '\0';
                strcpy(conf->player_name, temp);
        }


        /* 2: ask difficulty */
        wattron(win->win, COLOR_PAIR(CYAN_ON_BLACK));
        mvwaddstr(win->win, pos_y[1], 30, "Usa le frecce <- e ->");
        wattroff(win->win, COLOR_PAIR(CYAN_ON_BLACK));

        wmove(win->win, pos_y[1], pos_x[1]);

        i = lvl_default;
        while ((ch = wgetch(win->win)) != EOF && ch != 13) {
                switch (ch) {
                case KEY_LEFT:
                        i--;
                        break;
                case KEY_RIGHT:
                        i++;
                        break;
                }
                if (i < 1)
                        i = 3;
                else if (i > 3)
                        i = 1;
                mvwaddch(win->win, pos_y[1], pos_x[1], i + 48);

        }
        mvwaddstr(win->win, pos_y[1], 30, "                     ");
        conf->level = i;
        conf->ammo_tot = AMMO_AVAILABLE(conf->level);
        conf->ammo_left = conf->ammo_tot;


        /* 3: ask if switch on time */
        wattron(win->win, COLOR_PAIR(CYAN_ON_BLACK));
        mvwaddstr(win->win, pos_y[2], 30, "Usa le frecce <- e ->");
        wattroff(win->win, COLOR_PAIR(CYAN_ON_BLACK));

        wmove(win->win, pos_y[2], pos_x[2]+2);
        i = timer_default;
        while ((ch = wgetch(win->win)) != EOF && ch != 13) {
                switch (ch) {
                case KEY_LEFT:
                case KEY_RIGHT:
                        if (i) {
                                i = FALSE;
                                mvwaddstr(win->win, pos_y[2], pos_x[2], "no");
                        }
                        else if (!i) {
                                i = TRUE;
                                mvwaddstr(win->win, pos_y[2], pos_x[2], "si");
                        }
                        break;
                }
        }
        conf->timer = i;
        mvwaddstr(win->win, pos_y[2], 30, "                     ");

        if (term_colors) wattron(win->win, A_BOLD);
        mv_mtw_addstr_center(win, 15,
                             "Premere un tasto qualsiasi per iniziare"
                );

        if (term_colors) wattroff(win->win, A_BOLD);
        wgetch(win->win);

        /* exit */
        destroy_win(win);
        for (i=0; i<3; i++) free(field_default[i]);
        curs_set(0);
}


void init_panel(game_conf* conf)
{
        /* init the panel with the player name, the timer info and ammos
         */

        int i, col, row, j;

        /* labels */
        wattron(panel->win, COLOR_PAIR(BLUE_ON_BLACK));
        mvwaddstr(panel->win, 2, 3, "Giocatore:");
        mvwaddstr(panel->win, 2, 33, "Liv.:");
        mvwaddstr(panel->win, 2, 42, "Tempo:");
        mvwaddstr(panel->win, 4, 33, "x:");
        mvwaddstr(panel->win, 4, 42, "y:");
        wattroff(panel->win, COLOR_PAIR(BLUE_ON_BLACK));
        /* values */
        wattron(panel->win, COLOR_PAIR(RED_ON_BLACK));
        mvwaddstr(panel->win, 2, 14, conf->player_name);
        mvwaddch(panel->win, 2, 39, conf->level + 48);
        mvwaddstr(panel->win, 2, 49, "--");
        wattroff(panel->win, COLOR_PAIR(RED_ON_BLACK));

        /* veritical line */
        wattron(panel->win, COLOR_PAIR(panel->border));
        for (i=1; i<panel->height-1; i++)
                mvwaddch(panel->win, i, 57, ACS_VLINE);
        mvwaddch(panel->win, 0, 57, ACS_TTEE);
        mvwaddch(panel->win, panel->height-1, 57, ACS_BTEE);
        wattroff(panel->win, COLOR_PAIR(panel->border));

        /* init ammos */
        wattron(panel->win, COLOR_PAIR(BLUE_ON_BLACK));
        mvwaddstr(panel->win, 1, 67, "AMMO");
        wattroff(panel->win, COLOR_PAIR(BLUE_ON_BLACK));
        show_win(panel);
}
void upd_time_info(int time_value)
{
        char* time_str = malloc(3 * sizeof(char));

        if (term_colors) wattron(panel->win, COLOR_PAIR(RED_ON_BLACK));
        sprintf(time_str, "%02i", time_value);
        mvwaddstr(panel->win, 2, 49, time_str);
        wrefresh(panel->win);
        free(time_str);
        if (term_colors) wattroff(panel->win, COLOR_PAIR(RED_ON_BLACK));
}

void clear_ammo_info()
{
        int i, col, row;

        row = 2;
        col = 0;
        for (i=0; i<30; i++) {
                if (col > 9) {
                        col = 0;
                        row++;
                }
                mvwaddch(panel->win, row, 59 + col*2, ' ');
                col++;
        }
}

void upd_ammo_info(game_conf conf)
{
        int i, col, row;
        char ammo_ch = '*';
        char ammo_used_ch = '-';


        if (term_colors) wattron(panel->win, COLOR_PAIR(RED_ON_BLACK));

        row = 2;
        col = 0;
        for (i=1; i<=conf.ammo_tot; i++) {
                if (col > 9) {
                        col = 0;
                        row++;
                }
                if (i > conf.ammo_left) {
                        if (term_colors)
                                wattroff(panel->win, COLOR_PAIR(RED_ON_BLACK));
                        else
                                ammo_ch = ammo_used_ch;
                }
                mvwaddch(panel->win, row, 59 + col*2, ammo_ch);
                col++;
        }
        if (term_colors) wattroff(panel->win, COLOR_PAIR(RED_ON_BLACK));
        wrefresh(panel->win);
}


void init_traffic_lamp()
{
        toggle_lamp_lights(FALSE, FALSE, FALSE);
        show_win(lamp);
}

void light_the_lamp(unsigned int distance)
{
        if (distance < 2) {
                toggle_lamp_lights(MAGENTA_ON_BLACK,
                                   MAGENTA_ON_BLACK,
                                   MAGENTA_ON_BLACK
                        );
        }
        else if (distance < 10)
                toggle_lamp_lights(NO_COLOR, NO_COLOR, GREEN_ON_BLACK);
        else if (distance < 20)
                toggle_lamp_lights(NO_COLOR, YELLOW_ON_BLACK, NO_COLOR);
        else if (distance < 30)
                toggle_lamp_lights(RED_ON_BLACK, NO_COLOR, NO_COLOR);
        else
                toggle_lamp_lights(NO_COLOR, NO_COLOR, NO_COLOR);

        wrefresh(lamp->win);
}

void toggle_lamp_lights(int first, int second, int third)
{
        /* activate or deactivate lamp lights
         */
        if (first)
                draw_ascii_circle(lamp, 1, 2, first, " :( :(");
        else
                draw_ascii_circle(lamp, 1, 2, NO_COLOR, "       ");

        if (second)
                draw_ascii_circle(lamp, 6, 2, second, " :/ :/");
        else
                draw_ascii_circle(lamp, 6, 2, NO_COLOR, "      ");

        if (third)
                draw_ascii_circle(lamp, 11, 2, third, " :) :)");
        else
                draw_ascii_circle(lamp, 11, 2, NO_COLOR, "     ");
}
void draw_ascii_circle(mtWIN* win, int y, int x, int color, char* text)
{
        /* draw an ascii circle, using *y* and *x* as coords for the upper left
         * corner of *color*. The first 6 chars of *text* will be placed inside
         * the circle
         *
         *    _____
         *   /     \
         *  /  *te  \
         *  \  xt*  /
         *   \_____/
         *
         */
        int i, line, col;

        if (color == NO_COLOR) {
                text = "      ";
                if (term_colors) wattron(win->win, COLOR_PAIR(WHITE_ON_BLACK));
        }
        else if (term_colors) wattron(win->win, COLOR_PAIR(color));

        /* top border */
        for (i=3; i<8; i++) mvwaddch(win->win, y, x+i, '_');
        /* bottom border */
        for (i=3; i<8; i++) mvwaddch(win->win, y+4, x+i, '_');
        /* left border */
        mvwaddch(win->win, y+1, x+2, '/');
        mvwaddch(win->win, y+2, x+1, '/');
        mvwaddch(win->win, y+3, x+1, '\\');
        mvwaddch(win->win, y+4, x+2, '\\');
        /* right border */
        mvwaddch(win->win, y+1, x+8, '\\');
        mvwaddch(win->win, y+2, x+9, '\\');
        mvwaddch(win->win, y+3, x+9, '/');
        mvwaddch(win->win, y+4, x+8, '/');
        /* text */
        i = 0;
        line = 2;
        col = 4;
        while (text[i] != '\0') {
                if (i == 3) {
                        line++;
                        col = 3;
                }
                else if (i == 6) {
                        break;
                }

                mvwaddch(win->win, y+line, x+col++, text[i++]);
        }

        if (term_colors) wattroff(win->win, COLOR_PAIR(color));
}
void init_target_area()
{
        wclear(field->win);
        show_win(field);
}

point* get_random_point(int max_y, int max_x)
{
        point* p = malloc(sizeof(point));

        srand(time(NULL));
        p->y = rand() % max_y;
        p->x = rand() % max_x;
        return p;
}
point* get_new_target(mtWIN* win)
{
        point* p = get_random_point(win->height-2, win->width-4);

        if (p->y < 4) p->y = 3;    /* avoiding to cover the border */
        if (p->x < 4) p->x = 4;

        return p;
}


int main_cycle(game_conf* conf)
{
        int c, start_time, last_time, now, game_time;
        bool loop = TRUE;
        int exit_status = NEW_GAME;
        int game_status = GAME_RUNNING;
        point gunsight;
        unsigned int dist = 100;
        struct shot_item* next_shot = malloc(sizeof(struct shot_item));

        /* get a random target */
        conf->target = get_new_target(field);

        /* set up timer */
        if (conf->timer) {
                 start_time = (int)time(NULL);
                 last_time = (int)time(NULL);
                 game_time = TIME_VALUE;
        }

        /* update ammos */
        clear_ammo_info();
        upd_ammo_info(*conf);

        /* init the gunsight */
        gunsight.y = (int)field->height / 2;
        gunsight.x = (int)field->width / 2;
        draw_gunsight(field, gunsight, CYAN_ON_BLACK);

        /* start the cycle */
        wtimeout(field->win, 30);
        while (loop) {
                if (game_status > GAME_RUNNING) {
                        wtimeout(field->win, 0);
                        switch (game_status) {
                        case GAME_WIN:
                                set_msg("!!! BINATO !!!", MAGENTA_ON_BLACK);
                                break;
                        case GAME_LOSE:
                                set_msg("Hai perso...", CYAN_ON_BLACK);
                                break;
                        }
                        draw_gunsight(field, gunsight, NO_COLOR);
                        draw_border(field, field->border, FALSE);
                        draw_target(field, *(conf->target));
                        display_shots(next_shot);

                        while((c = wgetch(field->win)) != 'n' &&
                              c != 'N' &&
                              c != 'u' &&
                              c != 'U')
                                ;
                }
                else {
                        c = wgetch(field->win);
                }

                /* time management */
                if (conf->timer && game_status == GAME_RUNNING) {
                        now = (int)time(NULL);
                        if (now - last_time) {
                                game_time--;
                                last_time = now;
                                upd_time_info(game_time);
                        }
                        if (game_time == 0) {
                                conf->target = get_new_target(field);
                                start_time = (int)time(NULL);
                                game_time = TIME_VALUE;
                                set_msg("Nuovo bersaglio!!!", RED_ON_BLACK);
                        }
                }

                /* key pressed management */
                switch (c) {
                case 'u':
                case 'U':
                        loop = FALSE;
                        exit_status = EXIT_GAME;
                        break;
                case 'n':
                case 'N':
                        loop = FALSE;
                        exit_status = NEW_GAME;
                        break;
                case 'p':
                case 'P':
                        wtimeout(field->win, 0);
                        set_msg("IN PAUSA", CYAN_ON_BLACK);
                        while ((c=wgetch(msg->win)) != 'p' && c != 'P')
                                ;
                        clear_msg();
                        /* update last_time to preserv old countdown value */
                        last_time = (int)time(NULL);
                        wtimeout(field->win, 30);
                        break;
                case KEY_UP:
                        mv_info_gunsight(&gunsight, DIR_UP);
                        break;
                case KEY_RIGHT:
                        mv_info_gunsight(&gunsight, DIR_RIGHT);
                        break;
                case KEY_DOWN:
                        mv_info_gunsight(&gunsight, DIR_DOWN);
                        break;
                case KEY_LEFT:
                        mv_info_gunsight(&gunsight, DIR_LEFT);
                        break;
                case 's':
                case 'S':
                        conf->ammo_left--;
                        upd_ammo_info(*conf);
                        dist = distance(gunsight, *(conf->target));
                        light_the_lamp(dist);

                        /* save shots */
                        next_shot = push_shot(next_shot, gunsight, dist);

                        /* display all the shots */
                        display_shots(next_shot);

                        break;
                case 'C':
                        draw_target(field, *(conf->target));
                        set_msg("!!! IMBROGLIONE !!!", RED_ON_BLACK);
                        break;
                default:
                        break;
                }

                /* check if win or lose */
                if (dist < 2) game_status = GAME_WIN;
                else if (conf-> ammo_left == 0) game_status = GAME_LOSE;
        }

        free_shot_list(next_shot);
        return exit_status;
}

void draw_gunsight(mtWIN* win, point gs, int color)
{
        int vch, hch;

        /* rewrite window border */
        draw_border(win, win->border, FALSE);

        if (color == NO_COLOR) {
                vch = hch = ' ';
        }
        else {
                if (term_colors) wattron(win->win, COLOR_PAIR(color));
                vch = '|';
                hch = ACS_HLINE;
        }

        /* draw the gunsight */
        mvwaddch(win->win, gs.y, gs.x-2, hch);
        mvwaddch(win->win, gs.y, gs.x+2, hch);

        mvwaddch(win->win, gs.y-1, gs.x, vch);
        mvwaddch(win->win, gs.y+1, gs.x, vch);

        /* deactivate color */
        if (term_colors && color != NO_COLOR)
                wattroff(win->win, COLOR_PAIR(color));


        wrefresh(win->win);
}

void move_gunsight(mtWIN* win, point* gs, int dir)
{
        draw_gunsight(win, *gs, NO_COLOR);
        switch (dir) {
        case DIR_UP:
                if (gs->y > 1) gs->y--;       /* consider window border */
                break;
        case DIR_RIGHT:
                if (gs->x < win->width-2) gs->x++;
                break;
        case DIR_DOWN:
                if (gs->y < win->height-2) gs->y++;
                break;
        case DIR_LEFT:
                if (gs->x > 1) gs->x--;
                break;
        }
        draw_gunsight(win, *gs, CYAN_ON_BLACK);
}

unsigned int distance(point shot, point target)
{
        int true_dist, adjusted_dist, x_dist, y_dist;

        /* Siamo figli di Pitagora e di Caaaasaadeeeei ... */
        x_dist = abs(target.x - shot.x);
        y_dist = abs(target.y - shot.y);
        true_dist = floor(sqrt( pow(x_dist, 2) + pow(y_dist, 2)));

        /* adjust the distance considering that the target is wider than
         * higher */
        if (true_dist < 4) {
                switch (y_dist) {
                case 0:
                        if (x_dist == 3 || x_dist == 2) adjusted_dist = 1;
                        else adjusted_dist = true_dist;
                        break;
                case 1:
                        if (x_dist == 2) adjusted_dist = 1;
                        else adjusted_dist = true_dist;
                        break;
                default:
                        adjusted_dist = true_dist;
                        break;
                }
                return adjusted_dist;
        }
        else {
                return true_dist;
        }
}


void mv_info_gunsight(point* gs, int dir)
{
        char str[] = "00";

        /* clear the window, clear the shots */
        wclear(field->win);

        /* redraw the gunsight */
        move_gunsight(field, gs, dir);

        /* update coords on the panel */
        if (term_colors) wattron(panel->win, COLOR_PAIR(RED_ON_BLACK));
        sprintf(str, "%02i", gs->x);
        mvwaddstr(panel->win, 4, 36, str);
        sprintf(str, "%02i", gs->y);
        mvwaddstr(panel->win, 4, 45, str);
        wrefresh(panel->win);
        if (term_colors) wattroff(panel->win, COLOR_PAIR(RED_ON_BLACK));
}

void draw_target(mtWIN* win, point target)
{
        /* draw the target on the game window. the *target* x and y values
         * will be the coords for the center.
         *  _ _
         * / _ \
         *| (_) |
         * \_ _/
         */

        if (term_colors) wattron(win->win, COLOR_PAIR(MAGENTA_ON_BLACK));

        mvwaddch(win->win, target.y -2, target.x -1, '_');
        mvwaddch(win->win, target.y -2, target.x +1, '_');

        mvwaddch(win->win, target.y -1, target.x -2, '/');
        mvwaddch(win->win, target.y -1, target.x +2, '\\');

        mvwaddch(win->win, target.y, target.x -3, '|');
        mvwaddch(win->win, target.y, target.x +3, '|');

        mvwaddch(win->win, target.y, target.x -1, '(');
        mvwaddch(win->win, target.y, target.x +1, ')');

        mvwaddch(win->win, target.y -1, target.x , '_');
        mvwaddch(win->win, target.y, target.x , '_');

        mvwaddch(win->win, target.y +1, target.x -2, '\\');
        mvwaddch(win->win, target.y +1, target.x +2, '/');

        mvwaddch(win->win, target.y +1, target.x -1, '_');
        mvwaddch(win->win, target.y +1, target.x +1, '_');

        wrefresh(win->win);
        if (term_colors) wattroff(win->win, COLOR_PAIR(MAGENTA_ON_BLACK));
}

void clear_msg()
{
        wclear(msg->win);
        wrefresh(msg->win);
}
void set_msg(char* message, int color)
{
        /* print a centered message in the msg window
         */

        int x_pos = floor(msg->width / 2) - ceil(strlen(message) / 2);

        clear_msg();

        if (term_colors) wattron(msg->win, COLOR_PAIR(color));
        if (x_pos < 0) {
                message[msg->width] = '\0';    /* cut string */
                x_pos = 0;
        }
        mvwaddstr(msg->win, 0, x_pos, message);
        if (term_colors) wattroff(msg->win, COLOR_PAIR(color));
        wrefresh(msg->win);
}

struct shot_item* push_shot(struct shot_item* last, point coords,
                            unsigned int distance)
{
        struct shot_item* temp = malloc(sizeof(struct shot_item));

        if (temp) {
                last->next = temp;
                temp->prev = last;
                temp->next = NULL;
                last->coords = coords;
                last->distance = distance;
        }
        else {
                exit_ncurses();
                printf("Memory error.");
                exit(1);
        }
        return temp;
}

void display_shots(struct shot_item* last)
{
        struct shot_item* shot = last;
        int color = NO_COLOR;

        while (shot = shot->prev) {
                if (shot->distance < 2) color = CYAN_ON_BLACK;
                else if (shot->distance < 10) color = GREEN_ON_BLACK;
                else if (shot->distance < 20) color = YELLOW_ON_BLACK;
                else if (shot->distance < 30) color = RED_ON_BLACK;
                else if (shot->distance >= 30) color = WHITE_ON_BLACK;

                draw_shot(field, shot->coords, color);
        }
}

void draw_shot(mtWIN* win, point shot, int color)
{
        if (term_colors) wattron(win->win, COLOR_PAIR(color));
        mvwaddch(win->win, shot.y, shot.x, '+');
        if (term_colors) wattroff(win->win, COLOR_PAIR(color));
}

void free_shot_list(struct shot_item* last)
{
        struct shot_item* this = last;
        while (this = last->prev)
                free(this->next);
        free(this);
}
