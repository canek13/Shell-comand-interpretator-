#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PARSING_LINE_SYMB " \t"
//#define PRINT_STRUCT
//#define TEST_STRUCT

/* 
 
*/
struct cmd_inf
	{
		char **argv;//СПИСОК ИЗ ИМЕНИ КОМАНД АРГУМЕНТОВ
		char *infile;// ПЕРЕНАЗНАЧЕННЫЙ ФАЙЛ ВВОДА
		char *outfile;// ПЕРЕНАЗАНЯЯЕННЫЙ ФАЙЛ ВЫВОДА
		char *apfile;//добавочный файл
		int background;// =1 ЕСЛИ КОМАНДА ПОДЛЖЕИТ ФОНОВОМУ ИСПОЛНЕНТЮ
		struct cmd_inf *psubcmd;// КОМАНДЫ ДЛЯ ЗАПКСКА В ДОЧЕРНЕМ ШЕЛЕ(ГДЕ СКОБКИ)
		struct cmd_inf *pipe;// СЛЕДУЩАЯ КОМАНДА ПОСЛЕ "|"
		struct cmd_inf *next;// СДЕДУЮЩАЯ ПОСЛЕ ';'(ИЛИ ПОСЛЕ "&")
		struct cmd_inf *condition_or;//след коамнда после ||
		struct cmd_inf *condition_and;//след команда после &&
	};

int flag_end_shell = 1;//флаг заверш. для окончания работы программы(ввод ctr+D)
int cur_tok = 0;//счетчик текущей позиции по токенам слов *words[]
int kol_tokens;

char *getstr(void);
char **parsing_line(char *line, int *kol_tokens);
struct cmd_inf *build_struct(char *words[], int kol_tokens);
int analysis_end_comand(struct cmd_inf *d, char *token);
struct cmd_inf *create_comand(struct cmd_inf *d, char *words[], int kol_tokens);
struct cmd_inf *new_struct(struct cmd_inf *s);
void write_struct(struct cmd_inf *d);
void print_struct(struct cmd_inf *d);
int check_infile_outfile(char *words[], struct cmd_inf *tmp);
int conveer(struct cmd_inf *d, int input, int output,int flag_psubcmd);
void delete_struct(struct cmd_inf *d);

int main()
{
	char *line;
	char **words;
	int input, output;
	/*char *s = "\0";
	printf("%ld\n", strlen(s));
	s = "a\0";
	printf("%ld\n", strlen(s));*/
	while(flag_end_shell)
	{
		printf(">");
		line = getstr();
		cur_tok = 0;

		words = parsing_line(line, &kol_tokens);
		//printf("kol_tokens = %d\n", kol_tokens);

		struct cmd_inf *d = build_struct(words, kol_tokens);//ПОСТРОЕНИЕ СТРУКТУРЫ ДАННЫХ ДЛЯ ШЕЛА
		struct cmd_inf *d_copy1 = d;
		
		#ifdef PRINT_STRUCT
			print_struct(d_copy1);
		#endif
		
		#ifndef TEST_STRUCT
			struct cmd_inf *d_copy2 = d;
			input = 0;
			output = 1;
			int flag_psubcmd = 0;//флаг для пайпа после дочернего шела
			if(d_copy2 != NULL)
				conveer(d_copy2,input,output,flag_psubcmd);
		#endif
		
		delete_struct(d);
		free(line);
		free(words);

		printf("\n");
	}
	printf("END SHELL\n");
	return 0;
}
/*
	this function builds structure from massive of strings "words"
*/
struct cmd_inf *build_struct(char *words[], int kol_tokens)
{
	struct cmd_inf *data = NULL;
	struct cmd_inf *dc;

	while(cur_tok < kol_tokens)//ИДЕМ ПО ТОКЕНАМ
	{
		//printf("%s\n", words[i]);
		//printf("%c\n", words[i][0]);

		if(data == NULL)// ПРИ ПЕРВОМ ТОКЕНЕ СОЗДАЕМ ПЕРВУЮ СТРКУКТУРУ
		{
			//printf("##1\n");
			data = new_struct(data);
			//printf("##2\n");
			data = create_comand(data, words,kol_tokens);
			//printf("##3\n");
			dc = data;
		} 
		else
		{
			int st = analysis_end_comand(dc,words[cur_tok]);
			//printf("st = %d\n", st);
			switch (st) {
				case 0: //ЕСЛИ ПААРАЛЛЕЛЬНО " | "
					//printf("ПААРАЛЛЕЛЬНО\n");
					dc -> pipe = new_struct(dc -> pipe); 
					dc = dc -> pipe;
					cur_tok++;
					dc = create_comand(dc,words,kol_tokens);
					break;
				case 1: // ЕСЛИ ПОСЛЕДОВАТЕЛЬНО " ; "
					dc -> next = new_struct(dc -> next);
					dc = dc -> next;
					cur_tok++;
					dc = create_comand(dc,words,kol_tokens);
					break;
				case 2:// ЕСЛИ ФОНОВО
					cur_tok++; 
					break;
				case 3:// ДОЧЕРНИЙ ШЕЛ
					cur_tok++;
					return data;
				case 4:// ПЕРЕХОД ПО ИЛИ ||
					dc -> condition_or = new_struct(dc -> condition_or);
					dc = dc -> condition_or;
					cur_tok++;
					dc = create_comand(dc,words,kol_tokens);
					break;
				case 5:// ПЕРЕХОД ПО И &&
					dc -> condition_and = new_struct(dc -> condition_and);
					dc = dc -> condition_and;
					cur_tok++;
					dc = create_comand(dc,words,kol_tokens);
					break;
			}
		}

	}
	return data;
}
/*
	this fucntion put down the information from tokens to structure 
	of current comand
*/
struct cmd_inf *create_comand(struct cmd_inf *tmp, char *words[],int kol_tokens)
{
	//printf("5\n");
	tmp -> argv = malloc(1 * sizeof(char *));
	int position = 0;
	//printf("4\n");
	do{
		if(!strcmp(words[cur_tok], "(") || (words[cur_tok][0] == '('))//ЕСЛИ ДОЧЕРНИЙ ШЕЛ
		{
			//printf("shell\n");
			cur_tok++;
			tmp -> psubcmd = build_struct(words, kol_tokens);
			
			check_infile_outfile(words,tmp);// ДОБ ВО ВТ
			if((cur_tok < kol_tokens) &&
				(analysis_end_comand(tmp,words[cur_tok]) != -1))//ЕСЛИ КОНЕЦ КОМАНДЫ ВЫЙТИ ИЗ СОЗДАНИЯ СТРУКТУРЫ ДЛЯ СОЗДАНИЯ НОВОЙ СТРКУТУРЫ
					break;
				//выход за границы
		}

		//check_infile_outfile(words,tmp);

		tmp -> argv[position] = words[cur_tok];//БЕРЕМ ОЧЕРЕДНОЙ ТОКЕН В СТРУКУТУРУ КОМАНДЫ
		position++;
		cur_tok++;
		//printf("pos = %d, word = %s, cur = %d\n", position, words[cur_tok],cur_tok);
		tmp -> argv = realloc(tmp -> argv, (position + 1) * sizeof(char *));//ПЕРЕОПРЕДЕЛЕНИЕ ПАМЯТИ ПОД КОМАНДУ С ПАРАМЕТРАМИ

		check_infile_outfile(words,tmp);//ПРОВЕРКА ВВОД\ВЫВОД ФАЙЛА

		if((cur_tok < kol_tokens) &&
			(analysis_end_comand(tmp,words[cur_tok]) != -1))//ЕСЛИ КОНЕЦ КОМАНДЫ ВЫЙТИ ИЗ СОЗДАНИЯ СТРУКТУРЫ ДЛЯ СОЗДАНИЯ НОВОЙ СТРКУТУРЫ
			break;
	}while(cur_tok < kol_tokens);
	//printf("1\n");
	tmp -> argv[position] = NULL;
	//printf("2\n");
	return tmp;
}
/*
	checks if token tell us input or output or append file
*/
int check_infile_outfile(char *words[], struct cmd_inf *tmp)
{
	//cur_tok++;
	if((cur_tok < kol_tokens) && !strcmp(words[cur_tok], ">")){
		cur_tok++;
		tmp -> outfile = words[cur_tok];
		cur_tok++;
		check_infile_outfile(words,tmp);
	}
	else
		if((cur_tok < kol_tokens) && !strcmp(words[cur_tok], "<")){
			cur_tok++;
			tmp -> infile = words[cur_tok];
			cur_tok++;
			check_infile_outfile(words,tmp);
		}
		else
			if((cur_tok < kol_tokens) && !strcmp(words[cur_tok], ">>")){
				cur_tok++;
				tmp -> apfile = words[cur_tok];
				cur_tok++;
				check_infile_outfile(words, tmp);
			}
	return 0;
}
/*
	analysis of the transition to a new comand
*/
int analysis_end_comand(struct cmd_inf *d, char *token)//АНАЛИЗ ОКОНЧАНИЯ КОМАНДЫ ДЛЯ ПЕРЕХОДА НА СЛЕД КОМАНДУ
{
	if(!strcmp(token, "|") || !strcmp(token, "&|"))
	{
		if(token[0] == '&')
			d -> background = 1;
		return 0;
	}
	else
		if(!strcmp(token, ";") || !strcmp(token, "&;"))
		{	
			if(token[0] == '&')
				d -> background = 1;
			return 1;
		}
		else
			if(!strcmp(token, "&")){
				d -> background = 1;
				return 2;
			}
			else
				if(!strcmp(token, ")") || (token[strlen(token) - 1] == ')')){
					return 3;
				}
				else
					if(!strcmp(token, "||"))
						return 4;
					else
						if(!strcmp(token, "&&"))
							return 5;
						else
							return -1;
}
/*
	this fucntion creates new element of structure
*/
struct cmd_inf *new_struct(struct cmd_inf *data)//СОЗДАНИЕ НОВОГО ЭЛЕМЕНТА СПИСКА ДАННЫХ
{
	data = malloc(sizeof(struct cmd_inf));
	data -> argv = NULL;
	data -> infile = NULL;
	data -> outfile = NULL;
	data -> apfile = NULL;
	data -> background = 0;
	data -> psubcmd = NULL;
	data -> pipe = NULL;
	data -> next = NULL;
	data -> condition_or = NULL;
	data -> condition_and = NULL;
	return data;
}
/*
	deletes all the structure
*/
void delete_struct(struct cmd_inf *d)
{
	if(d != NULL)
	{
		delete_struct(d -> psubcmd);
		delete_struct(d -> pipe);
		delete_struct(d -> next);
		delete_struct(d -> condition_or);
		delete_struct(d -> condition_and);
		free(d);
	}
}
/*
	prints the information in the current struct 'd' 
*/
#ifdef PRINT_STRUCT
void write_struct(struct cmd_inf *d)
{
  if(d != NULL)
  {
	int position = 0;
	printf("argv = \n");
	do{
		printf("	%s\n", d -> argv[position]);
		position++;
	}while(d -> argv[position] != NULL);
	printf("background = %d\n", d -> background);
	printf("infile = %s\n", d -> infile);
	printf("outfile = %s\n", d -> outfile);
	printf("apfile = %s\n", d -> apfile);
	//printf("pipe = %d\n", d -> pipe);
	if(d -> psubcmd != NULL)
		printf("pSUBCMD has struct\n");
	else
		printf("no pSUBCMD\n");
	if(d -> pipe != NULL)
		printf("PIPE has struct\n");
	else
		printf("no PIPE\n");
	if(d -> next != NULL)
		printf("NEXT has struct\n");
	else
		printf("no NEXT\n");
	if(d -> condition_or != NULL)
		printf("condition_OR has struct\n");
	else
		printf("no condition_OR\n");
	if(d -> condition_and != NULL)
		printf("condition_AND has struct\n");
	else
		printf("no condition_AND\n");
  }
}
/*
	this function prints struct
*/
void print_struct(struct cmd_inf *d)
{
	if (d != NULL)
	{
		write_struct(d);//вывод текущей структуры
		if(d -> psubcmd != NULL)
			print_struct(d -> psubcmd);
		if(d -> pipe != NULL)
			print_struct(d -> pipe);//вывод ветви pipe
		if(d -> next != NULL)
			print_struct(d -> next);
		if(d -> condition_or != NULL)
			print_struct(d -> condition_or);
		if(d -> condition_and != NULL)
			print_struct(d -> condition_and);
	}
}
#endif
/*
	this function reads line from stdin
*/
char *getstr(void)
{
    int c;
    char *ptr = malloc(4);
    int i = 0;

    while ((c = getchar()) != '\n' && (c != EOF)){
        *(ptr + i) = c;
        i++;
        if(i % 4 == 0)
            ptr = realloc(ptr, 4 + i);
    }
    *(ptr+i) = '\0';
    if (c == EOF)
    	flag_end_shell = 0;
	return ptr;
}
/*
	this function parses line in comands and arguments
*/
char **parsing_line(char *line, int *position)//ПАРСИНГ СТРОКИ
{
	char *token;
	int bufsize = 8;
	char **tokens = malloc(bufsize*sizeof(char *));
	*position = 0;

	token = strtok(line, PARSING_LINE_SYMB);//КЛАДЕМ В СТРОКУ TOKEN СИМВОЛЛЫ ДО "РАЗДЕЛЕНИЯ"
	while(token != NULL)//ПОКА ЕСТЬ ЧТО РАЗДЕЛЯТЬ(ТОКЕНЫ)
	{
		tokens[*position] = token;//КЛАДЕМ НА ПОЗИЦИЮ УКАЗАТЕЛЬ НА СТРОКУ
		(*position)++;

		if(*position % bufsize == 0)
		{
			tokens = realloc(tokens, (bufsize + bufsize) * sizeof(char *));
		}
		token = strtok(NULL, PARSING_LINE_SYMB);
	}
	tokens[*position] = NULL;//в последний(пустой) кладем null
	return tokens;
}