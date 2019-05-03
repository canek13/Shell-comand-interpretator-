#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <signal.h>
#include <fcntl.h>

int cmd_count;

struct cmd_inf
	{
		char **argv;//СПИСОК ИЗ ИМЕНИ КОМАНД АРГУМЕНТОВ
		char *infile;// ПЕРЕНАЗНАЧЕННЫЙ ФАЙЛ ВВОДА
		char *outfile;// ПЕРЕНАЗАНЯЯЕННЫЙ ФАЙЛ ВЫВОДА
		char *apfile;
		int background;// =1 ЕСЛИ КОМАНДА ПОДЛЖЕИТ ФОНОВОМУ ИСПОЛНЕНТЮ
		struct cmd_inf *psubcmd;// КОМАНДЫ ДЛЯ ЗАПКСКА В ДОЧЕРНЕМ ШЕЛЕ(ГДЕ СКОБКИ)
		struct cmd_inf *pipe;// СЛЕДУЩАЯ КОМАНДА ПОСЛЕ "|"
		struct cmd_inf *next;// СДЕДУЮЩАЯ ПОСЛЕ ';'(ИЛИ ПОСЛЕ "&")
		struct cmd_inf *condition_or;
		struct cmd_inf *condition_and;
	};

struct cmd_inf *chek_free(struct cmd_inf *);

pid_t do_cmd(struct cmd_inf *d,int input, int output)
{
	pid_t pid;
	int flag = 1;

	if((pid = fork()) == 0)
	{
		if(input != 0)
		{
			dup2(input,0);
			close(input);
		}
		if(output != 1)
		{
			dup2(output, 1);
			close(output);
		}

		if(d -> background == 1){//ФОНОВЫЙ РЕЖИМ
			signal(SIGINT, SIG_IGN);/*установка игнорировани сигнала 
			SIGINT - приходящий сигнал, Ctrl+C 
			SIG_IGN - флаг игнорирования сигнала*/
			int f1 = open("/dev/null", O_RDONLY);
			int f2 = open("/dev/null", O_WRONLY);
			dup2(f1, input);
			dup2(f2, output);
			close(f1);
			close(f2);
			flag = 0;
			--cmd_count;
		}
		
		if(flag){//ОБЫЧНЫЕ ПРОЦЕССЫ
			return execvp(d -> argv[0], d -> argv);
			perror("команда не найдена\n");
		}
		else//ЕСЛИ ФОНОВЫЙ РЕЖИМ
			if(fork() == 0){
				return execvp(d -> argv[0], d -> argv);
				perror("комнда не найдена\n");
			}
			else
				//return 0;
				exit(0);
	}
	else
		if(pid < 0)
			return -1;
		else
			return pid;
}

int conveer(struct cmd_inf *d, int input, int output,int flag_psubcmd)
{
//	printf("conveer starts\n");

	struct cmd_inf *d_start = d;
	cmd_count = 0;
	int return_status;
	pid_t pid;
	int fd[2];
	FILE *input_file;
	FILE *output_file;
	int flcon_and = 1, flcon_or = 1;
	int flag_common;

	if(d -> infile != NULL){
		input_file = fopen(d -> infile, "r");
		input = fileno(input_file);
	}
	if(d -> outfile != NULL){
		output_file = fopen(d -> outfile, "w");
		output = fileno(output_file);
	}
	else
		if(d -> apfile != NULL){
			output_file = fopen(d -> apfile, "a");
			output = fileno(output_file);
		}

	while(d != NULL)//пока все след структуры не ноль
	{ 
		//ФЛАГИ БЛОКИРОВКИ ПРИ УСЛОВНОМ ВЫПОЛНЕНИИ
		if(!flcon_and){
			if(d -> condition_and){
				d = d -> condition_and;
				continue;
			}
			else
				if(d -> condition_or){
					d = d-> condition_or;
					flcon_and = 1;
					continue;
				}
				else{
					d = chek_free(d);
					if(d == NULL)
						break;
				}
		}
		if(!flcon_or){
			if(d -> condition_and){
				d = d -> condition_and;
				flcon_or = 1;
				continue;
			}
			else
				if(d -> condition_or){
					d = d -> condition_or;
					continue;
				}
				else{
					d = chek_free(d);
					if(d == NULL)
						break;
				}
		}

		if(d -> psubcmd != NULL)
		{//ДОЧЕРНИЙ ШЕЛ
			pid_t pid;
			int status, log_res;

			if(d -> infile != NULL){
				input_file = fopen(d -> infile, "r");
				input = fileno(input_file);
			}
			if(d -> outfile != NULL){
				output_file = fopen(d -> outfile, "w");
				output = fileno(output_file);
			}
			else
				if(d -> apfile != NULL){
					output_file = fopen(d -> apfile, "a");
					output = fileno(output_file);
				}	
			
			if(d -> pipe != NULL){//если следом после () идет труба
				pipe(fd);
				output = fd[1];
				flag_psubcmd = 1;
			}
			if(d -> background != 1)
				log_res = conveer(d -> psubcmd, input, output,flag_psubcmd);
			else
				if((pid = fork()) == 0){
					conveer(d -> psubcmd,input, output,flag_psubcmd);
					exit(0);
				}

			if(d -> pipe != NULL){
				d = d -> pipe;
				close(fd[1]);
				input = fd[0];
			}
			else
				if(d -> next != NULL){
					d = d -> next;
					input = 0;
					output = 1;
				}
				else
					if(d -> condition_and){
						d = d -> condition_and;
						input = 0;
						output = 1;
						flcon_and = log_res && flcon_and; //!(log_res || flcon_and);
					}
					else
						if(d -> condition_or){
							d = d -> condition_or;
							input = 0;
							output = 1;
							flcon_or = !(log_res || !flcon_or);
						}
						else
						{
							d = d -> pipe;
							input = 0;
							output = 1;
						}
			continue;
		}
		
		if(d -> condition_and != NULL){
			int status;
			pid = do_cmd(d,input,output);
			cmd_count++;
			for(int i = 0; i < cmd_count; i++)
				if(pid == wait(&status))
					return_status = status;
			if(d -> infile != NULL){
				input_file = fopen(d -> infile, "r");
				input = fileno(input_file);
			}
			if(d -> outfile != NULL){
				output_file = fopen(d -> outfile, "w");
				output = fileno(output_file);
			}
			else
				if(d -> apfile != NULL){
					output_file = fopen(d -> apfile, "a");
					output = fileno(output_file);
				}

			d = d -> condition_and;
			flcon_and = (WIFEXITED(return_status) && !WEXITSTATUS(return_status));
			//printf("%d\n", flcon_and);
		}
		else
			if(d ->condition_or != NULL)
			{
				int status;
				pid = do_cmd(d,input,output);
				cmd_count++;
				for(int i = 0; i < cmd_count; i++)
					if(pid == wait(&status))
						return_status = status;
				if(d -> infile != NULL){
					input_file = fopen(d -> infile, "r");
					input = fileno(input_file);
				}
				if(d -> outfile != NULL){
					output_file = fopen(d -> outfile, "w");
					output = fileno(output_file);
				}
				else
					if(d -> apfile != NULL){
						output_file = fopen(d -> apfile, "a");
						output = fileno(output_file);
					}

				d = d -> condition_or;
				flcon_or = !(WIFEXITED(return_status) && !WEXITSTATUS(return_status));
				//printf("%d\n", flcon_or);
			}
		else
			if(d -> pipe != NULL){
				pipe(fd);
				do_cmd(d,input,fd[1]);
				cmd_count++;
				close(fd[1]);
				input = fd[0];
				d = d -> pipe;
				flcon_or = 1;
				flcon_and = 1;
			}
			else
				if(d -> next != NULL){//ПОСЛЕДОВАТЕЛЬНО
						if(d -> infile != NULL){
							input_file = fopen(d -> infile, "r");
							input = fileno(input_file);
						}
						if(d -> outfile != NULL){
							output_file = fopen(d -> outfile, "w");
							output = fileno(output_file);
						}
						else
							if(d -> apfile != NULL){
								output_file = fopen(d -> apfile, "a");
								output = fileno(output_file);
							}
					do_cmd(d, input, output);
					cmd_count++;
					for(int i = 0; i < cmd_count; i++)
						wait(NULL);
					cmd_count = 0;
					input = 0;
					if (!flag_psubcmd)
						output = 1;
					d = d -> next;
					flcon_or = 1;
					flcon_and = 1;
				}
				else
					{//ЕСЛИ ВСЕГО ОДНА КОМАНДА (ПОСЛЕДНЯЯ)
						if(d -> infile != NULL){
							input_file = fopen(d -> infile, "r");
							input = fileno(input_file);
						}
						if(d -> outfile != NULL){
							output_file = fopen(d -> outfile, "w");
							output = fileno(output_file);
						}
						else
							if(d -> apfile != NULL){
								output_file = fopen(d -> apfile, "a");
								output = fileno(output_file);
							}
						//printf("output2 = %d\n", output);
						do_cmd(d,input,output);
						cmd_count++;
						d = d -> pipe;//ПЕРЕХОД  В ПУСТОЕ МЕСТО ДЛЯ ЗАВЕРШЕНИЯ ЦИКЛА
					}
	}
	int status;
	for(int i = 0; i < cmd_count; i++)
	{
		wait(&status);	
	}
	flag_common = (WIFEXITED(status) && !WEXITSTATUS(status));
	if(d_start -> infile != NULL)
		fclose(input_file);
	if(d_start -> outfile != NULL)
		fclose(output_file);
	//printf("end conveer %d\n",flag_common && (flcon_and || !flcon_or);
	return (flag_common && (flcon_and || !flcon_or));
}

struct cmd_inf *chek_free(struct cmd_inf *d)
{
	if (d -> pipe != NULL)
		return d -> pipe;
//	if (d -> psubcmd != NULL)
//		return d -> psubcmd;
	if (d -> next != NULL)
		return d -> next;
	else
		return NULL;
}