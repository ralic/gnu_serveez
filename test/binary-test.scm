;; -*-scheme-*-
;;
;; binary-test.scm - binary function test suite
;;
;; Copyright (C) 2002 Stefan Jahn <stefan@lkcc.org>
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
;; $Id: binary-test.scm,v 1.1 2002/07/25 13:45:37 ela Exp $
;;

;; Load the test suite module.
(use-modules (test-suite))

;; Run the test suite for the binary smob used in serveez.
(test-suite "binary function test suite"

  (pass-if "predicate"
	   (and (not (binary? "foo"))
		(binary? (string->binary "foo"))))

  (pass-if "string conversion"
	   (and (equal? "foo" (binary->string (string->binary "foo")))
		(equal? "" (binary->string (string->binary "")))))

  (pass-if-exception "search wrong argument type 1"
		     'wrong-type-arg
		     (binary-search (string->binary "foo") 'foo))

  (pass-if-exception "search wrong argument type 2"
		     'wrong-type-arg
		     (binary-search (string->binary "foo") 0.5))

  (pass-if "searching strings"
	   (let ((foobar (string->binary "foobar")))
	     (and (not (binary-search foobar "x"))
		  (= 0 (binary-search foobar "foo"))
		  (= 0 (binary-search foobar "foobar"))
		  (not (binary-search foobar "foobar_"))
		  (not (binary-search foobar ""))
		  (= 1 (binary-search foobar "oo"))
		  (= 3 (binary-search foobar "bar"))
		  (= 5 (binary-search foobar "r")))))

  (pass-if "searching binaries"
	   (let ((foobar (string->binary "foobar")))
	     (and (not (binary-search foobar (string->binary "x")))
		  (= 0 (binary-search foobar (string->binary "foo")))
		  (= 0 (binary-search foobar (string->binary "foobar")))
		  (not (binary-search foobar (string->binary "foobar_")))
		  (not (binary-search foobar (string->binary "")))
		  (= 1 (binary-search foobar (string->binary "oo")))
		  (= 3 (binary-search foobar (string->binary "bar")))
		  (= 5 (binary-search foobar (string->binary "r"))))))

  (pass-if "searching characters"
	   (let ((foobar (string->binary "foobar")))
	     (and (not (binary-search foobar #\x))
		  (= 0 (binary-search foobar #\f))
		  (= 1 (binary-search foobar #\o))
		  (= 3 (binary-search foobar #\b))
		  (= 5 (binary-search foobar #\r)))))

  (pass-if "searching numbers"
	   (let ((foobar (string->binary "foobar")))
	     (and (not (binary-search foobar (char->integer #\x)))
		  (= 0 (binary-search foobar (char->integer #\f)))
		  (not (binary-search foobar 0))
		  (= 1 (binary-search foobar (char->integer #\o)))
		  (= 3 (binary-search foobar (char->integer #\b)))
		  (= 5 (binary-search foobar (char->integer #\r))))))
)
