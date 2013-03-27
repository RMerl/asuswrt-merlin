;;; gdb-mi.el

;; Author: Nick Roberts <nickrob@gnu.org>
;; Maintainer: Nick Roberts <nickrob@gnu.org>
;; Keywords: unix, tools

;; Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

;; This file is part of GNU GDB.

;; GNU GDB is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;;; Commentary:

;; This mode acts as a graphical user interface to GDB and works with Emacs
;; 22.x and the version of GDB with which it is distributed.  You can interact
;; with GDB through the GUD buffer in the usual way, but there are also
;; buffers which control the execution and describe the state of your program.
;; It separates the input/output of your program from that of GDB and displays
;; expressions and their current values in their own buffers.  It also uses
;; features of Emacs 21 such as the fringe/display margin for breakpoints, and
;; the toolbar (see the GDB Graphical Interface section in the Emacs info
;; manual).

;; Start the debugger with M-x gdbmi.

;; This file uses GDB/MI as the primary interface to GDB. It is still under
;; development and is part of a process to migrate Emacs from annotations (as
;; used in gdb-ui.el) to GDB/MI.  It runs gdb with GDB/MI (-interp=mi) and
;; access CLI using "-interpreter-exec console cli-command".

;; This mode acts on top of gdb-ui.el.  After the release of 22.1,
;; mainline Emacs in the CVS repository will have a file also called gdb-mi.el
;; which will *replace* gdb-ui.el.  If you are interested in developing
;; this mode you should get this version.
;;
;; Known Bugs:
;;
;; 1) To handle program input, if required, and to avoid extra output in the
;; GUD buffer you must not use run, step, next or continue etc but their MI
;; counterparts through gud-run, gud-step etc, e.g clicking on the appropriate
;; icon in the toolbar.
;; 2) Some commands send extra prompts to the GUD buffer.
;; 3) Doesn't list catchpoints in breakpoints buffer.
;;
;; TODO:
;; 1) Prefix MI commands with a token instead of queueing commands.
;; 2) Use MI command -data-read-memory for memory window.
;; 3) Use MI command -data-disassemble for disassembly window.
;; 4) Allow separate buffers for Inferior IO and GDB IO.
;; 5) Watch windows to work with threads.
;;
;;; Code:

(require 'gud)
(require 'gdb-ui)

(defvar gdb-last-command nil)
(defvar gdb-prompt-name nil)

;;;###autoload
(defun gdbmi (command-line)
  "Run gdb on program FILE in buffer *gud-FILE*.
The directory containing FILE becomes the initial working directory
and source-file directory for your debugger.

If `gdb-many-windows' is nil (the default value) then gdb just
pops up the GUD buffer unless `gdb-show-main' is t. In this case
it starts with two windows: one displaying the GUD buffer and the
other with the source file with the main routine of the inferior.

If `gdb-many-windows' is t, regardless of the value of
`gdb-show-main', the layout below will appear. Keybindings are
given in relevant buffer.

Watch expressions appear in the speedbar/slowbar.

The following commands help control operation :

`gdb-many-windows'    - Toggle the number of windows gdb uses.
`gdb-restore-windows' - To restore the window layout.

See Info node `(emacs)GDB Graphical Interface' for a more
detailed description of this mode.


+--------------------------------------------------------------+
|                           GDB Toolbar                        |
+-------------------------------+------------------------------+
| GUD buffer (I/O of GDB)       | Locals buffer                |
|                               |                              |
|                               |                              |
|                               |                              |
+-------------------------------+------------------------------+
| Source buffer                                                |
|                                                              |
|                                                              |
|                                                              |
|                                                              |
|                                                              |
|                                                              |
|                                                              |
+-------------------------------+------------------------------+
| Stack buffer                  | Breakpoints buffer           |
| RET      gdb-frames-select    | SPC    gdb-toggle-breakpoint |
|                               | RET    gdb-goto-breakpoint   |
|                               | d      gdb-delete-breakpoint |
+-------------------------------+------------------------------+"
  ;;
  (interactive (list (gud-query-cmdline 'gdbmi)))
  ;;
  ;; Let's start with a basic gud-gdb buffer and then modify it a bit.
  (gdb command-line)
  ;;
  (setq gdb-debug-ring nil)
  (set (make-local-variable 'gud-minor-mode) 'gdbmi)
  (set (make-local-variable 'gud-marker-filter) 'gud-gdbmi-marker-filter)
  ;;
  (gud-def gud-step   "-exec-step %p"              "\C-s" 
	   "Step one source line with display.")
  (gud-def gud-stepi  "-exec-step-instruction %p"  "\C-i"
	   "Step one instruction with display.")
  (gud-def gud-next   "-exec-next %p"              "\C-n"
	   "Step one line (skip functions).")
  (gud-def gud-cont   "-exec-continue"             "\C-r"
	   "Continue with display.")
  (gud-def gud-finish "-exec-finish"               "\C-f"
	   "Finish executing current function.")
  (gud-def gud-run    "-exec-run"	     nil    "Run the program.")
  (gud-def gud-break (if (not (string-equal mode-name "Machine"))
			 (gud-call "break %f:%l" arg)
		       (save-excursion
			 (beginning-of-line)
			 (forward-char 2)
			 (gud-call "break *%a" arg)))
	   "\C-b" "Set breakpoint at current line or address.")
  ;;
  (gud-def gud-remove (if (not (string-equal mode-name "Machine"))
			  (gud-call "clear %f:%l" arg)
			(save-excursion
			  (beginning-of-line)
			  (forward-char 2)
			  (gud-call "clear *%a" arg)))
	   "\C-d" "Remove breakpoint at current line or address.")
  ;;
  (gud-def gud-until  (if (not (string-equal mode-name "Machine"))
			  (gud-call "-exec-until %f:%l" arg)
			(save-excursion
			  (beginning-of-line)
			  (forward-char 2)
			  (gud-call "-exec-until *%a" arg)))
	   "\C-u" "Continue to current line or address.")

  (define-key gud-minor-mode-map [left-margin mouse-1]
    'gdb-mouse-set-clear-breakpoint)
  (define-key gud-minor-mode-map [left-fringe mouse-1]
    'gdb-mouse-set-clear-breakpoint)
  (define-key gud-minor-mode-map [left-fringe mouse-2]
    'gdb-mouse-until)
  (define-key gud-minor-mode-map [left-fringe drag-mouse-1]
    'gdb-mouse-until)
  (define-key gud-minor-mode-map [left-margin mouse-3]
    'gdb-mouse-toggle-breakpoint-margin)
  (define-key gud-minor-mode-map [left-fringe mouse-3]
    'gdb-mouse-toggle-breakpoint-fringe)

  (setq comint-input-sender 'gdbmi-send)
  ;;
  ;; (re-)initialise
  (setq gdb-pc-address (if gdb-show-main "main" nil)
        gdb-previous-frame-address nil
        gdb-memory-address "main"
        gdb-previous-frame nil
        gdb-selected-frame nil
        gdb-frame-number nil
        gdb-var-list nil
        gdb-prompting nil
        gdb-input-queue nil
        gdb-current-item nil
        gdb-pending-triggers nil
        gdb-output-sink 'user
        gdb-server-prefix nil
        gdb-flush-pending-output nil
        gdb-location-alist nil
        gdb-source-file-list nil
        gdb-last-command nil
	gdb-prompt-name nil
	gdb-buffer-fringe-width (car (window-fringes)))
	gdb-debug-ring nil
	gdb-source-window nil
	gdb-inferior-status nil
	gdb-continuation nil
  ;;
  (setq gdb-buffer-type 'gdbmi)
  ;;
  ;; FIXME: use tty command to separate io.
  ;;(gdb-clear-inferior-io)
  ;;
  (if (eq window-system 'w32)
      (gdb-enqueue-input (list "-gdb-set new-console off\n" 'ignore)))
  (gdb-enqueue-input (list "-gdb-set height 0\n" 'ignore))
  ;; find source file and compilation directory here
  (gdb-enqueue-input
   ; Needs GDB 6.2 onwards.
   (list "-file-list-exec-source-files\n"
	 'gdb-set-gud-minor-mode-existing-buffers-1))
  (gdb-enqueue-input
   ; Needs GDB 6.0 onwards.
   (list "-file-list-exec-source-file\n" 'gdb-get-source-file))
  (gdb-enqueue-input
   (list "-data-list-register-names\n" 'gdb-get-register-names))
  (gdb-enqueue-input
   (list "-gdb-show prompt\n" 'gdb-get-prompt))
  ;;
  (setq gdb-locals-font-lock-keywords gdb-locals-font-lock-keywords-2)
  (run-hooks 'gdbmi-mode-hook))


(defun gdbmi-send (proc string)
  "A comint send filter for gdb."
  (if gud-running
      (process-send-string proc (concat string "\n"))
    (with-current-buffer gud-comint-buffer
      (let ((inhibit-read-only t))
	(remove-text-properties (point-min) (point-max) '(face))))
    (setq gdb-output-sink 'user)
    (setq gdb-prompting nil)
    ;; mimic <RET> key to repeat previous command in GDB
    (if (not (string-match "^\\s+$" string))
        (setq gdb-last-command string)
      (if gdb-last-command (setq string gdb-last-command)))
    (if gdb-enable-debug
	(push (cons 'mi-send (concat string "\n")) gdb-debug-ring))
     (if (string-match "^-" string)
	 ;; MI command
	 (process-send-string proc (concat string "\n"))
       ;; CLI command
    (if (string-match "\\\\$" string)
	(setq gdb-continuation (concat gdb-continuation string "\n"))
      (process-send-string proc
			   (concat "-interpreter-exec console \""
				   gdb-continuation string "\"\n"))
      (setq gdb-continuation nil)))))

(defcustom gud-gdbmi-command-name "gdb -interp=mi"
  "Default command to execute an executable under the GDB-UI debugger."
  :type 'string
  :group 'gud)

(defconst gdb-gdb-regexp "(gdb) \n")

(defconst gdb-running-regexp (concat "\\^running\n" gdb-gdb-regexp))

;; fullname added GDB 6.4+.
;; Probably not needed. -stack-info-frame computes filename and line.
(defconst gdb-stopped-regexp
  "\\*stopped,reason=.*?,file=\".*?\"\
,fullname=\"\\(.*?\\)\",line=\"\\(.*?\\)\"}\n")

(defconst gdb-error-regexp "\\^error,msg=\\(\".+\"\\)\n")

(defconst gdb-done-regexp "\\^done,*\n*")

(defconst gdb-console-regexp "~\\(\".*?[^\\]\"\\)\n")

(defconst gdb-internals-regexp "&\\(\".*?\\n\"\\)\n")

(defun gdbmi-prompt1 ()
  "Queue any GDB commands that the user interface needs."
  (unless gdb-pending-triggers
    (gdbmi-get-selected-frame)
    (gdbmi-invalidate-frames)
    (gdbmi-invalidate-breakpoints)
    (gdb-get-changed-registers)
    (gdb-invalidate-registers-1)
    (gdb-invalidate-locals-1)
    (if (and (boundp 'speedbar-frame) (frame-live-p speedbar-frame))
	(gdb-var-update-1))))

(defun gdbmi-prompt2 ()
  "Handle any output and send next GDB command."
  (let ((sink gdb-output-sink))
    (when (eq sink 'emacs)
      (let ((handler
	     (car (cdr gdb-current-item))))
	(with-current-buffer (gdb-get-buffer-create 'gdb-partial-output-buffer)
	  (funcall handler)))))
  (let ((input (gdb-dequeue-input)))
    (if input
	(gdb-send-item input)
      (progn
	(setq gud-running nil)
	(setq gdb-prompting t)
	(gud-display-frame)))))

(defun gud-gdbmi-marker-filter (string)
  "Filter GDB/MI output."
  (if gdb-flush-pending-output
      nil
    (if gdb-enable-debug (push (cons 'recv (list string gdb-output-sink))
					 gdb-debug-ring))
    ;; Recall the left over gud-marker-acc from last time
    (setq gud-marker-acc (concat gud-marker-acc string))
    ;; Start accumulating output for the GUD buffer
    (let ((output "") running)

      (if (string-match gdb-running-regexp gud-marker-acc) 
	  (setq
	   gud-marker-acc
	   (concat (substring gud-marker-acc 0 (match-beginning 0))
		   (substring gud-marker-acc (match-end 0)))
	   running t))

      (if (string-match gdb-stopped-regexp gud-marker-acc)
	  (setq

	   ;; Extract the frame position from the marker.
	   gud-last-frame (cons (match-string 1 gud-marker-acc)
				(string-to-number
				 (match-string 2 gud-marker-acc)))

	   gud-marker-acc
	   (concat (substring gud-marker-acc 0 (match-beginning 0))
		   (substring gud-marker-acc (match-end 0)))))

      ;; Filter error messages going to GUD buffer and
      ;; display in minibuffer.
      (when (eq gdb-output-sink 'user)
	(while (string-match gdb-error-regexp gud-marker-acc)
	  (message (read (match-string 1 gud-marker-acc)))
	  (setq 
	   gud-marker-acc
	   (concat (substring gud-marker-acc 0 (match-beginning 0))
		   (substring gud-marker-acc (match-end 0)))))

	(if (string-match gdb-done-regexp gud-marker-acc)
	    (setq 
	     gud-marker-acc
	     (concat (substring gud-marker-acc 0 (match-beginning 0))
		     (substring gud-marker-acc (match-end 0))))))

      (when (string-match gdb-gdb-regexp gud-marker-acc)
	(setq 
	 gud-marker-acc
	 (concat (substring gud-marker-acc 0 (match-beginning 0))
		   (substring gud-marker-acc (match-end 0))))

	;; Remove the trimmings from the console stream.
	(while (string-match gdb-console-regexp gud-marker-acc) 
	  (setq 
	   gud-marker-acc (concat
			   (substring gud-marker-acc 0 (match-beginning 0))
			   (read (match-string 1 gud-marker-acc))
			   (substring gud-marker-acc (match-end 0)))))

	;; Remove the trimmings from log stream containing debugging messages
	;; being produced by GDB's internals and use warning face.
	(while (string-match gdb-internals-regexp gud-marker-acc) 
	  (setq 
	   gud-marker-acc
	   (concat (substring gud-marker-acc 0 (match-beginning 0))
		   (let ((error-message
			  (read (match-string 1 gud-marker-acc))))
		     (put-text-property
		      0 (length error-message)
		      'face font-lock-warning-face
		      error-message)
		     error-message)
		   (substring gud-marker-acc (match-end 0)))))

	(setq output (gdbmi-concat-output output gud-marker-acc))
	(setq gud-marker-acc "")
	(gdbmi-prompt1)
	(unless gdb-input-queue
	  (setq output (concat output gdb-prompt-name)))
	(gdbmi-prompt2)
	(setq gud-running running))

      (when gud-running
	(setq output (gdbmi-concat-output output gud-marker-acc))
	(setq gud-marker-acc ""))

       output)))

(defun gdbmi-concat-output (so-far new)
  (let ((sink gdb-output-sink))
    (cond
     ((eq sink 'user) (concat so-far new))
     ((eq sink 'emacs)
      (gdb-append-to-partial-output new)
      so-far)
     ((eq sink 'inferior)
      (gdb-append-to-inferior-io new)
      so-far))))


;; Breakpoint buffer : This displays the output of `-break-list'.
;;
(def-gdb-auto-update-trigger gdbmi-invalidate-breakpoints
  (gdb-get-buffer 'gdb-breakpoints-buffer)
  "-break-list\n"
  gdb-break-list-handler)

(defconst gdb-break-list-regexp
"bkpt={.*?number=\"\\(.*?\\)\",.*?type=\"\\(.*?\\)\",.*?disp=\"\\(.*?\\)\",.*?\
enabled=\"\\(.\\)\",.*?addr=\"\\(.*?\\)\",\\(?:.*?func=\"\\(.*?\\)\",.*?\
file=\"\\(.*?\\)\",.*?fullname=\".*?\",.*?line=\"\\(.*?\\)\",\
\\|\\(?:.*?what=\"\\(.*?\\)\",\\)*\\).*?times=\"\\(.*?\\)\".*?}")

(defun gdb-break-list-handler ()
  (setq gdb-pending-triggers (delq 'gdbmi-invalidate-breakpoints
				  gdb-pending-triggers))
  (let ((breakpoint) (breakpoints-list))
    (with-current-buffer (gdb-get-buffer-create 'gdb-partial-output-buffer)
      (goto-char (point-min))
      (while (re-search-forward gdb-break-list-regexp nil t)
	(let ((breakpoint (list (match-string 1)
				(match-string 2)
				(match-string 3)
				(match-string 4)
				(match-string 5)
				(match-string 6)
				(match-string 7)
				(match-string 8)
				(match-string 9)
				(match-string 10))))
	  (push breakpoint breakpoints-list))))
    (let ((buf (gdb-get-buffer 'gdb-breakpoints-buffer)))
      (and buf (with-current-buffer buf
		 (let ((p (point))
		       (buffer-read-only nil))
		   (erase-buffer)
		   (insert "Num Type           Disp Enb Hits Addr       What\n")
		   (dolist (breakpoint breakpoints-list)
		     (insert
		      (concat
		       (nth 0 breakpoint) "   "
		       (nth 1 breakpoint) "     "
		       (nth 2 breakpoint) " "
		       (nth 3 breakpoint) "   "
		       (nth 9 breakpoint) " "
		       (nth 4 breakpoint) " "
		       (if (nth 5 breakpoint)
			   (concat "in " (nth 5 breakpoint) " at " (nth 6 breakpoint) ":" (nth 7 breakpoint) "\n")
			 (concat (nth 8 breakpoint) "\n")))))
		   (goto-char p))))))
  (gdb-info-breakpoints-custom))

(defun gdbmi-get-location (bptno line flag)
  "Find the directory containing the relevant source file.
Put in buffer and place breakpoint icon."
  (goto-char (point-min))
  (catch 'file-not-found
    (if (re-search-forward gdb-source-file-regexp-1 nil t)
	(delete (cons bptno "File not found") gdb-location-alist)
	(push (cons bptno (match-string 1)) gdb-location-alist)
      (gdb-resync)
      (unless (assoc bptno gdb-location-alist)
	(push (cons bptno "File not found") gdb-location-alist)
	(message-box "Cannot find source file for breakpoint location.
Add directory to search path for source files using the GDB command, dir."))
      (throw 'file-not-found nil))
    (with-current-buffer
	(find-file-noselect (match-string 1))
      (save-current-buffer
	(set (make-local-variable 'gud-minor-mode) 'gdbmi)
	(set (make-local-variable 'tool-bar-map) gud-tool-bar-map))
      ;; only want one breakpoint icon at each location
      (save-excursion
	(goto-line (string-to-number line))
	(gdb-put-breakpoint-icon (eq flag ?y) bptno)))))

;; Frames buffer.  This displays a perpetually correct bactrack trace.
;;
(def-gdb-auto-update-trigger gdbmi-invalidate-frames
  (gdb-get-buffer 'gdb-stack-buffer)
  "-stack-list-frames\n"
  gdb-stack-list-frames-handler)

(defconst gdb-stack-list-frames-regexp
"{.*?level=\"\\(.*?\\)\",.*?addr=\"\\(.*?\\)\",.*?func=\"\\(.*?\\)\",\
\\(?:.*?file=\".*?\",.*?fullname=\"\\(.*?\\)\",.*?line=\"\\(.*?\\)\".*?}\\|\
from=\"\\(.*?\\)\"\\)")

(defun gdb-stack-list-frames-handler ()
  (setq gdb-pending-triggers (delq 'gdbmi-invalidate-frames
				  gdb-pending-triggers))
  (let ((frame nil)
	(call-stack nil))
    (with-current-buffer (gdb-get-buffer-create 'gdb-partial-output-buffer)
      (goto-char (point-min))
      (while (re-search-forward gdb-stack-list-frames-regexp nil t)
	(let ((frame (list (match-string 1)
			   (match-string 2)
			   (match-string 3)
			   (match-string 4)
			   (match-string 5)
			   (match-string 6))))
	  (push frame call-stack))))
    (let ((buf (gdb-get-buffer 'gdb-stack-buffer)))
      (and buf (with-current-buffer buf
		 (let ((p (point))
		       (buffer-read-only nil))
		   (erase-buffer)
		   (insert "Level\tAddr\tFunc\tFile:Line\n")
		   (dolist (frame (nreverse call-stack))
		     (insert
		      (concat
		       (nth 0 frame) "\t"
		       (nth 1 frame) "\t"
		       (propertize (nth 2 frame)
				   'face font-lock-function-name-face) "\t"
		       (if (nth 3 frame)
			   (concat "at "(nth 3 frame) ":" (nth 4 frame) "\n")
			 (concat "from " (nth 5 frame) "\n")))))
		   (goto-char p))))))
  (gdb-stack-list-frames-custom))

(defun gdb-stack-list-frames-custom ()
  (with-current-buffer (gdb-get-buffer 'gdb-stack-buffer)
    (save-excursion
      (let ((buffer-read-only nil))
	(goto-char (point-min))
	(forward-line 1)
	(while (< (point) (point-max))
	  (add-text-properties (point-at-bol) (point-at-eol)
			     '(mouse-face highlight
			       help-echo "mouse-2, RET: Select frame"))
	  (beginning-of-line)
	  (when (and (looking-at "^[0-9]+\\s-+\\(\\S-+\\)")
		     (equal (match-string 1) gdb-selected-frame))
	    (put-text-property (point-at-bol) (point-at-eol)
			       'face '(:inverse-video t)))
	  (forward-line 1))))))


;; gdb-ui.el uses "info source" to find out if macro information is present.
(defun gdb-get-source-file ()
  "Find the source file where the program starts and display it with related
buffers, if required."
  (goto-char (point-min))
  (if (re-search-forward gdb-source-file-regexp-1 nil t)
      (setq gdb-main-file (match-string 1)))
 (if gdb-many-windows
      (gdb-setup-windows)
   (gdb-get-buffer-create 'gdb-breakpoints-buffer)
   (if gdb-show-main
       (let ((pop-up-windows t))
	 (display-buffer (gud-find-file gdb-main-file))))))

(defun gdbmi-get-selected-frame ()
  (if (not (member 'gdbmi-get-selected-frame gdb-pending-triggers))
      (progn
	(gdb-enqueue-input
	 (list "-stack-info-frame\n" 'gdbmi-frame-handler))
	(push 'gdbmi-get-selected-frame
	       gdb-pending-triggers))))

(defun gdbmi-frame-handler ()
  (setq gdb-pending-triggers
	(delq 'gdbmi-get-selected-frame gdb-pending-triggers))
  (with-current-buffer (gdb-get-buffer-create 'gdb-partial-output-buffer)
    (goto-char (point-min))
    (when (re-search-forward gdb-stack-list-frames-regexp nil t)
      (setq gdb-frame-number (match-string 1))
      (setq gdb-pc-address (match-string 2))
      (setq gdb-selected-frame (match-string 3))
      (when (match-string 4)
	(setq gud-last-frame
	      (cons (match-string 4) (string-to-number (match-string 5))))
	(gud-display-frame)
	(if gud-overlay-arrow-position
	    (let ((buffer (marker-buffer gud-overlay-arrow-position))
		  (position (marker-position gud-overlay-arrow-position)))
	      (when buffer
		(with-current-buffer buffer
		  (setq fringe-indicator-alist
			(if (string-equal gdb-frame-number "0")
			    nil
			  '((overlay-arrow . hollow-right-triangle))))
		  (setq gud-overlay-arrow-position (make-marker))
		  (set-marker gud-overlay-arrow-position position))))))
      (if (gdb-get-buffer 'gdb-locals-buffer)
	  (with-current-buffer (gdb-get-buffer 'gdb-locals-buffer)
	    (setq mode-name (concat "Locals:" gdb-selected-frame))))
      (if (gdb-get-buffer 'gdb-assembler-buffer)
	  (with-current-buffer (gdb-get-buffer 'gdb-assembler-buffer)
	    (setq mode-name (concat "Machine:" gdb-selected-frame)))))))

(defvar gdb-prompt-name-regexp "value=\"\\(.*?\\)\"")

(defun gdb-get-prompt ()
  "Find prompt for GDB session."
  (goto-char (point-min))
  (setq gdb-prompt-name nil)
  (re-search-forward gdb-prompt-name-regexp nil t)
  (setq gdb-prompt-name (match-string 1)))
	       
(provide 'gdb-mi)
;;; gdbmi.el ends here
