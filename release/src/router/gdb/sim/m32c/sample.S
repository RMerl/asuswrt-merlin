;;; sample.S --- simple test program for M32C simulator
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

;;; See the 'sample.x' and sample.mot targets in Makefile.in.

	.text

	.global _start
_start:
	mov.w	#0x1234,r1
	mov.w r1,r3 | sha.w #-8,r3 | sha.w #-7,r3
	brk
