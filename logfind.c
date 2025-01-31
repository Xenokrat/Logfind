#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include "dbg.h"

#define MAX_LINE_SIZE 10000
#define MAX_FPATH_SIZE 1000
#define MAX_LOG_FILES 100

const char *CONFIG_PATH = "/home/user/.logfind";

enum MatchMode {
	MATCH_AND,
	MATCH_OR
};

typedef struct Words {
	int wordc;
	char **wordv;
	enum MatchMode mode;
} Words;

typedef struct Grep {
	char *logfile_original_list[MAX_LOG_FILES];
	char *logfile_exp_list[MAX_LOG_FILES];
	unsigned int original_cnt;
	unsigned int exp_cnt;
	Words *words;
} Grep;

void r_trim(char *str);
void l_trim(char *str);
int Grep_read_log_config(Grep *);
int Grep_expand(Grep *grep);
void Grep_cleanup(Grep *grep);
int Grep_walk(Grep *grep);
void Words_clean(Words *words);
int Words_set(Words *words, int argc, char **argv);
int file_find_words(Grep *grep, FILE *file, const char* file_name);
int search_for_words(char *buffer, Words *words, enum MatchMode mode);

void 
r_trim(char *str) {
	int len = strlen(str) - 1;
	while (len >= 0 && isspace(str[len])) {
		len--;
	}
	str[len + 1] = '\0';
}

void 
l_trim(char *str)
{
	int i = 0;
	int first_non_space = 0;
	int chars_to_move = 0;

	int len = strlen(str);

	while (i < len && isspace(str[i])) {
        	i++;
	}
	if (i == 0) 
        	return;

	first_non_space = i;
	chars_to_move = len - i;

	for (i = 0; i < chars_to_move; i++, first_non_space++) {
        	str[i] = str[first_non_space];
	}
	str[i] = '\0';
}

int 
Grep_read_log_config(Grep *grep) {
	size_t i = 0;
	char *lf_path;
	char *line;
	char buffer[MAX_FPATH_SIZE] = {'\0'};

	FILE *config_p = fopen(CONFIG_PATH, "r");
	check(config_p, "Cannot open config file");

	for (i = 0; i < MAX_LOG_FILES; i++) {
		line = fgets(buffer, MAX_FPATH_SIZE, config_p);
		if (!line)
			break;

		r_trim(buffer);
		debug("Read from log-config: %s", buffer);

		lf_path = calloc(strlen(buffer) + 1, sizeof(char));
		grep->logfile_original_list[grep->original_cnt] = lf_path;
		strcpy(grep->logfile_original_list[grep->original_cnt], buffer);
		grep->original_cnt++;
	}

	#ifndef NDEBUG
	for (i = 0; i < grep->original_cnt; i++) {
		debug("Log files from config %s", grep->logfile_original_list[i]);
	}
	#endif

	check(i > 0, "Empty config file!");

	return 0;

error:
	return -1;
}


int 
Grep_expand(Grep *grep) {
	int rc = 0;
	size_t i = 0;

	wordexp_t word_buf;
	char **found = NULL;

	for (i = 0; i < grep->original_cnt; i++) {
		debug("Debug logfile i: %s %zu", grep->logfile_original_list[i], i);
		rc = wordexp(grep->logfile_original_list[i], &word_buf, 0);

		check(rc == 0, "Failed to expand");

		debug("Found %zu file matchs", word_buf.we_wordc);
		found = word_buf.we_wordv;

		for (i = 0; i < word_buf.we_wordc; i++) {
			grep->logfile_exp_list[grep->exp_cnt] = calloc(strlen(*found) + 1, sizeof(char));
			strcpy(grep->logfile_exp_list[grep->exp_cnt], *found);
			grep->exp_cnt++;
			found++;
		}
	}
	wordfree(&word_buf);

	for (i = 0; i < grep->exp_cnt; i++) {
		debug("Wordexp: %s", grep->logfile_exp_list[i]);
	}

	return 0;
error:
	wordfree(&word_buf);
	return -1;
}

int 
Grep_walk(Grep *grep) {
	size_t i = 0;
	size_t rc = 0;
	char *file_name;
	FILE *file = NULL;

	for (i = 0; i < grep->exp_cnt; i++) {
		file_name = grep->logfile_exp_list[i];
		file = fopen(file_name, "r");
		check(file, "Failed to open log file %s", grep->logfile_exp_list[i]);

		rc = file_find_words(grep, file, file_name);
		check(rc == 0, "Failed to process log file %s", grep->logfile_exp_list[i]);

		fclose(file);
	}

	return 0;
error:
	if (file) fclose(file);
	return -1;
}

void 
Grep_cleanup(Grep *grep) {
	size_t i = 0;

	for (i = 0; i <= grep->original_cnt; i++) {
		if (grep->logfile_original_list[i]) {
			free(grep->logfile_original_list[i]);
			grep->logfile_original_list[i] = NULL;
		}
	}

	for (i = 0; i <= grep->exp_cnt; i++) {
		if (grep->logfile_exp_list[i]) {
			free(grep->logfile_exp_list[i]);
			grep->logfile_exp_list[i] = NULL;
		}
	}

	for (i = 0; i <= grep->exp_cnt; i++) {
		if (grep->logfile_exp_list[i]) {
			free(grep->logfile_exp_list[i]);
			grep->logfile_exp_list[i] = NULL;
		}
	}

}

int 
Words_set(Words *words, int argc, char **argv)
{
	int i = 0;
	enum MatchMode mode = MATCH_AND;

	char **word_list = malloc(sizeof(char **) * argc);
	check_mem(word_list);

	words->wordv = word_list;
	words->wordc = 0;

	for (i = 0; i < argc; i++) {
		if (*argv[i] == '-') {
			switch (*(argv[i] + 1)) {
			case 'o':
				mode = MATCH_OR;
				break;
			default:
				sentinel("Unknown flag %c", *(argv[i] + 1));
			}
		} else {
			*(word_list + words->wordc) = calloc(strlen(argv[i]) + 1, sizeof(char));
			strcpy(*(word_list + words->wordc), argv[i]);
			words->wordc++;
		}
	}

	check(words->wordc > 0, "Should be at least 1 word!");
	words->mode = mode;

	return 0;

error:
	if (word_list) {
		for (i = 0; i < words->wordc; i++) {
			if (word_list[i]) {
				free(word_list[i]);
				word_list[i] = NULL;
			}
		}
		free(word_list);
		word_list = NULL;
		words->wordv = NULL;
	}
	return -1;
}

void 
Words_clean(Words *words)
{
	char **word_list = words->wordv;
	int wordc = words->wordc;
	int i = 0;

	if (word_list) {
		for (i = 0; i < wordc; i++) {
			if (word_list[i]) {
				free(word_list[i]);
				word_list[i] = NULL;
			}
		}
		free(words->wordv);
		words->wordv = NULL;
	}
}

int 
file_find_words(Grep *grep, FILE *file, const char* file_name)
{
	char *rc;
	char buffer[MAX_LINE_SIZE];
	int has_printed = 0;
	int is_found = 0;
	enum MatchMode mode = grep->words->mode;

	rc = fgets(buffer, MAX_LINE_SIZE, file);
	while (rc) {
		r_trim(buffer);
		l_trim(buffer);
		is_found = search_for_words(buffer, grep->words, mode);
		if (is_found) {
			if (!has_printed) {
				printf("%s\n", file_name);
				has_printed = 1;
			}
			printf("%s\n", buffer);
		}
		rc = fgets(buffer, MAX_LINE_SIZE, file);
	}

	printf("\n");
	return 0;  
}

int 
search_for_words(char *buffer, Words *words, enum MatchMode mode)
{
	char word_buf[MAX_LINE_SIZE];
	size_t i = 0;
	size_t line = 0;
	int rc = 0;

	/* copy word to buffer */
	while (buffer[line] && buffer[line != '\n']) {
		/* go to next word */
		while (isspace(buffer[line]))
			line++;
		for (i = 0; line < MAX_LINE_SIZE && isalpha(buffer[line]); line++, i++) {
			word_buf[i] = buffer[line];
		}
		word_buf[i] = '\0';
		for (i = 0; i < words->wordc; i++) {
			rc += (strcmp(word_buf, words->wordv[i]) == 0);
			if (mode == MATCH_OR && rc > 0)
				return 1;
		}
		line++;
	}

	return (rc == words->wordc);
}

int 
main(int argc, char *argv[]) {
	int rc = 0;

	Words words;
	Grep grep = {.original_cnt = 0, .exp_cnt = 0, .words = &words};

	rc = Words_set(&words, argc - 1, (argv + 1));
	check(rc == 0, "Failed to set up Words");

	rc = Grep_read_log_config(&grep);
	check(rc == 0, "Failed to read config");

	rc = Grep_expand(&grep);
	check(rc == 0, "Failed to expand glob");

	Grep_walk(&grep);

	Grep_cleanup(&grep);
	Words_clean(&words);
	return 0;

error:
	Grep_cleanup(&grep);
	Words_clean(&words);
	return -1;
}
