/* This testcase is part of GDB, the GNU debugger.

   Copyright 1998, 1999, 2001, 2003, 2004, Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
   */

/* Test GDB's ability to restore saved registers from stack frames
   when using the `return' command.
   Jim Blandy <jimb@cygnus.com> --- December 1998 */

#include <stdio.h>

/* This is the Emacs Lisp expression I used to generate the functions
   in this file.  If people modify the functions manually, instead of
   changing this expression and re-running it, then evaluating this
   expression could wipe out their work, so you probably shouldn't
   re-run it.  But I leave it here for reference. 

   (defun callee (n) (format "callee%d" n))
   (defun caller (n) (format "caller%d" n))
   (defun local  (n) (format "l%d"  n))
   (defun local-sum (n)
     (if (zerop n) (insert "0")
       (let ((j 1))
         (while (<= j n)
           (insert (local j))
           (if (< j n) (insert "+"))
           (setq j (1+ j))))))
   (defun local-chain (n previous first-end)
     (let ((j 1))
       (while (<= j n)
	 (insert "  register int " (local j)
                 " = increment (" previous  ");")
	 (if first-end 
	   (progn
             (insert "  /" "* " first-end " prologue *" "/")
             (setq first-end nil)))
	 (insert "\n")
	 (setq previous (local j))
	 (setq j (1+ j))))
     previous)

   (save-excursion
     (let ((limit 5))
       (goto-char (point-max))
       (search-backward "generated code starts here")
       (forward-line 1)
       (let ((start (point)))
	 (search-forward "generated code ends here")
	 (forward-line 0)
	 (delete-region start (point)))

       ;; Generate callee functions.
       (let ((i 0))
	 (while (<= i limit)
           (insert (format "/%s Returns n * %d + %d %s/\n"
                           "*" i (/ (+ i (* i i)) 2) "*"))
	   (insert "int\n")
	   (insert (callee i) " (int n)\n")
	   (insert "{\n")
	   (local-chain i "n" (callee i))
	   (insert "  return ")
	   (local-sum i)
	   (insert ";\n")
	   (insert "}\n\n")
	   (setq i (1+ i))))

       ;; Generate caller functions.
       (let ((i 1))
	 (while (<= i limit)
	   (insert "int\n")
	   (insert (caller i) " (void)\n")
	   (insert "{\n")
	   (let ((last (local-chain i "0x7eeb" (caller i))))
	     (insert "  register int n;\n")
	     (let ((j 0))
	       (while (<= j limit)
	         (insert "  n = " (callee j) " (" 
                         (if (> j 0) "n + " "")
		         last ");\n")
	         (setq j (1+ j)))))
	   (insert "  return n+")
	   (local-sum i)
           (insert ";\n")
	   (insert "}\n\n")
	   (setq i (1+ i))))

       ;; Generate driver function.
       (insert "void\n")
       (insert "driver (void)\n")
       (insert "{\n")
       (let ((i 1))
	 (while (<= i limit)
	   (insert "  printf (\"" (caller i) " () => %d\\n\", "
		   (caller i) " ());\n")
	   (setq i (1+ i))))
       (insert "}\n\n")))

	 */

int
increment (int n)
{
  return n + 1;
}

/* generated code starts here */
/* Returns n * 0 + 0 */
int
callee0 (int n)
{
  return 0;
}

/* Returns n * 1 + 1 */
int
callee1 (int n)
{
  register int l1 = increment (n);  /* callee1 prologue */
  return l1;
}

/* Returns n * 2 + 3 */
int
callee2 (int n)
{
  register int l1 = increment (n);  /* callee2 prologue */
  register int l2 = increment (l1);
  return l1+l2;
}

/* Returns n * 3 + 6 */
int
callee3 (int n)
{
  register int l1 = increment (n);  /* callee3 prologue */
  register int l2 = increment (l1);
  register int l3 = increment (l2);
  return l1+l2+l3;
}

/* Returns n * 4 + 10 */
int
callee4 (int n)
{
  register int l1 = increment (n);  /* callee4 prologue */
  register int l2 = increment (l1);
  register int l3 = increment (l2);
  register int l4 = increment (l3);
  return l1+l2+l3+l4;
}

/* Returns n * 5 + 15 */
int
callee5 (int n)
{
  register int l1 = increment (n);  /* callee5 prologue */
  register int l2 = increment (l1);
  register int l3 = increment (l2);
  register int l4 = increment (l3);
  register int l5 = increment (l4);
  return l1+l2+l3+l4+l5;
}

int
caller1 (void)
{
  register int l1 = increment (0x7eeb);  /* caller1 prologue */
  register int n;
  n = callee0 (l1);
  n = callee1 (n + l1);
  n = callee2 (n + l1);
  n = callee3 (n + l1);
  n = callee4 (n + l1);
  n = callee5 (n + l1);
  return n+l1;
}

int
caller2 (void)
{
  register int l1 = increment (0x7eeb);  /* caller2 prologue */
  register int l2 = increment (l1);
  register int n;
  n = callee0 (l2);
  n = callee1 (n + l2);
  n = callee2 (n + l2);
  n = callee3 (n + l2);
  n = callee4 (n + l2);
  n = callee5 (n + l2);
  return n+l1+l2;
}

int
caller3 (void)
{
  register int l1 = increment (0x7eeb);  /* caller3 prologue */
  register int l2 = increment (l1);
  register int l3 = increment (l2);
  register int n;
  n = callee0 (l3);
  n = callee1 (n + l3);
  n = callee2 (n + l3);
  n = callee3 (n + l3);
  n = callee4 (n + l3);
  n = callee5 (n + l3);
  return n+l1+l2+l3;
}

int
caller4 (void)
{
  register int l1 = increment (0x7eeb);  /* caller4 prologue */
  register int l2 = increment (l1);
  register int l3 = increment (l2);
  register int l4 = increment (l3);
  register int n;
  n = callee0 (l4);
  n = callee1 (n + l4);
  n = callee2 (n + l4);
  n = callee3 (n + l4);
  n = callee4 (n + l4);
  n = callee5 (n + l4);
  return n+l1+l2+l3+l4;
}

int
caller5 (void)
{
  register int l1 = increment (0x7eeb);  /* caller5 prologue */
  register int l2 = increment (l1);
  register int l3 = increment (l2);
  register int l4 = increment (l3);
  register int l5 = increment (l4);
  register int n;
  n = callee0 (l5);
  n = callee1 (n + l5);
  n = callee2 (n + l5);
  n = callee3 (n + l5);
  n = callee4 (n + l5);
  n = callee5 (n + l5);
  return n+l1+l2+l3+l4+l5;
}

void
driver (void)
{
  printf ("caller1 () => %d\n", caller1 ());
  printf ("caller2 () => %d\n", caller2 ());
  printf ("caller3 () => %d\n", caller3 ());
  printf ("caller4 () => %d\n", caller4 ());
  printf ("caller5 () => %d\n", caller5 ());
}

/* generated code ends here */

int main ()
{
  register int local;
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  driver ();
  printf("exiting\n");
  return 0;
}
