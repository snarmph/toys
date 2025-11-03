#include <time.h>

#include "colla/colla.c"
#include "term.c"

#include "wordle_la.h"
#include "wordle_ta.h"

typedef struct word_t word_t;
struct word_t {
    char str[5];
};

darr_define(word_arr_t, word_t);

typedef struct hset_t hset_t;
struct hset_t {
    word_t *words;
    u32 *hashes;
    int size;
    int collisions_count;
};

struct {
    bool quit;
    bool valid;
    bool found;
    bool finish;
    word_t word;
    int letters[26];
    char guess[5];
    char guesses[5][5];
    int tries;
    int count;
    hset_t words;
    word_arr_t *valid_guesses;
    int total_count;
} app = {0};

hset_t hset_init(arena_t *arena, usize pow2_size) {
    usize count = 1ull << pow2_size;
    return (hset_t) {
        .words = alloc(arena, word_t, count),
        .hashes = alloc(arena, u32, count),
        .size = (int)count,
    };
}

word_t word_from_strv(strview_t v) {
    word_t w = {0};
    memmove(w.str, v.buf, 5);
    return w;
}

u32 hset_hash(const char buf[5]) {
    return (buf[0] - 'a') * 456976 +  // 26^4
           (buf[1] - 'a') * 17576 +   // 26^3
           (buf[2] - 'a') * 676 +     // 26^2
           (buf[3] - 'a') * 26 +      // 26^1
           (buf[4] - 'a');            // 26^0
}

void hset_add(hset_t *set, strview_t word) {
    colla_assert(word.len == 5);
    u32 hash = hset_hash(word.buf);
    usize index = hash & (set->size - 1);

    for (usize i = index; i < set->size; ++i) {
        if (set->hashes[i] == 0) {
            set->hashes[i] = hash;
            set->words[i] = word_from_strv(word);
            return;
        }
        else {
            // debug
            set->collisions_count++;
        }
    }

    for (usize i = 0; i < index; ++i) {
        if (set->hashes[i] == 0) {
            set->hashes[i] = hash;
            set->words[i] = word_from_strv(word);
            return;
        }
        else {
            // debug
            set->collisions_count++;
        }

    }

    fatal("could not fit word in set");
}

word_t *hset_get(hset_t *set, strview_t word) {
    u32 hash = hset_hash(word.buf);
    usize index = hash & (set->size - 1);

    for (usize i = index; i < set->size; ++i) {
        if (set->hashes[i] == hash) {
            if (memcmp(set->words[i].str, word.buf, 5) == 0) {
                return &set->words[i];
            }
        }
    }

    for (usize i = 0; i < index; ++i) {
        if (set->hashes[i] == hash) {
            if (memcmp(set->words[i].str, word.buf, 5) == 0) {
                return &set->words[i];
            }
        }
    }

    return NULL;
}

int wordle_load(strview_t data, arena_t *arena) {
    int count = 0;

    instream_t in = istr_init(data);
    while (!istr_is_finished(&in)) {
        strview_t line = istr_get_view_len(&in, 5);
        istr_skip_whitespace(&in);
        if (strv_is_empty(line) || line.len != 5) {
            break;
        }

        if (arena) {
            darr_push(arena, app.valid_guesses, word_from_strv(line));
        }

        hset_add(&app.words, line);
        count++;
    }

    return count;
}

word_t choose_random_word(void) {
    int chosen = rand() % app.total_count;


    for_each (chunk, app.valid_guesses->head) {
        if (chunk->count < chosen) {
            chosen -= (int)chunk->count;
            continue;
        }

        return chunk->items[chosen];
    }

    colla_assert(false, "no valid word found?");
    return (word_t){0};
}

void wordle_reset(void) {
    app.quit = false;
    app.valid = false;
    app.found = false;
    app.finish = false;
    memset(&app.word, 0, sizeof(app.word));
    memset(app.letters, 0, sizeof(app.letters));
    memset(app.guess, 0, sizeof(app.guess));
    app.tries = 0;

    app.word = choose_random_word();
}

strview_t get_input(arena_t *arena) {
    TCHAR buffer[1024] = {0};
    DWORD read = 0;
    ReadConsole((HANDLE)os_win_conin().data, buffer, arrlen(buffer), &read, NULL);
    str_t input = str_from_tstr(arena, tstr_init(buffer, read));
    return strv_trim(strv(input));
}

const char *get_char_color(int p, char c, bool *found) {
    if (app.word.str[p] == c) {
        found[p] = true;
        return "green";
    }

    for (int i = 0; i < 5; ++i) {
        if (found[i]) continue;
        if (app.word.str[i] == c) {
            found[i] = true;
            return "yellow";
        }
    }

    return "white";
}

bool check_guess(void) {
    for (int g = 0; g < 5; ++g) {
        int index = app.guess[g] - 'a';
        int prev = app.letters[index];
        if (prev > 1) continue;

        int result = 3;
        for (int w = 0; w < 5; ++w) {
            if (app.guess[g] == app.word.str[w]) {
                result = g == w ? 2 : 1;
                break;
            }
        }

        if (result < prev) continue;
        app.letters[index] = result;
    }

    return memcmp(app.word.str, app.guess, 5) == 0;
}

bool app_update(arena_t *arena, float dt, void *udata) {
    COLLA_UNUSED(udata); COLLA_UNUSED(dt); COLLA_UNUSED(arena);
    strview_t guess = strv(app.guess, app.count);

    app.valid = app.count == 5 && hset_get(&app.words, guess);

    return app.quit;
}

str_t app_view(arena_t *arena, void *udata) {
    COLLA_UNUSED(udata);
    outstream_t out = ostr_init(arena);

    ostr_puts(&out, strv("\n "));

    strview_t guess = strv(app.guess, app.count);
    ostr_puts(&out, strv("<yellow>> </>"));

    if (app.valid) {
        ostr_puts(&out, strv("<green>"));
    }
    else {
        ostr_puts(&out, strv("<red>"));
    }
    ostr_print(&out, "%v</>\n", guess);

    ostr_puts(&out, strv("\n "));

    for (int i = 0; i < arrlen(app.letters); ++i) {
        char c = (char)i + 'a';
        strview_t colors[] = {
             cstrv("dark_grey"), cstrv("yellow"), cstrv("green"), cstrv("red"),
        };

        colla_assert(app.letters[i] < arrlen(colors));

        ostr_print(&out, "<%v>%c</> ", colors[app.letters[i]], c);
    }

    ostr_puts(&out, strv("\n"));

    for (int i = 0; i < app.tries; ++i) {
        char *try = app.guesses[i];
        bool found[5] = {0};
        ostr_puts(&out, strv("\n  "));
        for (int k = 0; k < 5; ++k) {
            ostr_print(&out, "<%s>%c</>", get_char_color(k, try[k], found), try[k]);
        }
    }

    if (app.finish) {
        if (app.found) {
            ostr_print(&out, "\n\n you guessed the words in <green>%d</> tries!\n", app.tries);
        }
        else {
            ostr_print(&out, "\n\n you didn't guess the word, it was <red>%.5s</>\n", app.word.str);
        }

        ostr_puts(&out, strv(" press 'c' to continue or 'q' to quit\n"));
    }

    return ostr_to_str(&out);
}

void app_event(termevent_t *event, void *udata) {
    COLLA_UNUSED(udata);

#define IS(v) strv_equals(strv(event->value), strv(v))

    switch (event->type) {
        case TERM_EVENT_KEY:
            if (app.finish) {
                if (IS("c")) {
                    wordle_reset();
                }
                if (IS("q")) {
                    app.quit = true;
                }

                break;
            }


            if (IS("escape") || IS("ctrl+c")) {
                app.quit = true;
            }
            else if (IS("backspace")) {
                if (app.count) app.count--;
            }
            else if (IS("enter")) {
                if (app.valid) {
                    if (check_guess()) {
                        app.found = true;
                        app.finish = true;
                    }

                    memmove(app.guesses[app.tries++], app.guess, 5);
                    app.count = 0;
                    
                    if (app.tries >= 5) {
                        app.finish = true;
                    }
                }
            }
            else if (event->value.len == 1) {
                char c = event->value.buf[0];
                if (!char_is_alpha(c)) {
                    break;
                }
                if (app.count < 5) {
                    app.guess[app.count++] = char_lower(c);
                }
            }
        default: break;
    }
}

int main() {
    colla_init(COLLA_OS);

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    app.words = hset_init(&arena, 18);

    app.total_count = wordle_load(strv((char*)debug_wordle_la_txt, debug_wordle_la_txt_len), &arena);
    wordle_load(strv((char*)debug_wordle_ta_txt, debug_wordle_ta_txt_len), NULL);

    srand((uint)time(NULL));

    app.word = choose_random_word();
    
    term_init(&(termdesc_t){
            .fullscreen = true,
            .app = {
                .update = app_update,
                .event = app_event,
                .view = app_view,
            },
        }
    );

    term_run();
}
