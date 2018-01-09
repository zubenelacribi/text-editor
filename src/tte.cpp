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

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

// Windows alternative to termios.n should be conio.h

#include <stdint.h>
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


int
main (int argc, char **argv)
{
  if (!init (argv[0])) return 1;

  size_t buffer_size = 4096;
  char *buffer = (char *) malloc (buffer_size);
  buffer[0] = 0;

  if (argc == 2)
    {
      int file = open (argv[1], O_RDONLY);
      assert (file != -1);

      ssize_t bytes_read = read (file, buffer, buffer_size - 1);
      buffer[bytes_read] = 0;
    }

  // write (1, STR ("\e[2J"));        // Clear screen
  // write (1, STR ("\e[?25l"));      // Hide cursor
  // write (1, STR ("\e7"));       // Save cursor position
  write (1, STR ("\e[?47h"));   // Save screen
  // write (1, STR ("\e[H"));      // Move cursor to top left

  for (char *buffer_p = parse_space (buffer); *buffer_p; )
    {
      if (buffer_p[0] == '/')
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
      else if (buffer_p[0] == '"')
        {
          write (1, "\e[1;33m", 7); // bold + green font effect
          write (1, buffer_p++, 1);
          buffer_p = parse_string_literal (buffer_p);
          write (1, "\e[m", 3); // disable font effects
        }
      else if (buffer_p[0] == '(' || buffer_p[0] == ')' ||
               buffer_p[0] == '{' || buffer_p[0] == '}' ||
               buffer_p[0] == '[' || buffer_p[0] == ']' ||
               buffer_p[0] == '=' ||
               buffer_p[0] == ',' ||
               buffer_p[0] == ';' ||
               buffer_p[0] == '*' ||
               buffer_p[0] == '&')
        {
          write (1, buffer_p++, 1);
        }
      else if (is_latin (buffer_p[0]))
        {
          write (1, "\e[1;34m", 7); // bold + green font effect
          buffer_p = parse_identifier (buffer_p);
          write (1, "\e[m", 3); // disable font effects
        }
      else if (is_digit (buffer_p[0]))
        {
          buffer_p = parse_num (buffer_p);
        }
      else
        {
          printf ("\nError: Unable to parse %d ('%c')\n", *buffer_p, *buffer_p);
          return 1;
        }

      buffer_p = parse_space (buffer_p);
    }

  int tcgetattr_error;
  termios original_terminal_attributes;
  tcgetattr_error = tcgetattr (0, &original_terminal_attributes);
  assert (!tcgetattr_error);
  termios terminal_attributes = original_terminal_attributes;
  terminal_attributes.c_lflag &= ~ICANON; // Immediate input
  terminal_attributes.c_lflag &= ~ECHO;   // Don't echo characters
  terminal_attributes.c_iflag &= ~(IXON | IXOFF);  // Disable ^s and ^q
  int tcsetattr_error = tcsetattr (0, TCSANOW, &terminal_attributes);
  assert (!tcsetattr_error);

  write (1, STR ("\e[H"));      // Move cursor to top left
  size_t buffer_line_pos = 0;
  size_t buffer_pos = 0;
  int keep_running = 1;

  while (keep_running)
    {
      char input[64];
      ssize_t bytes_read = read (0, input, 64);

      printf ("bytes_read: %ld\n", bytes_read);
      for (int i = 0; i < bytes_read; ++i)
        {
          printf ("key: %d (%c)\n", input[i], input[i]);
        }

      if (bytes_read == 1)
        {
          switch (input[0])
            {
            case 1: break;
            case '\e': {keep_running = 0;} break;
            case 127: {write (1, STR ("\b \b"));} break;
            default: {write (1, input, 1);} break;
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
                if (buffer_pos - buffer_line_pos > 0)
                  {
                    buffer_pos -= buffer_line_pos + 1;

                    size_t line_len = 0;

                    while (buffer_pos > 0 && buffer[buffer_pos - 1] != '\n')
                      {
                        ++line_len;
                        --buffer_pos;
                      }

                    buffer_line_pos =
                      line_len > buffer_line_pos ? buffer_line_pos : line_len;

                    write (1, STR ("\e[F"));

                    if (buffer_line_pos > 0)
                      {
                        buffer_pos += buffer_line_pos;
                        char line_pos_string[65];
                        s64_to_str (buffer_line_pos, line_pos_string);
                        write (1, STR ("\e["));
                        write (1, line_pos_string, strlen (line_pos_string));
                        write (1, "C", 1);
                      }
                  }
              } break;
            case 'B': // DOWN
              {
                size_t pos = buffer_pos;
                while (buffer[pos] && buffer[pos] != '\n')
                  {
                    ++pos;
                  }

                if (buffer[pos])
                  {
                    buffer_pos = pos + 1;

                    for (size_t line_pos = 0;
                         line_pos < buffer_line_pos;
                         ++line_pos)
                      {
                        if (!buffer[buffer_pos] || buffer[buffer_pos] == '\n')
                          {
                            buffer_line_pos = line_pos;
                            break;
                          }
                        ++buffer_pos;
                      }

                    write (1, STR ("\e[E"));

                    if (buffer_line_pos > 0)
                      {
                        char line_pos_string[65];
                        s64_to_str (buffer_line_pos, line_pos_string);
                        write (1, STR ("\e["));
                        write (1, line_pos_string, strlen (line_pos_string));
                        write (1, "C", 1);
                      }
                  }
              } break;
            case 'C': // RIGHT
              {
                if (buffer[buffer_pos] && buffer[buffer_pos] != '\n')
                  {
                    buffer_line_pos++;
                    buffer_pos++;
                    write (1, STR ("\e[C"));
                  }
              } break;
            case 'D': // LEFT
              {
                if (buffer_line_pos > 0)
                  {
                    buffer_line_pos--;
                    buffer_pos--;
                    write (1, STR ("\e[D"));
                  }
              } break;
            default: assert (!"Unhandled escape key input");
            }
        }
      else
        {
          assert (!"Unhandled key input");
        }
    }


  // tcsetattr_error = tcsetattr (0, TCSADRAIN, &original_terminal_attributes);
  // assert (!tcsetattr_error);

  // write (1, STR ("\e[?47l"));   // Restore screen
  // write (1, STR ("\e8"));       // Restore cursor position
  // write (1, STR ("\e[?25h"));   // Unhide cursor

  return 0;
}
