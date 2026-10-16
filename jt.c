#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define TAG_SIZE 34

typedef struct {
	char tag[TAG_SIZE];
	int32_t tag_number;
} Tag;

typedef struct {
	struct tm tmdate;
	int32_t total_time;
	int32_t record_number;
	int32_t record_first_index;
	int32_t tag_number;
} Date;

typedef struct {
	char tag[TAG_SIZE];
	struct tm tmdate;
	time_t time;
	Tag *parent_tag;
	Date *parent_date;
} Record;

typedef struct {
	struct tm tmdate;
	char* tag;
	int32_t time;
	int32_t total_time;
} Query;

typedef struct {
	int32_t h_streak;
	int32_t c_streak;
	int32_t total_time;
	int32_t record_number;
	int32_t record_cap;
	int32_t unique_tags_number;
	int32_t unique_tags_cap;
	int32_t date_number;
	int32_t date_cap;
	Tag *unique_tags;
	Record *records;
	Date *dates;
} Header;

char *help = "\
Options:\n\
  e\t\topen database in editor.\n\
  a <arg>\tdump all data; arg is number of days to show and is optional. 0 prints everything.\n\
  g <arg>\tprint graph; arg is number of days to show and is optional. 0 prints everything.\n\
  q\t\tlook up by tag name.\
";

void kys (char *message) {
	fprintf(stderr, "The program FUCKING died. Here's the possible cause: %s\n", message);
	exit(EXIT_FAILURE);
}

uint8_t record_append (Header *header, Record *new)
{
	if (header->records == NULL) {
		header->records = (Record *)malloc(sizeof(Record) * 512);
		if (header->records == NULL) return 1;
		header->record_cap = 512;
		header->records[header->record_number++] = *new;
		return 0;
	}

	if (header->record_number >= header->record_cap) {
		Record *tmp = (Record *)realloc(header->records, 
			sizeof(Record) * header->record_cap * 2
		);
		if (tmp == NULL) return 1;
		header->records = tmp;
		header->record_cap *= 2;
		header->records[header->record_number++] = *new;
		return 0;
	}

	header->records[header->record_number++] = *new;
	return 0;
}

uint8_t tag_append (Header *header, Tag *new)
{
	if (header->unique_tags == NULL) {
		header->unique_tags = (Tag *)malloc(sizeof(Tag) * 32);
		if (header->unique_tags == NULL) return 1;
		header->unique_tags_cap = 32;
		header->unique_tags[header->unique_tags_number++] = *new;
		return 0;
	}

	if (header->unique_tags_number >= header->unique_tags_cap) {
		Tag *tmp = (Tag *)realloc(header->unique_tags, 
			sizeof(Tag) * header->unique_tags_cap * 2
		);
		if (tmp == NULL) return 1;
		header->unique_tags = tmp;
		header->unique_tags_cap *= 2;
		header->unique_tags[header->unique_tags_number++] = *new;
		return 0;
	}

	header->unique_tags[header->unique_tags_number++] = *new;
	return 0;
}

uint8_t date_append (Header *header, Date *new)
{
	if (header->dates == NULL) {
		header->dates = (Date *)malloc(sizeof(Date) * 256);
		if (header->dates == NULL) return 1;
		header->date_cap = 256;
		header->dates[header->date_number++] = *new;
		return 0;
	}

	if (header->date_number >= header->date_cap) {
		Date *tmp = (Date *)realloc(header->dates, 
			sizeof(Date) * header->date_cap * 2
		);
		if (tmp == NULL) return 1;
		header->dates = tmp;
		header->date_cap *= 2;
		header->dates[header->date_number++] = *new;
		return 0;
	}

	header->dates[header->date_number++] = *new;
	return 0;
}

void database_line_validate (char *buf, int line_num)
{
	char *mask = "nnnn-nn-nn nn:nn";
	int i = 0;
	while (i < 16) {
		if ( 
			*(mask + i) == 'n' &&
			!(
				*(buf + i) >= '0' && 
				*(buf + i) <= '9'
			)
		) { goto error; }

		if ( 
			*(mask + i) != 'n' &&
			!( *(buf + i) == *(mask + i) ) 
		) { goto error; }

		i++;
		continue;
		error:
			char error[128];
			sprintf(error, "this line in database is fucked up: %d", line_num);
			kys(error);
	}
}

int32_t tag_check_if_exists (Header *header, char *query)
{
	if (header->unique_tags == NULL)
		return 0;
	for (int i = 0; i < header->unique_tags_number; i++) {
		if (!strncmp(query, header->unique_tags[i].tag, TAG_SIZE)) {
			return i + 1;
		}
	}

	return 0;
}

void date_check_if_same_date (Header *header)
{
	time_t last_rec = mktime(&header->records[header->record_number - 1].tmdate);
	time_t last_date = 0;
	if (header->date_number == 0) last_date = 0;
	else last_date = mktime(&header->dates[header->date_number - 1].tmdate);

	if (last_rec == last_date) {
		header->dates[header->date_number - 1].record_number++;
		header->dates[header->date_number - 1].total_time += header->records[header->record_number - 1].time;
	} else {
		Date new = (Date) { 
			.tmdate = header->records[header->record_number - 1].tmdate,
			.total_time = header->records[header->record_number - 1].time,
			.record_first_index = header->record_number - 1,
			.record_number = 1,
		};
		date_append(header, &new);
	}
}

void database_read (Header *header, FILE *f)
{
	// ...because safe code was never the goal
	char buf[100] = {0};
	char year[5] = {0};
	char mon[3] = {0};
	char day[3] = {0};
	char hour[3] = {0};
	char min[3] = {0};
	char tag[TAG_SIZE] = {0};
	int line_num = 0;
	while (fgets(buf, 100, f)) { // REDO
		line_num++;
		if (buf[0] == '#') continue; // comment
		if (buf[0] == '\n') continue; // skip empty new lines

		database_line_validate(buf, line_num);

		// records have to follow this format: YYYY-MM-DD HH:MM X...
		// X being the tag
		strncpy(year, buf, 5); 
		strncpy(mon, buf + 5, 3);
		strncpy(day, buf + 8, 3);
		strncpy(hour, buf + 11, 3);
		strncpy(min, buf + 14, 3);
		strncpy(tag, buf + 17, TAG_SIZE);
		// number added to buf is offset
		tag[strlen(tag) - 1] = '\0'; // make sure last byte is not a new line
		struct tm a = {0};
		a.tm_year = atol(year) - 1900;
		a.tm_mon = atol(mon) - 1;
		a.tm_mday = atol(day);
		Record tmp = (Record) {
			.tmdate = a,
			.time = 
				atol(hour) * 3600
				+ atol(min) * 60,
			.tag = {0},
			.parent_tag = 0,
		};
		strncpy(tmp.tag, tag, TAG_SIZE);
		header->total_time += tmp.time;

		if (record_append(header, &tmp))
			kys("memory alloc killed itself (record alloc). rest in piss.");

		if (int x = tag_check_if_exists(header, tag)) {
			header->unique_tags[x - 1].tag_number++;
			header->records[ header->record_number - 1 ].parent_tag = &header->unique_tags[x - 1];
		} else {
			Tag tmp_t;
			tmp_t.tag_number = 1;
			strncpy(tmp_t.tag, tag, TAG_SIZE);
			if(tag_append(header, &tmp_t))
				kys("memory alloc killed itself (tags alloc). rest in piss.");
			header->records[ header->record_number - 1 ].parent_tag = 
			&header->unique_tags[header->unique_tags_number - 1];
			// ok bro
		}

		date_check_if_same_date(header);
		header->records[ header->record_number - 1 ].parent_date =
		&header->dates[header->date_number - 1];
		// XDDDDDDDDDDDDD
	}
}

uint8_t is_leap_year (int year) {
	if (!(year % 4) && year % 100)
			return 1;
	if (!(year % 100) && !(year % 400))
			return 1;
	return 0;
}

void streak_calc (Header *header)
{
	if (header->records == NULL) return;
	if (header->record_number < 2) return;

	int streak = 0;
	int prev = 0;
	int is_current = 1;

	for (int i = 0; i < header->record_number; i++) {

		// omfg this fucking library is making me cry
		time_t tmp = mktime(&header->records[i].tmdate);
		struct tm *a = localtime(&tmp);
		int cur_day = a->tm_yday;
		tmp = mktime(&header->records[prev].tmdate);
		a = localtime(&tmp);
		int prev_day = a->tm_yday;

		int cur_year = header->records[i].tmdate.tm_year;
		int prev_year = header->records[prev].tmdate.tm_year;
		// printf("[%d], %d - %d\n", i, cur_day, prev_day); // debug

		// TEST - CHANGE ALL streak = 0; to streak = 1;
		if (i == 0) {
			tmp = time(NULL);
			a = localtime(&tmp);
			int prev_day = a->tm_yday;
			if (cur_day == prev_day - 1 || cur_day == prev_day)  { streak++; }
			else { is_current = 0; streak++; } // TEST - added one streak under this condition
			if (is_current) header->c_streak = streak;
			if (streak > header->h_streak) header->h_streak = streak;
			continue;
		}

		if (cur_year == prev_year) {
			if (cur_day == prev_day - 1) { streak++; }
			else if (cur_day == prev_day) { }
			else { streak = 1; is_current = 0; }
		}

		if (cur_year == prev_year - 1) {
			if (is_leap_year(cur_year))
				if (cur_day == 365 && prev_day == 0) { streak++; }
				else { streak = 1; is_current = 0; }
			else
				if (cur_day == 364 && prev_day == 0) { streak++; }
				else { streak = 1; is_current = 0; }
		}

		if (is_current) header->c_streak = streak;
		if (streak > header->h_streak) header->h_streak = streak;

		prev++;
	}
}

uint8_t compare_dates (struct tm *date1, struct tm *date2)
{
	time_t x = mktime(date1);
	time_t y = mktime(date2);

	if (x == 0 || y == 0) return 0;
	if (x == y) return 1;
	return 0;
}

void print_all_data (Header *header, int max)
{
	if (max > header->record_number || max == 0) max = header->record_number;

	puts("ALL RECORDS:");
	for (int i = 0; i < max; i++) {
		printf("[record: %d]\ndate\t%d-%02d-%02d\ntag\t\"%s\"\ntime\t%.2f hours\n\n",
			i + 1, 
			header->records[i].tmdate.tm_year + 1900,
			header->records[i].tmdate.tm_mon + 1,
			header->records[i].tmdate.tm_mday,
			header->records[i].tag, (float)header->records[i].time / 3600
		);
	}
	if (header->record_number - max > 0) printf("... and %d more\n\n", header->record_number - max);
	puts("ALL TAGS:");
	for (int i = 0; i < header->unique_tags_number; i++) {
		if (strcmp(header->unique_tags[i].tag, ""))
			printf("[tag: %d] %s %d\n",
				i + 1,
				header->unique_tags[i].tag,
				header->unique_tags[i].tag_number
			);
		else
			printf("[tag: %d] (no tag) %d\n", i + 1, header->unique_tags[i].tag_number);
	}
	printf("\n");
	puts("ALL DAYS:");
	for (int i = 0; i < header->date_number; i++) {
			printf("[date: %d]\ndate\t\t%d-%02d-%02d\ntotal time\t%.2fh (%d mins)\nfirst index\t%d\nrec number\t%d\ntags\t\t", i + 1,
				header->dates[i].tmdate.tm_year + 1900,
				header->dates[i].tmdate.tm_mon + 1,
				header->dates[i].tmdate.tm_mday,
				(float)header->dates[i].total_time / 3600,
				header->dates[i].total_time / 60,
				header->dates[i].record_first_index, 
				header->dates[i].record_number
			);
			for (int j = header->dates[i].record_first_index; j < header->dates[i].record_first_index + header->dates[i].record_number; j++) {
				printf("\"%s\" ", header->records[j].parent_tag->tag);
			}
			printf("\n\n");
	}
	puts("MISC DATA:");
	{
		time_t today = time(NULL);
		printf("today is: %s", ctime(&today));
	}
	printf("total time: %.2f hours\n", (float)header->total_time / 3600);
	printf("unique tag number: %d\n", header->unique_tags_number);
	printf("record number: %d\n", header->record_number);
	printf("current streak: %d\n", header->c_streak);
	printf("highest streak: %d\n", header->h_streak);
	printf("\n");
}

void mkbar (char *bar, char* color, int time, int highest_time)
{
	time = highest_time - (highest_time - time);
	time = time / (highest_time / 14);

	if (time < 14 / 4) 			strncpy(color, "\x1B[38;2;255;204;229m", 30); // light pink
	else if (time <= 14 / 3) 	strncpy(color, "\x1B[38;2;255;153;204m", 30);
	else if (time <= 14 / 2) 	strncpy(color, "\x1B[38;2;255;102;178m", 30);
	else if (time <= 14 * 0.6) 	strncpy(color, "\x1B[38;2;255;51;153m", 30);
	else if (time <= 14 * 0.8) 	strncpy(color, "\x1B[38;2;255;0;127m", 30);
	else 					strncpy(color, "\x1B[38;2;204;0;102m", 30); // dark pink

	for (int i = 0; i < time; i++) {
		bar[i] = '@';
	}
	for (int i = time; i < 14; i++) {
		bar[i] = '-';
	}
}

void print_graph (Header *header, int max)
{
	if (max > header->date_number || max == 0) max = header->date_number;

	int highest_time = 0;
	char *reset = "\x1B[m";

	for (int i = 0; i < max; i++) {
		if (header->dates[i].total_time > highest_time)
			highest_time = header->dates[i].total_time;
	}
	for (int i = 0; i < max; i++) {
		char bar[15] = {"-"};
		char color[30] = {0};
		mkbar(bar, color, header->dates[i].total_time, highest_time);
		printf("#%d\t|%d-%02d-%02d|%s%s%s|%05.2fh\n",
			i + 1,
			header->dates[i].tmdate.tm_year + 1900,
			header->dates[i].tmdate.tm_mon + 1,
			header->dates[i].tmdate.tm_mday,
			color,
			bar,
			reset,
			(float)header->dates[i].total_time/3600
		);
	}
	if (header->date_number - max > 0) printf("... and %d more\n\n", header->date_number - max);
}

void database_input_from_user ();

int32_t main (int32_t argc, char *argv[])
{
	Header header = (Header) {
		.h_streak = 0,
		.c_streak = 0,
		.record_number = 0,
		.record_cap = 0,
		.unique_tags_number = 0,
		.unique_tags_cap = 0,
		.unique_tags = NULL,
		.records = NULL,
	};

	char *fpath = getenv("JT_PATH");
	char *editor = getenv("JT_EDITOR");
	if (fpath == NULL) { puts("JT_PATH is not set."); return 1; }
	if (editor == NULL) { puts("JT_EDITOR is not set."); return 1; }
	FILE *f = fopen(fpath, "r");
	char editor_command[256];
	sprintf(editor_command, "%s %s", editor, fpath);

	database_read(&header, f);
	streak_calc(&header);

	int arg = 0;
	if (argc > 2) { arg = atol(argv[2]); }
	if (arg < 0) { kys("arg less than 0"); }

	if (argc > 1) {
		if (!strcmp(argv[1], "e")) { system(editor_command); }
		// else if (!strcmp(argv[1], "q")) { puts("query"); }
		else if (!strcmp(argv[1], "g")) { print_graph(&header, arg); }
		else if (!strcmp(argv[1], "a")) { print_all_data(&header, arg); }
		else if (!strcmp(argv[1], "h")) { puts(help); }
		else { puts("option not found."); }
	} else { print_graph(&header, 30); }

	return 0;
}
