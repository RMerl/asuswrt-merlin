;; Copyright (c) 1993, 1994, 1995 Rick Sladkey <jrs@world.std.com>
;; All rights reserved.
;;
;; Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions
;; are met:
;; 1. Redistributions of source code must retain the above copyright
;;    notice, this list of conditions and the following disclaimer.
;; 2. Redistributions in binary form must reproduce the above copyright
;;    notice, this list of conditions and the following disclaimer in the
;;    documentation and/or other materials provided with the distribution.
;; 3. The name of the author may not be used to endorse or promote products
;;    derived from this software without specific prior written permission.
;;
;; THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
;; IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
;; OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
;; IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
;; INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
;; NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
;; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
;; THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
;; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
;; THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;
;; $Id$

;; Description: Automate the construction of strace xlat tables.

;; Usage: Put point and mark around a set of definitions in a header
;; file.  Then grab them with C-c G.  Switch to the strace source file
;; and build the xlat table with C-c B.  Then type the name of the table.

(global-set-key "\C-cG" 'grab-xlate)
(global-set-key "\C-cB" 'build-xlate)

(defvar xlate-list nil
  "See grab-xlate and build-xlate.")

(defun grab-xlate (beg end)
  "Grab all of the defined names in the region and save them in xlate-list."
  (interactive "r")
  (save-excursion
    (setq xlate-list nil)
    (goto-char beg)
    (beginning-of-line)
    (while (< (point) end)
      (and (looking-at "^#[ \t]*define[ \t]+\\([A-Za-z0-9_]+\\)[ \t]+")
	   (setq xlate-list (cons (buffer-substring (match-beginning 1)
						    (match-end 1))
				  xlate-list)))
      (forward-line)))
  (and (fboundp 'deactivate-mark)
       (deactivate-mark))
  (setq xlate-list (nreverse xlate-list)))

(defun build-xlate (&optional list)
  "Build and insert an strace xlat table based on the last grab."
  (interactive)
  (or list
      (setq list xlate-list))
  (beginning-of-line)
  (save-excursion
    (insert "static struct xlat ?[] = {\n")
    (while list
      (insert "\t{ " (car list) ",\n")
      (backward-char)
      (move-to-column 24 'force)
      (end-of-line)
      (insert "\"" (car list) "\"")
      (move-to-column 40 'force)
      (end-of-line)
      (insert "},")
      (forward-line)
      (setq list (cdr list)))
    (insert "	{ 0,		NULL		},\n")
    (insert "};\n")
    (insert "\n"))
  (search-forward "?")
  (delete-backward-char 1))
