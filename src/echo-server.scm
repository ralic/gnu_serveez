;; -*-scheme-*-
;;
;; echo-server.scm - example server completely written in guile
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
;; $Id: echo-server.scm,v 1.3 2001/07/12 20:23:52 ela Exp $
;;

(primitive-load "serveez.scm")

(define (echo-global-init servertype)
  (println "Running echo global init " servertype ".")
  0)

(define (echo-init server)
  (println "Running echo init " server ".")
  0)

(define (echo-global-finalize servertype)
  (println "Running echo global finalizer " servertype ".")
  0)

(define (echo-finalize server)
  (println "Running echo finalizer " server ".")
  0)

(define (echo-detect-proto server sock)
  (println "Detecting echo protocol ...")
  1)

(define (echo-handle-request sock request len)
  (define ret '())
  (if (and (>= len 4) (equal? (substring request 0 4) "quit"))
      (set! ret -1)
      (begin
	(svz:sock:write sock (string-append "Echo: " request) (+ len 6))
	(set! ret 0)))
  ret)

(define (echo-connect-socket server sock)
  (println "Running connect socket.")
  (svz:sock:boundary sock "\n")
  (svz:sock:handle-request sock echo-handle-request)
  (svz:sock:write sock "Hello, type `quit' to end the connection.\n" 42)
  0)

;; Servertype definitions.
(define-servertype! '(
		      (prefix      . "echo")
		      (description . "guile echo server")
		      (detect-proto    . echo-detect-proto)
		      (global-init     . echo-global-init)
		      (init            . echo-init)
		      (finalize        . echo-finalize)
		      (global-finalize . echo-global-finalize)
		      (connect-socket  . echo-connect-socket)
		      ))

;; Port configuration.
(define-port! 'echo-port '((proto . tcp)
			   (port  . 2001)))

;; Server instantiation.
(define-server! 'echo-server)

;; Bind server to port.
(bind-server! 'echo-port 'echo-server)
