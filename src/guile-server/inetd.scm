;; -*-scheme-*-
;;
;; inetd.scm - replacement for the inetd
;;
;; Copyright (C) 2001 Stefan Jahn <stefan@lkcc.org>
;;
;; This is free software; you can redistribute it and/or modify it
;; under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.
;; 
;; This software is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; General Public License for more details.
;; 
;; You should have received a copy of the GNU General Public License
;; along with this package; see the file COPYING.  If not, write to
;; the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.
;;
;; $Id: inetd.scm,v 1.1 2001/11/25 23:08:27 raimi Exp $
;;

;; opens a configuration file, reads each line and uncomments them,
;; comments are marked with a leading '#', returns a list of non-empty
;; lines
(define (read-config file)
  (let* ((f (open-input-file file))
	 (lines '()))
    (let loop ((line (read-line f)))
      (if (not (eof-object? line))
	  (let ((n (string-index line #\#)))
	    (if n
		(set! line (substring line 0 n)))
	    (if (> (string-length line) 0)
		(set! lines (append lines `(,line))))
	    (loop (read-line f)))))
    (close-input-port f)
    lines))

;; removes leading white spaces from the given string and returns it
(define (crop-spaces string)
  (let ((ret string))
    (if (> (string-length string) 0)
	(let loop ((c (string-ref string 0)))
	  (if (<= (char->integer c) (char->integer #\space))
	      (begin
		(set! string (substring string 1))
		(loop (string-ref string 0))))))
    string))

;; returns the next position of a white space character in the given 
;; string or #f if there is no further space or the string was empty
(define (next-space-position string)
  (let ((i 0))
    (let loop ((i 0))
      (if (< i (string-length string))
	  (if (<= (char->integer (string-ref string i)) 
		  (char->integer #\space))
	      i
	      (loop (1+ i)))
	  #f))))

;; tokenizes a given string into a vector, if TOKENS is a number the 
;; procedure parses this number of tokens and stores the remaining tokens as
;; a variable length vector in the last element of the returned vector, 
;; otherwise it returns a variable length vector
(define (string-split string tokens)
  (let* ((vector (if (number? tokens) (make-vector (1+ tokens)) '())))
    (define (next-token string i)
      (if i (substring string 0 i) string))
    (let loop ((n 0))
      (set! string (crop-spaces string))
      (let* ((i (next-space-position string)))
	(if (number? tokens)
	    (vector-set! vector n (next-token string i))
	    (set! vector (append vector `(,(next-token string i)))))
	(if i
	    (set! string (substring string i))
	    (set! string ""))
	(if (and i (or (not (number? tokens)) (< n (1- tokens))))
	    (loop (1+ n)))))
    (if (number? tokens)
	(vector-set! vector tokens (string-split string #t))
	(set! vector (list->vector vector)))
    vector))

;; creates a port configuration for Serveez
(define (create-portcfg line)
  (let* ((service (getservbyname (vector-ref line 0) (vector-ref line 2)))
	 (port '()) (name ""))
    (set! port (append port `(,(cons "proto" (vector-ref service 3)))))
    (set! port (append port `(,(cons "port" (vector-ref service 2)))))
    (set! name (string-append "prog-port-"
			      (vector-ref service 3)
			      "-"
			      (number->string (vector-ref service 2))))
    (define-port! name port)
    name))

;; creates a program passthrough server for Serveez
(define (create-server line)
  (let* ((service (getservbyname (vector-ref line 0) (vector-ref line 2)))
	 (server '()) (name "") (argv '()))
    (set! server (append server `(,(cons "binary" (vector-ref line 5)))))
    (set! server (append server `(,(cons "directory" "/tmp"))))
    (set! server (append server `(,(cons "user" (vector-ref line 4)))))
    (set! argv (vector->list (vector-ref line 6)))
    (if (and (> (length argv) 0)
	     (not (equal? (vector-ref (vector-ref line 6) 0) "")))
	(set! server (append server `(,(cons "argv" argv)))))
    (set! server (append server `(,(cons "do-fork" #t))))
    (set! server (append server `(,(cons "single-threaded"
					 (equal? (vector-ref line 3)
						 "wait")))))
    (set! name (string-append "prog-server-"
			      (vector-ref service 3)
			      "-"
			      (number->string (vector-ref service 2))))
    (define-server! name server)
    name))

;; glues the above port configurations and servers together
(define (create-bindings lines)
  (for-each (lambda (x)
	      (let ((service (string-split x 6)))
		(bind-server! (create-portcfg service)
			      (create-server service))))
	    lines))

;; the inetd configuration file
(define config-file "/etc/inetd.conf")

;; main program entry point
(define (inetd-main)
  (let ((lines (read-config config-file)))
    (create-bindings lines)))

;; run
(inetd-main)
