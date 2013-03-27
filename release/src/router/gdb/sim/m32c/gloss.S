;;; gloss.S --- system calls for sample2.x
;;; 
;;; Copyright (C) 2005, 2007 Free Software Foundation, Inc.
;;; Contributed by Red Hat, Inc.
;;;
;;; This file is part of the GNU simulators.
;;;
;;; This program is free software; you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation; either version 3 of the License, or
;;; (at your option) any later version.
;;;
;;; This program is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; You should have received a copy of the GNU General Public License
;;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

;;; See the 'sample2.x' target in Makefile.in.
	.global	_exit
_exit:
	mov.b	#1,r0l
	ste.b	r0l,0xe0000
	rts

	.global	_foo
_foo:
	mov.b	#2,r0l
	ste.b	r0l,0xe0000
	rts
