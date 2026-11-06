#ifndef PHILEAS_EDITOR_H
#define PHILEAS_EDITOR_H

#include "buffer.h"
#include "calaix.h"

/* Accions que s'agrupen en un sol pas de desfer si es repeteixen seguides */
enum {
    ACT_NONE,
    ACT_TYPE,       /* escriure: una paraula per pas */
    ACT_BACKSPACE,
    ACT_DELETE,
};

/* Mode de desplaçament vertical (màquina d'escriure) */
enum {
    TW_SALT,    /* en passar el límit, el text salta i el cursor queda a l'ancoratge */
    TW_FIXA,    /* el cursor es queda sempre a la fila de l'ancoratge */
    TW_CLASSIC, /* el cursor pot arribar a dalt i a baix de tot */
};

/* Mode enfocament: la resta del text s'atenua */
enum {
    FOCUS_NO,
    FOCUS_PARAGRAPH,
    FOCUS_SENTENCE,
};

enum {
    SEL_NONE,   /* sense selecció */
    SEL_SHIFT,  /* Maj+fletxes: un moviment sense Maj la cancel·la */
    SEL_MARK,   /* Ctrl+Espai: els moviments l'amplien fins que es cancel·la */
};

/* Elements amb color configurable (temes: theme.c). Els de la barra, la
 * selecció i els menús, si no tenen cap color, van en vídeo invers (barra i
 * selecció) o amb els colors del text (menús). */
enum { PAL_H1, PAL_H2, PAL_H3, PAL_CODE, PAL_QUOTE, PAL_ERROR, PAL_MARK,
       PAL_TEXT, PAL_BG, PAL_BAR, PAL_BAR_BG, PAL_SEL, PAL_SEL_BG,
       PAL_MENU, PAL_MENU_BG, PAL_N };
#define PAL_DEFAULT (-1)   /* el color normal del terminal */

#define MAX_LANGS 4

typedef struct {
    Buffer  buf;
    char   *filename;          /* NULL si el document encara no té nom */

    size_t  cy, cx;            /* cursor: línia i byte dins la línia */
    int     want_x;            /* columna visual desitjada per a ↑/↓; -1 = la del cursor */
    size_t  top_line, top_row; /* primera fila visual a dalt de la pantalla */
    int     top_blank;         /* files en blanc per sobre de l'inici del document */
    int     recenter;          /* posar el cursor a l'ancoratge en el pròxim ed_layout */

    int     sel_mode;
    Pos     anchor;            /* extrem fix de la selecció */

    char   *clip;              /* porta-retalls intern */
    size_t  clip_len;

    int     columns;           /* amplada de text configurada */
    int     center;            /* columna de text centrada a la pantalla */
    int     margin_left;       /* columnes buides a l'esquerra de la pantalla */
    int     margin_right;      /* i a la dreta, si el terminal és estret */
    int     tw_mode;           /* TW_* */
    int     tw_limit;          /* % de l'alçada que el cursor no passa en baixar (salt) */
    int     tw_anchor;         /* % de l'alçada on queda el cursor després d'un salt */
    int     colors;            /* colors de Markdown */
    int     focus;             /* FOCUS_* */
    int     typo;              /* tipografia en escriure: -- « » … */
    int     typo_apos;         /* i l'apòstrof tipogràfic */
    short   palette[PAL_N];    /* colors 0-15 (els 8 de ncurses i els clars) o PAL_DEFAULT */
    int     theme;             /* índex del tema (theme_at) */
    int     console_palette;   /* a la consola de Linux, aplicar els colors rgb del tema */
    int     spell;             /* marcar els errors ortogràfics */
    char    dict[64];          /* diccionari triat: "ca", "es" (F8), o el de -d o «idioma» */
    char    dict_loaded[64];   /* el que hi ha carregat; "" = cap */
    char    calaix[1024];      /* fitxer del calaix d'idees (opció «calaix») */
    char    calaix_pi[1024];   /* calaix rebut de la Pi amb «envia» (opció «calaix-pi») */
    char    obs_vault[1024];   /* vault d'Obsidian (opció «obsidian-vault»); buit = cap */
    char    obs_inbox[128];    /* carpeta d'entrada del vault («obsidian-inbox») */
    char    obs_tipus[64];     /* propietat del tipus de nota («obsidian-tipus») */
    CalaixState cstate;        /* font activa del calaix i fonts recents */
    char    langs[MAX_LANGS][16]; /* rotació de F8 */
    int     nlangs;
    int     spell_start;       /* activar l'ortografia en arrencar */
    int     width, height;     /* àrea de text real */
    int     margin;            /* columna de pantalla on comença el text */

    int     last_act;          /* ACT_* de l'última acció, per agrupar el desfer */
    wchar_t last_ch;
    Pos     act_end;           /* on va quedar el cursor després de l'última acció */

    char    search[256];       /* últim text cercat */

    int     autosave;          /* segons d'inactivitat abans de desar; 0 = desactivat */
    int     autosave_max;      /* màxim de segons amb canvis sense desar */
    int     autosave_last;     /* últim valor actiu, per tornar a activar-lo */
    double  last_input;        /* moment de l'última tecla */
    double  dirty_since;       /* primer canvi sense desar; 0 = cap */
    int     autosave_warned;   /* ja s'ha avisat que el document no té nom */
    int     remember_pos;      /* en obrir un document, tornar on era el cursor */

    char    msg[256];          /* missatge de la línia inferior */
    int     quit;
    int     quit_confirm;      /* s'ha avisat de canvis sense desar */
} Editor;

void  ed_init(Editor *e, int columns);
void  ed_free(Editor *e);

/* Substitueix el document per un de nou (path NULL: sense nom). Si path no
 * existeix, comença un fitxer nou amb aquest nom. Retorna -1 amb errno si
 * no es pot llegir, i el document actual no canvia. */
int   ed_open(Editor *e, const char *path);
void  ed_msg(Editor *e, const char *fmt, ...);

/* Adapta l'àrea de text a la mida del terminal i fa visible el cursor. */
void  ed_layout(Editor *e, int screen_cols, int screen_lines);

Line *ed_line(Editor *e, size_t y);      /* línia amb el wordwrap calculat */
size_t ed_cursor_row(Editor *e);
int   ed_cursor_x(Editor *e);            /* columna dins l'àrea de text */

/* Avancen o recules una fila de pantalla (incloses les files buides de
 * separació de paràgraf); retornen 0 als extrems del document. */
int   ed_vpos_next(Editor *e, size_t *line, size_t *row);
int   ed_vpos_prev(Editor *e, size_t *line, size_t *row);

/* Moviment */
void  ed_move_left(Editor *e);
void  ed_move_right(Editor *e);
void  ed_move_up(Editor *e);
void  ed_move_down(Editor *e);
void  ed_row_start(Editor *e);
void  ed_row_end(Editor *e);
void  ed_page_up(Editor *e);
void  ed_page_down(Editor *e);
void  ed_word_prev(Editor *e);
void  ed_word_next(Editor *e);
void  ed_par_prev(Editor *e);
void  ed_par_next(Editor *e);
void  ed_doc_start(Editor *e);
void  ed_doc_end(Editor *e);
void  ed_goto_line(Editor *e, size_t n);   /* n comença a 1 */

/* Selecció */
int   ed_selection(Editor *e, Pos *a, Pos *z);   /* 1 si n'hi ha i no és buida */
void  ed_sel_start(Editor *e, int mode);
void  ed_sel_clear(Editor *e);
void  ed_select_all(Editor *e);

/* Edició: si hi ha selecció, primer s'esborra (la substitueix) */
void  ed_insert(Editor *e, const char *s, size_t n);
void  ed_newline(Editor *e);     /* continua llistes i cites Markdown */
void  ed_backspace(Editor *e);
void  ed_delete(Editor *e);
void  ed_delete_word_prev(Editor *e);
void  ed_delete_word_next(Editor *e);

/* Intercanvia els blocs de línies [a, b) i [b, c) (a < b < c <= nlines),
 * dins del mateix pas de desfer. */
void  ed_swap_lines(Editor *e, size_t a, size_t b, size_t c);

/* Mou el paràgraf del cursor (o les línies de la selecció) per sobre de
 * l'anterior (dir < 0) o per sota del següent. Retorna 0 a l'extrem. */
int   ed_move_par(Editor *e, int dir);

/* Frase de s on és pos: [*a, *b). Acaba darrere de . ! ? o … (i les
 * cometes o parèntesis que els segueixin) quan ve un espai o el final; els
 * espais de darrere hi pertanyen. */
void  text_sentence(const char *s, size_t len, size_t pos, size_t *a, size_t *b);

/* Part de la línia del cursor que no s'atenua amb l'enfocament. */
void  ed_focus_range(Editor *e, size_t *a, size_t *b);

/* Tipografia: es crida just després d'escriure el caràcter ch. Si en
 * completa un («--» seguit d'un altre caràcter, «<<», «>>», «...» o, amb
 * l'opció, l'apòstrof entre lletres), el substitueix en un pas de desfer
 * propi: Ctrl+Z torna a deixar el que s'havia escrit. No actua dins del
 * codi, les metadades, l'HTML, les taules ni les línies de guions.
 * Retorna 1 si ha substituït. */
int   ed_typography(Editor *e, wchar_t ch);

/* Desfer/refer. ed_begin s'ha de cridar abans de cada acció: decideix si
 * comença un grup nou de desfer o continua l'anterior (escriure seguit,
 * esborrar seguit). ed_end recorda on ha quedat el cursor. */
void  ed_begin(Editor *e, int act, wchar_t ch);
void  ed_end(Editor *e);
int   ed_undo(Editor *e);
int   ed_redo(Editor *e);

/* Selecciona el tram [a, z), amb el cursor al final. */
void  ed_select(Editor *e, Pos a, Pos z);

/* Porta-retalls: retornen el nombre de caràcters, o 0 si no hi havia res */
size_t ed_copy(Editor *e);
size_t ed_cut(Editor *e);
size_t ed_paste(Editor *e);

#endif
