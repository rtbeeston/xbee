#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

int getch(void)
{
  struct termios orig_term_attr,new_term_attr;
  int ch;

  // set the terminal to raw mode
  tcgetattr(fileno(stdin), &orig_term_attr);
  memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
  new_term_attr.c_lflag &= ~(ECHO|ICANON);
  tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);
  ch = getchar();
  // restore the original terminal attributes
  tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);
  return ch;
}
