/*  bfng93
 *
 *  Copyright (c) 2012, Antoine Balestrat, merkil@savhon.org
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>

#define MAX_STACK_SIZE 512
#define EOP -1

#define car2dir(c) ((c) == '<' ? LEFT : ((c) == '>' ? RIGHT : ((c) == '^' ? UP : DOWN)))
#define binaryopcar2index(c) ((c) == '+' ? 0 : ((c) == '-' ? 1 : ((c) == '*' ? 2 : ((c) == '/' ? 3 : 4))))

static struct
{
    struct
    {
        int stck[MAX_STACK_SIZE], pos;
    } stack;

    struct
    {
        int x, y;
    } pos;

    enum {UP, DOWN, RIGHT, LEFT, DIRMAX} direction;
    int stringmode;
} context;

static char matrix[25][80] = {{' '}};

static void clrbuf(void){int c;while((c=getchar())!='\n' && c != EOF);}

static int add(int v1, int v2){return v1+v2;}
static int sub(int v1, int v2){return v1-v2;}
static int mul(int v1, int v2){return v1*v2;}
static int divf(int v1, int v2){return v1/v2;}
static int mod(int v1, int v2){return v1%v2;}

static int (* const binaryfuncs[])(int, int) =
{
    add, sub, mul, divf, mod
};

static void die(char const *format, ...)
{
    va_list vargs;

    va_start(vargs, format);
    fprintf(stderr, "bfng93: ");
    vfprintf(stderr, format, vargs);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

static int xgetchar(void)
{
    char c;

    fputs("char> ", stdout);
    fflush(stdout);

    scanf("%c", &c);
    clrbuf();
    return c;
}

static int getint(void)
{
    char buff[64];

    fputs("int> ", stdout);
    fflush(stdout);

    if(fgets(buff, sizeof(buff), stdin))
        return strtol(buff, NULL, 10);

    return 0;
}

static void readsrc(char const *filename)
{
    int i = 0;
    char buff[256], *s;
    FILE *f;

    if(!(f = fopen(filename, "r")))
        die("couldn't open %s for reading.", filename);

    while(i++ < 25 && fgets(buff, sizeof(buff), f))
    {
        if((s = strchr(buff, '\n'))) *s = 0;

        if(strlen(buff) >= 81)
            die("parse error (line %d): invalid number of characters (%d%s, max is 80).", i, strlen(buff), strlen(buff) >= 255 ? "+" : "");

        memcpy(matrix[i - 1], buff, strlen(buff));
        memset(buff, 0, strlen(buff));
    }

    fclose(f);
}

static int pop(void)
{
    return context.stack.pos <= 0 ? 0 : context.stack.stck[--context.stack.pos];
}

static void push(int val)
{
    if(context.stack.pos == MAX_STACK_SIZE)
        die("runtime error (%d,%d): stack space exhausted (max is %d).", context.pos.y + 1, context.pos.x + 1, sizeof(context.stack.stck));

    context.stack.stck[context.stack.pos++] = val;
}

static void nextcell(void)
{
    switch(context.direction)
    {
    case RIGHT:
        context.pos.x = (context.pos.x + 1) % 80;
        break;

    case LEFT:
        context.pos.x = (context.pos.x == 0 ? 79 : context.pos.x - 1);
        break;

    case UP:
        context.pos.y = (context.pos.y == 0 ? 24 : context.pos.y - 1);
        break;

    case DOWN:
        context.pos.y = (context.pos.y + 1) % 25;
        break;

    default:
        die("are you kidding me ?");
    }
}

static int process(void)
{
    int val1, val2;

    if(context.stringmode)
    {
        if(matrix[context.pos.y][context.pos.x] == '"')
            context.stringmode = 0;
        else
            push(matrix[context.pos.y][context.pos.x]);

        return 0;
    }

    if(matrix[context.pos.y][context.pos.x] == ' ')
        return 0;

    switch(matrix[context.pos.y][context.pos.x])
    {
    /* @ (end) ends program */
    case '@':
        return EOP;

    case '>': case '<': case '^': case 'v':
        context.direction = car2dir(matrix[context.pos.y][context.pos.x]);
        break;

    /* _ (horizontal if) <boolean value> PC->left if <value>, else PC->right */
    case '_':
        context.direction = (pop() ? LEFT : RIGHT);
        break;

    /* | (vertical if) <boolean value> PC->up if <value>, else PC->down */
    case '|':
        context.direction = (pop() ? UP : DOWN);
        break;

    /* : (dup)  <value>   <value> <value> */
    case ':':
        push(val1 = pop());
        push(val1);
        break;

    /* ? (random) PC -> right? left? up? down? ??? */
    case '?':
        context.direction = rand() % DIRMAX;
        break;

    case '+': case '-': case '*': case '/': case '%':
        val1 = pop(), val2 = pop();
        push(binaryfuncs[binaryopcar2index(matrix[context.pos.y][context.pos.x])](val2, val1));
        break;

    case '!':
        push(!pop());
        break;

    /* ` (greater)  <value1> <value2>   <1 if value1 > value2, 0 otherwise> */
    case '`':
        push(pop() < pop());
        break;

    /* \ (swap) <value1> <value2>  <value2> <value1> */
    case '\\':
        val1 = pop(), val2 = pop();
        push(val1);
        push(val2);
        break;

    /* $ (pop) <value>   pops <value> but does nothing */
    case '$':
        (void) pop();
        break;

    /* . (pop) <value>   outputs <value> as integer */
    case '.':
        printf("%d", pop());
        break;

    /* , (pop)         <value>                 outputs <value> as ASCII */
    case ',':
        putchar(pop());
        break;

    /* # (bridge)  'jumps' PC one farther; skips over next command */
    case '#':
        nextcell();
        break;

    /* ~ (input character)   <character user entered> */
    case '~':
        push(xgetchar());
        break;

    case '"':
        context.stringmode = !context.stringmode;
        break;

    /* & (input value)   <value user entered> */
    case '&':
        push(getint());
        break;

    case 'p':
        val1 = pop(), val2 = pop();
        matrix[val1][val2] = pop();
        break;

    case 'g':
        val1 = pop(), val2 = pop();
        push(matrix[val1][val2]);
        break;

    default:
        if(isdigit(matrix[context.pos.y][context.pos.x]))
            push(matrix[context.pos.y][context.pos.x] - '0');
    }

    return 0;
}

static void execute(void)
{
    context.pos.x = context.pos.y = context.stack.pos = context.stringmode = 0;
    context.direction = RIGHT;
    memset(context.stack.stck, 0, sizeof(context.stack));

    while(process() != EOP)
        nextcell();
}

int main(int argc, char **argv)
{
    srand(time(NULL));

    if(argc < 2)
        die("please supply a source file to interpret.");

    readsrc(argv[1]);
    execute();
    return EXIT_SUCCESS;
}