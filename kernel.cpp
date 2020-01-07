__asm("jmp kmain");

#define VIDEO_BUF_PTR	(0xb8000)
#define IDT_TYPE_INTR	(0x0E)
#define IDT_TYPE_TRAP	(0x0F)
#define GDT_CS		(0x8) 		// селектор секции кода, установленный загрузчиком ОС
#define PIC1_PORT	(0x20)


// Базовый порт управления курсором текстового экрана. Подходит для большинства, но может отличаться в других BIOS и в общем случае адрес должен быть прочитан из BIOS data area.
#define CURSOR_PORT	(0x3D4)	
#define VIDEO_WIDTH	(80) 		// Ширина текстового экрана



//------------------------------------------------------------------------------ ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ -------------------------------------------------------------------

typedef void (*intr_handler)();
char buffer[40];
int buf_i = 0;
int shift = 0;
int COLOR = 0x07;			//Цвет надписей в программе(изначально серый)
int line = 0;				//Текущая линия на которой находится курсор и куда выводить инф-ию
int colum = 0;
char codes_low[] = 				
{
	0,0,'1','2','3','4','5','6','7','8','9','0', '-', '=',0,0,
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',' ',0,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '<','>','+',0,'\\',
	'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',0,'*',0,' ',
};						//чары для вывода после сканирования кода с клавиатуры
char codes_up[]=
{      '0',  //0
                               '0',  //ESC -- 1
                               '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
                               '0',  //BKSP -- 14
                               '0',  //TAB -- 15
                               'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
                               '0', //ENTER -- 28
                               '0', //LCTRL -- 29
                               'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
                               '0', //LSHIFT -- 42
                               '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
                               '0', // RSHIFT -- 54
                               '0', // LALT, RALT -- 56
                               '0', // SPACE -- 57
                               '0', // CAPS -- 58
                               '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', //F1--F10  -- 59 -- 68
                               '0', // NUM Lock -- 69
                               '0', // scrl lock -- 70
                               '0', // home -- 71
                               '0', // up arrow -- 72
                              };

//------------------------------------------------------------------------------- ФУНКЦИИ -------------------------------------------------------------------

//Команды клавиатуры

void keyb_process_keys();
void keyb_handler();
void keyb_init();
void on_key(unsigned int scan_code);

//Прерывания 

void default_intr_handler();
void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr);
void intr_init();
void intr_start();
void intr_enable();
void intr_disable();


//Курсор

void cursor_moveto(unsigned int strnum, unsigned int pos);

// Прописанные команды
void command_handler();
void print_symbl(int scan_code,int str,int col);
int strcmp(const char *str,int n);
void cls();
void set_color();
void nsconv();
void posixtime();
void wintime();
void szconv();



//-------------------------------------------------------------------------------------- СТРУКТУРЫ --------------------------------------------------------------------------------------

struct idt_entry			// Структура описывает данные об обработчике прерывания
{
	unsigned short base_lo;		// Младшие биты адреса обработчика
	unsigned short segm_sel;	// Селектор сегмента кода
	unsigned char always0;		// Этот байт всегда 0
	unsigned char flags;		// Флаги тип. Флаги: P, DPL, Типы - это константы - IDT_TYPE...
	unsigned short base_hi;		// Старшие биты адреса обработчика

} __attribute__((packed));		 // Выравнивание запрещено

struct idt_ptr				// Структура, адрес которой передается как аргумент команды lidt
{
	unsigned short limit;
	unsigned int base;

} __attribute__((packed)); 		// Выравнивание запрещено

struct idt_entry g_idt[256]; 		// Реальная таблица IDT
struct idt_ptr g_idtp;			// Описатель таблицы для команды lidt



static inline unsigned char inb (unsigned short port) 				// Чтение из порта
{
	unsigned char data;
	asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
	return data;
}
static inline void outb (unsigned short port, unsigned char data) 		// Запись
{
	asm volatile ("outb %b0, %w1" : : "a" (data), "Nd" (port));
}


//------------------------------------------------------------------------------ СОДЕРЖАНИЕ ФУНКЦИЙ-----------------------------

void out_str(const char* ptr, unsigned int strnum)
{
	unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
	video_buf += 80*2 * strnum;
	while (*ptr)
	{
		video_buf[0] = (unsigned char) *ptr; 
		video_buf[1] = COLOR; 
		video_buf += 2;
		ptr++;
		colum++;
	}
}

extern "C" int kmain()//-------------------------------------------------------- ГЛАВНАЯ ФУНКЦИЯ --------------------------------------------
{
	set_color();
	cls();
	intr_disable();
	intr_init();
	keyb_init();
	intr_start();
	intr_enable();
	const char* hello = "Welcome to ConvertOS!";
	out_str(hello, line);
	line++;
	colum = 0;
	cursor_moveto(line,colum);
	out_str("#",line);
	colum++;
	cursor_moveto(line,colum);
	
	while(1)
	{
		asm("hlt");
	}
	return 0;
}


//Команды клавиатуры


void on_key(unsigned int scan_code) // -------------------------------------------------- ДЕЙСТВИЕ С НАЖАТОЙ КЛАВИШЕЙ ----------------------------
{
	int i;
	if(scan_code == 28)
	{
		buffer[buf_i] = 0;
		command_handler();
		for(i = 0; i < buf_i;i++)
		  buffer[i] = '\0';
		buf_i = 0;
		line++;
		colum = 0;
		out_str("# ",line);
		cursor_moveto(line,colum);
		return;
	}
	if( scan_code == 14)
	{
		if(colum <=2)
		{
			return;
		}
		buf_i--;
		colum--;
		unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR;
		video_buf += 80 * 2 * line + (colum)*2;
		video_buf[0] = '\0';
		cursor_moveto(line, colum);
		return;
	}
	if(scan_code == 42)
	{
		shift = 1;
		print_symbl(scan_code, line, colum);
		return;
	}
	if(scan_code == 54)
	{
		shift = 1;
		print_symbl(scan_code, line, colum);
		return;
	}

	print_symbl(scan_code, line, colum);
	return;
	

}


void keyb_handler()//------------------------------------------------------- ОБРАБОТЧИК НАЖАТИЙ КЛАВИАТУРЫ ---------------------------------------
{
	asm("pusha");			// Обработка поступивших данных
	keyb_process_keys();		// Отправка контроллеру 8259 нотификации о том, что прерывание обработано
	outb(PIC1_PORT, 0x20);
	asm("popa; leave; iret");
}



void keyb_init()//------------------------------------------------------------------ Регистрация обработчика прерывания ------------------------------------------------
{
	intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler);		// segm_sel=0x8, P=1, DPL=0, Type=Intr. Разрешение только прерываний клавиатуры от контроллера 8259
	outb(PIC1_PORT + 1, 0xFF ^ 0x02); 						// 0xFF - все прерывания, 0x02 -бит IRQ1 (клавиатура). Разрешены будут только прерывания, чьи биты установлены в 0
}



void keyb_process_keys()
{
	if (inb(0x64) & 0x01)								// Проверка что буфер PS/2 клавиатуры не пуст (младший бит присутствует)
	{
		unsigned char scan_code;
		unsigned char state;
		scan_code = inb(0x60); 							// Считывание символа с PS/2 клавиатуры
		if (scan_code < 128)							// Скан-коды выше 128 - это отпускание клавиши
		{
			on_key(scan_code);
		}
		else
		{
		  if(scan_code == 170 || scan_code == 182)
		    shift = 0;		
		}
	}
}


//Прерывания

//---------------------------------------------------------- Пустой обработчик прерываний. Другие обработчики могут быть реализованы по этому шаблону -----------------------------
void default_intr_handler()		
{
	asm("pusha");
	// ... (реализация обработки)
	asm("popa; leave; iret");
}


void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr)
{
	unsigned int hndlr_addr = (unsigned int) hndlr;
	g_idt[num].base_lo = (unsigned short) (hndlr_addr & 0xFFFF);
	g_idt[num].segm_sel = segm_sel;
	g_idt[num].always0 = 0;
	g_idt[num].flags = flags;
	g_idt[num].base_hi = (unsigned short) (hndlr_addr >> 16);
}


//------------------------------------------------------------------- Функция инициализации системы прерываний: заполнение массива с адресами обработчиков ---------------------------------
void intr_init()			
{
	int i;
	int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
	for(i = 0; i < idt_count; i++)
		intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR,default_intr_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
}



void intr_start()
{
	int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
	g_idtp.base = (unsigned int) (&g_idt[0]);
	g_idtp.limit = (sizeof (struct idt_entry) * idt_count) - 1;
	asm("lidt %0" : : "m" (g_idtp) );
}



void intr_enable()
{
	asm("sti");
}



void intr_disable()
{
	asm("cli");
}


//Курсор 
//----------------------------------------------------------------------- ПЕРЕМЕЩЕНИЕ КУРСОРА ----------------------------------------------------------
void cursor_moveto(unsigned int strnum, unsigned int pos)
{
	if (strnum >=24)
	{
		cls();
		line = colum = 0;
		pos = strnum = 0;
	}

	if (pos > 42)
	{
		unsigned char *video_buf = (unsigned char*) VIDEO_BUF_PTR;
		video_buf += (80*line+colum)*2;
		video_buf[0] = '\0';
		pos--;
	}
	unsigned short new_pos = (strnum * 80) + pos;
	outb(CURSOR_PORT, 0x0F);
	outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
	outb(CURSOR_PORT, 0x0E);
	outb(CURSOR_PORT + 1, (unsigned char)( (new_pos >> 8) & 0xFF));
	line = strnum;
	colum = pos;
	
}

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$  Прописанные команды  $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$



//----------------------------------------------------------------- КОМАНДА СРАВНЕНИЯ ПОЛУЧЕННОЙ СТРОКИ С БУФЕРОМ ------------------------------
int strcmp(const char*str,int n)
{
	unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR;
	video_buf += 80 * 2 * line + 4;
	int i = 0;
	for(i = 0;i < n;i++)
	{
		if(str[i] != *(video_buf +i*2))
		{
		return 0;
		}
	}
	return 1;
}
//--------------------------------------------------------------- ВЫВОД НА ЭКРАН НАЖАТОГО СИМВОЛА ----------------------------------------------------------
void print_symbl(int scan_code,int str,int col)
{
	char letter;
	unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR;
	if(shift)
	{
	  letter = codes_up[scan_code];
	}
	else
	{
	  letter = codes_low[scan_code];
	}

	buffer[buf_i] = letter;
	buf_i++;
	video_buf+=80 * 2 * str + 2 * col;
	video_buf[0] = (unsigned char)letter;
	video_buf[1] = COLOR;

	colum++;
	cursor_moveto(line,colum);
}

//------------------------------------------------------------------ ОБРАБОТЧИК ВВЕДЁННЫХ КОМАНД -------------------------------------------------------------------
void command_handler()
{
	int check = 0;
	if (strcmp ("info",4))
	{
		line++;
		colum = 0;
		out_str("ConvertOS GCC edition.", line);
		line++;
		colum = 0;
		out_str("Bootloader Translator - GNU,syntax - AT&T,Mother OS - Ubuntu 16.04", line);
		line++;
		colum = 0;
		out_str("Developed by Chagochkin Alexandr.SPbPU 2018 group - 23656/3", line);
		return;
	}

	if (strcmp ("clear",5))
	{
		cls();
		return;
	}

	if (strcmp ("nsconv",6))
	{
		nsconv();
		return;
	}

	if (strcmp ("szconv",6))
	{
		//szconv();
		return;
	}
	
	if (strcmp ("posixtime",9))
	{
		posixtime();
		return;
	}

	if (strcmp ("wintime",7))
	{
		wintime();
		return;
	}

	if (strcmp("shutdown",8))
	{
		outb(0xf4,0x00);
	}

	line++;
	out_str("incorrect input", line);

}
//-------------------------------------------------------------------------------- ОЧИСТКА ЭКРАНА -----------------------------------
void cls()
{
	int i;
	unsigned char *video_buf = (unsigned char*) VIDEO_BUF_PTR;
	for (i = 0; i < 80 *24*2; i++)
	{
		video_buf[0] =  0;
		video_buf += 2;
	}
}

//----------------------------------------------------------------------------------------- ОЧИСТКА БУФЕРА ----------------------------------------------
//------------------------------------------------------------------------ УСТАНАВЛИВАЕМ ЦВЕТ В СООТВЕТСТВИИ С ПЕРЕДАННЫМ ЗНАЧЕНИЕМ ----------------------------------
void set_color()
{
	int adres = *(int*)0x9000;
	switch(adres)
	{
	case 0:
	    COLOR = 0x08;
	    break;
	case 1:
	    COLOR = 0x07;
	    break;
	case 2:
	    COLOR = 0x0e;
	    break;
	case 3:
	    COLOR= 0x01;
	    break;
	case 4:
	    COLOR = 0x04;
	    break;
	case 5:
	    COLOR = 0x02;
	    break;
	default:
	   break;
	}
}
//--------------------------------------------------------------------------------- КОНВЕРТИРОВАНИЕ СИСТЕМ ИСЧИСЛЕНИЯ ----------------------------------------------
void nsconv()
{
	char NumCh[27],FromCh[3],ToCh[3],result[10],tempCh; // leftover[40];
	int gear = 0,temp = 0,FromInt = 0,ToInt=0,lenNum,lenFrom,lenTo,x,j;
	unsigned int NumInt = 0,check =0;

	while(buffer[gear] != ' ')
	  gear++;
	gear++;
	while(buffer[gear] != ' ')					//переводимое число
	{
	  if(buffer[gear] == 0)
	  {
	   line++;
	   out_str("incorrect input - only number", line);
	   return;
	  }
	  NumCh[temp] = buffer[gear];
	  gear++;
	  temp++;
	  if(temp == 27 && buffer[gear] != ' ')
	  {
	    line++;
	    out_str("incorrect input - lost number", line);
	    return;
	  }
	}
	lenNum = temp;
	NumCh[temp] = '\0';
	temp = 0;
	gear++;

	while(buffer[gear] != ' ')					//Начальная система исчисления
	{
	  if(buffer[gear] == 0)
	  {
	   line++;
	   out_str("incorrect input - only number and initial base", line);
	   return;
	  }
	  if(temp == 2)
	    break;
	  if(buffer[gear] > 57 || buffer[gear] < 48){line++;out_str("Wromg symbol of initial base",line);return;}
	  FromCh[temp]= buffer[gear];
	  gear++;
	  temp++;
	  if(temp == 2 && buffer[gear] != ' ')
	  {
	    line++;
	    out_str("incorrect input - lost initial base", line);
	    return;
	  }
	}
	lenFrom = temp;
	FromCh[temp] = '\0';
	temp = 0;
	gear++;
	while(buffer[gear] != '\0' && buffer[gear] != ' ')		// Конечная систем исчисления
	{
	  if(temp == 2)
	    break;
	  if(buffer[gear] > 57 || buffer[gear] < 48){line++;out_str("Wromg symbol of final base",line);return;}
	  ToCh[temp] = buffer[gear];
	  gear++;
	  temp++;
	}	
	if(temp < 1 || temp > 2)
	{
	     line++;
	     out_str("incorrect input - lost final base", line);
	     return;
	}
	lenTo = temp;
	if(buffer[gear] !='\0'){line++;out_str("Too much arguments",line);return;}
	ToCh[temp] = '\0';
	x = 1;
	gear = 0;

	//------------------------------------------------------------ПЕРЕВОД ИЗ СТРОКИ В ЧИСЛО
	while(FromCh[gear] !='\0' && gear < 2)						//Начальная база
	{
	  if(int(FromCh[gear]) > 57 || int(FromCh[gear]) < 48)
	  {
	    line++;
	    out_str("incorrect symbol of initial base ", line);
		return;
	  }
	  FromInt *= x;
	  FromInt += (int(FromCh[gear])-48);
	  x *= 10;
	  gear++;
	}

	if(FromInt > 32 || FromInt < 2)
	{
	  line++;
	  out_str("incorrect size initial base", line);
		return;
	}

	x = 1;
	gear = 0;
	ToInt = 0;
	while(ToCh[gear] !='\0' && gear < 2)						//Конечная база
	{
	  if(int(ToCh[gear]) > 57 || int(ToCh[gear]) < 48)
	  {
	    line++;
	    out_str("incorrect final base", line);
		return;
	  }
	  ToInt *= x;
	  ToInt += (int(ToCh[gear])-48);
	  x *= 10;
	  gear++;
	}
	
	if(ToInt >32 || ToInt < 2)
	{
	  line++;
	  out_str("incorrect final base", line);
		return;
	}
						// Перевод полученного числа из char в int и тут же из Начальной базы в 10ую
	x = 1;
	temp = j = 0;
	
	gear = 0;
	while(gear < lenNum)
	{
	  check = NumInt;
	  if(int(NumCh[gear]) > 47 && int(NumCh[gear] < 58))
		temp = int(NumCh[gear])- 48;
	  else if(int(NumCh[gear]) > 96 && int(NumCh[gear] < 123))
		temp = int(NumCh[gear])- 87;
	  else
	  {
	      line++;
	      out_str("incorrect symbol of number", line);
		return;
	  }

	  if(temp >= FromInt)
	  {
	    line++;
	    out_str("incorrect symbol of number for that base", line);
	    return;
	  }

	  for(j = 0;j < (lenNum-1)-gear;j++)
	    x*=FromInt;

	  temp = temp * x;
	  x = 1;
	  NumInt += temp;  
	  
	  gear++;
	  if(NumInt < check)
	  {
	    line++;
	    out_str("Number is too big", line);	
		return;
	  }

	}
	gear = 0;					// Перевод получеyного числа в нужную систему и вывод его в char
	while(NumInt != 0)
	{
	  temp = NumInt % ToInt;
	  if(temp < 10)
	    result[gear] = char(48 + temp);
	  if(temp > 9)
	    result[gear] = char(87 + temp);
	  gear++;
	  NumInt /= ToInt;
	  if(gear == 10 && NumInt != 0)
	    {
	      line++;
	      out_str("Number is too big for output", line);
		return;	
	    }
	}
	result[gear] = '\0';
	lenNum = gear;
	for(gear = 0; gear < lenNum / 2;gear++)
	{
	  tempCh = result[gear];
	  result[gear] = result[(lenNum-1)-gear];
	  result[(lenNum-1)-gear] = tempCh;
	}
	line++;
	out_str(result,line);

}
// ---------------------------------------------------------------------------------------- P O S I X T I M E -----------------------------------------------------------------------
void posixtime()
{
	int year = 1970,day = 1,month = 1,hour = 0,min = 0,sec = 0;
	int i = 0,temp;
	unsigned int num = 0;
	char strTime[11],result[20];

	while(buffer[10+i] != '\0' && buffer[10+i] != ' ' && i < 10)
	{
	  if(buffer[10+i] < 48 || buffer[10+i] > 57)
	  {
	    line++;
	    out_str("incorrect symbol of number",line);
	    return;
	  }
	  strTime[i] = buffer[10+i];
	  i++;
	}
	strTime[i] = '\0';
	temp = 0;
	while(temp < i)
	{
	  if(temp>0)
		num = num * 10;
	  num = num + (int(strTime[temp]) - 48);
	  temp++;
	}
	temp = num % 60;			// узнаем кол-во секунд
	sec+=temp;				
	num = num / 60;				//усекаем до минут
	temp = num % 60;			//узнаем кол-во минут
	min +=temp;
	num = num /60;				//усекаем до часов
	temp = num % 24;			//узнаем кол-во часов
	hour += temp;
	num = num / 24;				//усекаем до кол-ва дней
	while(num >365)			//высчитываем сколько из этого кол-ва дней получится годов учитывая високосные
	{
	  if((year % 4 == 0 && year % 100 != 0)|| (year % 400 == 0))
	  {
		num -= 366;
		year++;
	  }
	  else
	  {
		num -=365;
		year++;
	  }
	}
	// ------------------------------------------------------------------------ ПОЛУЧЕНИЕ МЕСЯЦА И ЧИСЛА --------------------------------------------------------
	while(true)
	{
	  if((month < 8 && month % 2 == 1)||(month > 7 && month % 2 == 0))		//Поверяем состоит ли месяц из 31 дня
	  {
	    if(num > 30)							//Проверяю именно > 30 ибо если там будет 31, то месяц пройдёт, останется 0 дней => будет 1 число след месяца
	    {
		num -=31;
		month++;
	    }
	    else
	    {
		day += num;
		break;
	    }
	  }

	  if((month < 8 && month % 2 == 0)||(month > 7 && month % 2 == 1))		//Поверяем состоит ли месяц из 30 дней или это февраль
	  {

//--------------------------------------------------------------- ФЕВРАЛИ --------------------------------------------------
	    if(month == 2)
	    {
		if((year % 4 == 0 && year % 100 != 0)|| (year % 400 == 0))
		{
		    if(num > 28)
		    {
			num -=29;
			month++;
		    }
		    else
		    {
			day += num;
			break;
		    }	
		}
		else
		{
		    if(num > 27)
		    {
			num -=28;
			month++;
		    }
		    else
		    {
			day = num;
			break;
		    }	
		}
	    }				// ------------------------------ Феврали кончились ----------------
	    else
	    {
		    if(num > 29)
		    {
			num -=30;
			month++;
		    }
		    else
		    {
			day = num;
			break;
		    }
	    }
	  }
	}

// -------------------------------------------- Переыодим в строку ------------------------------------------
	i = 1;
	while(i > -1)		// День
	{
	  temp = day % 10;
	  result[i]= 48 + temp;
	  i--;
	  day /=10;
	}
	result[2]= '.';
	i = 4;
	while(i > 2)		// Месяц
	{
	  temp = month % 10;
	  result[i]= 48 + temp;
	  i--;
	  month /=10;
	}
	result[5]= '.';
	i = 9;
	while(i > 5)		// Год
	{
	  temp = year % 10;
	  result[i]= 48 + temp;
	  i--;
	  year /=10;
	}
	result[10]= ' ';
	i = 12;
	while(i > 10)		// Час
	{
	  temp = hour % 10;
	  result[i]= 48 + temp;
	  i--;
	  hour /=10;
	}
	result[13]= ':';
	i = 15;
	while(i > 13)		// Минуты
	{
	  temp = min % 10;
	  result[i]= 48 + temp;
	  i--;
	  min /=10;
	}
	result[16]= ':';
	i = 18;
	while(i > 16)		// Секунды
	{
	  temp = sec % 10;
	  result[i]= 48 + temp;
	  i--;
	  sec /=10;
	}
	result[19]= '\0';
	line++;
	out_str(result,line);


	//return;			// Цэ такi  РЕТУРН
	
}
// ----------------------------------------------------------------------------  W I N   T I M E  ---------------------------------------------------
void wintime()
{
	int year = 1601,day = 1,month = 1,hour = 0,min = 0,sec = 0;
	int i = 0,temp,len,addsec = 0;
	unsigned int num = 0,tempnum = 0,tempsec = 0;
	char strTime[19],result[20];
// ---------------------------------------------------------------------------- Получаем число из строки -----------------------
	while(buffer[8+i] != '\0' && buffer[8+i] != ' ' && i < 18)
	{
	  if(buffer[8+i] < 48 || buffer[8+i] > 57)
	  {
	    line++;
	    out_str("incorrect symbol of number",line);
	    return;
	  }
	  strTime[i] = buffer[8+i];
	  i++;
	}
	strTime[i] = '\0';
	len = i;
	temp = 0;
	i = len - 7;
// ---------------------------------------------------------------------------- округляем часть после запятой для последующего дополнения секунды, если после запятой > 6
	if(len < 8)
	{
	  line++;
	  out_str("Number is too low",line);
	  return;
	}
	while(i<len)
	{
	  if(temp>0)
		temp = temp * 10;
	  temp += (int(strTime[i]) - 48);
	  i++;
	}
	while(temp > 9)
	{
	  i = temp % 10;
	  temp /= 10;
	  if(i > 5)
	    temp++;
	}
	if(temp > 5)
	  addsec = 1;
// ------------------------------------------------------------------------ Получили округление ------------------------

	i = 0;
	temp = 0;

// -------------------------------------------------------------------------ВЫЧИСЛЯЕМ ПОСТУПИВШЕЕ ЧИСЛО В СЕКУНДАХ


	if(len > 16)			// Если число больше габаритов int, подробности в отчёте	
	{
	  i = len - 16;
	  while(i < len-7)
	  {
	      if(tempnum>0)
		tempnum *= 10;
	      tempnum += (int(strTime[i]) - 48);
	      i++;
	  }
	  tempnum +=addsec;
	  tempsec = tempnum %60;
	  tempnum /= 60;
	  
	  i = 0;
	  while(i < len-16)
	  {
	      if(temp>0)
		temp *= 10;
	      temp += (int(strTime[i]) - 48);
	      i++;
	  }
	  if(temp<5)
	  {
	    temp *=1000000000;
	    tempsec += temp % 60;
	    tempnum += temp / 60;
	    sec = tempsec % 60;
	    tempnum += tempsec / 60;
	    num = tempnum;
	  }
	  else
	  {
	    tempsec += (temp / 4) * 40;
	    tempnum += (temp /4) * 66666666;
	    temp = temp % 4;
	    temp *=1000000000;
	    tempsec += temp % 60;
	    tempnum += temp / 60;
	    sec = tempsec % 60;
	    tempnum += tempsec / 60;
	    num = tempnum;	
	  }
	}
	else				// Если поместится в инт
	{
	  while(i < len-7)
	  {
	      if(num>0)
		num *= 10;
	      num += (int(strTime[i]) - 48);
	      i++;
	  }
	  temp += addsec;			// учитываем секунду от округления
	  temp = num % 60;			// узнаем кол-во секунд
	  sec+=temp;				
	  num = num / 60;				//усекаем до минут
	}





	temp = num % 60;			//узнаем кол-во минут
	min +=temp;
	num = num /60;				//усекаем до часов
	temp = num % 24;			//узнаем кол-во часов
	hour += temp;
	num = num / 24;				//усекаем до кол-ва дней
	while(num >365)			//высчитываем сколько из этого кол-ва дней получится годов учитывая високосные
	{
	  if((year % 4 == 0 && year % 100 != 0)|| (year % 400 == 0))
	  {
		num -= 366;
		year++;
	  }
	  else
	  {
		num -=365;
		year++;
	  }
	}
	// ------------------------------------------------------------------------ ПОЛУЧЕНИЕ МЕСЯЦА И ЧИСЛА --------------------------------------------------------
	while(true)
	{
	  if((month < 8 && month % 2 == 1)||(month > 7 && month % 2 == 0))		//Поверяем состоит ли месяц из 31 дня
	  {
	    if(num > 30)							//Проверяю именно > 30 ибо если там будет 31, то месяц пройдёт, останется 0 дней => будет 1 число след месяца
	    {
		num -=31;
		month++;
	    }
	    else
	    {
		day += num;
		break;
	    }
	  }

	  if((month < 8 && month % 2 == 0)||(month > 7 && month % 2 == 1))		//Поверяем состоит ли месяц из 30 дней или это февраль
	  {

//--------------------------------------------------------------- ФЕВРАЛИ --------------------------------------------------
	    if(month == 2)
	    {
		if((year % 4 == 0 && year % 100 != 0)|| (year % 400 == 0))
		{
		    if(num > 28)
		    {
			num -=29;
			month++;
		    }
		    else
		    {
			day += num;
			break;
		    }	
		}
		else
		{
		    if(num > 27)
		    {
			num -=28;
			month++;
		    }
		    else
		    {
			day = num;
			break;
		    }	
		}
	    }				// ------------------------------ Феврали кончились ----------------
	    else
	    {
		    if(num > 29)
		    {
			num -=30;
			month++;
		    }
		    else
		    {
			day = num;
			break;
		    }
	    }
	  }
	}

// -------------------------------------------- Переыодим в строку ------------------------------------------
	i = 1;
	while(i > -1)		// День
	{
	  temp = day % 10;
	  result[i]= 48 + temp;
	  i--;
	  day /=10;
	}
	result[2]= '.';
	i = 4;
	while(i > 2)		// Месяц
	{
	  temp = month % 10;
	  result[i]= 48 + temp;
	  i--;
	  month /=10;
	}
	result[5]= '.';
	i = 9;
	while(i > 5)		// Год
	{
	  temp = year % 10;
	  result[i]= 48 + temp;
	  i--;
	  year /=10;
	}
	result[10]= ' ';
	i = 12;
	while(i > 10)		// Час
	{
	  temp = hour % 10;
	  result[i]= 48 + temp;
	  i--;
	  hour /=10;
	}
	result[13]= ':';
	i = 15;
	while(i > 13)		// Минуты
	{
	  temp = min % 10;
	  result[i]= 48 + temp;
	  i--;
	  min /=10;
	}
	result[16]= ':';
	i = 18;
	while(i > 16)		// Секунды
	{
	  temp = sec % 10;
	  result[i]= 48 + temp;
	  i--;
	  sec /=10;
	}
	result[19]= '\0';
	line++;
	out_str(result,line);

}
// -------------------------------------------------------------------------- S I Z E   C O N V E R T ----------------------------------------------------------
/*void szconv()
{
	char NumCh[27],FromCh[3],ToCh[3],result[10],tempCh; // leftover[40];
	int gear = 0,temp = 0,FromInt = 0,ToInt=0,lenNum,lenFrom,lenTo,beforeP = 0,afterP = 0;
	unsigned int NumInt = 0,check =0;

	while(buffer[gear] != ' ')
	  gear++;
	gear++;
	while(buffer[gear] != ' ')					//переводимое число
	{
	  if(buffer[gear] == 0)
	  {
	   line++;
	   out_str("incorrect input - only number", line);
	   return;
	  }
	  
	  NumCh[temp] = buffer[gear];
	  gear++;
	  temp++;
	  if(temp == 27 && buffer[gear] != ' ')
	  {
	    line++;
	    out_str("incorrect input - lost number", line);
	    return;
	  }
	}
	lenNum = temp;
	NumCh[temp] = '\0';
	temp = 0;
	gear++;

	while(buffer[gear] != ' ')					//Начальная система исчисления
	{
	  if(buffer[gear] == 0)
	  {
	   line++;
	   out_str("incorrect input - only number and initial base", line);
	   return;
	  }
	  if(temp == 2)
	    break;
	  FromCh[temp]= buffer[gear];
	  gear++;
	  temp++;
	  if(temp == 2 && buffer[gear] != ' ')
	  {
	    line++;
	    out_str("incorrect input - lost initial base", line);
	    return;
	  }
	}
	lenFrom = temp;
	FromCh[temp] = '\0';
	temp = 0;
	gear++;

	while(buffer[gear] != '\0' && buffer[gear] != ' ')		// Конечная систем исчисления
	{
	  if(temp == 2)
	    break;
	  ToCh[temp] = buffer[gear];
	  gear++;
	  temp++;
	}	
	if(temp < 1 || temp > 2)
	{
	     line++;
	     out_str("incorrect input - lost final base", line);
	     return;
	}
	lenTo = temp;
	ToCh[temp] = '\0';

// -------------------------------------------------------------------------- Получение систем ИЗ и В
	gear = temp = 0;
	while(gear < lenFrom)
	{
	  if(temp>0)
		temp = temp * 10;
	  temp += (int(FromCh[gear]) - 97);
	  gear++;
	}
	switch(temp)
	{
	case 1://b
	    FromInt = 0;
	    break;
	case 121://m
	    FromInt = 2;
	    break;
	case 101://k
	    FromInt = 1;
	    break;
	case 61://g
	    FromInt = 3;
	    break;
	case 191:
	    FromInt = 4;
	    break;
	default:
	   line++;
	   out_line("Wrong initial base",line);
	   return;
	}
	gear = temp = 0;
	while(gear < lento)
	{
	  if(temp>0)
		temp = temp * 10;
	  temp += (int(ToCh[gear]) - 97);
	  gear++;
	}
	switch(temp)
	{
	case 1://b
	    ToInt = 0;
	    break;
	case 121://m
	    ToInt = 2;
	    break;
	case 101://k
	    ToInt = 1;
	    break;
	case 61://g
	    ToInt = 3;
	    break;
	case 191:
	    ToInt = 4;
	    break;
	default:
	   line++;
	   out_line("Wrong final base",line);
	   return;
	}


	gear = 0;
	while(gear < lenNum)
	{
	  if(NumInt>0)
		NumInt *= 10;
	  NumInt += (int(NumCh[gear]) - 97);
	  gear++;
	}
	if(ToInt > FromInt)
	{
	  while(FromInt < ToInt)
	  {
	    if(NumInt < 1024)
	    {
		afterP += (NumInt / 10);
		beforeP += (afterP / 100)
		afterP %= 100;
	    }
	    afterP += (NumInt % 1024) / 10;
	    beforeP += NumInt / 1024;
	    beforeP += (afterP / 100)
	    afterP %= 100;
	    NumInt /= 1024;
	    FromInt++    	
	  }

}*/

