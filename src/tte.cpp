/* Text Editor
 *
 * Copyright (C) 2017 LibTec
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>     // read, write, close
#include <fcntl.h>      // open
#include <termios.h>    // tcgetattr, tcsetattr
#include <sys/stat.h>   // stat, fstat
#include <sys/ioctl.h>  // ioctl

// Windows alternative to termios.n should be conio.h

#include <stdint.h>  // uint*_t and int*_t
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>


typedef float  r32;
typedef double r64;

typedef uint8_t b8;

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#define STR(string) (string), (sizeof (string) - 1)

struct Buffer {
  char *data;
  size_t used;
  size_t size;
};


static Buffer
new_buffer (size_t size)
{
  Buffer buffer;
  buffer.data = (char *) malloc (size);
  buffer.used = 0;
  buffer.size = size;
  return buffer;
}

enum TextContextType {
  TEXT_CONTEXT_GLOBAL,
  TEXT_CONTEXT_BLOCK_COMMENT,
  TEXT_CONTEXT_INLINE_COMMENT,
  TEXT_CONTEXT_STRING_LITERAL,
  TEXT_CONTEXT_CHAR_LITERAL,
};

struct TextContext {
  TextContextType type;
  size_t length;
};


static int
is_latin (char c)
{
  return ((c >= 'A' && c <= 'Z') ||
          (c >= 'a' && c <= 'z'));
}


static int
is_digit (char c)
{
  return (c >= '0' && c <= '9');
}


static char *
parse_space (char *p)
{
  while (*p == ' ' ||
         *p == '\n' ||
         *p == '\r' ||
         *p == '\t')
    {
      write (1, p++, 1);
    }

  return p;
}


static char *
parse_block_comment (char *p)
{
  while (*p)
    {
      if (p[0] == '*' && p[1] == '/')
        {
          write (1, p, 2);
          p += 2;
          break;
        }
      else
        {
          write (1, p++, 1);
        }
    }

  return p;
}


static char *
parse_inline_comment (char *p)
{
  while (*p &&
         *p != '\n' &&
         *p != '\r')
    {
      write (1, p++, 1);
    }

  return p;
}


static char *
parse_string_literal (char *p)
{
  while (*p)
    {
      if (p[0] == '"' && p[-1] != '\\')
        {
          write (1, p++, 1);
          break;
        }
      else
        {
          write (1, p++, 1);
        }
    }

  return p;
}


static char *
parse_identifier (char *p)
{
  while (is_latin (p[0]))
    {
      write (1, p++, 1);
    }

  return p;
}


static char *
parse_num (char *p)
{
  while (is_digit (p[0]))
    {
      write (1, p++, 1);
    }

  return p;
}


static void
u64_to_str (u64 num, char *out)
{
  if (num == 0)
    {
      out[0] = '0';
      out[1] = 0;
      return;
    }

  char *end = out;

  while (num > 0)
    {
      u64 new_num = num / 10;
      char digit = num - new_num * 10;
      *(end++) = digit + '0';
      num = new_num;
    }

  *(end--) = 0;

  while (out < end)
    {
      char tmp = *out;
      *(out++) = *end;
      *(end--) = tmp;
    }
}


static void
s64_to_str (s64 num, char *out)
{
  if (num < 0)
    {
      *(out++) = '-';
      num = -num;
    }

  u64_to_str (num, out);
}


static b8
init (char *exec_path)
{
  char *term = getenv ("TERM");

  if (!term)
    {
      write (2, exec_path, strlen (exec_path));
      write (2, STR (": The environment variable TERM isn't set "
                     "- it should be set to `xterm'.\n"));
      return 0;
    }
  else if (strcmp (term, "xterm") && strcmp (term, "xterm-256color"))
    {
      // (!strcmp (term, "dumb")
      write (2, exec_path, strlen (exec_path));
      write (2, STR (": The environment variable TERM is set to `"));
      write (2, term, strlen (term));
      write (2, STR ("' - should be `xterm'.\n"));
      return 0;
    }

  return 1;
}


static termios
init_screen (void)
{
  write (1, STR ("\e7"));       // Save cursor position
  // write (1, STR ("\e[s"));   // Save cursor position
  write (1, STR ("\e[?47h"));   // Save screen
  // write (1, STR ("\e[2J"));        // Clear screen
  // write (1, STR ("\e[?25l"));      // Hide cursor

  termios original_terminal_attributes;
  int tcgetattr_error = tcgetattr (0, &original_terminal_attributes);
  assert (!tcgetattr_error);
  termios terminal_attributes = original_terminal_attributes;
  terminal_attributes.c_lflag &= ~ICANON; // Immediate input
  terminal_attributes.c_lflag &= ~ECHO;   // Don't echo characters
  terminal_attributes.c_iflag &= ~IXON;   // Disable ^s and ^q
  int tcsetattr_error = tcsetattr (0, TCSANOW, &terminal_attributes);
  assert (!tcsetattr_error);

  return original_terminal_attributes;
}


static void
destroy_screen (termios original_terminal_attributes)
{
  // TODO: Use restore screen to restore colors?

  int tcsetattr_error = tcsetattr (0, TCSADRAIN, &original_terminal_attributes);
  assert (!tcsetattr_error);

  write (1, STR ("\e[?47l"));   // Restore screen
  write (1, STR ("\e8"));       // Restore cursor position
  // write (1, STR ("\e[u"));   // Restore cursor position
  // write (1, STR ("\e[?25h"));   // Unhide cursor
}

static Buffer
load_file (const char *filepath)
{
  int fd = open (filepath, O_RDONLY);
  assert (fd != -1);

  struct stat file_stat;
  int fstat_error = fstat (fd, &file_stat);
  assert (!fstat_error);

  Buffer buffer = new_buffer (file_stat.st_size + 1);

  ssize_t bytes_read = read (fd, buffer.data, file_stat.st_size);
  assert (bytes_read != -1);
  assert (bytes_read == file_stat.st_size);
  buffer.data[file_stat.st_size] = 0;
  buffer.used = file_stat.st_size + 1;

  for (char *buffer_p = parse_space (buffer.data); *buffer_p;)
    {
      char c = buffer_p[0];
      if (c == '/')
        {
          if (buffer_p[1] == '*')
            {
              write (1, "\e[6m", 5);
              buffer_p = parse_block_comment (buffer_p);
              write (1, "\e[m", 3);
            }
          else if (buffer_p[1] == '/')
            {
              write (1, "\e[30m", 5);
              buffer_p = parse_inline_comment (buffer_p);
              write (1, "\e[m", 3);
            }
          else
            {
              write (1, buffer_p++, 1);
            }
        }
      else if (c == '"')
        {
          write (1, "\e[1;33m", 7); // bold + green font effect
          write (1, buffer_p++, 1);
          buffer_p = parse_string_literal (buffer_p);
          write (1, "\e[m", 3); // disable font effects
        }
      else if (c == '(' || c == ')' ||
               c == '{' || c == '}' ||
               c == '[' || c == ']' ||
               c == '=' ||
               c == ',' ||
               c == ';' ||
               c == '*' ||
               c == '&')
        {
          write (1, buffer_p++, 1);
        }
      else if (is_latin (c))
        {
          write (1, "\e[1;34m", 7); // bold + green font effect
          buffer_p = parse_identifier (buffer_p);
          write (1, "\e[m", 3); // disable font effects
        }
      else if (is_digit (c))
        {
          buffer_p = parse_num (buffer_p);
        }
      else
        {
          printf ("\nError: Unable to parse %d ('%c')\n", *buffer_p, *buffer_p);
        }

      buffer_p = parse_space (buffer_p);
    }

  return buffer;
}


int
main (int argc, char **argv)
{
  if (!init (argv[0])) return 1;

  Buffer buffer;
  if (argc == 2) buffer = load_file (argv[1]);
  else           buffer = new_buffer (4096);

  termios original_terminal_attributes = init_screen ();

  write (1, STR ("\e[H"));      // Move cursor to top left
  size_t x = 0;
  size_t y = 0;
  int keep_running = 1;

  char line_buffer[1024];
  line_buffer[0] = 0;

  while (keep_running)
    {
      struct winsize window_size;
      ioctl (STDOUT_FILENO, TIOCGWINSZ, &window_size);

      printf ("\e[%u;1H\e[7m", window_size.ws_row);
      fflush (stdout);
      size_t line_buffer_len = strlen (line_buffer);
      for (int i = line_buffer_len; i < window_size.ws_col; ++i)
        {
          line_buffer[i] = '-';
        }
      write (1, line_buffer, window_size.ws_col);
      line_buffer[0] = 0;
      printf ("\e[0m\e[%lu;%luH", y+1, x+1); fflush (stdout);

      char input[64];
      ssize_t bytes_read = read (0, input, 64);
      assert (bytes_read != -1);
      sprintf  (line_buffer, "Size: %ux%u; Status: \"",
                window_size.ws_col,
                window_size.ws_row);

      for (int i = 0; i < bytes_read; ++i)
        {
          char numstr[64];
          if (input[i] >= ' ' && input[i] <= '~')
            {
              sprintf (numstr, "%c", input[i]);
            }
          else
            {
              sprintf (numstr, "\\x%x", input[i]);
            }
          strcat (line_buffer, numstr);
        }
      strcat  (line_buffer, "\"");

      if (bytes_read == 1)
        {
          char c = input[0];

          if (c >= ' ' && c <= '~')
            {
              write (1, input, 1);
              x++;
              write (1, STR ("\e[C"));
            }
          else
            {
              switch (c)
                {
                case '\n':
                  {
                    x = 0;
                    ++y;
                    write (1, STR ("\e[C"));
                    break;
                  }
                case 0x7f: // DEL (<backspace>)
                  {
                    if (x > 0)
                      {
                        --x;
                        write (1, STR ("\e[D"));
                        write (1, " ", 1);
                      }
                    else if (y > 0)
                      {
                        --y;
                        write (1, STR ("\e[F"));
                      }
                    break;
                  }
                case 'Q' - '@':
                case '\e': {keep_running = 0;} break;
                }
            }
        }
      else if (bytes_read == 3 &&
               input[0] == '\e' &&
               input[1] == '[')
        {
          switch (input[2])
            {
            case 'A': // UP
              {
                if (y - x > 0)
                  {
                    y -= x + 1;

                    size_t line_len = 0;

                    while (y > 0 && buffer.data[y - 1] != '\n')
                      {
                        ++line_len;
                        --y;
                      }

                    x =
                      line_len > x ? x : line_len;

                    write (1, STR ("\e[F"));

                    if (x > 0)
                      {
                        y += x;
                        char line_pos_string[65];
                        s64_to_str (x, line_pos_string);
                        write (1, STR ("\e["));
                        write (1, line_pos_string, strlen (line_pos_string));
                        write (1, "C", 1);
                      }
                  }
              } break;
            case 'B': // DOWN
              {
                size_t pos = y;
                while (buffer.data[pos] && buffer.data[pos] != '\n')
                  {
                    ++pos;
                  }

                if (buffer.data[pos])
                  {
                    y = pos + 1;

                    for (size_t line_pos = 0;
                         line_pos < x;
                         ++line_pos)
                      {
                        if (!buffer.data[y] || buffer.data[y] == '\n')
                          {
                            x = line_pos;
                            break;
                          }
                        ++y;
                      }

                    write (1, STR ("\e[E"));

                    if (x > 0)
                      {
                        char line_pos_string[65];
                        s64_to_str (x, line_pos_string);
                        write (1, STR ("\e["));
                        write (1, line_pos_string, strlen (line_pos_string));
                        write (1, "C", 1);
                      }
                  }
              } break;
            case 'C': // RIGHT
              {
                // if (buffer.data[y] && buffer.data[y] != '\n')
                  {
                    x++;
                    // y++;
                    write (1, STR ("\e[C"));
                  }
              } break;
            case 'D': // LEFT
              {
                if (x > 0)
                  {
                    --x;
                    // --y;
                    write (1, STR ("\e[D"));
                  }
              } break;
            default: assert (!"Unhandled escape key input");
            }
        }
    }

  destroy_screen (original_terminal_attributes);

  return 0;
}
