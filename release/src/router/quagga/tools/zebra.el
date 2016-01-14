;; -*- lisp -*-
;;; zebra-mode.el -- major mode for editing zebra configuration file.

;; Copyright (C) 1998 Kunihiro Ishiguro

;; Author:     1998 Kunihiro Ishiguro
;;                  SeonMeyong HEO
;; Maintainer: kunihiro@zebra.org
;;             seirios@Matrix.IRI.Co.JP
;; Created:    Jan 28 1998
;; Version:    Alpha 0.2
;; Keywords:   zebra bgpd ripd ripngd languages

;; You can get the latest version of zebra from
;;
;;    http://www.zebra.org/
;;
;; Install this Emacs Lisp code
;;
;; Compile zebra.el
;;   % $(EMACS) -batch -f batch-byte-compile zebra.el
;; Install zebra.el,zebra.elc to Emacs-load-path
;;   % cp zebra.el zebra.elc $(emacs-load-path)
;; Add .emacs or (site-load.el | site-start.el)
;;   (auto-load 'zebra-mode "zebra" nil t)
;;   (auto-load 'bgp-mode "zebra" nil t)
;;   (auto-load 'rip-mode "zebra" nil t)
;;

;;; Code:

;; Set keywords

(defvar zebra-font-lock-keywords
  (list
   '("#.*$" . font-lock-comment-face)
   '("!.*$" . font-lock-comment-face)
   '("no\\|interface" . font-lock-type-face)
   '("ip6\\|ip\\|route\\|address" . font-lock-function-name-face)
   '("ipforward\\|ipv6forward" . font-lock-keyword-face)
   '("hostname\\|password\\|enable\\|logfile\\|no" . font-lock-keyword-face))
  "Default value to highlight in zebra mode.")

(defvar bgp-font-lock-keywords
  (list
   '("#.*$" . font-lock-comment-face)
   '("!.*$" . font-lock-comment-face)
   '("no\\|router" . font-lock-type-face)
   '("bgp\\|router-id\\|neighbor\\|network" . font-lock-function-name-face)
   '("ebgp\\|multihop\\|next\\|zebra\\|remote-as" . font-lock-keyword-face)
   '("hostname\\|password\\|enable\\|logfile\\|no" . font-lock-keyword-face))
  "Default value to highlight in bgp mode.")

(defvar rip-font-lock-keywords
  (list
   '("#.*$" . font-lock-comment-face)
   '("!.*$" . font-lock-comment-face)
   '("no\\|router\\|interface\\|ipv6\\|ip6\\|ip" . font-lock-type-face)
   '("ripng\\|rip\\|recive\\|advertize\\|accept" . font-lock-function-name-face)
   '("version\\|network" . font-lock-function-name-face)
   '("default\\|none\\|zebra" . font-lock-keyword-face)
   '("hostname\\|password\\|enable\\|logfile\\|no" . font-lock-keyword-face))
  "Default value to highlight in bgp mode.")

;; set font-lock-mode

(defun zebra-font-lock ()
  (make-local-variable 'font-lock-defaults)
  (setq font-lock-defaults '(zebra-font-lock-keywords nil t)))

(defun bgp-font-lock ()
  (make-local-variable 'font-lock-defaults)
  (setq font-lock-defaults '(bgp-font-lock-keywords nil t)))

(defun rip-font-lock ()
  (make-local-variable 'font-lock-defaults)
  (setq font-lock-defaults '(rip-font-lock-keywords nil t)))

;; define Major mode

(defun major-mode-define ()
  (interactive)
  (progn
    (setq comment-start "[#!]"
	  comment-end ""
	  comment-start-skip "!+ ")
    (run-hooks 'zebra-mode-hook)
    (cond
     ((string< "20" emacs-version)
      (font-lock-mode)))))

(defun zebra-mode ()
  (progn
    (setq mode-name "zebra")
    (zebra-font-lock))
  (major-mode-define))

(defun bgp-mode ()
  (progn
    (setq mode-name "bgp") 
    (bgp-font-lock))
  (major-mode-define))

(defun rip-mode ()
  (progn
    (setq mode-name "rip")
    (rip-font-lock))
  (major-mode-define))
